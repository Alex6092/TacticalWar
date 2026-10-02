#include "BotBrain.h"
#include "BattleRules.h"

#include <algorithm>

namespace tw
{
	namespace battle
	{
		namespace
		{
			bool isOffensive(const SpellDef & spell)
			{
				for (const EffectDef & effect : spell.effects)
				{
					if (effect.type == EffectType::DAMAGE || effect.type == EffectType::LIFESTEAL || effect.type == EffectType::DOT)
						return true;
				}
				return false;
			}

			// Portée d'attaque la plus longue du combattant (sorts offensifs).
			int attackRange(const BattleState & state, const GameData & data, const Fighter & fighter)
			{
				const ClassDef * classDef = data.findClass(fighter.classId);
				int range = 1;
				if (classDef == nullptr)
					return range;
				for (const SpellDef & spell : classDef->spells)
				{
					if (isOffensive(spell))
						range = std::max(range, effectiveMaxRange(state, data, fighter, spell));
				}
				return range;
			}

			int nearestEnemyDistance(const BattleState & state, int team, const Cell & from)
			{
				int nearest = 1000;
				for (const Fighter & fighter : state.fighters)
				{
					if (fighter.alive && fighter.team != team)
						nearest = std::min(nearest, manhattan(fighter.position, from));
				}
				return nearest;
			}

			bool hasEffectFrom(const Fighter & fighter, const std::string & spellId)
			{
				for (const ActiveEffect & effect : fighter.effects)
				{
					if (effect.spellId == spellId)
						return true;
				}
				return false;
			}

			int countEffects(const Fighter & fighter, bool positive)
			{
				int count = 0;
				for (const ActiveEffect & effect : fighter.effects)
				{
					if (effect.positive == positive && effect.spellId != "__passive")
						count++;
				}
				return count;
			}

			bool affects(const EffectDef & effect, const Fighter & caster, const Fighter & target)
			{
				switch (effect.targets)
				{
				case TargetFilter::ENEMIES: return target.team != caster.team;
				case TargetFilter::ALLIES: return target.team == caster.team;
				case TargetFilter::CASTER: return target.id == caster.id;
				default: return true;
				}
			}

			// Valeur estimée d'un effet sur une cible (positive si l'effet sert l'équipe du lanceur).
			int effectValue(const BattleState & state, const GameData & data, const Fighter & caster, const std::string & spellId, const EffectDef & effect, const Fighter & target)
			{
				bool enemy = target.team != caster.team;
				int average = (effect.min + std::max(effect.min, effect.max)) / 2;
				int threat = nearestEnemyDistance(state, target.team, target.position);

				switch (effect.type)
				{
				case EffectType::DAMAGE:
				case EffectType::LIFESTEAL:
					// Bonus pour achever une cible affaiblie ; un allié touché coûte plus cher.
					return enemy ? average + (target.hp <= average ? 15 : 0) : -average * 3 / 2;
				case EffectType::DOT:
					if (hasEffectFrom(target, spellId))
						return enemy ? effect.min : -effect.min;
					return (enemy ? 1 : -1) * effect.min * std::max(1, effect.duration) * 2 / 3;
				case EffectType::HEAL:
				case EffectType::HOT:
					return enemy ? -average : std::min(target.maxHp - target.hp, average * std::max(1, effect.duration));
				case EffectType::SHIELD:
					return enemy ? 0 : (threat <= 4 ? effect.min * 2 / 3 : effect.min / 5);
				case EffectType::STAT_MOD:
				{
					bool buff = effect.min > 0;
					if (enemy == buff)
						return -4;
					if (hasEffectFrom(target, spellId))
						return 1;
					return (enemy || threat <= 3) ? 9 : 2;
				}
				case EffectType::PUSH:
					return enemy ? (attackRange(state, data, caster) >= 4 ? 7 : 3) : -3;
				case EffectType::PULL:
					// Attirer un tireur au contact d'un combattant de mêlée.
					return enemy ? (attackRange(state, data, caster) < 4 && attackRange(state, data, target) >= 4 ? 16 : 4) : -3;
				case EffectType::DASH:
					return attackRange(state, data, caster) < 4 ? 6 : 1;
				case EffectType::TELEPORT:
					// Se dégager quand on est au contact (pour un tireur).
					return attackRange(state, data, caster) >= 4 && nearestEnemyDistance(state, caster.team, caster.position) <= 1 ? 12 : 0;
				case EffectType::DISPEL:
				{
					// Effets positifs des ennemis, négatifs des alliés.
					bool removesPositive = effect.dispel != DispelMode::NEGATIVE;
					bool removesNegative = effect.dispel != DispelMode::POSITIVE;
					int useful = 0;
					if (enemy && removesPositive)
						useful += countEffects(target, true);
					if (!enemy && removesNegative)
						useful += countEffects(target, false);
					return 6 * useful;
				}
				case EffectType::STATE:
					return enemy ? 3 : 0;
				default:
					return 0;
				}
			}

