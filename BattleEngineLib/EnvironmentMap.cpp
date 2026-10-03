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

			bool obstacle = cell->getIsObstacle();
			bool walkable = cell->getIsWalkable() && !obstacle;
			map.setCell({ x, y }, walkable, obstacle);

			int team = cell->getTeamStartPointNumber();
			if (walkable && (team == 1 || team == 2))
				map.startCells[team].push_back({ x, y });
			if (walkable && cell->getIsZone())
				map.zoneCells.push_back({ x, y });
		}
	}
	return map;
}
