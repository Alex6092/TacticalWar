#include "BattleMirror.h"

#include <algorithm>

using namespace tw::battle;
using nlohmann::json;

namespace
{
	BattlePhase phaseFromString(const std::string & text)
	{
		if (text == "FIGHT") return BattlePhase::FIGHT;
		if (text == "ENDED") return BattlePhase::ENDED;
		return BattlePhase::PLACEMENT;
	}

	EndReason reasonFromString(const std::string & text)
	{
		if (text == "KO") return EndReason::KO;
		if (text == "ROUND_LIMIT") return EndReason::ROUND_LIMIT;
		if (text == "FORFEIT") return EndReason::FORFEIT;
		if (text == "ADMIN") return EndReason::ADMIN;
		if (text == "OBJECTIVE") return EndReason::OBJECTIVE;
		return EndReason::NONE;
	}

	std::map<std::string, int> intMap(const json & object)
	{
		std::map<std::string, int> values;
		if (object.is_object())
		{
			for (auto it = object.begin(); it != object.end(); it++)
				values[it.key()] = it.value().get<int>();
		}
		return values;
	}
}

Cell BattleMirror::cellFromJson(const json & value)
{
	return { value.at(0).get<int>(), value.at(1).get<int>() };
}

ActiveEffect BattleMirror::effectFromJson(const json & value)
{
	ActiveEffect effect;
	effect.uid = value.value("uid", 0);
	std::string kind = value.value("kind", std::string("STAT_MOD"));
	if (kind == "SHIELD") effect.type = EffectType::SHIELD;
	else if (kind == "DOT") effect.type = EffectType::DOT;
	else if (kind == "HOT") effect.type = EffectType::HOT;
	else if (kind == "STATE") effect.type = EffectType::STATE;
	else effect.type = EffectType::STAT_MOD;

	parseStat(value.value("stat", std::string("POWER")), effect.stat);
	effect.value = value.value("value", 0);
	effect.minValue = value.value("min", 0);
	effect.maxValue = value.value("max", 0);
	effect.remainingTurns = value.value("turns", 0);
	effect.skipNextDecrement = value.value("skip", false);
	effect.name = value.value("name", std::string());
	effect.spellId = value.value("spell", std::string());
	effect.casterId = value.value("caster", -1);
	effect.positive = value.value("positive", false);
	effect.state = value.value("state", std::string());
	return effect;
}

Glyph BattleMirror::glyphFromJson(const json & value)
{
	Glyph glyph;
	glyph.uid = value.value("uid", 0);
	glyph.casterId = value.value("caster", -1);
	glyph.team = value.value("team", 0);
	glyph.spellId = value.value("spell", std::string());
	glyph.name = value.value("name", std::string());
	glyph.remainingTurns = value.value("turns", 0);
	for (const json & cell : value.value("cells", json::array()))
		glyph.cells.push_back(cellFromJson(cell));
	return glyph;
}

