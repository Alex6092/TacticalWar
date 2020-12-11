#include "MapClickSight.h"
#include <Sight.h>
#include <iostream>

MapClickListener::MapClickListener(tw::Environment * e, TestColorator * colorator)
{
	this->colorator = colorator;
	this->environment = e;
	startPointSelected = false;
	endPointSelected = false;
}

MapClickListener::~MapClickListener()
{

}

void MapClickListener::onCellClicked(int x, int y)
{
	std::cout << "Cellule click x=" << x << ", y=" << y << std::endl;

	if (!startPointSelected)
	{
		std::cout << "Set startCell" << std::endl;
		startCell.setX(x);
		startCell.setY(y);
		startPointSelected = true;
	}
	else if (!endPointSelected)
	{
		std::cout << "Set endCell" << std::endl;
		endCell.setX(x);
		endCell.setY(y);
		endPointSelected = true;

		processedPath = tw::Pathfinder::getInstance()->getPath(startCell, endCell, environment, std::vector<tw::Obstacle*>());
		colorator->setPathToHighlight(processedPath);
	}
	else
	{
		std::cout << "Clear path" << std::endl;
		startPointSelected = false;
		endPointSelected = false;
		processedPath.clear();
		colorator->setPathToHighlight(processedPath);
	}
}

void MapClickListener::onCellHover(int x, int y)
{
}

void MapClickListener::onCellMouseDown(int x, int y)
{
}

