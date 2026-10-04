#include "Puzzle.h"

#include <algorithm>

#include <nlohmann/json.hpp>

using namespace tw::battle;
using nlohmann::json;

namespace
{
	// Effet de marque (STATE) défini dans les données de jeu, pour reprendre son nom affiché.
	const EffectDef * findMark(const std::vector<EffectDef> & effects, const std::string & state)
	{
		for (const EffectDef & effect : effects)
		{
			if (effect.type == EffectType::STATE && effect.state == state)
				return &effect;
			if (const EffectDef * nested = findMark(effect.glyphEffects, state))
				return nested;
		}
		return nullptr;
	}

	const EffectDef * findMark(const GameData & data, const std::string & state)
	{
		for (const ClassDef & classDef : data.classes)
		{
			for (const SpellDef & spell : classDef.spells)
			{
				if (const EffectDef * effect = findMark(spell.effects, state))
					return effect;
			}
		}
		return nullptr;
	}

	Cell cellOf(const json & value)
	{
		return { value.at(0).get<int>(), value.at(1).get<int>() };
	}

	// Actions : {"fighter", "move": [[x, y]...]}, {"fighter", "cast": emplacement, "x", "y"} ou {"fighter", "end": true}.
	std::vector<PuzzleAction> actionsOf(const json & values)
	{
		std::vector<PuzzleAction> actions;
		for (const json & value : values)
		{
			PuzzleAction action;
			action.fighter = value.value("fighter", 0);
			if (value.contains("move"))
			{
				action.kind = PuzzleAction::Kind::MOVE;
				for (const json & cell : value["move"])
					action.path.push_back(cellOf(cell));
			}
			else if (value.contains("cast"))
			{
				action.kind = PuzzleAction::Kind::CAST;
				action.slot = value["cast"].get<int>();
				action.target = { value.at("x").get<int>(), value.at("y").get<int>() };
			}
			actions.push_back(action);
		}
		return actions;
	}
}

bool tw::battle::parsePuzzle(const std::string & text, Puzzle & puzzle, std::string & error)
{
	json root = json::parse(text, nullptr, false, true);
	if (!root.is_object())
	{
		error = "JSON invalide";
		return false;
	}
	try
	{
		puzzle = Puzzle();
		puzzle.id = root.at("id").get<std::string>();
		puzzle.order = root.value("order", 0);
		puzzle.title = root.value("title", puzzle.id);
		puzzle.goal = root.value("goal", std::string());
		puzzle.hint = root.value("hint", std::string());
		puzzle.mapId = root.at("map").get<int>();
		for (const json & value : root.at("fighters"))
		{
			PuzzleFighter fighter;
			fighter.team = value.value("team", 1);
			fighter.classId = value.at("class").get<int>();
			fighter.name = value.value("name", std::string());
			fighter.position = { value.at("x").get<int>(), value.at("y").get<int>() };
			fighter.hp = value.value("hp", 0);
			fighter.spells = value.value("spells", std::vector<int>());
			fighter.marks = value.value("marks", std::vector<std::string>());
			fighter.ap = value.value("ap", -1);
			fighter.mp = value.value("mp", -1);
			puzzle.fighters.push_back(fighter);
		}
		puzzle.solution = actionsOf(root.value("solution", json::array()));
		puzzle.trap = actionsOf(root.value("trap", json::array()));
	}
	catch (const json::exception & e)
	{
		error = e.what();
		return false;
	}

	bool teams[3] = { false, false, false };
	for (const PuzzleFighter & fighter : puzzle.fighters)
		teams[fighter.team == 2 ? 2 : 1] = true;
	if (!teams[1] || !teams[2])
	{
		error = "il faut au moins un combattant dans chaque équipe";
		return false;
	}
	return true;
}

BattleState tw::battle::puzzleState(const GameData & data, const BattleMap & map, const Puzzle & puzzle)
{
	BattleEngine builder(data, map, 1);
	for (const PuzzleFighter & fighter : puzzle.fighters)
		builder.addFighter(fighter.team == 2 ? 2 : 1, fighter.classId, fighter.name, fighter.spells);
	BattleState state = builder.getState();

	std::vector<int> first;
	std::vector<int> second;
	for (std::size_t i = 0; i < puzzle.fighters.size() && i < state.fighters.size(); i++)
	{
		const PuzzleFighter & spec = puzzle.fighters[i];
		Fighter & fighter = state.fighters[i];
		fighter.position = spec.position;
		fighter.ready = true;
		if (spec.hp > 0)
			fighter.hp = std::min(spec.hp, fighter.maxHp);
		if (spec.ap >= 0)
			fighter.ap = spec.ap;
		if (spec.mp >= 0)
			fighter.mp = spec.mp;
		for (const std::string & mark : spec.marks)
		{
			const EffectDef * definition = findMark(data, mark);
			ActiveEffect effect;
			effect.uid = state.nextUid++;
			effect.type = EffectType::STATE;
			effect.state = mark;
			effect.name = definition != nullptr ? definition->name : mark;
			effect.remainingTurns = 2;
			fighter.effects.push_back(effect);
		}
		(fighter.team == 1 ? first : second).push_back(fighter.id);
	}

	// Le joueur joue tous les combattants de l'équipe 1 : le premier est le sien, les autres sont pilotés.
	for (std::size_t i = 1; i < first.size(); i++)
		state.findFighter(first[i])->piloted = true;

	state.turnOrder = first;
	state.turnOrder.insert(state.turnOrder.end(), second.begin(), second.end());
	state.phase = BattlePhase::FIGHT;
	state.round = 1;
	state.turnIndex = 0;
	state.deadlineMs = 0;
	return state;
}

bool tw::battle::puzzleSolved(const BattleState & state)
{
	for (const Fighter & fighter : state.fighters)
	{
		if (fighter.team == 2 && fighter.alive)
			return false;
	}
	return true;
}

bool tw::battle::puzzleFailed(const BattleState & state)
{
	if (puzzleSolved(state))
		return false;
	const Fighter * active = state.findFighter(state.activeFighterId());
	return state.phase != BattlePhase::FIGHT || state.round > 1 || active == nullptr || active->team != 1;
}

ActionResult tw::battle::playPuzzleAction(BattleEngine & engine, const PuzzleAction & action, std::int64_t nowMs)
{
	switch (action.kind)
	{
	case PuzzleAction::Kind::MOVE:
		return engine.move(action.fighter, action.path, nowMs);
	case PuzzleAction::Kind::CAST:
		return engine.cast(action.fighter, action.slot, action.target, nowMs);
	default:
		return engine.endTurn(action.fighter, nowMs);
	}
}
