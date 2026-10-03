#include "FxGalleryScreen.h"

#include <algorithm>
#include <climits>
#include <fstream>
#include <iostream>

#include <BattleRules.h>
#include <EnvironmentManager.h>

#include "ClientGameData.h"
#include "LinkToServer.h"
#include "MusicManager.h"

using namespace tw;
using nlohmann::json;

namespace
{
	const char * GAMEDATA_FILE = "assets/data/gamedata.json";
	const char * CATALOG_FILE = "assets/spellsprites/effects.json";
	// Zoom de la caméra sur les personnages (moins de 1 : plus gros qu'au zoom par défaut).
	const float GALLERY_ZOOM = 0.6f;

	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}

	// Classe d'un personnage factice (par sa clé), à défaut la première classe.
	int classIdByKey(const battle::GameData & data, const std::string & key)
	{
		for (const battle::ClassDef & classDef : data.classes)
		{
			if (classDef.key == key)
				return classDef.id;
		}
		return data.classes.empty() ? 0 : data.classes.front().id;
	}

	bool hasEffect(const battle::SpellDef & spell, battle::EffectType type)
	{
		for (const battle::EffectDef & effect : spell.effects)
		{
			if (effect.type == type)
				return true;
		}
		return false;
	}

	sf::String fileName(const std::string & path)
	{
		std::size_t slash = path.find_last_of("/\\");
		return fromServerText(slash == std::string::npos ? path : path.substr(slash + 1));
	}

}

FxGalleryScreen::FxGalleryScreen(tgui::Gui * gui, const std::string & spellId, int environmentId)
	: BattleScreen(gui, existingEnvironment(environmentId), Mode::PLAYER),
	nowMs(0), current(0), shownClass(-1), focusPending(true), step(Step::PAUSE), wait(0), command(Command::NONE)
{
	// Pas de musique : on écoute les sons des sorts.
	MusicManager::getInstance()->stopMusic();
	// La caméra est placée sur les personnages plutôt que sur toute la carte.
	cameraFitted = true;
	baseMap = map;

	std::ifstream repository("../../" + std::string(CATALOG_FILE));
	root = repository ? "../../" : "./";
	reload(false);

	label = tgui::Label::create();
	label->setInheritedFont(font);
	label->setTextSize(14);
	label->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
	label->getRenderer()->setTextColor(sf::Color::White);
	label->getRenderer()->setTextOutlineColor(sf::Color::Black);
	label->getRenderer()->setTextOutlineThickness(2);
	gui->add(label);

	int index = 0;
	for (std::size_t i = 0; i < entries.size(); i++)
	{
		if (entries[i].spellId == spellId)
			index = (int)i;
	}
	if (!spellId.empty() && (entries.empty() || entries[index].spellId != spellId))
		std::cout << "Galerie des effets : sort inconnu " << spellId << std::endl;
	select(index);
}

int FxGalleryScreen::existingEnvironment(int requested)
{
	for (int id = 0; id <= 100; id++)
	{
		int candidate = id == 0 ? requested : id;
		Environment * environment = EnvironmentManager::getInstance()->loadEnvironment(candidate);
		if (environment != NULL)
		{
			delete environment;
			return candidate;
		}
	}
	return requested;
}

//----------------------------------------------------------
// Boucle principale
//----------------------------------------------------------

void FxGalleryScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	BattleScreen::handleEvents(window, gui);

	if (focusPending)
	{
		focusCamera();
		focusPending = false;
	}
	label->setPosition((window->getSize().x - label->getSize().x) / 2.f, 48.f);
}

void FxGalleryScreen::update(float deltatime)
{
	Command pending = command;
	command = Command::NONE;
	switch (pending)
	{
	case Command::PREVIOUS: select(current - 1); break;
	case Command::NEXT: select(current + 1); break;
	case Command::REPLAY: restart(); break;
	case Command::RELOAD: reload(true); restart(); break;
	case Command::QUIT:
		if (window != NULL)
			window->close();
		break;
	default: break;
	}

	// Le moteur local suit le temps réel ; ses minuteurs de tour ne sont jamais appliqués.
	nowMs += (std::int64_t)(deltatime * 1000);

	// Étape suivante de la démonstration quand les animations en cours sont terminées.
	if (engine && idle())
	{
		wait -= deltatime;
		if (wait <= 0)
			advance();
	}

	BattleScreen::update(deltatime);
}

