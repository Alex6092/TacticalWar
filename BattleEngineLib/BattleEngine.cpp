#include "BattleEngine.h"

#include <algorithm>
#include <cmath>

using namespace tw::battle;
using nlohmann::json;

namespace
{
	json cellJson(const Cell & cell)
	{
		return json::array({ cell.x, cell.y });
	}
}

BattleEngine::BattleEngine(const GameData & data, const BattleMap & map, std::uint32_t seed)
	: data(data), map(map), rng(seed), seed(seed), seq(0), pendingEvents(json::array())
{
}

BattleEngine::BattleEngine(const GameData & data, const BattleMap & map, const BattleState & state, std::uint32_t seed)
	: data(data), map(map), state(state), rng(seed), seed(seed), seq(0), pendingEvents(json::array())
{
}

int BattleEngine::addFighter(int team, int classId, const std::string & name)
{
	const ClassDef * classDef = data.findClass(classId);
	if (classDef == nullptr || (team != 1 && team != 2) || state.phase != BattlePhase::PLACEMENT || state.round != 0)
		return -1;

	Fighter fighter;
	fighter.id = (int)state.fighters.size();
	fighter.team = team;
	fighter.classId = classId;
	fighter.name = name;
	fighter.baseStats = classDef->baseStats;
	fighter.maxHp = fighter.baseStats.get(Stat::MAX_HP);
	fighter.hp = fighter.maxHp;
	fighter.ap = fighter.baseStats.get(Stat::AP);
	fighter.mp = fighter.baseStats.get(Stat::MP);
	fighter.position = { -1, -1 };

	for (const SpellDef & spell : classDef->spells)
		fighter.cooldowns[spell.id] = spell.initialCooldown;

	state.fighters.push_back(fighter);
	return fighter.id;
}

void BattleEngine::startPlacement(std::int64_t nowMs)
{
	// Chaque combattant est placé sur une cellule de départ libre de son équipe
	// (ou, à défaut, sur n'importe quelle cellule libre).
	for (Fighter & fighter : state.fighters)
	{
		bool placed = false;
		for (const Cell & cell : map.startCells[fighter.team])
		{
			if (map.isWalkable(cell) && state.fighterAt(cell) == nullptr)
			{
				fighter.position = cell;
				placed = true;
				break;
			}
		}

		for (int y = 0; y < map.getHeight() && !placed; y++)
		{
			for (int x = 0; x < map.getWidth() && !placed; x++)
			{
				Cell cell = { x, y };
				if (map.isWalkable(cell) && state.fighterAt(cell) == nullptr)
				{
					fighter.position = cell;
					placed = true;
				}
			}
		}
	}

	state.phase = BattlePhase::PLACEMENT;
	state.deadlineMs = nowMs + (std::int64_t)data.rules.placementSeconds * 1000;
	emit({ { "t", "placement" }, { "ms", data.rules.placementSeconds * 1000 } });
}

ActionResult BattleEngine::place(int fighterId, const Cell & cell, std::int64_t nowMs)
{
	Fighter * fighter = state.findFighter(fighterId);
	if (fighter == nullptr || state.phase != BattlePhase::PLACEMENT)
		return ActionResult::failure("Placement impossible maintenant.");
	if (fighter->ready)
		return ActionResult::failure("Annulez « prêt » pour changer de place.");

	const std::vector<Cell> & starts = map.startCells[fighter->team];
	if (std::find(starts.begin(), starts.end(), cell) == starts.end())
		return ActionResult::failure("Cellule de départ invalide.");

	const Fighter * occupant = state.fighterAt(cell);
	if (occupant != nullptr && occupant->team != fighter->team)
		return ActionResult::failure("Cellule occupée.");

	if (occupant != nullptr && occupant->id != fighter->id)
	{
		// Échange de place avec un coéquipier.
		Fighter * teammate = state.findFighter(occupant->id);
		teammate->position = fighter->position;
		emit({ { "t", "place" }, { "f", teammate->id }, { "x", teammate->position.x }, { "y", teammate->position.y } });
	}

	fighter->position = cell;
	emit({ { "t", "place" }, { "f", fighter->id }, { "x", cell.x }, { "y", cell.y } });
	return ActionResult::success();
}

