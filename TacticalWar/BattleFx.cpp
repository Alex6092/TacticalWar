#include "BattleFx.h"
#include "ClientGameData.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <BattleRules.h>
#include <Camera.h>
#include <nlohmann/json.hpp>

using nlohmann::json;
using namespace tw::battle;

const char * BattleFx::CATALOG_PATH = "./assets/spellsprites/effects.json";
const float BattleFx::WINDUP_SECONDS = 0.3f;
const float BattleFx::SLIDE_CELLS_PER_SECOND = 10.f;

namespace
{
	const float PROJECTILE_WINDUP_SECONDS = 0.2f;
	// Hauteur d'une case de trajectoire, en pixels du monde (arc des projectiles).
	const float ARC_PIXELS_PER_CELL = 60.f;
	const float PI = 3.14159265f;

	sf::Color parseColor(const json & value, sf::Color fallback)
	{
		if (!value.is_array() || value.size() < 3)
			return fallback;
		return sf::Color((sf::Uint8)value[0].get<int>(), (sf::Uint8)value[1].get<int>(), (sf::Uint8)value[2].get<int>(),
			(sf::Uint8)(value.size() > 3 ? value[3].get<int>() : 255));
	}
}

BattleFx::BattleFx()
{
}

bool BattleFx::dashes(const SpellDef & spell)
{
	for (const tw::battle::EffectDef & effect : spell.effects)
	{
		if (effect.type == EffectType::DASH)
			return true;
	}
	return false;
}

bool BattleFx::loadCatalog(const std::string & path, std::string * error, const std::string & assetRoot)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		if (error != nullptr)
			*error = "Catalogue d'effets introuvable : " + path;
		return false;
	}
	std::stringstream content;
	content << file.rdbuf();
	json root = json::parse(content.str(), nullptr, false);
	if (root.is_discarded() || !root.is_object())
	{
		if (error != nullptr)
			*error = "Catalogue d'effets invalide : " + path;
		return false;
	}

	std::map<std::string, EffectDef> loaded;
	json effectsJson = root.value("effects", json::object());
	for (auto it = effectsJson.begin(); it != effectsJson.end(); ++it)
	{
		const json & item = it.value();
		EffectDef def;
		def.sheet = item.value("sheet", std::string());
		if (def.sheet.compare(0, 2, "./") == 0)
			def.sheet = def.sheet.substr(2);
		if (!def.sheet.empty())
			def.sheet = assetRoot + def.sheet;
		def.fps = item.value("fps", def.fps);
		def.loop = item.value("loop", def.loop);
		def.scale = item.value("scale", def.scale);
		if (item.contains("anchor") && item["anchor"].is_array() && item["anchor"].size() == 2)
		{
			def.anchorX = item["anchor"][0].get<float>();
			def.anchorY = item["anchor"][1].get<float>();
		}
		def.offsetY = item.value("offsetY", def.offsetY);
		def.layer = item.value("layer", std::string("top")) == "ground" ? SpellView::Layer::GROUND : SpellView::Layer::TOP;
		def.rotate = item.value("rotate", def.rotate);
		def.additive = item.value("additive", def.additive);
		def.color = parseColor(item.value("color", json()), def.color);
		def.color.a = (sf::Uint8)(def.color.a * std::max(0.f, std::min(1.f, item.value("alpha", 1.f))));
		def.duration = item.value("duration", def.duration);
		loaded[it.key()] = def;
	}

	std::map<std::string, std::string> loadedEvents;
	json eventsJson = root.value("events", json::object());
	for (auto it = eventsJson.begin(); it != eventsJson.end(); ++it)
	{
		if (it.value().is_string())
			loadedEvents[it.key()] = it.value().get<std::string>();
	}

	// Les vues existantes pointent vers les anciennes définitions : on repart de zéro.
	clear();
	effects = loaded;
	events = loadedEvents;

	// Planches chargées dès maintenant : pas d'à-coup au premier lancement de chaque sort.
	for (const auto & entry : effects)
		SpellView::preload(entry.second.sheet);
	return true;
}

const BattleFx::EffectDef * BattleFx::find(const std::string & name) const
{
	auto it = effects.find(name);
	return it == effects.end() ? nullptr : &it->second;
}

const SpellDef * BattleFx::spellById(const std::string & spellId) const
{
	for (const ClassDef & classDef : ClientGameData::get().data().classes)
	{
		for (const SpellDef & spell : classDef.spells)
		{
			if (spell.id == spellId)
				return &spell;
		}
	}
	return nullptr;
}

bool BattleFx::fighterCell(int fighterId, sf::Vector2f & cell) const
{
	return positionOf && positionOf(fighterId, cell);
}

