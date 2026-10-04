#include "pch.h"
#include "Environment.h"
#include "TileRegistry.h"

using namespace tw;

Environment::Environment(int width, int height, int environmentId, const std::string & fillTile)
{
	this->obstacleCacheInitDone = false;
	this->id = environmentId;
	this->width = width > 0 ? width : 1;
	this->height = height > 0 ? height : 1;
	this->tournamentPool = true;

	cells.reserve(this->width * this->height);
	for (int i = 0; i < this->width; i++)
	{
		for (int j = 0; j < this->height; j++)
		{
			cells.push_back(new CellData(i, j));
			if (!fillTile.empty())
				setTile(i, j, fillTile);
		}
	}
}

Environment::~Environment()
{
	for (CellData * cell : cells)
		delete cell;
}

CellData* Environment::getMapData(int x, int y)
{
	if (x >= 0 && x < width && y >= 0 && y < height)
		return cells[x * height + y];
	return NULL;
}

void Environment::setTile(int x, int y, const std::string & tile)
{
	const TileDef * def = TileRegistry::get().find(tile);
	setTile(x, y, tile, def != nullptr ? def->rules : TileRules::unknown());
}

bool Environment::hasSpecialCells()
{
	for (CellData * cell : cells)
	{
		const TileRules & rules = cell->getRules();
		if (rules.hasTurnEffect() || (rules.walkable && rules.blocksLineOfSight))
			return true;
	}
	return false;
}

void Environment::setTile(int x, int y, const std::string & tile, const TileRules & rules)
{
	CellData * cell = getMapData(x, y);
	if (cell == NULL)
		return;

	cell->setTile(tile, rules);
	obstacleCacheInitDone = false;
	staticObstacles.clear();
}