ActionResult BattleEngine::setReady(int fighterId, bool ready, std::int64_t nowMs)
{
	Fighter * fighter = state.findFighter(fighterId);
	if (fighter == nullptr || state.phase != BattlePhase::PLACEMENT)
		return ActionResult::failure("Le combat a déjà commencé.");

	fighter->ready = ready;
	emit({ { "t", "ready" }, { "f", fighter->id }, { "ready", ready } });

	bool everybodyReady = true;
	for (const Fighter & other : state.fighters)
		everybodyReady = everybodyReady && (other.ready || !other.connected);

	if (everybodyReady)
		startFight(nowMs);

	return ActionResult::success();
}

void BattleEngine::computeTurnOrder()
{
	// Dans chaque équipe : initiative décroissante (égalités départagées au hasard).
	std::vector<int> teams[3];
	std::vector<std::pair<int, int>> keyed[3];
	for (const Fighter & fighter : state.fighters)
	{
		int initiative = effectiveStat(state, data, fighter, Stat::INITIATIVE);
		keyed[fighter.team].push_back(std::make_pair(initiative * 1000 + (int)(rng() % 1000), fighter.id));
	}

	for (int team = 1; team <= 2; team++)
	{
		std::sort(keyed[team].begin(), keyed[team].end(), [](const std::pair<int, int> & a, const std::pair<int, int> & b) { return a.first > b.first; });
		for (const auto & entry : keyed[team])
			teams[team].push_back(entry.second);
	}

	// L'équipe du combattant le plus rapide commence, puis les équipes alternent.
	int first = 1;
	if (!keyed[2].empty() && (keyed[1].empty() || keyed[2][0].first > keyed[1][0].first))
		first = 2;
	int second = 3 - first;

	state.turnOrder.clear();
	std::size_t count = std::max(teams[1].size(), teams[2].size());
	for (std::size_t i = 0; i < count; i++)
	{
		if (i < teams[first].size())
			state.turnOrder.push_back(teams[first][i]);
		if (i < teams[second].size())
			state.turnOrder.push_back(teams[second][i]);
	}
}

void BattleEngine::startFight(std::int64_t nowMs)
{
	if (state.phase != BattlePhase::PLACEMENT)
		return;

	computeTurnOrder();
	state.phase = BattlePhase::FIGHT;
	state.round = 1;
	state.turnIndex = 0;

	emit({ { "t", "fight" }, { "order", state.turnOrder } });
	beginTurn(nowMs);
}

void BattleEngine::beginTurn(std::int64_t nowMs)
{
	for (std::size_t guard = 0; guard <= state.turnOrder.size() && state.phase == BattlePhase::FIGHT; guard++)
	{
		Fighter & fighter = *state.findFighter(state.activeFighterId());
		if (!fighter.alive)
		{
			finishTurn(nowMs);
			return;
		}

		fighter.castsThisTurn.clear();
		fighter.castsOnTarget.clear();
		fighter.ap = std::max(0, effectiveStat(state, data, fighter, Stat::AP));
		fighter.mp = std::max(0, effectiveStat(state, data, fighter, Stat::MP));

		for (auto & cooldown : fighter.cooldowns)
		{
			if (cooldown.second > 0)
				cooldown.second--;
		}

		emit({ { "t", "turn" }, { "f", fighter.id }, { "round", state.round } });

		// Les glyphes du combattant s'usent au début de ses tours.
		for (auto it = state.glyphs.begin(); it != state.glyphs.end();)
		{
			if (it->casterId == fighter.id && --it->remainingTurns <= 0)
			{
				emit({ { "t", "glyph-" }, { "uid", it->uid } });
				it = state.glyphs.erase(it);
			}
			else
			{
				it++;
			}
		}

		tickEffectsAtTurnStart(fighter);
		if (fighter.alive)
			triggerGlyphs(fighter);

		// Mort subite : des dégâts croissants empêchent les combats sans fin.
		if (fighter.alive && state.round >= data.rules.suddenDeathRound)
		{
			int steps = state.round - data.rules.suddenDeathRound + 1;
			int damage = (int)std::ceil(fighter.initialMaxHp() * data.rules.suddenDeathPercentPerRound * steps / 100.0);
			dealDamage(fighter, damage, -1, "sudden");
		}

		if (checkEnd(fighter.id))
			return;

		emitStats(fighter);

		if (!fighter.alive)
		{
			finishTurn(nowMs);
			return;
		}

		int seconds = fighter.connected ? data.rules.turnSeconds : data.rules.disconnectedTurnSeconds;
		state.deadlineMs = nowMs + (std::int64_t)seconds * 1000;
		emit({ { "t", "timer" }, { "f", fighter.id }, { "ms", seconds * 1000 } });
		return;
	}
}