void BattleMirror::applySnapshot(BattleState & state, BattleMap & map, const json & snapshot)
{
	BattleState fresh;
	fresh.phase = phaseFromString(snapshot.value("phase", std::string()));
	fresh.round = snapshot.value("round", 0);
	fresh.turnOrder = snapshot.value("order", std::vector<int>());
	fresh.winnerTeam = snapshot.value("winner", 0);
	fresh.endReason = reasonFromString(snapshot.value("reason", std::string()));
	fresh.mvpFighterId = snapshot.value("mvp", -1);

	int active = snapshot.value("active", -1);
	for (int i = 0; i < (int)fresh.turnOrder.size(); i++)
	{
		if (fresh.turnOrder[i] == active)
			fresh.turnIndex = i;
	}

	for (const json & value : snapshot.value("fighters", json::array()))
	{
		Fighter fighter;
		fighter.id = value.value("id", 0);
		fighter.team = value.value("team", 0);
		fighter.classId = value.value("classId", 0);
		fighter.name = value.value("name", std::string());
		fighter.spells = value.value("spells", std::vector<int>());
		fighter.position = { value.value("x", 0), value.value("y", 0) };
		fighter.hp = value.value("hp", 0);
		fighter.maxHp = value.value("maxHp", 0);
		fighter.shield = value.value("shield", 0);
		fighter.ap = value.value("ap", 0);
		fighter.mp = value.value("mp", 0);
		fighter.alive = value.value("alive", true);
		fighter.ready = value.value("ready", false);
		fighter.connected = value.value("connected", true);
		for (const auto & stat : intMap(value.value("stats", json::object())))
		{
			Stat parsed;
			if (parseStat(stat.first, parsed))
				fighter.baseStats.set(parsed, stat.second);
		}
		fighter.cooldowns = intMap(value.value("cooldowns", json::object()));
		fighter.castsThisTurn = intMap(value.value("casts", json::object()));
		for (const json & effect : value.value("effects", json::array()))
			fighter.effects.push_back(effectFromJson(effect));
		fighter.record = recordFromJson(value.value("record", json::object()));
		fresh.fighters.push_back(fighter);
	}

	for (const json & glyph : snapshot.value("glyphs", json::array()))
		fresh.glyphs.push_back(glyphFromJson(glyph));

	const json & zone = snapshot.contains("zone") ? snapshot["zone"] : json();
	if (zone.is_object())
	{
		fresh.zone.enabled = true;
		for (const json & cell : zone.value("cells", json::array()))
			fresh.zone.cells.push_back(cellFromJson(cell));
		fresh.zone.pointsToWin = zone.value("points", 0);
		std::vector<int> scores = zone.value("scores", std::vector<int>());
		for (int team = 1; team <= 2 && team <= (int)scores.size(); team++)
			fresh.zone.scores[team] = scores[team - 1];
		fresh.zone.holder = zone.value("holder", 0);
	}

	if (snapshot.contains("startCells"))
	{
		for (int team = 1; team <= 2; team++)
		{
			map.startCells[team].clear();
			for (const json & cell : snapshot["startCells"].value(std::to_string(team), json::array()))
				map.startCells[team].push_back(cellFromJson(cell));
		}
	}

	state = fresh;
}

