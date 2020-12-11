#pragma once

#include "CellColorator.h"
#include <Point2D.h>

class TestColorator : public tw::CellColorator
{
	std::vector<tw::Point2D> path;
public:
	virtual sf::Color getColorForCell(tw::CellData * cell);
	void setPathToHighlight(std::vector<tw::Point2D> path);
};