void BattleEngine::finishTurn(std::int64_t nowMs)
{
	if (state.phase != BattlePhase::FIGHT)
		return;

	Fighter & fighter = *state.findFighter(state.activeFighterId());

	// Durée des effets du combattant (voir Annexe A : le tour d'application ne compte pas).
	for (ActiveEffect & effect : fighter.effects)
	{
		if (effect.skipNextDecrement)
			effect.skipNextDecrement = false;
		else
			effect.remainingTurns--;
	}
	removeEffects(fighter, [](const ActiveEffect & effect) { return effect.remainingTurns <= 0; });

	if (fighter.alive)
		emit({ { "t", "endturn" }, { "f", fighter.id } });

	// Combattant suivant (les morts restent dans l'ordre mais sont sautés).
	bool anyAlive = false;
	for (const Fighter & other : state.fighters)
		anyAlive = anyAlive || other.alive;
	if (!anyAlive)
	{
		checkEnd(fighter.id);
		return;
	}

	do
	{
		state.turnIndex++;
		if (state.turnIndex >= (int)state.turnOrder.size())
		{
			state.turnIndex = 0;
			state.round++;
		}
	} while (!state.findFighter(state.activeFighterId())->alive);

	if (state.round > data.rules.maxRounds)
	{
		double hp1 = teamHpPercent(1);
		double hp2 = teamHpPercent(2);
		endBattle(hp1 >= hp2 ? 1 : 2, EndReason::ROUND_LIMIT);
		return;
	}

	beginTurn(nowMs);
}

ActionResult BattleEngine::endTurn(int fighterId, std::int64_t nowMs)
{
	if (state.phase != BattlePhase::FIGHT || state.activeFighterId() != fighterId)
		return ActionResult::failure("Ce n'est pas votre tour.");

	finishTurn(nowMs);
	return ActionResult::success();
}

ActionResult BattleEngine::move(int fighterId, const std::vector<Cell> & path, std::int64_t nowMs)
{
	if (state.phase != BattlePhase::FIGHT || state.activeFighterId() != fighterId)
		return ActionResult::failure("Ce n'est pas votre tour.");

	Fighter & fighter = *state.findFighter(fighterId);
	if (fighter.mp <= 0)
		return ActionResult::failure("Plus de PM.");

	MovePreview preview = previewMove(state, map, data, fighter, path);
	if (!preview.error.empty())
		return ActionResult::failure(preview.error);

	json pathJson = json::array();
	for (const Cell & cell : preview.path)
		pathJson.push_back(cellJson(cell));

	json tackles = json::array();
	for (const TackleLoss & loss : preview.tackles)
		tackles.push_back({ { "step", loss.stepIndex }, { "mp", loss.lostMp }, { "ap", loss.lostAp } });

	if (!preview.path.empty())
		fighter.position = preview.path.back();
	fighter.mp = preview.mpAfter;
	fighter.ap = preview.apAfter;

	emit({ { "t", "move" }, { "f", fighter.id }, { "path", pathJson }, { "tackles", tackles }, { "ap", fighter.ap }, { "mp", fighter.mp } });

	if (fighter.ap <= 0 && fighter.mp <= 0)
		finishTurn(nowMs);

	return ActionResult::success();
}

ActionResult BattleEngine::cast(int fighterId, int spellIndex, const Cell & target, std::int64_t nowMs)
{
	if (state.phase != BattlePhase::FIGHT || state.activeFighterId() != fighterId)
		return ActionResult::failure("Ce n'est pas votre tour.");

	Fighter & caster = *state.findFighter(fighterId);
	const SpellDef * spell = spellOf(data, caster, spellIndex);
	if (spell == nullptr)
		return ActionResult::failure("Sort inconnu.");

	std::string error = checkSpellResources(caster, *spell);
	if (error.empty())
		error = checkTarget(state, map, data, caster, *spell, target);
	if (!error.empty())
		return ActionResult::failure(error);

	caster.ap -= spell->apCost;
	if (spell->cooldown > 0)
		caster.cooldowns[spell->id] = spell->cooldown;
	caster.castsThisTurn[spell->id]++;
	const Fighter * targeted = state.fighterAt(target);
	if (targeted != nullptr)
		caster.castsOnTarget[spell->id][targeted->id]++;

	emit({ { "t", "cast" }, { "f", caster.id }, { "spell", spell->id }, { "slot", spellIndex }, { "x", target.x }, { "y", target.y } });

	// Les combattants touchés sont déterminés au moment du lancer (avant poussées et bonds).
	std::vector<int> targetIds;
	for (const Cell & cell : impactCells(map, caster.position, target, spell->impact))
	{
		const Fighter * hit = state.fighterAt(cell);
		if (hit != nullptr)
			targetIds.push_back(hit->id);
	}

	for (const EffectDef & effect : spell->effects)
	{
		if (state.phase == BattlePhase::ENDED)
			break;
		applySpellEffect(caster, *spell, effect, target, targetIds);
	}

	if (caster.alive)
		applyOnCastPassive(caster);

	emitStats(caster);

	if (checkEnd(caster.id))
		return ActionResult::success();

	if (!caster.alive || (caster.ap <= 0 && caster.mp <= 0))
		finishTurn(nowMs);

	return ActionResult::success();
}

