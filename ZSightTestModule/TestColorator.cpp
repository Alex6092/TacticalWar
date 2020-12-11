#include "TestColorator.h"

sf::Color TestColorator::getColorForCell(tw::CellData * cell)
{
	for (int i = 0; i < path.size(); i++)
	{
		if ((*cell) == path[i])
		{
			return sf::Color(255, 0, 255);
		}
	}
	
	return sf::Color(255, 255, 255);
}

void TestColorator::setPathToHighlight(std::vector<tw::Point2D> path)
{
	this->path = path;
}