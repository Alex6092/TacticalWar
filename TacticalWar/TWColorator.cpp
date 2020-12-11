#include "TWColorator.h"


sf::Color TWColorator::getColorForCell(tw::CellData * cell)
{
	if (cell->getIsWalkable() && !cell->getIsObstacle())
	{
		for (int i = 0; i < pathToHighlight.size(); i++)
		{
			if ((*cell) == pathToHighlight[i])
			{
				return getPathHighlightColor();
			}
		}

		for (int i = 0; i < pathZone.size(); i++)
		{
			if ((*cell) == pathZone[i])
			{
				return getPathZoneColor();
			}
		}
	}

	return sf::Color::White;
}