void BattleEngine::tick(std::int64_t nowMs)
{
	if (state.phase == BattlePhase::PLACEMENT && state.deadlineMs > 0 && nowMs >= state.deadlineMs)
	{
		startFight(nowMs);
	}
	else if (state.phase == BattlePhase::FIGHT && nowMs >= state.deadlineMs)
	{
		emit({ { "t", "timeout" }, { "f", state.activeFighterId() } });
		finishTurn(nowMs);
	}
}

void BattleEngine::setConnected(int fighterId, bool connected, std::int64_t nowMs)
{
	Fighter * fighter = state.findFighter(fighterId);
	if (fighter == nullptr || fighter->connected == connected)
		return;

	fighter->connected = connected;
	emit({ { "t", "connection" }, { "f", fighterId }, { "connected", connected } });

	// Le tour d'un joueur déconnecté est raccourci.
	if (!connected && state.phase == BattlePhase::FIGHT && state.activeFighterId() == fighterId)
	{
		std::int64_t shortened = nowMs + (std::int64_t)data.rules.disconnectedTurnSeconds * 1000;
		if (shortened < state.deadlineMs)
		{
			state.deadlineMs = shortened;
			emit({ { "t", "timer" }, { "f", fighterId }, { "ms", data.rules.disconnectedTurnSeconds * 1000 } });
		}
	}

	// En placement, un joueur absent ne bloque pas le départ.
	if (!connected && state.phase == BattlePhase::PLACEMENT)
	{
		bool everybodyReady = true;
		for (const Fighter & other : state.fighters)
			everybodyReady = everybodyReady && (other.ready || !other.connected);
		bool someoneConnected = false;
		for (const Fighter & other : state.fighters)
			someoneConnected = someoneConnected || other.connected;
		if (everybodyReady && someoneConnected)
			startFight(nowMs);
	}
}

void BattleEngine::forfeit(int team, std::int64_t nowMs)
{
	if (state.phase == BattlePhase::ENDED)
		return;
	endBattle(team == 1 ? 2 : 1, EndReason::FORFEIT);
}

void BattleEngine::stopByDecision(std::int64_t nowMs)
{
	if (state.phase == BattlePhase::ENDED)
		return;
	double hp1 = teamHpPercent(1);
	double hp2 = teamHpPercent(2);
	endBattle(hp1 >= hp2 ? 1 : 2, EndReason::ADMIN);
}

void BattleEngine::declareWinner(int winnerTeam, std::int64_t nowMs)
{
	if (state.phase == BattlePhase::ENDED || (winnerTeam != 1 && winnerTeam != 2))
		return;
	endBattle(winnerTeam, EndReason::ADMIN);
}

double BattleEngine::teamHpPercent(int team) const
{
	int hp = 0;
	int max = 0;
	for (const Fighter & fighter : state.fighters)
	{
		if (fighter.team != team)
			continue;
		hp += fighter.alive ? fighter.hp : 0;
		max += fighter.initialMaxHp();
	}
	return max > 0 ? 100.0 * hp / max : 0;
}

bool BattleEngine::checkEnd(int actingFighterId)
{
	if (state.phase == BattlePhase::ENDED)
		return true;

	bool alive[3] = { false, false, false };
	for (const Fighter & fighter : state.fighters)
		alive[fighter.team] = alive[fighter.team] || fighter.alive;

	if (alive[1] && alive[2])
		return false;

	int winner;
	if (!alive[1] && !alive[2])
	{
		// Les deux équipes tombent sur la même action : l'équipe de celui qui a agi perd.
		const Fighter * acting = state.findFighter(actingFighterId);
		winner = acting != nullptr && acting->team == 1 ? 2 : 1;
	}
	else
	{
		winner = alive[1] ? 1 : 2;
	}

	endBattle(winner, EndReason::KO);
	return true;
}

