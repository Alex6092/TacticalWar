#include "BattlePreview.h"

#include <algorithm>
#include <map>
#include <set>

#include "BattleEngine.h"
#include "BattleRules.h"

using namespace tw::battle;
using nlohmann::json;

namespace
{
	// Résultat d'un lancer simulé, par combattant.
	struct Outcome
	{
		bool cast = false;
		std::map<int, int> damage;
		std::map<int, int> absorbed;
		std::map<int, int> heal;
		std::map<int, int> shield;
		std::set<int> dead;
		std::map<int, std::vector<std::string>> notes;
		// Dégâts et soins par tour des effets périodiques posés : nom de l'effet -> valeur.
		std::map<int, std::map<std::string, int>> dots;
		std::map<int, std::map<std::string, int>> hots;
		// Blocs de mur : dégâts subis, blocs détruits.
		std::map<int, int> blockDamage;
		std::set<int> blockDestroyed;
	};

	bool isPercent(Stat stat)
	{
		return stat == Stat::POWER || stat == Stat::RESISTANCE || stat == Stat::HEAL_BONUS || stat == Stat::EROSION;
	}

	const char * statLabel(Stat stat)
	{
		switch (stat)
		{
		case Stat::MAX_HP: return "PV max";
		case Stat::AP: return "PA";
		case Stat::MP: return "PM";
		case Stat::INITIATIVE: return "initiative";
		case Stat::POWER: return "puissance";
		case Stat::RESISTANCE: return "résistance";
		case Stat::RANGE: return "portée";
		case Stat::HEAL_BONUS: return "soins";
		case Stat::LOCK: return "tacle";
		case Stat::DODGE: return "fuite";
		case Stat::EROSION: return "érosion";
		default: return "";
		}
	}

	void addNote(Outcome & outcome, int fighterId, const std::string & note)
	{
		std::vector<std::string> & notes = outcome.notes[fighterId];
		if (std::find(notes.begin(), notes.end(), note) == notes.end())
			notes.push_back(note);
	}

	Outcome simulate(const BattleState & state, const BattleMap & map, const GameData & data, int casterId, int slot, const Cell & target,
		BattleEngine::RollMode mode)
	{
		Outcome outcome;
		BattleState copy = state;
		Fighter * caster = copy.findFighter(casterId);
		if (caster == nullptr)
			return outcome;
		// Un lancer qui épuise les PA et les PM terminerait le tour et ferait jouer le suivant.
		caster->mp = std::max(caster->mp, 1);
		// Effets posés par le sort : identifiants après ceux déjà en place. L'état miroir des clients ne
		// suit pas le compteur du serveur : il est recalé sur les effets et glyphes existants.
		int firstUid = copy.nextUid;
		for (const Fighter & fighter : copy.fighters)
		{
			for (const ActiveEffect & effect : fighter.effects)
				firstUid = std::max(firstUid, effect.uid + 1);
		}
		for (const Glyph & glyph : copy.glyphs)
			firstUid = std::max(firstUid, glyph.uid + 1);
		for (const Block & block : copy.blocks)
			firstUid = std::max(firstUid, std::max(block.uid, block.group) + 1);
		copy.nextUid = firstUid;

		BattleEngine engine(data, map, copy, 1);
		engine.setRollMode(mode);
		if (!engine.cast(casterId, slot, target, 0).ok)
			return outcome;
		outcome.cast = true;

		json batch = engine.flushEvents();
		for (const json & event : batch["ev"])
		{
			std::string type = event.value("t", std::string());
			int fighterId = event.value("f", -1);
			if (type == "damage")
			{
				outcome.damage[fighterId] += event.value("amount", 0);
				outcome.absorbed[fighterId] += event.value("absorbed", 0);
				if (event.value("kind", std::string()) == "collision")
					addNote(outcome, fighterId, "Collision");
			}
			else if (type == "heal")
			{
				outcome.heal[fighterId] += event.value("amount", 0);
			}
			else if (type == "death")
			{
				outcome.dead.insert(fighterId);
			}
			else if (type == "slide")
			{
				std::string kind = event.value("kind", std::string());
				addNote(outcome, fighterId, kind == "push" ? "Repoussé" : kind == "pull" ? "Attiré" : kind == "dash" ? "Bondit" : "Téléporté");
			}
			else if (type == "combo")
			{
				addNote(outcome, fighterId, "Combo " + event.value("name", std::string()) + " +" + std::to_string(event.value("percent", 0)) + " %");
			}
			else if (type == "swap")
			{
				addNote(outcome, fighterId, "Échange de place");
				addNote(outcome, event.value("other", -1), "Échange de place");
			}
			else if (type == "blockhit")
			{
				outcome.blockDamage[event.value("uid", 0)] += event.value("amount", 0);
			}
			else if (type == "block-" && event.value("reason", std::string()) == "destroyed")
			{
				outcome.blockDestroyed.insert(event.value("uid", 0));
			}
		}

		// Effets durables posés par le sort.
		const BattleState & after = engine.getState();
		for (const Fighter & fighter : after.fighters)
		{
			for (const ActiveEffect & effect : fighter.effects)
			{
				if (effect.uid < firstUid)
					continue;

				int value = mode == BattleEngine::RollMode::MAX ? effect.maxValue : effect.minValue;
				switch (effect.type)
				{
				case EffectType::SHIELD:
					outcome.shield[fighter.id] += effect.value;
					break;
				case EffectType::DOT:
					outcome.dots[fighter.id][effect.name] = periodicDamage(after, data, fighter, value, effect.casterPower);
					break;
				case EffectType::HOT:
					outcome.hots[fighter.id][effect.name] = value * (100 + effect.casterPower) / 100;
					break;
				case EffectType::STAT_MOD:
				{
					std::string amount = (effect.value > 0 ? "+" : "") + std::to_string(effect.value) + (isPercent(effect.stat) ? " % " : " ") + statLabel(effect.stat);
					addNote(outcome, fighter.id, effect.name.empty() ? amount : effect.name + " (" + amount + ")");
					break;
				}
				case EffectType::STATE:
					addNote(outcome, fighter.id, effect.name);
					break;
				default:
					break;
				}
			}
		}
		return outcome;
	}