void FxGalleryScreen::onEvent(void * e)
{
	// Les commandes sont exécutées dans update() : les événements arrivent pendant l'affichage,
	// qui utilise encore les vues des personnages et des effets.
	sf::Event * event = (sf::Event*)e;
	if (event != NULL && event->type == sf::Event::KeyPressed)
	{
		switch (event->key.code)
		{
		case sf::Keyboard::Left: command = Command::PREVIOUS; return;
		case sf::Keyboard::Right: command = Command::NEXT; return;
		case sf::Keyboard::R: command = Command::REPLAY; return;
		case sf::Keyboard::F5: command = Command::RELOAD; return;
		case sf::Keyboard::Escape:
			// Échap annule d'abord un sort sélectionné.
			if (selectedSpell < 0)
			{
				command = Command::QUIT;
				return;
			}
			break;
		default: break;
		}
	}

	BattleScreen::onEvent(e);
}

void FxGalleryScreen::onCellHover(int cellX, int cellY)
{
	// Pendant la visée montrée par la galerie (sort sélectionné hors du mode manuel), la souris ne
	// déplace pas la case survolée.
	if (step == Step::MANUAL || selectedSpell < 0)
		BattleScreen::onCellHover(cellX, cellY);
}

void FxGalleryScreen::sendToServer(const std::string & op, const json & body)
{
	if (!engine)
		return;

	// Signal d'équipe : montré tout de suite (pas de coéquipier humain), resynchronisation : état
	// complet du moteur local.
	if (op == "CG")
	{
		onMessageReceived("BG" + json({ { "f", you }, { "x", body.value("x", -1) }, { "y", body.value("y", -1) } }).dump());
		return;
	}
	if (op == "BR")
	{
		deliver();
		onMessageReceived("BI" + engine->snapshot(you, nowMs).dump());
		return;
	}
	if (op == "CE")
	{
		battle::ActionResult emoted = engine->emote(you, body.value("id", -1), nowMs);
		if (!emoted.ok)
			onMessageReceived("ER" + json({ { "message", emoted.error } }).dump());
		deliver();
		return;
	}

	// Action à la souris : la démonstration automatique s'arrête (R pour la reprendre).
	step = Step::MANUAL;
	wait = 0.8f;

	battle::ActionResult result = battle::ActionResult::failure("Action inconnue.");
	if (op == "CL")
	{
		result = engine->cast(you, body.value("slot", -1), { body.value("x", 0), body.value("y", 0) }, nowMs);
	}
	else if (op == "Cm")
	{
		std::vector<battle::Cell> path;
		for (const json & cell : body.value("path", json::array()))
			path.push_back({ cell.at(0).get<int>(), cell.at(1).get<int>() });
		result = engine->move(you, path, nowMs);
	}
	else if (op == "Ct")
	{
		result = engine->endTurn(you, nowMs);
	}

	if (!result.ok)
		onMessageReceived("ER" + json({ { "message", result.error } }).dump());
	if (deliver())
	{
		step = Step::PAUSE;
		wait = 2.f;
	}
}

//----------------------------------------------------------
// Démonstration
//----------------------------------------------------------

void FxGalleryScreen::buildEntries()
{
	entries.clear();
	for (const battle::ClassDef & classDef : ClientGameData::get().data().classes)
	{
		for (int slot = 0; slot < (int)classDef.spells.size(); slot++)
			entries.push_back({ classDef.id, slot, classDef.spells[slot].id });
	}
}

const battle::ClassDef * FxGalleryScreen::currentClass() const
{
	if (current < 0 || current >= (int)entries.size())
		return nullptr;
	return ClientGameData::get().data().findClass(entries[current].classId);
}

