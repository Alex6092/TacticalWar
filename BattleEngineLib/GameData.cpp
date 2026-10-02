#include "GameData.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

using namespace tw::battle;
using nlohmann::json;

namespace
{
	const char * STAT_NAMES[STAT_COUNT] = {
		"MAX_HP", "AP", "MP", "INITIATIVE", "POWER", "RESISTANCE", "RANGE", "HEAL_BONUS", "LOCK", "DODGE", "EROSION"
	};

	template<typename Enum>
	Enum parseName(const json & object, const char * key, Enum defaultValue, std::initializer_list<std::pair<const char *, Enum>> names)
	{
		if (!object.contains(key))
			return defaultValue;

		std::string value = object[key].get<std::string>();
		for (const auto & entry : names)
		{
			if (value == entry.first)
				return entry.second;
		}
		throw std::runtime_error(std::string("valeur inconnue pour \"") + key + "\" : " + value);
	}

	Stat parseStatField(const json & object, const char * key, Stat defaultValue)
	{
		if (!object.contains(key))
			return defaultValue;

		Stat stat;
		std::string value = object[key].get<std::string>();
		if (!parseStat(value, stat))
			throw std::runtime_error("caractéristique inconnue : " + value);
		return stat;
	}

	ZoneShape parseZoneShape(const json & object, const char * key)
	{
		return parseName<ZoneShape>(object, key, ZoneShape::SINGLE, {
			{ "SINGLE", ZoneShape::SINGLE }, { "CIRCLE", ZoneShape::CIRCLE }, { "CROSS", ZoneShape::CROSS },
			{ "SQUARE", ZoneShape::SQUARE }, { "LINE", ZoneShape::LINE }, { "PERPENDICULAR", ZoneShape::PERPENDICULAR },
			{ "RING", ZoneShape::RING } });
	}

	EffectDef parseEffect(const json & object)
	{
		EffectDef effect;
		effect.type = parseName<EffectType>(object, "type", EffectType::DAMAGE, {
			{ "DAMAGE", EffectType::DAMAGE }, { "HEAL", EffectType::HEAL }, { "LIFESTEAL", EffectType::LIFESTEAL },
			{ "SHIELD", EffectType::SHIELD }, { "DOT", EffectType::DOT }, { "HOT", EffectType::HOT },
			{ "STAT_MOD", EffectType::STAT_MOD }, { "PUSH", EffectType::PUSH }, { "PULL", EffectType::PULL },
			{ "DASH", EffectType::DASH }, { "TELEPORT", EffectType::TELEPORT }, { "DISPEL", EffectType::DISPEL },
			{ "GLYPH", EffectType::GLYPH }, { "STATE", EffectType::STATE } });
		effect.targets = parseName<TargetFilter>(object, "targets", TargetFilter::ENEMIES, {
			{ "ENEMIES", TargetFilter::ENEMIES }, { "ALLIES", TargetFilter::ALLIES },
			{ "ALL", TargetFilter::ALL }, { "CASTER", TargetFilter::CASTER } });
		effect.dispel = parseName<DispelMode>(object, "dispel", DispelMode::NEGATIVE, {
			{ "NEGATIVE", DispelMode::NEGATIVE }, { "POSITIVE", DispelMode::POSITIVE }, { "ALL", DispelMode::ALL } });

		effect.min = object.value("min", 0);
		effect.max = object.value("max", effect.min);
		effect.duration = object.value("duration", 0);
		effect.stat = parseStatField(object, "stat", Stat::POWER);
		effect.percent = object.value("percent", 0);
		effect.allowSwap = object.value("allowSwap", false);
		effect.refresh = object.value("refresh", true);
		effect.state = object.value("state", std::string());
		effect.name = object.value("name", std::string());

		if (object.contains("glyph"))
		{
			const json & glyph = object["glyph"];
			effect.glyphShape = parseZoneShape(glyph, "shape");
			effect.glyphSize = glyph.value("size", 0);
			for (const json & triggered : glyph.value("effects", json::array()))
				effect.glyphEffects.push_back(parseEffect(triggered));
		}

		if (effect.max < effect.min)
			throw std::runtime_error("effet avec max < min");

		return effect;
	}

