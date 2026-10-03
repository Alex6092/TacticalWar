#include "EnvironmentMap.h"

#include <Environment.h>

tw::battle::BattleMap tw::battle::battleMapFromEnvironment(tw::Environment * environment)
{
	BattleMap map(environment->getWidth(), environment->getHeight());
	for (int x = 0; x < environment->getWidth(); x++)
	{
		for (int y = 0; y < environment->getHeight(); y++)
		{
			tw::CellData * cell = environment->getMapData(x, y);
			if (cell == nullptr)
				continue;

			const tw::TileRules & rules = cell->getRules();
			bool walkable = rules.walkable;
			map.setCell({ x, y }, walkable, rules.blocksLineOfSight);
			if (walkable)
				map.setTurnEffect({ x, y }, rules.turnDamage, rules.turnHeal);

			int team = cell->getTeamStartPointNumber();
			if (walkable && (team == 1 || team == 2))
				map.startCells[team].push_back({ x, y });
			if (walkable && cell->getIsZone())
				map.zoneCells.push_back({ x, y });
		}
	}
	return map;
}