const battle::SpellDef * FxGalleryScreen::currentSpell() const
{
	const battle::ClassDef * classDef = currentClass();
	if (classDef == nullptr || entries[current].slot >= (int)classDef->spells.size())
		return nullptr;
	return &classDef->spells[entries[current].slot];
}

void FxGalleryScreen::select(int index)
{
	if (entries.empty())
		return;
	int count = (int)entries.size();
	current = ((index % count) + count) % count;
	restart();
}

void FxGalleryScreen::reload(bool announce)
{
	std::string dataError;
	std::string fxError;
	bool dataLoaded = ClientGameData::get().loadFromFile(root + GAMEDATA_FILE, dataError);
	// Plus aucun effet affiché : les planches peuvent être relues depuis les fichiers.
	fx.clear();
	SpellView::clearCache();
	bool fxLoaded = fx.loadCatalog(root + CATALOG_FILE, &fxError, root);

	std::string selected = current < (int)entries.size() ? entries[current].spellId : std::string();
	buildEntries();
	layouts.clear();
	current = 0;
	for (std::size_t i = 0; i < entries.size(); i++)
	{
		if (entries[i].spellId == selected)
			current = (int)i;
	}

	if (!dataLoaded)
		std::cout << dataError << std::endl;
	if (!fxLoaded)
		std::cout << fxError << std::endl;
	if (announce)
	{
		if (dataLoaded && fxLoaded)
			hud->showMessage(L"Fichiers relus", sf::Color(120, 255, 120), 1.2f);
		else
			hud->showMessage(fromServerText(dataLoaded ? fxError : dataError), sf::Color(255, 110, 90), 5.f);
	}
}

void FxGalleryScreen::restart()
{
	engine.reset();
	step = Step::PAUSE;
	wait = 0;
	refreshLabel();

	const battle::ClassDef * classDef = currentClass();
	const battle::SpellDef * spell = currentSpell();
	if (classDef == nullptr || spell == nullptr)
		return;

	std::string error;
	Layout chosen;
	auto known = layouts.find(spell->id);
	bool found = known != layouts.end();
	if (found)
		chosen = known->second;
	else
		found = findLayout(*classDef, *spell, chosen, error);
	if (found)
		engine = createEngine(*classDef, *spell, chosen, error);
	if (!engine)
	{
		hud->showMessage(L"Impossible de lancer " + fromServerText(spell->name) + L" sur cette carte : " + fromServerText(error),
			sf::Color(255, 110, 90), 5.f);
		return;
	}

	layouts[spell->id] = chosen;
	if (!(chosen == layout))
		focusPending = true;
	layout = chosen;

	// Lanceur d'une autre classe : les personnages sont recréés.
	if (shownClass != classDef->id)
	{
		for (auto & entry : views)
			delete entry.second;
		views.clear();
		shownClass = classDef->id;
	}

	onMessageReceived("BI" + engine->snapshot(CASTER, nowMs).dump());
	step = Step::AIM_ZONE;
	wait = 0.4f;
}