BattleFx::Instance * BattleFx::spawn(const std::string & name, sf::Vector2f cell, float delay)
{
	const EffectDef * def = find(name);
	if (def == nullptr)
		return nullptr;

	Instance instance;
	instance.view.reset(new SpellView((int)std::lround(cell.x), (int)std::lround(cell.y)));
	if (!instance.view->loadAnimation(def->sheet))
		return nullptr;

	SpellView & view = *instance.view;
	view.setPosition(cell.x, cell.y);
	view.setHeight(def->offsetY);
	view.setScale(def->scale);
	view.setAnchor(def->anchorX, def->anchorY);
	view.setColor(def->color);
	view.setLayer(def->layer);
	view.setAdditive(def->additive);
	view.setFrameRate(def->fps);
	view.setLoop(def->loop);

	instance.def = def;
	instance.delay = delay;
	instance.limited = def->loop && def->duration > 0;
	instance.remaining = def->duration;
	instances.push_back(std::move(instance));
	return &instances.back();
}

void BattleFx::removeWhere(const std::function<bool(const Instance &)> & predicate)
{
	instances.erase(std::remove_if(instances.begin(), instances.end(), predicate), instances.end());
}

float BattleFx::castSpell(const BattleMap & map, const SpellDef & spell, int casterId, const Cell & casterCell, const Cell & target, bool fast)
{
	if (fast)
		return 0;

	const SpellVisual & visual = spell.visual;
	sf::Vector2f from((float)casterCell.x, (float)casterCell.y);
	sf::Vector2f to((float)target.x, (float)target.y);

	// Ancien format : une seule animation ("fx"), jouée sur la case visée.
	std::string impact = visual.impact;
	if (impact.empty() && !spell.fxSprite.empty())
	{
		impact = "@" + spell.fxSprite;
		if (effects.find(impact) == effects.end())
		{
			EffectDef def;
			def.sheet = spell.fxSprite;
			def.fps = 16.f;
			effects[impact] = def;
		}
	}

	if (!visual.cast.empty())
		spawn(visual.cast, from, 0);

	float impactDelay = WINDUP_SECONDS;
	if (dashes(spell))
	{
		// Le lanceur s'arrête sur la case voisine de la cible.
		int steps = std::max(std::abs(target.x - casterCell.x), std::abs(target.y - casterCell.y)) - 1;
		impactDelay += std::max(0, steps) / SLIDE_CELLS_PER_SECOND;
	}
	else if (!visual.projectile.empty() && from != to)
	{
		float distance = std::sqrt((to.x - from.x) * (to.x - from.x) + (to.y - from.y) * (to.y - from.y));
		float flight = std::max(0.1f, distance / std::max(1.f, visual.projectileSpeed));
		Instance * projectile = spawn(visual.projectile, from, PROJECTILE_WINDUP_SECONDS);
		if (projectile != nullptr)
		{
			projectile->projectile = true;
			projectile->from = from;
			projectile->to = to;
			projectile->flight = flight;
			projectile->arc = visual.projectileArc * ARC_PIXELS_PER_CELL;
			projectile->view->setLoop(true);
			impactDelay = PROJECTILE_WINDUP_SECONDS + flight;
		}
	}

	if (!impact.empty())
	{
		if (visual.impactOn == "cells")
		{
			for (const Cell & cell : impactCells(map, casterCell, target, spell.impact))
				spawn(impact, sf::Vector2f((float)cell.x, (float)cell.y), impactDelay);
		}
		else
		{
			spawn(impact, visual.impactOn == "caster" ? from : to, impactDelay);
		}
	}

	if (!visual.impactSound.empty())
		pendingSounds.push_back({ impactDelay, visual.impactSound });
	return impactDelay;
}

void BattleFx::effectAdded(int fighterId, int effectUid, const std::string & spellId)
{
	const SpellDef * spell = spellById(spellId);
	if (spell == nullptr || spell->visual.status.empty())
		return;

	// Un seul visuel par combattant et par sort, même si le sort pose plusieurs effets.
	std::string key = std::to_string(fighterId) + ":" + spellId;
	statusOfUid[effectUid] = key;
	Status & status = statuses[key];
	bool first = status.uids.empty();
	status.uids.insert(effectUid);
	if (!first)
		return;

	sf::Vector2f cell;
	if (!fighterCell(fighterId, cell))
		return;
	Instance * instance = spawn(spell->visual.status, cell, 0);
	if (instance != nullptr)
	{
		instance->follow = fighterId;
		instance->statusKey = key;
		instance->limited = false;
		instance->view->setLoop(true);
	}
}

void BattleFx::effectRemoved(int effectUid)
{
	auto it = statusOfUid.find(effectUid);
	if (it == statusOfUid.end())
		return;

	std::string key = it->second;
	statusOfUid.erase(it);
	auto status = statuses.find(key);
	if (status == statuses.end())
		return;
	status->second.uids.erase(effectUid);
	if (status->second.uids.empty())
	{
		statuses.erase(status);
		removeWhere([&key](const Instance & instance) { return instance.statusKey == key; });
	}
}

void BattleFx::periodic(int fighterId, const std::string & spellId)
{
	const SpellDef * spell = spellById(spellId);
	sf::Vector2f cell;
	if (spell != nullptr && !spell->visual.tick.empty() && fighterCell(fighterId, cell))
		spawn(spell->visual.tick, cell, 0);
}