			// Lance le sort le plus utile, d'après la valeur estimée de ses effets sur les combattants touchés.
			bool chooseCast(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me, std::mt19937 & rng, BotAction & action)
			{
				const ClassDef * classDef = data.findClass(me.classId);
				if (classDef == nullptr)
					return false;

				int bestScore = 0;
				for (int slot = 0; slot < (int)classDef->spells.size(); slot++)
				{
					const SpellDef & spell = classDef->spells[slot];
					if (!checkSpellResources(me, spell).empty())
						continue;

					for (const Cell & cell : castableCells(state, map, data, me, spell))
					{
						std::vector<Cell> zone = impactCells(map, me.position, cell, spell.impact);
						int value = 0;
						for (const EffectDef & effect : spell.effects)
						{
							if (effect.type == EffectType::GLYPH)
							{
								// Glyphe : les ennemis déjà dans la zone le subiront au début de leur tour.
								ZoneDef glyphZone;
								glyphZone.shape = effect.glyphShape;
								glyphZone.size = effect.glyphSize;
								for (const Cell & hit : impactCells(map, me.position, cell, glyphZone))
								{
									const Fighter * fighter = state.fighterAt(hit);
									if (fighter == nullptr || !fighter->alive || fighter->team == me.team)
										continue;
									for (const EffectDef & triggered : effect.glyphEffects)
										value += effectValue(state, data, me, spell.id, triggered, *fighter) * 3 / 4;
								}
								continue;
							}

							if (effect.targets == TargetFilter::CASTER || effect.type == EffectType::DASH || effect.type == EffectType::TELEPORT)
							{
								value += effectValue(state, data, me, spell.id, effect, me);
								continue;
							}

							for (const Cell & hit : zone)
							{
								const Fighter * fighter = state.fighterAt(hit);
								if (fighter != nullptr && fighter->alive && affects(effect, me, *fighter))
									value += effectValue(state, data, me, spell.id, effect, *fighter);
							}
						}

						// Un peu de hasard pour varier les combats.
						int score = value * 4 + (int)(rng() % 4);
						if (value >= 2 && score > bestScore)
						{
							bestScore = score;
							action.kind = BotAction::Kind::CAST;
							action.slot = slot;
							action.target = cell;
						}
					}
				}
				return action.kind == BotAction::Kind::CAST;
			}

			// Un sort offensif pourrait-il toucher un ennemi depuis cette case (sans compter les PA) ?
			bool canHitFrom(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me, const Cell & from)
			{
				const ClassDef * classDef = data.findClass(me.classId);
				if (classDef == nullptr)
					return false;

				Fighter moved = me;
				moved.position = from;
				for (const SpellDef & spell : classDef->spells)
				{
					if (!isOffensive(spell))
						continue;
					for (const Cell & cell : castableCells(state, map, data, moved, spell))
					{
						const Fighter * target = state.fighterAt(cell);
						if (target != nullptr && target->alive && target->team != me.team)
							return true;
					}
				}
				return false;
			}

			// Déplacement : vers une case d'où un ennemi est à portée. Les combattants à distance y
			// gardent leurs distances (et évitent le contact, qui les expose au tacle) ; ceux de
			// mêlée vont au contact. Si aucun ennemi n'est atteignable ce tour-ci, tous s'approchent.
			bool chooseMove(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me, BotAction & action)
			{
				if (me.mp <= 0)
					return false;

				std::vector<Cell> candidates = reachableCells(state, map, me);
				candidates.push_back(me.position);

				int range = attackRange(state, data, me);
				bool ranged = range >= 4;
				std::vector<bool> hits;
				bool anyHit = false;
				for (const Cell & cell : candidates)
				{
					hits.push_back(canHitFrom(state, map, data, me, cell));
					anyHit = anyHit || hits.back();
				}

				Cell best = me.position;
				int bestScore = -1000000;
				for (std::size_t i = 0; i < candidates.size(); i++)
				{
					const Cell & cell = candidates[i];
					int distance = nearestEnemyDistance(state, me.team, cell);
					int score = -manhattan(me.position, cell);
					if (!anyHit)
						score -= distance * 4;
					else if (ranged)
						score += (hits[i] ? 100 : 0) + std::min(distance, range) * 4 - (distance <= 1 ? 60 : 0);
					else
						score += (hits[i] ? 100 : 0) - distance * 4;

					if (score > bestScore)
					{
						bestScore = score;
						best = cell;
					}
				}
				if (best == me.position)
					return false;

				action.kind = BotAction::Kind::MOVE;
				action.path = findPath(state, map, me, best);
				return !action.path.empty();
			}
		}

		BotAction chooseBotAction(const BattleState & state, const BattleMap & map, const GameData & data, int fighterId, std::mt19937 & rng)
		{
			BotAction action;
			const Fighter * me = state.findFighter(fighterId);
			if (me == nullptr || !me->alive)
				return action;

			if (chooseCast(state, map, data, *me, rng, action))
				return action;

			action = BotAction();
			if (chooseMove(state, map, data, *me, action))
				return action;

			return BotAction();
		}
	}
}
