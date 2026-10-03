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

int tw::battle::recordScore(const FighterRecord & record)
{
	return record.dealt + record.healed + record.shielded / 2 + 25 * record.kills;
}

int tw::battle::chooseMvp(const BattleState & state)
{
	int best = -1;
	int bestScore = 0;
	bool bestWinner = false;
	for (const Fighter & fighter : state.fighters)
	{
		int score = recordScore(fighter.record);
		bool winner = fighter.team == state.winnerTeam;
		if (score > 0 && (score > bestScore || (score == bestScore && winner && !bestWinner)))
		{
			best = fighter.id;
			bestScore = score;
			bestWinner = winner;
		}
	}
	return best;
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

namespace
{
	// Distance de marche (cases praticables, sans les combattants) depuis les cases "sources" ;
	// -1 pour une case inaccessible, 0 partout s'il n'y a aucune source.
	std::vector<int> walkDistances(const BattleMap & map, const std::vector<Cell> & sources)
	{
		int width = map.getWidth();
		std::vector<int> distances(width * map.getHeight(), sources.empty() ? 0 : -1);
		std::deque<Cell> queue;
		for (const Cell & cell : sources)
		{
			if (map.contains(cell) && map.isWalkable(cell) && distances[cell.y * width + cell.x] < 0)
			{
				distances[cell.y * width + cell.x] = 0;
				queue.push_back(cell);
			}
		}

		const Cell steps[4] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		while (!queue.empty())
		{
			Cell cell = queue.front();
			queue.pop_front();
			for (const Cell & step : steps)
			{
				Cell next = { cell.x + step.x, cell.y + step.y };
				if (!map.contains(next) || !map.isWalkable(next) || distances[next.y * width + next.x] >= 0)
					continue;
				distances[next.y * width + next.x] = distances[cell.y * width + cell.x] + 1;
				queue.push_back(next);
			}
		}
		return distances;
	}

	// Symétries possibles d'une carte, autour du point (doubledX / 2, doubledY / 2) : rotation d'un
	// demi-tour, miroirs vertical et horizontal, miroirs selon les deux diagonales.
	const int SYMMETRY_COUNT = 5;

	bool mirrorCell(int kind, int doubledX, int doubledY, const Cell & cell, Cell & twin)
	{
		switch (kind)
		{
		case 0: twin = { doubledX - cell.x, doubledY - cell.y }; return true;
		case 1: twin = { doubledX - cell.x, cell.y }; return true;
		case 2: twin = { cell.x, doubledY - cell.y }; return true;
		case 3:
			if ((doubledX - doubledY) % 2 != 0)
				return false;
			twin = { cell.y + (doubledX - doubledY) / 2, cell.x - (doubledX - doubledY) / 2 };
			return true;
		case 4:
			if ((doubledX + doubledY) % 2 != 0)
				return false;
			twin = { (doubledX + doubledY) / 2 - cell.y, (doubledX + doubledY) / 2 - cell.x };
			return true;
		}
		return false;
	}

	// Symétrie qui échange les cases de départ des deux équipes et conserve les cases praticables
	// (-1 si aucune).
	int findSymmetry(const BattleMap & map, int doubledX, int doubledY)
	{
		if (map.startCells[1].empty() || map.startCells[1].size() != map.startCells[2].size())
			return -1;
		for (int kind = 0; kind < SYMMETRY_COUNT; kind++)
		{
			bool symmetric = true;
			for (const Cell & start : map.startCells[1])
			{
				Cell twin;
				symmetric = symmetric && mirrorCell(kind, doubledX, doubledY, start, twin)
					&& std::find(map.startCells[2].begin(), map.startCells[2].end(), twin) != map.startCells[2].end();
			}
			for (int y = 0; y < map.getHeight() && symmetric; y++)
			{
				for (int x = 0; x < map.getWidth() && symmetric; x++)
				{
					Cell twin;
					if (!map.isWalkable({ x, y }))
						continue;
					symmetric = mirrorCell(kind, doubledX, doubledY, { x, y }, twin) && map.contains(twin) && map.isWalkable(twin);
				}
			}
			if (symmetric)
				return kind;
		}
		return -1;
	}
}

std::vector<Cell> tw::battle::objectiveZone(const BattleMap & map)
{
	std::vector<Cell> painted;
	for (const Cell & cell : map.zoneCells)
	{
		if (map.contains(cell) && map.isWalkable(cell) && std::find(painted.begin(), painted.end(), cell) == painted.end())
			painted.push_back(cell);
	}
	if (!painted.empty())
		return painted;

	// Distance de marche de chaque équipe : depuis sa case de départ la plus proche (les joueurs
	// choisissent leur case) et en moyenne sur toutes ses cases de départ.
	int width = map.getWidth();
	int size = width * map.getHeight();
	std::vector<int> nearest[3];
	std::vector<double> average[3];
	for (int team = 1; team <= 2; team++)
	{
		nearest[team] = walkDistances(map, map.startCells[team]);
		average[team].assign(size, 0.0);
		std::vector<int> reached(size, 0);
		for (const Cell & start : map.startCells[team])
		{
			std::vector<int> distances = walkDistances(map, { start });
			for (int i = 0; i < size; i++)
			{
				if (distances[i] < 0)
					continue;
				average[team][i] += distances[i];
				reached[i]++;
			}
		}
		for (int i = 0; i < size; i++)
			average[team][i] = reached[i] > 0 ? average[team][i] / reached[i] : 0.0;
	}
	// Centre : à mi-chemin des deux groupes de cases de départ (à défaut, celui de la carte).
	double centerX = (map.getWidth() - 1) / 2.0;
	double centerY = (map.getHeight() - 1) / 2.0;
	if (!map.startCells[1].empty() && !map.startCells[2].empty())
	{
		double sumX = 0;
		double sumY = 0;
		for (int team = 1; team <= 2; team++)
		{
			double x = 0;
			double y = 0;
			for (const Cell & start : map.startCells[team])
			{
				x += start.x;
				y += start.y;
			}
			sumX += x / map.startCells[team].size();
			sumY += y / map.startCells[team].size();
		}
		centerX = sumX / 2;
		centerY = sumY / 2;
	}

	// Coût d'une case : écarts de distance entre les équipes, puis éloignement du centre de la carte.
	// Deux cases symétriques (carte symétrique) ont le même coût.
	std::vector<double> cost(size, -1.0);
	for (int y = 0; y < map.getHeight(); y++)
	{
		for (int x = 0; x < width; x++)
		{
			int index = y * width + x;
			if (!map.isWalkable({ x, y }) || nearest[1][index] < 0 || nearest[2][index] < 0)
				continue;
			cost[index] = std::fabs(average[1][index] - average[2][index]) * 4.0 + absValue(nearest[1][index] - nearest[2][index]) * 2.0
				+ std::fabs(x - centerX) + std::fabs(y - centerY);
		}
	}

	// Carte symétrique (les cartes de tournoi le sont) : la zone l'est aussi, donc équitable. Elle part
	// de la paire de cases symétriques voisines (ou de la case sur l'axe) la moins chère près du
	// centre, puis s'étend par paires de cases voisines jusqu'à 5 cases au moins.
	int doubledX = (int)std::lround(centerX * 2);
	int doubledY = (int)std::lround(centerY * 2);
	int symmetry = std::fabs(centerX * 2 - doubledX) < 1e-6 && std::fabs(centerY * 2 - doubledY) < 1e-6
		? findSymmetry(map, doubledX, doubledY) : -1;
	if (symmetry >= 0)
	{
		std::vector<Cell> zone;
		std::vector<bool> taken(size, false);
		while (zone.size() < 5)
		{
			bool found = false;
			Cell best;
			Cell bestTwin;
			double bestCost = 0;
			for (int y = 0; y < map.getHeight(); y++)
			{
				for (int x = 0; x < width; x++)
				{
					int index = y * width + x;
					Cell cell = { x, y };
					Cell twin;
					if (cost[index] < 0 || taken[index] || !mirrorCell(symmetry, doubledX, doubledY, cell, twin))
						continue;
					bool adjacent = false;
					if (zone.empty())
						adjacent = absValue(twin.x - x) <= 1 && absValue(twin.y - y) <= 1;
					for (const Cell & other : zone)
						adjacent = adjacent || (absValue(other.x - x) <= 1 && absValue(other.y - y) <= 1);
					if (!adjacent)
						continue;
					double total = cost[index] + 2.0 * std::sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));
					if (!found || total < bestCost - 0.001)
					{
						found = true;
						best = cell;
						bestTwin = twin;
						bestCost = total;
					}
				}
			}
			if (!found)
				break;
			zone.push_back(best);
			taken[best.y * width + best.x] = true;
			if (bestTwin != best)
			{
				zone.push_back(bestTwin);
				taken[bestTwin.y * width + bestTwin.x] = true;
			}
		}
		if (zone.size() >= 5)
			return zone;
	}

	// Sinon, la zone part de la meilleure case et s'étend aux cases voisines (diagonales comprises) les
	// moins chères, un peu pénalisées par leur éloignement du départ : 5 cases, ou 6 quand la
	// suivante est à égalité avec la dernière (paire de cases symétriques).
	const double TIE = 0.001;
	std::vector<Cell> zone;
	std::vector<bool> taken(size, false);
	double lastCost = 0;
	while (zone.size() < 6)
	{
		bool found = false;
		Cell best;
		double bestCost = 0;
		for (int y = 0; y < map.getHeight(); y++)
		{
			for (int x = 0; x < width; x++)
			{
				int index = y * width + x;
				if (cost[index] < 0 || taken[index])
					continue;
				double total = cost[index];
				if (!zone.empty())
				{
					bool adjacent = false;
					for (const Cell & cell : zone)
						adjacent = adjacent || (absValue(cell.x - x) <= 1 && absValue(cell.y - y) <= 1);
					if (!adjacent)
						continue;
					total += std::max(absValue(zone[0].x - x), absValue(zone[0].y - y));
				}
				if (!found || total < bestCost - TIE)
				{
					found = true;
					best = { x, y };
					bestCost = total;
				}
			}
		}
		if (!found || (zone.size() == 5 && bestCost > lastCost + TIE))
			break;
		zone.push_back(best);
		taken[best.y * width + best.x] = true;
		lastCost = bestCost;
	}
	return zone;
}

void tw::battle::zoneDistances(const BattleMap & map, const std::vector<Cell> & zone, int distances[3])
{
	std::vector<int> fromZone = walkDistances(map, zone);
	for (int team = 1; team <= 2; team++)
	{
		distances[team] = -1;
		for (const Cell & start : map.startCells[team])
		{
			if (!map.contains(start))
				continue;
			int distance = fromZone[start.y * map.getWidth() + start.x];
			if (distance >= 0 && (distances[team] < 0 || distance < distances[team]))
				distances[team] = distance;
		}
	}
}

void tw::battle::zonePresence(const BattleState & state, bool present[3])
{
	present[0] = present[1] = present[2] = false;
	for (const Fighter & fighter : state.fighters)
	{
		if (fighter.alive && (fighter.team == 1 || fighter.team == 2) && state.zone.contains(fighter.position))
			present[fighter.team] = true;
	}
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
