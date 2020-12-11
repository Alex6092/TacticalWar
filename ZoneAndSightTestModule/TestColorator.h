#pragma once

#include "CellColorator.h"

class TestColorator : public tw::CellColorator
{
	std::vector<tw::Point2D> pathZone;
	sf::Color pathZoneColor;

public:
	TestColorator(sf::Color pathZoneColor)
	{
		this->pathZoneColor = pathZoneColor;
	}

	virtual sf::Color getColorForCell(tw::CellData * cell);

	void setPathZone(std::vector<tw::Point2D> pathZone)
	{
		this->pathZone = pathZone;
	}

	sf::Color getPathZoneColor() { return pathZoneColor; }
};