void FxGalleryScreen::advance()
{
	const battle::BattleState & state = engine->getState();
	bool fighting = state.phase == battle::BattlePhase::FIGHT;
	int active = state.activeFighterId();

	switch (step)
	{
	case Step::AIM_ZONE:
	{
		selectSpell(entries[current].slot);
		const battle::Fighter * caster = truth.findFighter(CASTER);
		const battle::SpellDef * spell = currentSpell();
		battle::Cell aim = { -1, -1 };
		if (selectedSpell >= 0 && caster != NULL && spell != nullptr)
		{
			// Case à portée mais pas ciblable, la plus proche de la cible : la raison s'affiche.
			const battle::GameData & data = ClientGameData::get().data();
			std::vector<battle::Cell> castable = battle::castableCells(truth, map, data, *caster, *spell);
			int best = INT_MAX;
			for (const battle::Cell & cell : battle::launchCells(truth, map, data, *caster, *spell))
			{
				bool shown = map.isWalkable(cell) || truth.fighterAt(cell) != NULL;
				bool valid = std::find(castable.begin(), castable.end(), cell) != castable.end();
				if (shown && !valid && battle::manhattan(cell, layout.target) < best)
				{
					best = battle::manhattan(cell, layout.target);
					aim = cell;
				}
			}
		}
		aimAt(aim);
		step = Step::AIM_TARGET;
		wait = aim.x >= 0 ? 1.f : 0.6f;
		break;
	}
	case Step::AIM_TARGET:
		aimAt(layout.target);
		step = Step::CAST;
		wait = 1.f;
		break;
	case Step::CAST:
	{
		selectSpell(-1);
		aimAt({ -1, -1 });
		const battle::SpellDef * spell = currentSpell();
		battle::ActionResult result = spell != nullptr ? engine->cast(CASTER, entries[current].slot, layout.target, nowMs)
			: battle::ActionResult::failure("Sort introuvable.");
		if (!result.ok)
		{
			hud->showMessage(fromServerText(result.error), sf::Color(255, 110, 90), 3.f);
			step = Step::PAUSE;
			wait = 3.f;
			return;
		}

		// Effets qui durent : le tour suivant est joué (dégâts périodiques, glyphe qui se déclenche).
		const battle::SpellVisual & visual = spell->visual;
		bool lasting = !visual.status.empty() || !visual.tick.empty() || !visual.glyph.empty();
		bool ended = deliver();
		step = lasting && !ended ? Step::AFTER_CAST : Step::PAUSE;
		wait = lasting ? 1.f : 1.5f;
		break;
	}
	case Step::AFTER_CAST:
		if (fighting && active == CASTER)
			engine->endTurn(CASTER, nowMs);
		step = deliver() ? Step::PAUSE : Step::ROUND;
		wait = 0.8f;
		break;
	case Step::ROUND:
		if (fighting && active != CASTER)
		{
			engine->endTurn(active, nowMs);
			if (deliver())
				step = Step::PAUSE;
			wait = 0.8f;
		}
		else
		{
			step = Step::PAUSE;
			wait = 1.5f;
		}
		break;
	case Step::PAUSE:
		restart();
		break;
	case Step::MANUAL:
		// Les autres personnages passent leur tour.
		if (fighting && active != CASTER)
		{
			engine->endTurn(active, nowMs);
			if (deliver())
				step = Step::PAUSE;
		}
		wait = 0.8f;
		break;
	}
}

void FxGalleryScreen::aimAt(const battle::Cell & cell)
{
	hoveredCell = cell;
	const battle::Fighter * fighter = truth.fighterAt(cell);
	hoveredFighter = fighter != NULL ? fighter->id : -1;
}

bool FxGalleryScreen::deliver()
{
	if (!engine || !engine->hasPendingEvents())
		return false;

	// Pas de fin de combat dans la galerie (écran de fin, retour à l'attente) : la scène est rejouée.
	json batch = engine->flushEvents();
	json & events = batch["ev"];
	bool ended = false;
	for (auto it = events.begin(); it != events.end();)
	{
		if (it->value("t", std::string()) == "end")
		{
			ended = true;
			it = events.erase(it);
		}
		else
		{
			++it;
		}
	}

	onMessageReceived("BV" + batch.dump());
	return ended;
}

bool FxGalleryScreen::idle() const
{
	return visualQueue.empty() && stepRemaining <= 0 && !waitingMove;
}

