#include "StateJson.h"

using namespace tw::battle;
using nlohmann::json;

json statejson::cell(const Cell & cell)
{
	return json::array({ cell.x, cell.y });
}

json statejson::effect(const ActiveEffect & effect)
{
	const char * kind = "STAT_MOD";
	switch (effect.type)
	{
	case EffectType::SHIELD: kind = "SHIELD"; break;
	case EffectType::DOT: kind = "DOT"; break;
	case EffectType::HOT: kind = "HOT"; break;
	case EffectType::STATE: kind = "STATE"; break;
	default: break;
	}

	return {
		{ "uid", effect.uid },
		{ "kind", kind },
		{ "stat", toString(effect.stat) },
		{ "value", effect.value },
		{ "min", effect.minValue },
		{ "max", effect.maxValue },
		{ "turns", effect.remainingTurns },
		{ "skip", effect.skipNextDecrement },
		{ "name", effect.name },
		{ "spell", effect.spellId },
		{ "caster", effect.casterId },
		{ "positive", effect.positive },
		{ "state", effect.state }
	};
}

json statejson::glyph(const Glyph & glyph)
{
	json cells = json::array();
	for (const Cell & each : glyph.cells)
		cells.push_back(cell(each));

	return {
		{ "uid", glyph.uid },
		{ "caster", glyph.casterId },
		{ "team", glyph.team },
		{ "spell", glyph.spellId },
		{ "name", glyph.name },
		{ "cells", cells },
		{ "turns", glyph.remainingTurns }
	};
}

json statejson::block(const Block & block)
{
	return {
		{ "uid", block.uid },
		{ "group", block.group },
		{ "caster", block.casterId },
		{ "team", block.team },
		{ "spell", block.spellId },
		{ "name", block.name },
		{ "x", block.cell.x },
		{ "y", block.cell.y },
		{ "hp", block.hp },
		{ "maxHp", block.maxHp },
		{ "turns", block.remainingTurns },
		{ "move", block.blocksMove },
		{ "sight", block.blocksSight }
	};
}

json statejson::record(const FighterRecord & record)
{
	return {
		{ "dealt", record.dealt },
		{ "taken", record.taken },
		{ "healed", record.healed },
		{ "shielded", record.shielded },
		{ "kills", record.kills },
		{ "casts", record.casts },
		{ "combos", record.combos },
		{ "zonePoints", record.zonePoints },
		{ "badges", record.badges }
	};
}

json statejson::zone(const ZoneState & zone)
{
	if (!zone.enabled)
		return nullptr;
	json cells = json::array();
	for (const Cell & each : zone.cells)
		cells.push_back(cell(each));
	return { { "cells", cells }, { "points", zone.pointsToWin }, { "scores", { zone.scores[1], zone.scores[2] } }, { "holder", zone.holder } };
}

json statejson::fighter(const Fighter & fighter)
{
	json cooldowns = json::object();
	for (const auto & cooldown : fighter.cooldowns)
		cooldowns[cooldown.first] = cooldown.second;

	json casts = json::object();
	for (const auto & entry : fighter.castsThisTurn)
		casts[entry.first] = entry.second;

	json effects = json::array();
	for (const ActiveEffect & each : fighter.effects)
		effects.push_back(effect(each));

	json stats = json::object();
	for (int i = 0; i < STAT_COUNT; i++)
		stats[toString((Stat)i)] = fighter.baseStats.get((Stat)i);

	return {
		{ "id", fighter.id },
		{ "stats", stats },
		{ "team", fighter.team },
		{ "classId", fighter.classId },
		{ "name", fighter.name },
		{ "spells", fighter.spells },
		{ "talents", fighter.talents },
		{ "appearance", fighter.appearance },
		{ "x", fighter.position.x },
		{ "y", fighter.position.y },
		{ "hp", fighter.hp },
		{ "maxHp", fighter.maxHp },
		{ "shield", fighter.shield },
		{ "ap", fighter.ap },
		{ "mp", fighter.mp },
		{ "alive", fighter.alive },
		{ "ready", fighter.ready },
		{ "connected", fighter.connected },
		{ "piloted", fighter.piloted },
		{ "bank", fighter.timeBankMs },
		{ "cooldowns", cooldowns },
		{ "casts", casts },
		{ "effects", effects },
		{ "record", record(fighter.record) }
	};
}

json statejson::snapshot(const BattleState & state, const BattleMap & map, std::uint64_t seq, int viewer, std::int64_t remainingMs)
{
	json fighters = json::array();
	for (const Fighter & each : state.fighters)
		fighters.push_back(fighter(each));

	json glyphs = json::array();
	for (const Glyph & each : state.glyphs)
		glyphs.push_back(glyph(each));

	json blocks = json::array();
	for (const Block & each : state.blocks)
		blocks.push_back(block(each));

	json orbs = json::array();
	for (const Orb & orb : state.orbs)
		orbs.push_back({ { "uid", orb.uid }, { "kind", orb.kind }, { "x", orb.cell.x }, { "y", orb.cell.y } });

	json startCells = json::object();
	for (int team = 1; team <= 2; team++)
	{
		json cells = json::array();
		for (const Cell & each : map.startCells[team])
			cells.push_back(cell(each));
		startCells[std::to_string(team)] = cells;
	}

	return {
		{ "seq", seq },
		{ "you", viewer },
		{ "phase", toString(state.phase) },
		{ "round", state.round },
		{ "active", state.activeFighterId() },
		{ "order", state.turnOrder },
		{ "ms", remainingMs },
		{ "fighters", fighters },
		{ "glyphs", glyphs },
		{ "blocks", blocks },
		{ "bonuses", state.bonuses },
		{ "orbs", orbs },
		{ "startCells", startCells },
		{ "winner", state.winnerTeam },
		{ "reason", toString(state.endReason) },
		{ "mvp", state.mvpFighterId },
		{ "zone", zone(state.zone) }
	};
}
