// Application des effets des sorts, des glyphes et des passifs.
#include "BattleEngine.h"
#include "StateJson.h"

#include <algorithm>
#include <cmath>

using namespace tw::battle;
using nlohmann::json;

namespace
{
	const char * PASSIVE_SPELL_ID = "__passive";

	bool matchesFilter(TargetFilter filter, const Fighter & caster, const Fighter & target)
	{
		switch (filter)
		{
		case TargetFilter::ENEMIES: return target.team != caster.team;
		case TargetFilter::ALLIES: return target.team == caster.team;
		case TargetFilter::ALL: return true;
		case TargetFilter::CASTER: return target.id == caster.id;
		}
		return false;
	}
}

void BattleEngine::applySpellEffect(Fighter & caster, const SpellDef & spell, const EffectDef & effect, const Cell & target, const std::vector<int> & targetIds,
	const std::vector<int> & blockIds)
{
	switch (effect.type)
	{
	case EffectType::DASH:
	{
		// Bond sur la cellule adjacente à la cible, du côté du lanceur.
		Cell direction = directionBetween(caster.position, target);
		Cell destination = { target.x - direction.x, target.y - direction.y };
		if (destination != caster.position && cellWalkable(state, map, destination) && state.fighterAt(destination) == nullptr)
			moveFighterTo(caster, destination, "dash");
		return;
	}
	case EffectType::TELEPORT:
	{
		const Fighter * occupant = state.fighterAt(target);
		if (occupant == nullptr && cellWalkable(state, map, target))
		{
			moveFighterTo(caster, target, "teleport");
		}
		else if (occupant != nullptr && effect.allowSwap && occupant->team == caster.team && occupant->id != caster.id
			&& !occupant->hasState("unmovable"))
		{
			Fighter & ally = *state.findFighter(occupant->id);
			Cell casterCell = caster.position;
			caster.position = ally.position;
			ally.position = casterCell;
			emit({ { "t", "swap" }, { "f", caster.id }, { "other", ally.id },
				{ "x", caster.position.x }, { "y", caster.position.y },
				{ "ox", ally.position.x }, { "oy", ally.position.y } });
			pickUpOrb(caster, caster.position);
			pickUpOrb(ally, ally.position);
		}
		return;
	}
	case EffectType::GLYPH:
	{
		Glyph glyph;
		glyph.uid = state.nextUid++;
		glyph.casterId = caster.id;
		glyph.team = caster.team;
		glyph.spellId = spell.id;
		glyph.name = effect.name.empty() ? spell.name : effect.name;
		glyph.cells = impactCells(map, caster.position, target, { effect.glyphShape, effect.glyphSize });
		glyph.remainingTurns = effect.duration;
		glyph.targets = effect.targets;
		glyph.effects = effect.glyphEffects;
		state.glyphs.push_back(glyph);
		emit({ { "t", "glyph+" }, { "glyph", statejson::glyph(glyph) } });
		return;
	}
	case EffectType::WALL:
		placeWall(caster, spell, effect, target);
		return;
	default:
		break;
	}

	if (effect.targets == TargetFilter::CASTER)
	{
		applyEffectToTarget(caster, spell.id, effect, caster, target);
		return;
	}

	// Poussées : les cibles les plus éloignées d'abord (elles libèrent la place),
	// attirances : les plus proches d'abord.
	std::vector<int> ordered = targetIds;
	if (effect.type == EffectType::PUSH || effect.type == EffectType::PULL)
	{
		Cell origin = caster.position;
		bool farthestFirst = effect.type == EffectType::PUSH;
		std::stable_sort(ordered.begin(), ordered.end(), [&](int a, int b) {
			int da = manhattan(state.findFighter(a)->position, origin);
			int db = manhattan(state.findFighter(b)->position, origin);
			return farthestFirst ? da > db : da < db;
		});
	}

	for (int id : ordered)
	{
		Fighter & fighter = *state.findFighter(id);
		if (fighter.alive && matchesFilter(effect.targets, caster, fighter))
			applyEffectToTarget(caster, spell.id, effect, fighter, target);
		if (state.phase == BattlePhase::ENDED)
			return;
	}

	// Dégâts directs : les blocs de mur de la zone sont touchés aussi, quel que soit leur camp. Jet du
	// sort et puissance du lanceur, sans combinaison, passif ni résistance ; le vol de vie ne rend rien.
	if (effect.type == EffectType::DAMAGE || effect.type == EffectType::LIFESTEAL)
	{
		int power = effectiveStat(state, data, caster, Stat::POWER);
		for (int uid : blockIds)
		{
			if (state.findBlock(uid) != nullptr)
				damageBlock(uid, std::max(0, (int)std::lround(roll(effect.min, effect.max) * (100.0 + power) / 100.0)), caster.id, "spell");
		}
	}
}