	SpellDef parseSpell(const json & object)
	{
		SpellDef spell;
		spell.id = object.at("id").get<std::string>();
		spell.name = object.value("name", spell.id);
		spell.description = object.value("description", std::string());
		spell.icon = object.value("icon", std::string());
		spell.apCost = object.value("ap", 3);

		if (object.contains("range"))
		{
			spell.minRange = object["range"].at(0).get<int>();
			spell.maxRange = object["range"].at(1).get<int>();
		}
		spell.rangeModifiable = object.value("rangeModifiable", false);
		spell.lineOfSight = object.value("lineOfSight", true);
		spell.launch = parseName<LaunchShape>(object, "launch", LaunchShape::CIRCLE, {
			{ "CIRCLE", LaunchShape::CIRCLE }, { "LINE", LaunchShape::LINE }, { "DIAGONAL", LaunchShape::DIAGONAL },
			{ "STAR", LaunchShape::STAR }, { "SELF", LaunchShape::SELF } });
		spell.requirement = parseName<CellRequirement>(object, "target", CellRequirement::ANY, {
			{ "ANY", CellRequirement::ANY }, { "FREE_CELL", CellRequirement::FREE_CELL },
			{ "FREE_CELL_OR_ALLY", CellRequirement::FREE_CELL_OR_ALLY }, { "CHARACTER", CellRequirement::CHARACTER },
			{ "ENEMY", CellRequirement::ENEMY }, { "ALLY", CellRequirement::ALLY }, { "ALLY_OR_SELF", CellRequirement::ALLY_OR_SELF } });
		spell.castsPerTurn = object.value("castsPerTurn", 0);
		spell.castsPerTarget = object.value("castsPerTarget", 0);
		spell.cooldown = object.value("cooldown", 0);
		spell.initialCooldown = object.value("initialCooldown", 0);

		if (object.contains("zone"))
		{
			spell.impact.shape = parseZoneShape(object["zone"], "shape");
			spell.impact.size = object["zone"].value("size", 0);
		}

		for (const json & effect : object.value("effects", json::array()))
			spell.effects.push_back(parseEffect(effect));

		spell.fxSprite = object.value("fx", std::string());
		spell.sound = object.value("sound", std::string());
		spell.casterAnimation = object.value("animation", std::string("magical"));

		if (spell.launch == LaunchShape::SELF)
		{
			spell.minRange = 0;
			spell.maxRange = 0;
		}
		if (spell.maxRange < spell.minRange)
			throw std::runtime_error("sort " + spell.id + " : portée max < portée min");

		return spell;
	}

	PassiveDef parsePassive(const json & object)
	{
		PassiveDef passive;
		passive.type = parseName<PassiveType>(object, "type", PassiveType::NONE, {
			{ "NONE", PassiveType::NONE }, { "LOW_HP_DAMAGE", PassiveType::LOW_HP_DAMAGE },
			{ "DISTANCE_DAMAGE", PassiveType::DISTANCE_DAMAGE }, { "ON_CAST_POWER", PassiveType::ON_CAST_POWER },
			{ "ALLY_AURA", PassiveType::ALLY_AURA } });
		passive.name = object.value("name", std::string());
		passive.description = object.value("description", std::string());
		passive.threshold = object.value("threshold", 0);
		passive.bonus = object.value("bonus", 0);
		passive.threshold2 = object.value("threshold2", 0);
		passive.bonus2 = object.value("bonus2", 0);
		passive.distance = object.value("distance", 0);
		passive.maxStacks = object.value("maxStacks", 0);
		passive.stat = parseStatField(object, "stat", Stat::RESISTANCE);
		return passive;
	}
}