void BattleFx::glyphAdded(int glyphUid, const std::string & spellId, const std::vector<Cell> & cells)
{
	glyphSpells[glyphUid] = spellId;
	const SpellDef * spell = spellById(spellId);
	if (spell == nullptr || spell->visual.glyph.empty())
		return;

	for (const Cell & cell : cells)
	{
		Instance * instance = spawn(spell->visual.glyph, sf::Vector2f((float)cell.x, (float)cell.y), 0);
		if (instance != nullptr)
		{
			instance->glyph = glyphUid;
			instance->limited = false;
			instance->view->setLoop(true);
		}
	}
}

void BattleFx::glyphTriggered(int glyphUid, int fighterId)
{
	auto it = glyphSpells.find(glyphUid);
	const SpellDef * spell = it != glyphSpells.end() ? spellById(it->second) : nullptr;
	sf::Vector2f cell;
	if (spell != nullptr && !spell->visual.glyphTrigger.empty() && fighterCell(fighterId, cell))
		spawn(spell->visual.glyphTrigger, cell, 0);
}

void BattleFx::glyphRemoved(int glyphUid)
{
	glyphSpells.erase(glyphUid);
	removeWhere([glyphUid](const Instance & instance) { return instance.glyph == glyphUid; });
}

void BattleFx::playEvent(const std::string & name, int fighterId)
{
	auto it = events.find(name);
	sf::Vector2f cell;
	if (it != events.end() && fighterCell(fighterId, cell))
		spawn(it->second, cell, 0);
}

void BattleFx::playEffect(const std::string & name, sf::Vector2f cell, float delay)
{
	spawn(name, cell, delay);
}

void BattleFx::fighterRemoved(int fighterId)
{
	// Combattant hors combat : ses effets durables disparaissent avec lui.
	std::string prefix = std::to_string(fighterId) + ":";
	for (auto it = statuses.begin(); it != statuses.end();)
	{
		if (it->first.compare(0, prefix.size(), prefix) == 0)
		{
			for (int uid : it->second.uids)
				statusOfUid.erase(uid);
			it = statuses.erase(it);
		}
		else
		{
			++it;
		}
	}
	removeWhere([fighterId](const Instance & instance) { return instance.follow == fighterId; });
}

void BattleFx::rebuild(const BattleState & state)
{
	clear();
	for (const Fighter & fighter : state.fighters)
	{
		if (!fighter.alive)
			continue;
		for (const ActiveEffect & effect : fighter.effects)
			effectAdded(fighter.id, effect.uid, effect.spellId);
	}
	for (const Glyph & glyph : state.glyphs)
		glyphAdded(glyph.uid, glyph.spellId, glyph.cells);
}

void BattleFx::clear()
{
	instances.clear();
	statuses.clear();
	statusOfUid.clear();
	glyphSpells.clear();
	pendingSounds.clear();
}

void BattleFx::update(float deltatime)
{
	for (auto it = pendingSounds.begin(); it != pendingSounds.end();)
	{
		it->delay -= deltatime;
		if (it->delay <= 0)
		{
			if (playSound)
				playSound(it->path);
			it = pendingSounds.erase(it);
		}
		else
		{
			++it;
		}
	}

	for (Instance & instance : instances)
	{
		float step = deltatime;
		if (instance.delay > 0)
		{
			instance.delay -= deltatime;
			if (instance.delay > 0)
				continue;
			step = -instance.delay;
			instance.delay = 0;
		}
		instance.view->update(step);

		sf::Vector2f cell;
		if (instance.follow >= 0 && fighterCell(instance.follow, cell))
			instance.view->setPosition(cell.x, cell.y);

		if (instance.projectile)
		{
			// Trajet en ligne droite (cases), avec un arc vertical ; l'image suit la tangente.
			instance.travelled += step;
			float t = std::min(1.f, instance.travelled / instance.flight);
			instance.view->setPosition(instance.from.x + (instance.to.x - instance.from.x) * t, instance.from.y + (instance.to.y - instance.from.y) * t);
			instance.view->setHeight(instance.def->offsetY - instance.arc * 4.f * t * (1.f - t));
			if (instance.def->rotate)
			{
				sf::Vector2f a = tw::Camera::cellToWorld(instance.from.x, instance.from.y);
				sf::Vector2f b = tw::Camera::cellToWorld(instance.to.x, instance.to.y);
				float dx = b.x - a.x;
				float dy = (b.y - a.y) - instance.arc * 4.f * (1.f - 2.f * t);
				instance.view->setRotation(std::atan2(dy, dx) * 180.f / PI);
			}
		}

		if (instance.limited)
			instance.remaining -= step;
	}

	removeWhere([](const Instance & instance) {
		if (instance.delay > 0)
			return false;
		if (instance.projectile)
			return instance.travelled >= instance.flight;
		if (instance.limited)
			return instance.remaining <= 0;
		return instance.follow < 0 && instance.glyph < 0 && instance.view->isFinished();
	});
}

void BattleFx::collectViews(std::vector<tw::AbstractSpellView<sf::Sprite*>*> & views)
{
	for (Instance & instance : instances)
	{
		if (instance.delay <= 0)
			views.push_back(instance.view.get());
	}
}