	int valueOf(const std::map<int, int> & values, int fighterId)
	{
		auto it = values.find(fighterId);
		return it == values.end() ? 0 : it->second;
	}

	std::string span(int a, int b)
	{
		int low = std::min(a, b);
		int high = std::max(a, b);
		return low == high ? std::to_string(low) : std::to_string(low) + " à " + std::to_string(high);
	}

	// Effets périodiques d'un combattant : « Brûlure 4 à 5/tour », « Soin +3/tour ».
	void addPeriodicNotes(std::vector<std::string> & notes, const std::map<int, std::map<std::string, int>> & low,
		const std::map<int, std::map<std::string, int>> & high, int fighterId, const std::string & sign)
	{
		auto lowEffects = low.find(fighterId);
		auto highEffects = high.find(fighterId);
		if (highEffects == high.end())
			return;
		for (const auto & entry : highEffects->second)
		{
			int other = entry.second;
			if (lowEffects != low.end() && lowEffects->second.count(entry.first) > 0)
				other = lowEffects->second.at(entry.first);
			notes.push_back(entry.first + " " + sign + span(other, entry.second) + "/tour");
		}
	}
}

std::vector<TargetPreview> tw::battle::previewSpell(const BattleState & state, const BattleMap & map, const GameData & data,
	int casterId, int slot, const Cell & target)
{
	std::vector<TargetPreview> previews;
	Outcome low = simulate(state, map, data, casterId, slot, target, BattleEngine::RollMode::MIN);
	Outcome high = simulate(state, map, data, casterId, slot, target, BattleEngine::RollMode::MAX);
	if (!low.cast || !high.cast)
		return previews;

	std::set<int> ids;
	for (const Outcome * outcome : { &low, &high })
	{
		for (const auto & entry : outcome->damage) ids.insert(entry.first);
		for (const auto & entry : outcome->heal) ids.insert(entry.first);
		for (const auto & entry : outcome->shield) ids.insert(entry.first);
		for (const auto & entry : outcome->notes) ids.insert(entry.first);
		for (const auto & entry : outcome->dots) ids.insert(entry.first);
		for (const auto & entry : outcome->hots) ids.insert(entry.first);
		ids.insert(outcome->dead.begin(), outcome->dead.end());
	}

	for (int id : ids)
	{
		if (state.findFighter(id) == nullptr)
			continue;

		TargetPreview preview;
		preview.fighterId = id;
		preview.minDamage = std::min(valueOf(low.damage, id), valueOf(high.damage, id));
		preview.maxDamage = std::max(valueOf(low.damage, id), valueOf(high.damage, id));
		preview.minAbsorbed = std::min(valueOf(low.absorbed, id), valueOf(high.absorbed, id));
		preview.maxAbsorbed = std::max(valueOf(low.absorbed, id), valueOf(high.absorbed, id));
		preview.minHeal = std::min(valueOf(low.heal, id), valueOf(high.heal, id));
		preview.maxHeal = std::max(valueOf(low.heal, id), valueOf(high.heal, id));
		preview.minShield = std::min(valueOf(low.shield, id), valueOf(high.shield, id));
		preview.maxShield = std::max(valueOf(low.shield, id), valueOf(high.shield, id));
		preview.koCertain = low.dead.count(id) > 0 && high.dead.count(id) > 0;
		preview.koPossible = low.dead.count(id) > 0 || high.dead.count(id) > 0;

		for (const Outcome * outcome : { &low, &high })
		{
			auto notes = outcome->notes.find(id);
			if (notes == outcome->notes.end())
				continue;
			for (const std::string & note : notes->second)
			{
				if (std::find(preview.notes.begin(), preview.notes.end(), note) == preview.notes.end())
					preview.notes.push_back(note);
			}
		}

		addPeriodicNotes(preview.notes, low.dots, high.dots, id, "");
		addPeriodicNotes(preview.notes, low.hots, high.hots, id, "+");

		previews.push_back(preview);
	}

	// Blocs de mur touchés.
	std::set<int> blocks;
	for (const Outcome * outcome : { &low, &high })
	{
		for (const auto & entry : outcome->blockDamage)
			blocks.insert(entry.first);
	}
	for (int uid : blocks)
	{
		if (state.findBlock(uid) == nullptr)
			continue;
		TargetPreview preview;
		preview.blockUid = uid;
		preview.minDamage = std::min(valueOf(low.blockDamage, uid), valueOf(high.blockDamage, uid));
		preview.maxDamage = std::max(valueOf(low.blockDamage, uid), valueOf(high.blockDamage, uid));
		preview.koCertain = low.blockDestroyed.count(uid) > 0 && high.blockDestroyed.count(uid) > 0;
		preview.koPossible = low.blockDestroyed.count(uid) > 0 || high.blockDestroyed.count(uid) > 0;
		previews.push_back(preview);
	}
	return previews;
}

std::vector<Cell> tw::battle::nextTurnReach(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & fighter)
{
	Fighter atTurnStart = fighter;
	atTurnStart.mp = std::max(0, effectiveStat(state, data, fighter, Stat::MP));
	return reachableCells(state, map, atTurnStart);
}
