#pragma once

#include <Environment.h>
#include <CellColorator.h>
#include <EditorController.h>

// Couleurs des cases dans l'éditeur : départs des équipes, zone à tenir, aperçu du rectangle,
// case survolée et case signalée par la validation.
class EnvironmentEditorColorator : public tw::CellColorator
{
private:
	tw::editor::EditorController * controller;
	int hoverX;
	int hoverY;
	int flaggedX;
	int flaggedY;

public:
	EnvironmentEditorColorator(tw::editor::EditorController * controller)
		: controller(controller), hoverX(-1), hoverY(-1), flaggedX(-1), flaggedY(-1)
	{
	}

	void setHover(int x, int y)
	{
		hoverX = x;
		hoverY = y;
	}

	void setFlagged(int x, int y)
	{
		flaggedX = x;
		flaggedY = y;
	}

	virtual sf::Color getColorForCell(tw::CellData * cell)
	{
		int x = cell->getX();
		int y = cell->getY();

		int x0, y0, x1, y1;
		if (controller->getRectangle(x0, y0, x1, y1) && x >= x0 && x <= x1 && y >= y0 && y <= y1)
			return sf::Color(255, 240, 120);

		if (x == flaggedX && y == flaggedY)
			return sf::Color(255, 140, 40);

		sf::Color color(255, 255, 255);
		if (cell->getTeamStartPointNumber() == 1)
			color = sf::Color(50, 200, 255);
		else if (cell->getTeamStartPointNumber() == 2)
			color = sf::Color(255, 50, 50);
		else if (cell->getIsZone())
			color = sf::Color(255, 200, 40);

		if (x == hoverX && y == hoverY)
			color = sf::Color(color.r * 3 / 4, color.g * 3 / 4, color.b * 3 / 4);
		return color;
	}
};
