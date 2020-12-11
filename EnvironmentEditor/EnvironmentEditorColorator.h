#pragma once

#include <Environment.h>
#include <CellColorator.h>

class EnvironmentEditorColorator : public tw::CellColorator
{
private:
	tw::Environment * environment;
public:
	EnvironmentEditorColorator()
	{
		this->environment = NULL;
	}

	void setEnvironment(tw::Environment * e)
	{
		this->environment = e;
	}

	virtual sf::Color getColorForCell(tw::CellData * cell)
	{
		if (cell->isTeam1StartPoint())
		{
			return sf::Color(50, 200, 255);
		}
		else if (cell->isTeam2StartPoint())
		{
			return sf::Color(255, 50, 50);
		}
		
		return sf::Color(255, 255, 255);
	}
};