void BattleMirror::applyEvent(BattleState & state, const json & event)
{
	const std::string type = event.value("t", std::string());
	Fighter * fighter = event.contains("f") ? state.findFighter(event["f"].get<int>()) : nullptr;

	if (type == "placement")
	{
		state.phase = BattlePhase::PLACEMENT;
	}
	else if (type == "place" && fighter != nullptr)
	{
		fighter->position = { event["x"].get<int>(), event["y"].get<int>() };
	}
	else if (type == "ready" && fighter != nullptr)
	{
		fighter->ready = event["ready"].get<bool>();
	}
	else if (type == "fight")
	{
		state.phase = BattlePhase::FIGHT;
		state.turnOrder = event.value("order", std::vector<int>());
		state.round = 1;
		state.turnIndex = 0;
	}
	else if (type == "turn" && fighter != nullptr)
	{
		state.round = event.value("round", state.round);
		for (int i = 0; i < (int)state.turnOrder.size(); i++)
		{
			if (state.turnOrder[i] == fighter->id)
				state.turnIndex = i;
		}
		fighter->castsThisTurn.clear();
		fighter->castsOnTarget.clear();

		// Les glyphes du combattant s'usent au début de ses tours.
		for (auto it = state.glyphs.begin(); it != state.glyphs.end();)
		{
			if (it->casterId == fighter->id && --it->remainingTurns <= 0)
				it = state.glyphs.erase(it);
			else
				it++;
		}
	}
	else if (type == "endturn" && fighter != nullptr)
	{
		for (ActiveEffect & effect : fighter->effects)
		{
			if (effect.skipNextDecrement)
				effect.skipNextDecrement = false;
			else
				effect.remainingTurns--;
		}
	}
	else if ((type == "damage" || type == "heal") && fighter != nullptr)
	{
		fighter->hp = event.value("hp", fighter->hp);
		fighter->maxHp = event.value("maxHp", fighter->maxHp);
		fighter->shield = event.value("shield", fighter->shield);
		fighter->alive = fighter->hp > 0;
	}
	else if (type == "death" && fighter != nullptr)
	{
		fighter->alive = false;
		fighter->hp = 0;
		for (auto it = state.glyphs.begin(); it != state.glyphs.end();)
		{
			if (it->casterId == fighter->id)
				it = state.glyphs.erase(it);
			else
				it++;
		}
	}
	else if (type == "effect+" && fighter != nullptr)
	{
		fighter->effects.push_back(effectFromJson(event["effect"]));
		fighter->shield = event.value("shield", fighter->shield);
	}
	else if (type == "effect-" && fighter != nullptr)
	{
		int uid = event.value("uid", 0);
		fighter->effects.erase(std::remove_if(fighter->effects.begin(), fighter->effects.end(),
			[uid](const ActiveEffect & effect) { return effect.uid == uid; }), fighter->effects.end());
		fighter->shield = event.value("shield", fighter->shield);
	}
	else if (type == "stats" && fighter != nullptr)
	{
		fighter->ap = event.value("ap", fighter->ap);
		fighter->mp = event.value("mp", fighter->mp);
		fighter->hp = event.value("hp", fighter->hp);
		fighter->maxHp = event.value("maxHp", fighter->maxHp);
		fighter->shield = event.value("shield", fighter->shield);
		fighter->cooldowns = intMap(event.value("cooldowns", json::object()));
		fighter->castsThisTurn = intMap(event.value("casts", json::object()));
	}
	else if (type == "move" && fighter != nullptr)
	{
		const json & path = event["path"];
		if (!path.empty())
			fighter->position = cellFromJson(path.back());
		fighter->ap = event.value("ap", fighter->ap);
		fighter->mp = event.value("mp", fighter->mp);
	}
	else if (type == "cast" && fighter != nullptr)
	{
		const Fighter * target = state.fighterAt({ event["x"].get<int>(), event["y"].get<int>() });
		if (target != nullptr)
			fighter->castsOnTarget[event.value("spell", std::string())][target->id]++;
	}
	else if (type == "slide" && fighter != nullptr)
	{
		fighter->position = { event["x"].get<int>(), event["y"].get<int>() };
	}
	else if (type == "swap" && fighter != nullptr)
	{
		fighter->position = { event["x"].get<int>(), event["y"].get<int>() };
		Fighter * other = state.findFighter(event.value("other", -1));
		if (other != nullptr)
			other->position = { event["ox"].get<int>(), event["oy"].get<int>() };
	}
	else if (type == "glyph+")
	{
		state.glyphs.push_back(glyphFromJson(event["glyph"]));
	}
	else if (type == "glyph-")
	{
		int uid = event.value("uid", 0);
		state.glyphs.erase(std::remove_if(state.glyphs.begin(), state.glyphs.end(),
			[uid](const Glyph & glyph) { return glyph.uid == uid; }), state.glyphs.end());
	}
	else if (type == "connection" && fighter != nullptr)
	{
		fighter->connected = event.value("connected", true);
	}
	else if (type == "score")
	{
		std::vector<int> scores = event.value("scores", std::vector<int>());
		for (int team = 1; team <= 2 && team <= (int)scores.size(); team++)
			state.zone.scores[team] = scores[team - 1];
		state.zone.holder = event.value("holder", 0);
	}
	else if (type == "end")
	{
		state.phase = BattlePhase::ENDED;
		state.winnerTeam = event.value("winner", 0);
		state.endReason = reasonFromString(event.value("reason", std::string()));
		state.mvpFighterId = event.value("mvp", -1);
		for (const json & record : event.value("records", json::array()))
		{
			Fighter * fighter = state.findFighter(record.value("f", -1));
			if (fighter != nullptr)
				fighter->record = recordFromJson(record);
		}
	}
}

FighterRecord BattleMirror::recordFromJson(const json & value)
{
	FighterRecord record;
	record.dealt = value.value("dealt", 0);
	record.taken = value.value("taken", 0);
	record.healed = value.value("healed", 0);
	record.shielded = value.value("shielded", 0);
	record.kills = value.value("kills", 0);
	record.casts = value.value("casts", 0);
	return record;
}
