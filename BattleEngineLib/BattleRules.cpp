#include "BattleRules.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <map>

using namespace tw::battle;

namespace
{
	int absValue(int value)
	{
		return value < 0 ? -value : value;
	}

	int sign(int value)
	{
		return (value > 0) - (value < 0);
	}

	bool isOccupied(const BattleState & state, const Cell & cell, int ignoredFighterId)
	{
		const Fighter * fighter = state.fighterAt(cell);
		return fighter != nullptr && fighter->id != ignoredFighterId;
	}

	const Cell NEIGHBOURS[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
}

int tw::battle::effectiveStat(const BattleState & state, const GameData & data, const Fighter & fighter, Stat stat)
{
	int value = fighter.baseStats.get(stat);

	for (const ActiveEffect & effect : fighter.effects)
	{
		if (effect.type == EffectType::STAT_MOD && effect.stat == stat)
			value += effect.value;
	}

	// Auras des alliés (ex : Protecteur).
	for (const Fighter & ally : state.fighters)
	{
		if (!ally.alive || ally.id == fighter.id || ally.team != fighter.team)
			continue;

		const ClassDef * classDef = data.findClass(ally.classId);
		if (classDef != nullptr && classDef->passive.type == PassiveType::ALLY_AURA && classDef->passive.stat == stat
			&& manhattan(ally.position, fighter.position) <= classDef->passive.distance)
		{
			value += classDef->passive.bonus;
		}
	}

	return value;
}

int tw::battle::periodicDamage(const BattleState & state, const GameData & data, const Fighter & bearer, int value, int casterPower)
{
	int maxResistance = data.rules.maxResistance;
	int resistance = std::max(-maxResistance, std::min(maxResistance, effectiveStat(state, data, bearer, Stat::RESISTANCE)));
	int damage = (int)std::lround(value * (100.0 + casterPower) / 100.0 * (100.0 - resistance) / 100.0);
	return std::max(0, damage);
}

bool tw::battle::hasLineOfSight(const BattleState & state, const BattleMap & map, const Cell & from, const Cell & to)
{
	if (from == to)
		return true;

	auto blocking = [&](const Cell & cell) {
		if (cell == from || cell == to)
			return false;
		return map.blocksSight(cell) || state.fighterAt(cell) != nullptr;
	};

	// Parcours des cellules traversées par le segment reliant les centres (Amanatides & Woo).
	int dx = to.x - from.x;
	int dy = to.y - from.y;
	int stepX = sign(dx);
	int stepY = sign(dy);
	const double infinity = std::numeric_limits<double>::infinity();
	double deltaX = dx != 0 ? 1.0 / absValue(dx) : infinity;
	double deltaY = dy != 0 ? 1.0 / absValue(dy) : infinity;
	double maxX = dx != 0 ? 0.5 / absValue(dx) : infinity;
	double maxY = dy != 0 ? 0.5 / absValue(dy) : infinity;

	Cell current = from;
	for (int guard = 0; guard < 4 * (absValue(dx) + absValue(dy)) + 4; guard++)
	{
		if (std::fabs(maxX - maxY) < 1e-9)
		{
			// Le segment passe exactement par un coin : il ne frôle les deux cellules voisines
			// qu'en un point. La vue n'est bloquée que si les deux le sont.
			Cell sideA = { current.x + stepX, current.y };
			Cell sideB = { current.x, current.y + stepY };
			if (blocking(sideA) && blocking(sideB))
				return false;

			current.x += stepX;
			current.y += stepY;
			maxX += deltaX;
			maxY += deltaY;
		}
		else if (maxX < maxY)
		{
			current.x += stepX;
			maxX += deltaX;
		}
		else
		{
			current.y += stepY;
			maxY += deltaY;
		}

		if (current == to)
			return true;
		if (blocking(current))
			return false;
	}

	return true;
}

const SpellDef * tw::battle::spellOf(const GameData & data, const Fighter & fighter, int spellIndex)
{
	const ClassDef * classDef = data.findClass(fighter.classId);
	if (classDef == nullptr || spellIndex < 0 || spellIndex >= (int)classDef->spells.size())
		return nullptr;
	return &classDef->spells[spellIndex];
}

int tw::battle::effectiveMaxRange(const BattleState & state, const GameData & data, const Fighter & fighter, const SpellDef & spell)
{
	int maxRange = spell.maxRange;
	if (spell.rangeModifiable)
		maxRange += effectiveStat(state, data, fighter, Stat::RANGE);
	return maxRange < spell.minRange ? spell.minRange : maxRange;
}

std::vector<Cell> tw::battle::launchCells(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & fighter, const SpellDef & spell)
{
	std::vector<Cell> cells;
	const Cell & origin = fighter.position;

	if (spell.launch == LaunchShape::SELF)
	{
		cells.push_back(origin);
		return cells;
	}

	int maxRange = effectiveMaxRange(state, data, fighter, spell);

	for (int y = 0; y < map.getHeight(); y++)
	{
		for (int x = 0; x < map.getWidth(); x++)
		{
			int dx = absValue(x - origin.x);
			int dy = absValue(y - origin.y);
			bool inLine = dx == 0 || dy == 0;
			bool inDiagonal = dx == dy;
			int distance = -1;

			switch (spell.launch)
			{
			case LaunchShape::CIRCLE:
				distance = dx + dy;
				break;
			case LaunchShape::LINE:
				distance = inLine ? dx + dy : -1;
				break;
			case LaunchShape::DIAGONAL:
				distance = inDiagonal ? dx : -1;
				break;
			case LaunchShape::STAR:
				distance = inLine ? dx + dy : (inDiagonal ? dx : -1);
				break;
			default:
				break;
			}

			if (distance >= spell.minRange && distance <= maxRange)
				cells.push_back({ x, y });
		}
	}

	return cells;
}

std::string tw::battle::checkSpellResources(const Fighter & fighter, const SpellDef & spell)
{
	if (fighter.ap < spell.apCost)
		return "Pas assez de PA.";

	auto cooldown = fighter.cooldowns.find(spell.id);
	if (cooldown != fighter.cooldowns.end() && cooldown->second > 0)
		return "Sort en cours de relance (" + std::to_string(cooldown->second) + " tour(s)).";

	if (spell.castsPerTurn > 0)
	{
		auto casts = fighter.castsThisTurn.find(spell.id);
		if (casts != fighter.castsThisTurn.end() && casts->second >= spell.castsPerTurn)
			return "Nombre de lancers par tour atteint.";
	}

	return "";
}

std::string tw::battle::checkTarget(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & fighter, const SpellDef & spell, const Cell & target)
{
	if (!map.contains(target))
		return "Cible hors de la carte.";

	bool inZone = false;
	for (const Cell & cell : launchCells(state, map, data, fighter, spell))
	{
		if (cell == target)
		{
			inZone = true;
			break;
		}
	}
	if (!inZone)
		return "Cible hors de portée.";

	if (spell.lineOfSight && !hasLineOfSight(state, map, fighter.position, target))
		return "Pas de ligne de vue.";

	const Fighter * occupant = state.fighterAt(target);
	bool isSelf = occupant != nullptr && occupant->id == fighter.id;
	bool isAlly = occupant != nullptr && occupant->team == fighter.team && !isSelf;
	bool isEnemy = occupant != nullptr && occupant->team != fighter.team;
	bool isFree = occupant == nullptr && map.isWalkable(target);

	switch (spell.requirement)
	{
	case CellRequirement::FREE_CELL:
		if (!isFree) return "La cellule doit être libre.";
		break;
	case CellRequirement::FREE_CELL_OR_ALLY:
		if (!isFree && !isAlly) return "La cellule doit être libre ou occupée par un allié.";
		break;
	case CellRequirement::CHARACTER:
		if (occupant == nullptr) return "Il faut cibler un combattant.";
		break;
	case CellRequirement::ENEMY:
		if (!isEnemy) return "Il faut cibler un ennemi.";
		break;
	case CellRequirement::ALLY:
		if (!isAlly) return "Il faut cibler un allié.";
		break;
	case CellRequirement::ALLY_OR_SELF:
		if (!isAlly && !isSelf) return "Il faut cibler un allié ou soi-même.";
		break;
	default:
		break;
	}

	if (spell.castsPerTarget > 0 && occupant != nullptr)
	{
		auto bySpell = fighter.castsOnTarget.find(spell.id);
		if (bySpell != fighter.castsOnTarget.end())
		{
			auto count = bySpell->second.find(occupant->id);
			if (count != bySpell->second.end() && count->second >= spell.castsPerTarget)
				return "Nombre de lancers sur cette cible atteint.";
		}
	}

	return "";
}

std::vector<Cell> tw::battle::castableCells(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & fighter, const SpellDef & spell)
{
	std::vector<Cell> cells;
	for (const Cell & cell : launchCells(state, map, data, fighter, spell))
	{
		if (checkTarget(state, map, data, fighter, spell, cell).empty())
			cells.push_back(cell);
	}
	return cells;
}

Cell tw::battle::directionBetween(const Cell & from, const Cell & to)
{
	int dx = to.x - from.x;
	int dy = to.y - from.y;
	if (dx == 0 && dy == 0)
		return { 0, 0 };
	if (absValue(dx) == absValue(dy))
		return { sign(dx), sign(dy) };
	if (absValue(dx) > absValue(dy))
		return { sign(dx), 0 };
	return { 0, sign(dy) };
}

std::vector<Cell> tw::battle::impactCells(const BattleMap & map, const Cell & caster, const Cell & target, const ZoneDef & zone)
{
	std::vector<Cell> cells;
	auto add = [&](int x, int y) {
		Cell cell = { x, y };
		if (map.contains(cell))
			cells.push_back(cell);
	};

	int size = zone.size;
	Cell direction = directionBetween(caster, target);
	if (direction.x == 0 && direction.y == 0)
		direction = { 1, 0 };

	switch (zone.shape)
	{
	case ZoneShape::SINGLE:
		add(target.x, target.y);
		break;
	case ZoneShape::CIRCLE:
	case ZoneShape::RING:
	case ZoneShape::SQUARE:
		for (int dy = -size; dy <= size; dy++)
		{
			for (int dx = -size; dx <= size; dx++)
			{
				int manhattanDistance = absValue(dx) + absValue(dy);
				bool inside = zone.shape == ZoneShape::SQUARE
					|| (zone.shape == ZoneShape::CIRCLE && manhattanDistance <= size)
					|| (zone.shape == ZoneShape::RING && manhattanDistance == size);
				if (inside)
					add(target.x + dx, target.y + dy);
			}
		}
		break;
	case ZoneShape::CROSS:
		add(target.x, target.y);
		for (int i = 1; i <= size; i++)
		{
			add(target.x + i, target.y);
			add(target.x - i, target.y);
			add(target.x, target.y + i);
			add(target.x, target.y - i);
		}
		break;
	case ZoneShape::LINE:
		for (int i = 0; i <= size; i++)
			add(target.x + direction.x * i, target.y + direction.y * i);
		break;
	case ZoneShape::PERPENDICULAR:
		add(target.x, target.y);
		for (int i = 1; i <= size; i++)
		{
			add(target.x - direction.y * i, target.y + direction.x * i);
			add(target.x + direction.y * i, target.y - direction.x * i);
		}
		break;
	}

	return cells;
}

std::vector<Cell> tw::battle::reachableCells(const BattleState & state, const BattleMap & map, const Fighter & fighter)
{
	std::vector<Cell> cells;
	std::map<Cell, int> distances;
	std::deque<Cell> queue;
	distances[fighter.position] = 0;
	queue.push_back(fighter.position);

	while (!queue.empty())
	{
		Cell current = queue.front();
		queue.pop_front();
		int distance = distances[current];
		if (distance >= fighter.mp)
			continue;

		for (const Cell & offset : NEIGHBOURS)
		{
			Cell next = { current.x + offset.x, current.y + offset.y };
			if (!map.isWalkable(next) || isOccupied(state, next, fighter.id) || distances.count(next) > 0)
				continue;

			distances[next] = distance + 1;
			cells.push_back(next);
			queue.push_back(next);
		}
	}

	return cells;
}

std::vector<Cell> tw::battle::findPath(const BattleState & state, const BattleMap & map, const Fighter & fighter, const Cell & target)
{
	std::map<Cell, Cell> previous;
	std::deque<Cell> queue;
	previous[fighter.position] = fighter.position;
	queue.push_back(fighter.position);

	while (!queue.empty())
	{
		Cell current = queue.front();
		queue.pop_front();
		if (current == target)
			break;

		for (const Cell & offset : NEIGHBOURS)
		{
			Cell next = { current.x + offset.x, current.y + offset.y };
			if (!map.isWalkable(next) || isOccupied(state, next, fighter.id) || previous.count(next) > 0)
				continue;

			previous[next] = current;
			queue.push_back(next);
		}
	}

	std::vector<Cell> path;
	if (previous.count(target) == 0 || target == fighter.position)
		return path;

	for (Cell cell = target; cell != fighter.position; cell = previous[cell])
		path.insert(path.begin(), cell);
	return path;
}

MovePreview tw::battle::previewMove(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & fighter, const std::vector<Cell> & path)
{
	MovePreview preview;
	preview.mpAfter = fighter.mp;
	preview.apAfter = fighter.ap;

	if (path.empty())
	{
		preview.error = "Chemin vide.";
		return preview;
	}

	Cell position = fighter.position;
	int dodge = effectiveStat(state, data, fighter, Stat::DODGE);

	for (std::size_t step = 0; step < path.size(); step++)
	{
		const Cell & next = path[step];
		if (manhattan(position, next) != 1 || !map.isWalkable(next) || isOccupied(state, next, fighter.id))
		{
			preview.error = "Chemin invalide.";
			return preview;
		}

		// Tacle : quitter une cellule au contact d'ennemis coûte une partie des PM et des PA.
		int lockSum = 0;
		bool engaged = false;
		for (const Fighter & enemy : state.fighters)
		{
			if (enemy.alive && enemy.team != fighter.team && manhattan(enemy.position, position) == 1)
			{
				engaged = true;
				lockSum += effectiveStat(state, data, enemy, Stat::LOCK);
			}
		}

		if (engaged)
		{
			double ratio = (double)(dodge + 2) / (2.0 * (lockSum + 2));
			if (ratio < 1.0)
			{
				TackleLoss loss;
				loss.stepIndex = (int)step;
				loss.lostMp = (int)std::lround(preview.mpAfter * (1.0 - ratio));
				loss.lostAp = (int)std::lround(preview.apAfter * (1.0 - ratio) * data.rules.tackleApFactor);
				if (loss.lostMp > 0 || loss.lostAp > 0)
				{
					preview.mpAfter -= loss.lostMp;
					preview.apAfter -= loss.lostAp;
					preview.tackles.push_back(loss);
				}
			}
		}

		if (preview.mpAfter < 1)
			break;

		preview.mpAfter--;
		position = next;
		preview.path.push_back(next);
	}

	return preview;
}