bool FxGalleryScreen::findLayout(const battle::ClassDef & classDef, const battle::SpellDef & spell, Layout & result, std::string & error)
{
	// Distance de la cible : à portée, assez près pour tout voir ; une cible repoussée garde
	// des cases libres derrière elle.
	bool self = spell.launch == battle::LaunchShape::SELF;
	bool push = hasEffect(spell, battle::EffectType::PUSH);
	int distance = self ? 3 : std::max(1, std::max(spell.minRange, std::min(spell.maxRange, push ? 2 : 4)));
	int behind = push ? 3 : 0;

	// Lanceur au plus près du centre de la carte.
	std::vector<battle::Cell> cells;
	for (int y = 0; y < baseMap.getHeight(); y++)
	{
		for (int x = 0; x < baseMap.getWidth(); x++)
			cells.push_back({ x, y });
	}
	float centerX = (baseMap.getWidth() - 1) / 2.f;
	float centerY = (baseMap.getHeight() - 1) / 2.f;
	std::stable_sort(cells.begin(), cells.end(), [centerX, centerY](const battle::Cell & a, const battle::Cell & b) {
		float da = (a.x - centerX) * (a.x - centerX) + (a.y - centerY) * (a.y - centerY);
		float db = (b.x - centerX) * (b.x - centerX) + (b.y - centerY) * (b.y - centerY);
		return da < db;
	});

	auto open = [this](const battle::Cell & cell) {
		return baseMap.contains(cell) && baseMap.isWalkable(cell) && !baseMap.blocksSight(cell);
	};

	// La cible en ligne droite devant le lanceur, l'allié à deux cases sur le côté.
	const battle::Cell directions[4] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };
	int attempts = 0;
	for (const battle::Cell & caster : cells)
	{
		for (const battle::Cell & direction : directions)
		{
			for (int side = 1; side >= -1; side -= 2)
			{
				battle::Cell across = { -direction.y * side, direction.x * side };
				bool clear = true;
				for (int k = 0; k <= distance + behind && clear; k++)
					clear = open({ caster.x + direction.x * k, caster.y + direction.y * k });
				for (int k = 1; k <= 2 && clear; k++)
					clear = open({ caster.x + across.x * k, caster.y + across.y * k });
				if (!clear)
					continue;

				Layout candidate;
				candidate.caster = caster;
				candidate.enemy = { caster.x + direction.x * distance, caster.y + direction.y * distance };
				candidate.ally = { caster.x + across.x * 2, caster.y + across.y * 2 };
				switch (spell.requirement)
				{
				case battle::CellRequirement::ALLY:
				case battle::CellRequirement::ALLY_OR_SELF:
					candidate.target = candidate.ally;
					break;
				case battle::CellRequirement::FREE_CELL:
				case battle::CellRequirement::FREE_CELL_OR_ALLY:
					// Case libre entre le lanceur et la cible (ou à côté du lanceur).
					candidate.target = distance >= 2 ? battle::Cell{ caster.x + direction.x * (distance / 2), caster.y + direction.y * (distance / 2) }
						: battle::Cell{ caster.x + across.x, caster.y + across.y };
					break;
				default:
					candidate.target = candidate.enemy;
					break;
				}
				if (self)
					candidate.target = caster;

				// Le moteur vérifie la portée, la ligne de vue et la cible.
				if (createEngine(classDef, spell, candidate, error))
				{
					result = candidate;
					return true;
				}
				if (++attempts >= 40)
					return false;
			}
		}
	}

	if (error.empty())
		error = "pas assez de cases libres.";
	return false;
}

