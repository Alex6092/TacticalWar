#include "BotBrain.h"
#include "BattleEngine.h"
#include "BattleRules.h"

#include <algorithm>
#include <climits>
#include <cmath>

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
				int range = 1;
				for (const SpellDef * spell : fighterSpells(data, fighter))
				{
					if (isOffensive(*spell))
						range = std::max(range, effectiveMaxRange(state, data, fighter, *spell));
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

			// Un coéquipier vivant du lanceur a un sort qui profite de cet état (combinaison).
			bool teammateExploits(const BattleState & state, const GameData & data, const Fighter & caster, const std::string & stateName)
			{
				for (const Fighter & ally : state.fighters)
				{
					if (!ally.alive || ally.team != caster.team || ally.id == caster.id)
						continue;
					for (const SpellDef * spell : fighterSpells(data, ally))
					{
						for (const EffectDef & effect : spell->effects)
						{
							if (effect.comboState == stateName)
								return true;
						}
					}
				}
				return false;
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
					// Combinaison : dégâts augmentés, et un petit bonus pour la rechercher.
					if (enemy && !effect.comboState.empty() && target.hasState(effect.comboState))
						average = average * (100 + effect.comboPercent) / 100 + 3;
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
					// Un état négatif sert contre un ennemi, un état positif sur un allié ; un état négatif
					// vaut plus si un coéquipier peut en profiter (combinaison).
					if (enemy != effect.negative)
						return -3;
					return effect.negative && teammateExploits(state, data, caster, effect.state) ? 7 : 3;
				default:
					return 0;
				}
			}

			// Nombre de pas pour arriver au contact de "target" (0 : déjà au contact, 99 : impossible).
			int stepsToReach(const BattleState & state, const BattleMap & map, const Fighter & fighter, const Cell & target)
			{
				if (manhattan(fighter.position, target) <= 1)
					return 0;
				int best = 99;
				const Cell around[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
				for (const Cell & offset : around)
				{
					std::vector<Cell> path = findPath(state, map, fighter, { target.x + offset.x, target.y + offset.y });
					if (!path.empty())
						best = std::min(best, (int)path.size());
				}
				return best;
			}

			// Valeur d'un mur posé sur "target" : menaces ennemies du prochain tour qu'il coupe (ligne de
			// vue d'un tireur vers un allié, chemin d'un combattant de mêlée vers un allié).
			int wallValue(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me, const SpellDef & spell,
				const EffectDef & effect, const Cell & target)
			{
				std::vector<Cell> cells = wallCells(state, map, me.position, target, spell.impact);
				if (cells.empty())
					return 0;
				BattleState after = state;
				for (const Cell & cell : cells)
				{
					Block block;
					block.cell = cell;
					block.blocksMove = effect.wallBlocksMove;
					block.blocksSight = effect.wallBlocksSight;
					after.blocks.push_back(block);
				}

				int value = 0;
				for (const Fighter & enemy : state.fighters)
				{
					if (!enemy.alive || enemy.team == me.team)
						continue;
					int range = attackRange(state, data, enemy);
					int mp = std::max(0, effectiveStat(state, data, enemy, Stat::MP));
					for (const Fighter & ally : state.fighters)
					{
						if (!ally.alive || ally.team != me.team)
							continue;
						int distance = manhattan(enemy.position, ally.position);
						if (range >= 4)
						{
							if (effect.wallBlocksSight && distance <= range + mp && hasLineOfSight(state, map, enemy.position, ally.position)
								&& !hasLineOfSight(after, map, enemy.position, ally.position))
								value += 6;
						}
						else if (effect.wallBlocksMove && distance <= mp + 3)
						{
							int before = stepsToReach(state, map, enemy, ally.position);
							if (before <= mp && stepsToReach(after, map, enemy, ally.position) >= before + 2)
								value += 6;
						}
					}
				}
				return value;
			}

			// Valeur de dégâts sur un bloc de mur : utile s'il barre le chemin (ou la vue d'un tireur) vers
			// l'ennemi le plus proche ; abîmer un mur de son équipe coûte un peu.
			int blockValue(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me, const Block & block, int damage)
			{
				if (block.team == me.team)
					return -4;
				BattleState without = state;
				without.blocks.erase(std::remove_if(without.blocks.begin(), without.blocks.end(),
					[&](const Block & other) { return other.uid == block.uid; }), without.blocks.end());
				bool inTheWay = false;
				int range = attackRange(state, data, me);
				for (const Fighter & enemy : state.fighters)
				{
					if (!enemy.alive || enemy.team == me.team)
						continue;
					if (range >= 4)
					{
						inTheWay = inTheWay || (block.blocksSight && manhattan(me.position, enemy.position) <= range
							&& !hasLineOfSight(state, map, me.position, enemy.position) && hasLineOfSight(without, map, me.position, enemy.position));
					}
					else if (block.blocksMove)
					{
						int before = stepsToReach(state, map, me, enemy.position);
						inTheWay = inTheWay || stepsToReach(without, map, me, enemy.position) + 2 <= before;
					}
				}
				if (!inTheWay)
					return 0;
				return damage >= block.hp ? 8 : 3;
			}

			// Lance le sort le plus utile, d'après la valeur estimée de ses effets sur les combattants touchés.
			bool chooseCast(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me, std::mt19937 & rng,
				bool mistake, BotAction & action)
			{
				std::vector<BotAction> useful;
				int bestScore = 0;
				for (int slot = 0; slot < SPELL_SLOTS; slot++)
				{
					const SpellDef * slotSpell = spellOf(data, me, slot);
					if (slotSpell == nullptr)
						continue;
					const SpellDef & spell = *slotSpell;
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
								// Piège sur une case libre : un ennemi tout proche risque d'y passer.
								if (effect.glyphShape == ZoneShape::SINGLE && state.fighterAt(cell) == nullptr && nearestEnemyDistance(state, me.team, cell) == 1)
								{
									for (const Fighter & enemy : state.fighters)
									{
										if (!enemy.alive || enemy.team == me.team || manhattan(enemy.position, cell) != 1)
											continue;
										for (const EffectDef & triggered : effect.glyphEffects)
											value += effectValue(state, data, me, spell.id, triggered, enemy) / 3;
										break;
									}
								}
								continue;
							}

							if (effect.type == EffectType::WALL)
							{
								value += wallValue(state, map, data, me, spell, effect, cell);
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
								const Block * block = state.blockAt(hit);
								if (block != nullptr && (effect.type == EffectType::DAMAGE || effect.type == EffectType::LIFESTEAL))
									value += blockValue(state, map, data, me, *block, (effect.min + std::max(effect.min, effect.max)) / 2);
							}
						}

						// Un peu de hasard pour varier les combats.
						int score = value * 4 + (int)(rng() % 4);
						if (value >= 2)
						{
							BotAction candidate;
							candidate.kind = BotAction::Kind::CAST;
							candidate.slot = slot;
							candidate.target = cell;
							useful.push_back(candidate);
						}
						if (value >= 2 && score > bestScore)
						{
							bestScore = score;
							action.kind = BotAction::Kind::CAST;
							action.slot = slot;
							action.target = cell;
						}
					}
				}
				// Erreur volontaire (difficulté « Facile ») : un sort utile au hasard.
				if (mistake && !useful.empty())
					action = useful[rng() % useful.size()];
				return action.kind == BotAction::Kind::CAST;
			}

			// Un sort offensif pourrait-il toucher un ennemi depuis cette case (sans compter les PA) ?
			bool canHitFrom(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me, const Cell & from)
			{
				Fighter moved = me;
				moved.position = from;
				for (const SpellDef * offensive : fighterSpells(data, me))
				{
					const SpellDef & spell = *offensive;
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
			// Ennemis qui pourront atteindre la case au prochain tour (portée d'attaque + PM, sans la ligne de
			// vue) ; meleeOnly : seulement les combattants de mêlée.
			int threatsOn(const BattleState & state, const GameData & data, const Fighter & me, const Cell & cell, bool meleeOnly = false)
			{
				int threats = 0;
				for (const Fighter & enemy : state.fighters)
				{
					if (!enemy.alive || enemy.team == me.team)
						continue;
					int range = attackRange(state, data, enemy);
					if (meleeOnly && range >= 4)
						continue;
					int reach = range + std::max(0, effectiveStat(state, data, enemy, Stat::MP));
					if (manhattan(enemy.position, cell) <= reach)
						threats++;
				}
				return threats;
			}

			bool chooseMove(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me, std::mt19937 & rng,
				bool mistake, BotAction & action, bool careful = false)
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

				// Zone à tenir : y entrer ou y rester (surtout pour la disputer), sinon s'en rapprocher.
				bool zone = state.zone.enabled && !state.zone.cells.empty();
				bool present[3] = { false, false, false };
				if (zone)
					zonePresence(state, present);
				int holdBonus = present[3 - me.team] ? 90 : 60;
				bool allyHolds = false;
				for (const Fighter & ally : state.fighters)
					allyHolds = allyHolds || (zone && ally.alive && ally.team == me.team && ally.id != me.id && state.zone.contains(ally.position));

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

					if (zone)
					{
						int toZone = 1000;
						for (const Cell & zoneCell : state.zone.cells)
							toZone = std::min(toZone, manhattan(zoneCell, cell));
						if (ranged)
						{
							// Un tireur laisse la zone à un coéquipier qui la tient, et ne s'y expose pas au contact.
							score += toZone == 0 ? (allyHolds ? 10 : holdBonus * 2 / 3) : -toZone * 3;
							if (distance <= 1)
								score -= 60;
						}
						else
						{
							score += toZone == 0 ? holdBonus : -toZone * 5;
						}
					}

					// Cases à effet : éviter les braises, rejoindre une source quand on est blessé.
					score -= map.turnDamage(cell) * 8;
					score += std::min(map.turnHeal(cell), std::max(0, me.maxHp - me.hp)) * 5;

					// Prudence (difficulté « Difficile ») : un combattant blessé finit hors d'atteinte.
					if (careful && me.hp * 2 < me.maxHp)
						score -= threatsOn(state, data, me, cell) * 30;

					// Orbe bonus : soin quand on est blessé, énergie, protection quand un ennemi est proche.
					const Orb * orb = state.orbAt(cell);
					const OrbDef * bonus = orb != nullptr ? data.findOrb(orb->kind) : nullptr;
					if (bonus != nullptr)
					{
						score += std::min(bonus->heal, std::max(0, me.maxHp - me.hp)) * 4;
						score += bonus->ap * 30;
						score += bonus->shield > 0 ? (distance <= 4 ? bonus->shield * 3 : bonus->shield) : 0;
					}

					if (score > bestScore)
					{
						bestScore = score;
						best = cell;
					}
				}
				// Erreur volontaire (difficulté « Facile ») : une case au hasard.
				if (mistake)
					best = candidates[rng() % candidates.size()];
				if (best == me.position)
					return false;

				action.kind = BotAction::Kind::MOVE;
				action.path = findPath(state, map, me, best);
				return !action.path.empty();
			}

			//------------------------------------------------------------------
			// Difficulté « Difficile » : actions essayées sur une copie du combat
			//------------------------------------------------------------------

			// Effets nouveaux d'un combattant (absents avant), positifs ou négatifs.
			int newEffects(const Fighter & before, const Fighter & after, bool positive)
			{
				int count = 0;
				for (const ActiveEffect & effect : after.effects)
				{
					if (effect.positive != positive || effect.spellId == "__passive")
						continue;
					bool existed = false;
					for (const ActiveEffect & old : before.effects)
						existed = existed || old.uid == effect.uid;
					count += existed ? 0 : 1;
				}
				return count;
			}

			// Un coéquipier qui profite de la marque joue avant le prochain tour de la cible.
			bool teammatePlaysBefore(const BattleState & state, const GameData & data, const Fighter & me, int targetId, const std::string & mark)
			{
				int count = (int)state.turnOrder.size();
				int start = -1;
				for (int i = 0; i < count; i++)
				{
					if (state.turnOrder[i] == me.id)
						start = i;
				}
				for (int step = 1; start >= 0 && step < count; step++)
				{
					const Fighter * next = state.findFighter(state.turnOrder[(start + step) % count]);
					if (next == nullptr || !next->alive)
						continue;
					if (next->id == targetId)
						return false;
					if (next->team != me.team)
						continue;
					for (const SpellDef * spell : fighterSpells(data, *next))
					{
						for (const EffectDef & effect : spell->effects)
						{
							if (effect.comboState == mark)
								return true;
						}
					}
				}
				return false;
			}

			// Valeur d'un état simulé par rapport à l'état de départ, pour l'équipe de "me" : dégâts et KO
			// sur les ennemis (jusqu'à trois fois plus sur un ennemi presque à terre : on concentre les coups), dégâts subis par l'équipe, soins, boucliers,
			// effets posés ; marques de combinaison qu'un coéquipier exploitera avant le tour de la cible.
			int outcomeValue(const BattleState & before, const BattleState & after, const GameData & data, const Fighter & me)
			{
				int value = 0;
				for (const Fighter & old : before.fighters)
				{
					const Fighter * now = after.findFighter(old.id);
					if (now == nullptr || !old.alive)
						continue;
					int hpChange = (now->alive ? now->hp : 0) - old.hp;
					int shieldChange = (now->alive ? now->shield : 0) - old.shield;
					if (old.team != me.team)
					{
						int lost = std::max(0, -hpChange) + std::max(0, -shieldChange);
						double wounded = 1.0 + 2.0 * (1.0 - (double)old.hp / std::max(1, old.maxHp));
						value += (int)std::lround(lost * wounded);
						if (!now->alive)
							value += 60;
						value += 4 * newEffects(old, *now, false) - 4 * newEffects(old, *now, true);
						for (const ActiveEffect & effect : now->effects)
						{
							bool fresh = std::none_of(old.effects.begin(), old.effects.end(), [&](const ActiveEffect & e) { return e.uid == effect.uid; });
							if (fresh && effect.type == EffectType::STATE && !effect.positive && teammatePlaysBefore(after, data, me, old.id, effect.state))
								value += 12;
						}
					}
					else
					{
						value += hpChange < 0 ? hpChange * 3 / 2 : hpChange;
						value += shieldChange / 2;
						if (!now->alive)
							value -= 70;
						value += 4 * newEffects(old, *now, true) - 4 * newEffects(old, *now, false);
						if (now->id == me.id)
							value += (now->ap - old.ap) > 0 ? (now->ap - old.ap) * 8 : 0;	// Orbe d'énergie
					}
				}
				return value;
			}

			// Effets dont le résultat n'apparaît pas tout de suite : glyphes et murs (estimés).
			int deferredValue(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me, const SpellDef & spell, const Cell & cell)
			{
				int value = 0;
				for (const EffectDef & effect : spell.effects)
				{
					if (effect.type == EffectType::WALL)
						value += wallValue(state, map, data, me, spell, effect, cell);
					if (effect.type != EffectType::GLYPH)
						continue;
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
				}
				return value;
			}

			// Blocs de mur touchés : utiles s'ils barrent le chemin ou la vue.
			int blocksValue(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me, const SpellDef & spell, const Cell & cell)
			{
				if (!damagesBlocks(spell))
					return 0;
				int damage = 0;
				for (const EffectDef & effect : spell.effects)
				{
					if (effect.type == EffectType::DAMAGE || effect.type == EffectType::LIFESTEAL)
						damage += (effect.min + std::max(effect.min, effect.max)) / 2;
				}
				int value = 0;
				for (const Cell & hit : impactCells(map, me.position, cell, spell.impact))
				{
					const Block * block = state.blockAt(hit);
					if (block != nullptr)
						value += blockValue(state, map, data, me, *block, damage);
				}
				return value;
			}

			// Lancer simulé (jets les plus faibles et les plus forts) : valeur moyenne, INT_MIN si impossible.
			// "after" reçoit l'état après le lancer aux jets les plus faibles (suite prudente).
			int simulateCast(const BattleState & state, const BattleMap & map, const GameData & data, int fighterId, int slot, const Cell & cell,
				BattleState * after = nullptr)
			{
				BattleState copy = state;
				Fighter * caster = copy.findFighter(fighterId);
				if (caster == nullptr)
					return INT_MIN;
				// Un lancer qui épuise PA et PM terminerait le tour (et ferait jouer le suivant).
				caster->mp = std::max(caster->mp, 1);
				int total = 0;
				for (BattleEngine::RollMode mode : { BattleEngine::RollMode::MIN, BattleEngine::RollMode::MAX })
				{
					BattleEngine engine(data, map, copy, 1);
					engine.setRollMode(mode);
					if (!engine.cast(fighterId, slot, cell, 0).ok)
						return INT_MIN;
					total += outcomeValue(state, engine.getState(), data, *caster);
					if (after != nullptr && mode == BattleEngine::RollMode::MIN)
						*after = engine.getState();
				}
				return total / 2;
			}

			// Cases utiles pour un sort : toutes celles où il peut être lancé, sauf pour un sort de zone
			// sans contrainte de cible (seulement près d'un combattant ou d'un bloc).
			bool worthTrying(const BattleState & state, const SpellDef & spell, const Cell & cell)
			{
				if (spell.requirement != CellRequirement::ANY)
					return true;
				int reach = std::max(1, spell.impact.size) + 1;
				for (const Fighter & fighter : state.fighters)
				{
					if (fighter.alive && manhattan(fighter.position, cell) <= reach)
						return true;
				}
				for (const Block & block : state.blocks)
				{
					if (manhattan(block.cell, cell) <= reach)
						return true;
				}
				return false;
			}

			struct Candidate
			{
				int value = INT_MIN;
				BotAction action;
			};

			// Interruption demandée par le fil qui attend la décision (BotOptions::cancel), pour le calcul
			// en cours dans ce fil.
			thread_local const std::atomic<bool> * cancelFlag = nullptr;

			bool cancelled()
			{
				return cancelFlag != nullptr && cancelFlag->load(std::memory_order_relaxed);
			}

			// Meilleur sort à lancer maintenant. Avec "lookahead", les lancers les plus prometteurs sont
			// aussi jugés sur le meilleur sort qui pourra les suivre (Provocation puis Taillade, bond puis
			// coup…).
			Candidate bestCast(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me, bool lookahead = false)
			{
				struct Option
				{
					int value;
					BotAction action;
					BattleState after;
				};
				std::vector<Option> options;
				for (int slot = 0; slot < SPELL_SLOTS; slot++)
				{
					const SpellDef * spell = spellOf(data, me, slot);
					if (spell == nullptr || !checkSpellResources(me, *spell).empty())
						continue;
					for (const Cell & cell : castableCells(state, map, data, me, *spell))
					{
						if (cancelled())
							return Candidate();
						if (!worthTrying(state, *spell, cell))
							continue;
						Option option;
						option.value = simulateCast(state, map, data, me.id, slot, cell, lookahead ? &option.after : nullptr);
						if (option.value == INT_MIN)
							continue;
						option.value += deferredValue(state, map, data, me, *spell, cell) + blocksValue(state, map, data, me, *spell, cell);
						option.action.kind = BotAction::Kind::CAST;
						option.action.slot = slot;
						option.action.target = cell;
						options.push_back(option);
					}
				}

				std::stable_sort(options.begin(), options.end(), [](const Option & a, const Option & b) { return a.value > b.value; });
				Candidate best;
				for (std::size_t i = 0; i < options.size(); i++)
				{
					int value = options[i].value;
					// Suite : les 8 premiers lancers seulement (temps de calcul).
					if (lookahead && i < 8 && !cancelled())
					{
						const Fighter * after = options[i].after.findFighter(me.id);
						if (after != nullptr && after->alive && options[i].after.activeFighterId() == me.id)
						{
							Candidate next = bestCast(options[i].after, map, data, *after, false);
							if (next.value > 0)
								value += next.value * 4 / 5;
						}
					}
					if (value > best.value)
					{
						best.value = value;
						best.action = options[i].action;
					}
				}
				return best;
			}

			// Meilleur « se déplacer, puis lancer un sort » parmi les cases les plus prometteuses.
			Candidate bestMoveThenCast(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me)
			{
				Candidate best;
				if (me.mp <= 0)
					return best;

				int range = attackRange(state, data, me);
				std::vector<std::pair<int, Cell>> ranked;
				for (const Cell & cell : reachableCells(state, map, me))
				{
					// Près d'un ennemi à sa portée d'attaque (ou sur un orbe), sans trop marcher.
					int distance = nearestEnemyDistance(state, me.team, cell);
					int score = -std::abs(distance - std::min(range, 4)) * 3 - manhattan(me.position, cell) + (state.orbAt(cell) != nullptr ? 20 : 0);
					ranked.push_back({ score, cell });
				}
				std::stable_sort(ranked.begin(), ranked.end(), [](const std::pair<int, Cell> & a, const std::pair<int, Cell> & b) { return a.first > b.first; });
				if (ranked.size() > 18)
					ranked.resize(18);

				struct Option
				{
					int base;		// Déplacement (orbes, prudence) sans les sorts
					int value;		// Avec le meilleur sort lancé ensuite
					std::vector<Cell> path;
					BattleState moved;
				};
				std::vector<Option> options;
				for (const auto & entry : ranked)
				{
					if (cancelled())
						return best;
					std::vector<Cell> path = findPath(state, map, me, entry.second);
					if (path.empty())
						continue;
					BattleEngine engine(data, map, state, 1);
					if (!engine.move(me.id, path, 0).ok)
						continue;
					const BattleState & moved = engine.getState();
					const Fighter * after = moved.findFighter(me.id);
					if (after == nullptr || !after->alive || moved.activeFighterId() != me.id)
						continue;
					Option option;
					option.base = outcomeValue(state, moved, data, me) - (int)path.size();
					// Prudence : la case d'arrivée exposée coûte, pour un combattant blessé.
					if (me.hp * 2 < me.maxHp)
						option.base -= threatsOn(moved, data, *after, after->position) * 8;
					Candidate cast = bestCast(moved, map, data, *after);
					option.value = option.base + std::max(0, cast.value);
					option.path = path;
					option.moved = moved;
					options.push_back(option);
				}

				// Les 4 meilleures cases : jugées aussi sur le sort suivant (comme un lancer immédiat).
				std::stable_sort(options.begin(), options.end(), [](const Option & a, const Option & b) { return a.value > b.value; });
				for (std::size_t i = 0; i < options.size(); i++)
				{
					int value = options[i].value;
					if (i < 4 && !cancelled())
					{
						const Fighter * after = options[i].moved.findFighter(me.id);
						Candidate cast = bestCast(options[i].moved, map, data, *after, true);
						value = options[i].base + std::max(0, cast.value);
					}
					if (value > best.value)
					{
						best.value = value;
						best.action.kind = BotAction::Kind::MOVE;
						best.action.path = options[i].path;
					}
				}
				return best;
			}

			BotAction choosePlannedAction(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & me, std::mt19937 & rng)
			{
				Candidate cast = bestCast(state, map, data, me, true);
				Candidate move = bestMoveThenCast(state, map, data, me);
				if (cast.value >= 4 && cast.value >= move.value - 3)
					return cast.action;
				if (move.value >= 6 && move.value > cast.value)
					return move.action;
				if (cast.value >= 2)
					return cast.action;

				// Rien d'utile à lancer : se placer, prudemment.
				BotAction action;
				if (chooseMove(state, map, data, me, rng, false, action, true))
					return action;
				return BotAction();
			}
		}

		BotAction chooseBotAction(const BattleState & state, const BattleMap & map, const GameData & data, int fighterId, std::mt19937 & rng,
			const BotOptions & options)
		{
			BotAction action;
			const Fighter * me = state.findFighter(fighterId);
			if (me == nullptr || !me->alive)
				return action;

			if (options.planner)
			{
				cancelFlag = options.cancel;
				action = choosePlannedAction(state, map, data, *me, rng);
				cancelFlag = nullptr;
				return action;
			}

			// Pas de tirage sans erreurs prévues : le bot réseau et la simulation restent identiques.
			bool mistake = options.mistakePercent > 0 && (int)(rng() % 100) < options.mistakePercent;
			if (chooseCast(state, map, data, *me, rng, mistake, action))
				return action;

			action = BotAction();
			if (chooseMove(state, map, data, *me, rng, mistake, action))
				return action;

			return BotAction();
		}
	}
}
