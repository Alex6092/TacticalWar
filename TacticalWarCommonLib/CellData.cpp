#include "pch.h"
#include "CellData.h"
#include "TileRegistry.h"

using namespace tw;

CellData::CellData(int x, int y, bool isWalkable, bool isObstacle, int teamStartPoint)
	:
	Point2D(x, y)
{
	rules.walkable = isWalkable;
	rules.blocksLineOfSight = isObstacle;
	this->teamStartPoint = teamStartPoint;
}

std::string CellData::getDisplayTile() const
{
	if (!tile.empty())
		return tile;
	return TileRegistry::legacyTile(rules.walkable, rules.blocksLineOfSight);
}