void BattleEngine::placeWall(Fighter & caster, const SpellDef & spell, const EffectDef & effect, const Cell & target)
{
	json blocks = json::array();
	int group = state.nextUid++;
	for (const Cell & cell : wallCells(state, map, caster.position, target, spell.impact))
	{
		Block block;
		block.uid = state.nextUid++;
		block.group = group;
		block.casterId = caster.id;
		block.team = caster.team;
		block.spellId = spell.id;
		block.name = effect.name.empty() ? spell.name : effect.name;
		block.cell = cell;
		block.hp = effect.wallHp;
		block.maxHp = effect.wallHp;
		block.remainingTurns = effect.duration;
		block.blocksMove = effect.wallBlocksMove;
		block.blocksSight = effect.wallBlocksSight;
		state.blocks.push_back(block);
		blocks.push_back(statejson::block(block));
	}
	if (!blocks.empty())
		emit({ { "t", "block+" }, { "f", caster.id }, { "blocks", blocks } });
}

void BattleEngine::damageBlock(int blockUid, int amount, int sourceId, const std::string & kind)
{
	Block * block = state.findBlock(blockUid);
	if (block == nullptr || amount <= 0)
		return;
	block->hp = std::max(0, block->hp - amount);
	emit({ { "t", "blockhit" }, { "uid", blockUid }, { "src", sourceId }, { "kind", kind }, { "amount", amount }, { "hp", block->hp },
		{ "x", block->cell.x }, { "y", block->cell.y } });
	if (block->hp <= 0)
		removeBlocks([blockUid](const Block & candidate) { return candidate.uid == blockUid; }, "destroyed");
}