void BattleEngine::endBattle(int winnerTeam, EndReason reason)
{
	state.phase = BattlePhase::ENDED;
	state.winnerTeam = winnerTeam;
	state.endReason = reason;
	state.deadlineMs = 0;

	emit({
		{ "t", "end" },
		{ "winner", winnerTeam },
		{ "reason", toString(reason) },
		{ "hp1", teamHpPercent(1) },
		{ "hp2", teamHpPercent(2) },
		{ "round", state.round }
	});
}

int BattleEngine::roll(int min, int max)
{
	if (max <= min)
		return min;
	if (rollMode == RollMode::MIN)
		return min;
	if (rollMode == RollMode::MAX)
		return max;
	return std::uniform_int_distribution<int>(min, max)(rng);
}

void BattleEngine::emit(const json & event)
{
	pendingEvents.push_back(event);
}

void BattleEngine::emitStats(const Fighter & fighter)
{
	json cooldowns = json::object();
	for (const auto & cooldown : fighter.cooldowns)
		cooldowns[cooldown.first] = cooldown.second;

	json casts = json::object();
	for (const auto & entry : fighter.castsThisTurn)
		casts[entry.first] = entry.second;

	emit({
		{ "t", "stats" },
		{ "f", fighter.id },
		{ "ap", fighter.ap },
		{ "mp", fighter.mp },
		{ "hp", fighter.hp },
		{ "maxHp", fighter.maxHp },
		{ "shield", fighter.shield },
		{ "cooldowns", cooldowns },
		{ "casts", casts }
	});
}

json BattleEngine::flushEvents()
{
	json batch = { { "seq", ++seq }, { "ev", pendingEvents } };
	pendingEvents = json::array();
	return batch;
}

json BattleEngine::effectJson(const ActiveEffect & effect) const
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

json BattleEngine::glyphJson(const Glyph & glyph) const
{
	json cells = json::array();
	for (const Cell & cell : glyph.cells)
		cells.push_back(cellJson(cell));

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

json BattleEngine::fighterJson(const Fighter & fighter) const
{
	json cooldowns = json::object();
	for (const auto & cooldown : fighter.cooldowns)
		cooldowns[cooldown.first] = cooldown.second;

	json casts = json::object();
	for (const auto & entry : fighter.castsThisTurn)
		casts[entry.first] = entry.second;

	json effects = json::array();
	for (const ActiveEffect & effect : fighter.effects)
		effects.push_back(effectJson(effect));

	json stats = json::object();
	for (int i = 0; i < STAT_COUNT; i++)
		stats[toString((Stat)i)] = fighter.baseStats.get((Stat)i);

	return {
		{ "id", fighter.id },
		{ "stats", stats },
		{ "team", fighter.team },
		{ "classId", fighter.classId },
		{ "name", fighter.name },
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
		{ "cooldowns", cooldowns },
		{ "casts", casts },
		{ "effects", effects }
	};
}

json BattleEngine::snapshot(int viewerFighterId, std::int64_t nowMs) const
{
	json fighters = json::array();
	for (const Fighter & fighter : state.fighters)
		fighters.push_back(fighterJson(fighter));

	json glyphs = json::array();
	for (const Glyph & glyph : state.glyphs)
		glyphs.push_back(glyphJson(glyph));

	json startCells = json::object();
	for (int team = 1; team <= 2; team++)
	{
		json cells = json::array();
		for (const Cell & cell : map.startCells[team])
			cells.push_back(cellJson(cell));
		startCells[std::to_string(team)] = cells;
	}

	std::int64_t remaining = state.deadlineMs > nowMs ? state.deadlineMs - nowMs : 0;

	return {
		{ "seq", seq },
		{ "you", viewerFighterId },
		{ "phase", toString(state.phase) },
		{ "round", state.round },
		{ "active", state.activeFighterId() },
		{ "order", state.turnOrder },
		{ "ms", remaining },
		{ "fighters", fighters },
		{ "glyphs", glyphs },
		{ "startCells", startCells },
		{ "winner", state.winnerTeam },
		{ "reason", toString(state.endReason) }
	};
}