const char * tw::battle::toString(Stat stat)
{
	int index = (int)stat;
	return index >= 0 && index < STAT_COUNT ? STAT_NAMES[index] : "";
}

bool tw::battle::parseStat(const std::string & text, Stat & stat)
{
	for (int i = 0; i < STAT_COUNT; i++)
	{
		if (text == STAT_NAMES[i])
		{
			stat = (Stat)i;
			return true;
		}
	}
	return false;
}

bool tw::battle::isPositiveEffect(const EffectDef & effect)
{
	switch (effect.type)
	{
	case EffectType::HEAL:
	case EffectType::HOT:
	case EffectType::SHIELD:
		return true;
	case EffectType::STAT_MOD:
		return effect.min >= 0;
	case EffectType::STATE:
		return true;
	default:
		return false;
	}
}

const ClassDef * GameData::findClass(int classId) const
{
	for (const ClassDef & classDef : classes)
	{
		if (classDef.id == classId)
			return &classDef;
	}
	return nullptr;
}

bool GameData::loadFromJsonText(const std::string & text, std::string & error)
{
	try
	{
		json root = json::parse(text, nullptr, true, true);

		GameData loaded;
		loaded.version = root.value("version", 1);

		if (root.contains("rules"))
		{
			const json & rules = root["rules"];
			BattleRules & r = loaded.rules;
			r.turnSeconds = rules.value("turnSeconds", r.turnSeconds);
			r.placementSeconds = rules.value("placementSeconds", r.placementSeconds);
			r.disconnectedTurnSeconds = rules.value("disconnectedTurnSeconds", r.disconnectedTurnSeconds);
			r.suddenDeathRound = rules.value("suddenDeathRound", r.suddenDeathRound);
			r.suddenDeathPercentPerRound = rules.value("suddenDeathPercentPerRound", r.suddenDeathPercentPerRound);
			r.maxRounds = rules.value("maxRounds", r.maxRounds);
			r.maxResistance = rules.value("maxResistance", r.maxResistance);
			r.collisionDamagePerCell = rules.value("collisionDamagePerCell", r.collisionDamagePerCell);
			r.collisionDamageToHit = rules.value("collisionDamageToHit", r.collisionDamageToHit);
			r.tackleApFactor = rules.value("tackleApFactor", r.tackleApFactor);
		}

		for (const json & classJson : root.at("classes"))
		{
			ClassDef classDef;
			classDef.id = classJson.at("id").get<int>();
			classDef.key = classJson.value("key", std::string());
			classDef.name = classJson.value("name", std::string());
			classDef.description = classJson.value("description", std::string());
			classDef.graphicsPath = classJson.value("graphicsPath", std::string());
			classDef.icon = classJson.value("icon", std::string());
			classDef.preview = classJson.value("preview", std::string());

			const json & stats = classJson.at("stats");
			for (auto it = stats.begin(); it != stats.end(); it++)
			{
				Stat stat;
				if (!parseStat(it.key(), stat))
					throw std::runtime_error("classe " + classDef.name + " : caractéristique inconnue " + it.key());
				classDef.baseStats.set(stat, it.value().get<int>());
			}

			if (classJson.contains("passive"))
				classDef.passive = parsePassive(classJson["passive"]);

			for (const json & spellJson : classJson.at("spells"))
				classDef.spells.push_back(parseSpell(spellJson));

			if (classDef.baseStats.get(Stat::MAX_HP) <= 0)
				throw std::runtime_error("classe " + classDef.name + " : MAX_HP doit être positif");

			loaded.classes.push_back(classDef);
		}

		loaded.sourceText = text;
		*this = loaded;
		return true;
	}
	catch (const std::exception & e)
	{
		error = std::string("Données de jeu invalides : ") + e.what();
		return false;
	}
}

bool GameData::loadFromFile(const std::string & path, std::string & error)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		error = "Fichier introuvable : " + path;
		return false;
	}

	std::stringstream content;
	content << file.rdbuf();
	return loadFromJsonText(content.str(), error);
}