void BattleEngine::removeBlocks(const std::function<bool(const Block &)> & predicate, const std::string & reason)
{
	for (auto it = state.blocks.begin(); it != state.blocks.end();)
	{
		if (predicate(*it))
		{
			emit({ { "t", "block-" }, { "uid", it->uid }, { "reason", reason }, { "spell", it->spellId }, { "name", it->name },
				{ "x", it->cell.x }, { "y", it->cell.y } });
			it = state.blocks.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void BattleEngine::applyEffectToTarget(Fighter & caster, const std::string & spellId, const EffectDef & effect, Fighter & target, const Cell & targetCell)
{
	if (!target.alive)
		return;

	switch (effect.type)
	{
	case EffectType::DAMAGE:
	{
		int combo = triggerCombo(caster, effect, target);
		dealDamage(target, computeDamage(caster, target, roll(effect.min, effect.max), combo), caster.id, "spell");
		break;
	}

	case EffectType::LIFESTEAL:
	{
		int combo = triggerCombo(caster, effect, target);
		int dealt = dealDamage(target, computeDamage(caster, target, roll(effect.min, effect.max), combo), caster.id, "spell");
		if (caster.alive && dealt > 0)
			heal(caster, dealt * effect.percent / 100, caster.id, "lifesteal");
		break;
	}

	case EffectType::HEAL:
	{
		int bonus = effectiveStat(state, data, caster, Stat::HEAL_BONUS);
		heal(target, roll(effect.min, effect.max) * (100 + bonus) / 100, caster.id, "spell");
		break;
	}

	case EffectType::SHIELD:
	{
		ActiveEffect shield;
		shield.type = EffectType::SHIELD;
		shield.value = roll(effect.min, effect.max);
		shield.remainingTurns = effect.duration;
		shield.casterId = caster.id;
		shield.spellId = spellId;
		shield.name = effect.name;
		shield.positive = true;
		caster.record.shielded += shield.value;
		addActiveEffect(target, shield, effect.refresh);
		break;
	}

	case EffectType::DOT:
	case EffectType::HOT:
	{
		ActiveEffect overTime;
		overTime.type = effect.type;
		overTime.minValue = effect.min;
		overTime.maxValue = effect.max;
		overTime.casterPower = effect.type == EffectType::DOT
			? effectiveStat(state, data, caster, Stat::POWER)
			: effectiveStat(state, data, caster, Stat::HEAL_BONUS);
		overTime.remainingTurns = effect.duration;
		overTime.casterId = caster.id;
		overTime.spellId = spellId;
		overTime.name = effect.name;
		overTime.positive = effect.type == EffectType::HOT;
		addActiveEffect(target, overTime, effect.refresh);
		break;
	}

	case EffectType::STAT_MOD:
	{
		ActiveEffect modifier;
		modifier.type = EffectType::STAT_MOD;
		modifier.stat = effect.stat;
		modifier.value = roll(effect.min, effect.max);
		modifier.remainingTurns = effect.duration;
		modifier.casterId = caster.id;
		modifier.spellId = spellId;
		modifier.name = effect.name;
		modifier.positive = modifier.value > 0;
		addActiveEffect(target, modifier, effect.refresh);
		break;
	}

	case EffectType::STATE:
	{
		ActiveEffect stateEffect;
		stateEffect.type = EffectType::STATE;
		stateEffect.state = effect.state;
		stateEffect.remainingTurns = effect.duration;
		stateEffect.casterId = caster.id;
		stateEffect.spellId = spellId;
		stateEffect.name = effect.name.empty() ? effect.state : effect.name;
		stateEffect.positive = !effect.negative;
		addActiveEffect(target, stateEffect, effect.refresh);
		break;
	}

	case EffectType::PUSH:
		pushFighter(caster, target, effect.min, false);
		break;

	case EffectType::PULL:
		pushFighter(caster, target, effect.min, true);
		break;

	case EffectType::DISPEL:
		removeEffects(target, [&](const ActiveEffect & active) {
			switch (effect.dispel)
			{
			case DispelMode::NEGATIVE: return !active.positive;
			case DispelMode::POSITIVE: return active.positive;
			default: return true;
			}
		});
		emitStats(target);
		break;

	default:
		break;
	}
}

int BattleEngine::triggerCombo(Fighter & caster, const EffectDef & effect, Fighter & target)
{
	if (effect.comboState.empty() || effect.comboPercent <= 0 || !target.hasState(effect.comboState))
		return 0;

	caster.record.combos++;
	emit({ { "t", "combo" }, { "f", target.id }, { "src", caster.id }, { "name", effect.comboName }, { "percent", effect.comboPercent } });
	if (effect.comboConsumes)
	{
		removeEffects(target, [&](const ActiveEffect & active) {
			return active.type == EffectType::STATE && active.state == effect.comboState;
		});
	}
	return effect.comboPercent;
}

int BattleEngine::computeDamage(const Fighter & caster, const Fighter & target, int baseRoll, int comboPercent) const
{
	int power = effectiveStat(state, data, caster, Stat::POWER);

	const ClassDef * classDef = data.findClass(caster.classId);
	if (classDef != nullptr)
	{
		const PassiveDef & passive = classDef->passive;
		if (passive.type == PassiveType::LOW_HP_DAMAGE)
		{
			int hpPercent = caster.hp * 100 / std::max(1, caster.initialMaxHp());
			if (passive.threshold2 > 0 && hpPercent < passive.threshold2)
				power += passive.bonus2;
			else if (hpPercent < passive.threshold)
				power += passive.bonus;
		}
		else if (passive.type == PassiveType::DISTANCE_DAMAGE && manhattan(caster.position, target.position) >= passive.distance)
		{
			power += passive.bonus;
		}
	}

	int maxResistance = data.rules.maxResistance;
	int resistance = std::max(-maxResistance, std::min(maxResistance, effectiveStat(state, data, target, Stat::RESISTANCE)));

	double damage = baseRoll * (100.0 + power) / 100.0 * (100.0 + comboPercent) / 100.0 * (100.0 - resistance) / 100.0;
	return std::max(0, (int)std::lround(damage));
}

int BattleEngine::dealDamage(Fighter & target, int amount, int sourceId, const std::string & kind)
{
	if (!target.alive || amount <= 0)
		return 0;

	// Le bouclier absorbe en premier (les boucliers les plus anciens d'abord).
	int absorbed = 0;
	for (ActiveEffect & effect : target.effects)
	{
		if (effect.type != EffectType::SHIELD || absorbed >= amount)
			continue;
		int taken = std::min(effect.value, amount - absorbed);
		effect.value -= taken;
		absorbed += taken;
	}
	target.shield -= absorbed;

	int hpLoss = amount - absorbed;
	target.hp -= hpLoss;

	// Bilan : dégâts subis, et infligés par un ennemi (le lanceur d'un poison, le pousseur d'une collision).
	Fighter * source = state.findFighter(sourceId);
	bool byEnemy = source != nullptr && source->team != target.team;
	target.record.taken += amount;
	if (byEnemy)
		source->record.dealt += amount;

	// Érosion : une partie des PV perdus est retirée des PV max.
	int erosion = effectiveStat(state, data, target, Stat::EROSION);
	if (erosion > 0 && hpLoss > 0)
	{
		target.maxHp -= hpLoss * erosion / 100;
		if (target.maxHp < 1)
			target.maxHp = 1;
	}
	if (target.hp > target.maxHp)
		target.hp = target.maxHp;

	if (target.hp <= 0)
	{
		target.hp = 0;
		target.alive = false;
		if (byEnemy)
		{
			source->record.kills++;
			if (state.firstBloodFighterId < 0)
				state.firstBloodFighterId = source->id;
		}
	}

	emit({
		{ "t", "damage" },
		{ "f", target.id },
		{ "src", sourceId },
		{ "kind", kind },
		{ "amount", amount },
		{ "absorbed", absorbed },
		{ "hp", target.hp },
		{ "maxHp", target.maxHp },
		{ "shield", target.shield }
	});

	// Boucliers épuisés.
	removeEffects(target, [](const ActiveEffect & effect) { return effect.type == EffectType::SHIELD && effect.value <= 0; });

	if (!target.alive)
	{
		emit({ { "t", "death" }, { "f", target.id } });

		// Les glyphes et les murs d'un combattant mort disparaissent.
		for (auto it = state.glyphs.begin(); it != state.glyphs.end();)
		{
			if (it->casterId == target.id)
			{
				emit({ { "t", "glyph-" }, { "uid", it->uid } });
				it = state.glyphs.erase(it);
			}
			else
			{
				it++;
			}
		}
		int deadId = target.id;
		removeBlocks([deadId](const Block & block) { return block.casterId == deadId; }, "caster");
	}

	return amount;
}

int BattleEngine::heal(Fighter & target, int amount, int sourceId, const std::string & kind)
{
	if (!target.alive || amount <= 0)
		return 0;

	int healed = std::min(amount, target.maxHp - target.hp);
	target.hp += healed;
	Fighter * source = state.findFighter(sourceId);
	if (source != nullptr)
		source->record.healed += healed;
	emit({ { "t", "heal" }, { "f", target.id }, { "src", sourceId }, { "kind", kind }, { "amount", healed }, { "hp", target.hp }, { "maxHp", target.maxHp } });
	return healed;
}

void BattleEngine::addActiveEffect(Fighter & target, ActiveEffect effect, bool refresh)
{
	// Effet non cumulable : celui du même sort et du même lanceur est remplacé.
	if (refresh)
	{
		removeEffects(target, [&](const ActiveEffect & existing) {
			return existing.spellId == effect.spellId && existing.casterId == effect.casterId
				&& existing.type == effect.type && existing.stat == effect.stat && existing.state == effect.state;
		});
	}

	effect.uid = state.nextUid++;
	// Appliqué pendant le tour du porteur : ce tour ne compte pas dans la durée.
	effect.skipNextDecrement = target.id == state.activeFighterId() && effect.remainingTurns > 0;

	if (effect.type == EffectType::SHIELD)
		target.shield += effect.value;

	// Les PA / PM du porteur actif changent immédiatement.
	if (effect.type == EffectType::STAT_MOD && target.id == state.activeFighterId())
	{
		if (effect.stat == Stat::AP)
			target.ap = std::max(0, target.ap + effect.value);
		else if (effect.stat == Stat::MP)
			target.mp = std::max(0, target.mp + effect.value);
	}

	target.effects.push_back(effect);
	emit({ { "t", "effect+" }, { "f", target.id }, { "effect", statejson::effect(effect) }, { "shield", target.shield } });

	if (effect.type == EffectType::STAT_MOD || effect.type == EffectType::SHIELD)
		emitStats(target);
}

void BattleEngine::removeEffects(Fighter & target, const std::function<bool(const ActiveEffect &)> & predicate)
{
	for (auto it = target.effects.begin(); it != target.effects.end();)
	{
		if (predicate(*it))
		{
			if (it->type == EffectType::SHIELD)
				target.shield = std::max(0, target.shield - it->value);

			emit({ { "t", "effect-" }, { "f", target.id }, { "uid", it->uid }, { "shield", target.shield } });
			it = target.effects.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void BattleEngine::moveFighterTo(Fighter & fighter, const Cell & cell, const std::string & kind)
{
	fighter.position = cell;
	emit({ { "t", "slide" }, { "f", fighter.id }, { "kind", kind }, { "x", cell.x }, { "y", cell.y } });
	pickUpOrb(fighter, cell);
}

void BattleEngine::pushFighter(Fighter & caster, Fighter & target, int distance, bool towardsCaster)
{
	if (target.id == caster.id || target.hasState("unmovable") || distance <= 0)
		return;

	Cell direction = towardsCaster ? directionBetween(target.position, caster.position) : directionBetween(caster.position, target.position);
	if (direction.x == 0 && direction.y == 0)
		return;

	Cell position = target.position;
	int remaining = distance;
	Fighter * collided = nullptr;
	int collidedBlock = 0;

	while (remaining > 0)
	{
		Cell next = { position.x + direction.x, position.y + direction.y };
		if (towardsCaster && next == caster.position)
			break;

		const Fighter * occupant = state.fighterAt(next);
		if (!cellWalkable(state, map, next) || occupant != nullptr)
		{
			if (occupant != nullptr)
				collided = state.findFighter(occupant->id);
			else if (state.blockAt(next) != nullptr && map.isWalkable(next))
				collidedBlock = state.blockAt(next)->uid;
			break;
		}

		position = next;
		remaining--;
	}

	if (position != target.position)
		moveFighterTo(target, position, towardsCaster ? "pull" : "push");

	// Poussée bloquée : dégâts de collision (fixes, sans bonus ni résistance).
	if (!towardsCaster && remaining > 0)
	{
		dealDamage(target, remaining * data.rules.collisionDamagePerCell, caster.id, "collision");
		if (collided != nullptr)
			dealDamage(*collided, remaining * data.rules.collisionDamageToHit, caster.id, "collision");
		// Un mur percuté est abîmé aussi.
		if (collidedBlock != 0)
			damageBlock(collidedBlock, remaining * data.rules.collisionDamageToHit, caster.id, "collision");
	}
}

void BattleEngine::tickEffectsAtTurnStart(Fighter & fighter)
{
	std::vector<ActiveEffect> overTime;
	for (const ActiveEffect & effect : fighter.effects)
	{
		if (effect.type == EffectType::DOT || effect.type == EffectType::HOT)
			overTime.push_back(effect);
	}

	for (const ActiveEffect & effect : overTime)
	{
		if (!fighter.alive)
			return;

		int value = roll(effect.minValue, effect.maxValue);
		if (effect.type == EffectType::DOT)
		{
			dealDamage(fighter, periodicDamage(state, data, fighter, value, effect.casterPower), effect.casterId, "dot");
		}
		else
		{
			heal(fighter, value * (100 + effect.casterPower) / 100, effect.casterId, "hot");
		}
	}
}

void BattleEngine::triggerGlyphs(Fighter & fighter)
{
	std::vector<Glyph> glyphs = state.glyphs;
	for (const Glyph & glyph : glyphs)
	{
		if (std::find(glyph.cells.begin(), glyph.cells.end(), fighter.position) == glyph.cells.end())
			continue;

		Fighter * caster = state.findFighter(glyph.casterId);
		if (caster == nullptr)
			continue;

		for (const EffectDef & effect : glyph.effects)
		{
			if (!fighter.alive || state.phase == BattlePhase::ENDED)
				return;
			if (!matchesFilter(effect.targets, *caster, fighter))
				continue;

			emit({ { "t", "glyph" }, { "uid", glyph.uid }, { "f", fighter.id } });
			applyEffectToTarget(*caster, glyph.spellId, effect, fighter, fighter.position);
		}
	}
}

void BattleEngine::applyTerrain(Fighter & fighter)
{
	// Valeurs fixes (ni puissance ni résistance) ; le bouclier absorbe les dégâts comme d'habitude.
	int damage = map.turnDamage(fighter.position);
	if (damage > 0)
		dealDamage(fighter, damage, -1, "terrain");
	int healing = map.turnHeal(fighter.position);
	if (healing > 0 && fighter.alive)
		heal(fighter, healing, -1, "terrain");
}

void BattleEngine::applyOnCastPassive(Fighter & caster)
{
	const ClassDef * classDef = data.findClass(caster.classId);
	if (classDef == nullptr || classDef->passive.type != PassiveType::ON_CAST_POWER)
		return;

	int stacks = 0;
	for (const ActiveEffect & effect : caster.effects)
	{
		if (effect.spellId == PASSIVE_SPELL_ID)
			stacks++;
	}
	if (stacks >= classDef->passive.maxStacks)
		return;

	// Bonus jusqu'à la fin du tour en cours (durée 0).
	ActiveEffect bonus;
	bonus.type = EffectType::STAT_MOD;
	bonus.stat = Stat::POWER;
	bonus.value = classDef->passive.bonus;
	bonus.remainingTurns = 0;
	bonus.casterId = caster.id;
	bonus.spellId = PASSIVE_SPELL_ID;
	bonus.name = classDef->passive.name;
	bonus.positive = true;
	addActiveEffect(caster, bonus, false);
}
