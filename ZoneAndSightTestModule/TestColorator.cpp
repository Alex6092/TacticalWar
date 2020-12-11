#include "TestColorator.h"

sf::Color TestColorator::getColorForCell(tw::CellData * cell)
{
	for (int i = 0; i < pathZone.size(); i++)
	{
		if ((*cell) == pathZone[i])
		{
			return getPathZoneColor();
		}
	}

	return sf::Color::White;
}