std::unique_ptr<battle::BattleEngine> FxGalleryScreen::createEngine(const battle::ClassDef & classDef, const battle::SpellDef & spell,
	const Layout & candidate, std::string & error)
{
	const battle::GameData & data = ClientGameData::get().data();

	// Les cellules de départ placent les personnages : lanceur et allié (équipe 1), cible (équipe 2).
	battle::BattleMap engineMap = baseMap;
	engineMap.startCells[1] = { candidate.caster, candidate.ally };
	engineMap.startCells[2] = { candidate.enemy };

	std::unique_ptr<battle::BattleEngine> result(new battle::BattleEngine(data, engineMap, 1));
	result->addFighter(1, classDef.id, classDef.name);
	result->addFighter(2, classIdByKey(data, "archer"), u8"Cible");
	result->addFighter(1, classIdByKey(data, "guerrier"), u8"Allié");
	result->startPlacement(nowMs);
	for (int id = CASTER; id <= ALLY; id++)
		result->setReady(id, true, nowMs);

	// Les autres passent leur tour jusqu'à celui du lanceur. Pour un vol de vie ou un soin, la cible
	// tire d'abord sur le lanceur ou sur l'allié : ils ont des PV à récupérer.
	int victim = hasEffect(spell, battle::EffectType::LIFESTEAL) ? CASTER
		: (hasEffect(spell, battle::EffectType::HEAL) || hasEffect(spell, battle::EffectType::HOT)) ? ALLY : -1;
	bool shot = victim < 0;
	for (int guard = 0; guard < 12; guard++)
	{
		const battle::BattleState & state = result->getState();
		if (state.phase != battle::BattlePhase::FIGHT)
			break;
		int active = state.activeFighterId();
		const battle::Fighter * caster = state.findFighter(CASTER);
		if (active == CASTER && shot && battle::checkSpellResources(*caster, spell).empty())
			break;
		if (active == ENEMY && !shot)
		{
			// Premier sort de la cible ; sans effet s'il n'est pas à portée.
			battle::Cell aim = state.findFighter(victim)->position;
			result->cast(ENEMY, 0, aim, nowMs);
			shot = true;
		}
		result->endTurn(active, nowMs);
	}
	if (result->hasPendingEvents())
		result->flushEvents();

	const battle::BattleState & state = result->getState();
	const battle::Fighter * caster = state.findFighter(CASTER);
	if (state.phase != battle::BattlePhase::FIGHT || caster == nullptr || state.activeFighterId() != CASTER)
	{
		error = "le lanceur ne peut pas jouer.";
		return nullptr;
	}
	error = battle::checkSpellResources(*caster, spell);
	if (error.empty())
		error = battle::checkTarget(state, result->getMap(), data, *caster, spell, candidate.target);
	if (!error.empty())
		return nullptr;
	return result;
}

void FxGalleryScreen::focusCamera()
{
	// Personnages au centre de la vue (un peu au-dessus de leurs cases, qui sont à leurs pieds).
	float x = (layout.caster.x + layout.enemy.x + layout.ally.x) / 3.f;
	float y = (layout.caster.y + layout.enemy.y + layout.ally.y) / 3.f;
	camera.setFollowing(false);
	camera.setZoom(GALLERY_ZOOM);
	camera.centerOn(x - 1.f, y - 1.f);
}

void FxGalleryScreen::refreshLabel()
{
	const battle::ClassDef * classDef = currentClass();
	const battle::SpellDef * spell = currentSpell();
	if (classDef == nullptr || spell == nullptr)
	{
		label->setText(L"Galerie des effets : aucun sort");
		return;
	}

	// Une ligne par paire d'éléments, pour tenir entre les panneaux du haut de l'écran.
	const battle::SpellVisual & visual = spell->visual;
	std::vector<sf::String> items;
	auto add = [&items](const sf::String & name, const std::string & value, const sf::String & suffix) {
		if (!value.empty())
			items.push_back(name + L" : " + fromServerText(value) + suffix);
	};
	add(L"lancement", visual.cast, L"");
	add(L"projectile", visual.projectile, L"");
	add(L"impact", visual.impact, visual.impactOn == "cells" ? L" (zone)" : visual.impactOn == "caster" ? L" (lanceur)" : L"");
	add(L"statut", visual.status, L"");
	add(L"tick", visual.tick, L"");
	add(L"glyphe", visual.glyph, L"");
	add(L"déclenchement", visual.glyphTrigger, L"");
	if (items.empty())
		add(L"ancien effet", spell->fxSprite, L"");

	sf::String sounds = fileName(spell->sound);
	if (!visual.impactSound.empty())
		sounds += L" + " + fileName(visual.impactSound);
	items.push_back(L"son : " + sounds);

	sf::String text = L"Galerie des effets " + num(current + 1) + L"/" + num((int)entries.size()) + L" : "
		+ fromServerText(spell->name) + L" (" + fromServerText(classDef->name) + L")";
	for (std::size_t i = 0; i < items.size(); i++)
		text += (i % 2 == 0 ? L"\n" : L"   ") + items[i];
	text += L"\nGauche/Droite : sort   R : rejouer   F5 : relire";
	label->setText(text);
}
