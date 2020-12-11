

#include <Environment.h>
#include <BaseCharacterModel.h>
#include <vector>
#include <IsometricRenderer.h>
#include <SFML/Graphics.hpp>
#include "TestColorator.h"
#include "TestCharacterModel.h"
#include <ZoneAndSightCalculator.h>

using namespace tw;


void setNextPosition(std::vector<BaseCharacterModel*> & characters, bool &testDirection, bool &XDirection, bool &YDirection, Environment * environment)
{
	int targetX = characters[0]->getCurrentX();
	int targetY = characters[0]->getCurrentY();
	if (!testDirection)
		targetX += (XDirection ? 1 : -1);
	else
		targetY += (YDirection ? 1 : -1);

	if (targetX < 0)
	{
		XDirection = !XDirection;
		targetX = 1;
	}

	if (targetY < 0)
	{
		YDirection = !YDirection;
		targetY = 1;
	}

	if (targetX >= environment->getWidth())
	{
		XDirection = !XDirection;
		targetX = environment->getWidth() - 2;
	}

	if (targetY >= environment->getHeight())
	{
		YDirection = !YDirection;
		targetY = environment->getHeight() - 2;
	}

	characters[0]->setTargetPosition(targetX, targetY);

	testDirection = !testDirection;
}

int main(int argc, char** argv)
{
	sf::RenderWindow window(sf::VideoMode(800, 600), "Isometric renderer test module");
	sf::Clock deltaClock;

	IsometricRenderer renderer(&window);

	bool testDirection;
	bool XDirection;
	bool YDirection;
	std::vector<tw::BaseCharacterModel*> characters;
	Environment environment(20, 20, 0);
	TestColorator * colorator = new TestColorator(sf::Color::Red);

	std::vector<Point2D> pathZone;

	characters.push_back(new TestCharacterModel(&environment, 0, 0, 0));
	XDirection = true;
	YDirection = true;
	testDirection = false;
	setNextPosition(characters, testDirection, XDirection, YDirection, &environment);


	environment.getMapData(0, 0)->setIsObstacle(true);
	environment.getMapData(0, 1)->setIsWalkable(false);
	environment.getMapData(0, 2)->setIsObstacle(true);
	environment.getMapData(0, 3)->setIsWalkable(false);

	environment.getMapData(1, 0)->setIsWalkable(false);
	environment.getMapData(1, 1)->setIsObstacle(true);
	environment.getMapData(1, 2)->setIsWalkable(false);
	environment.getMapData(1, 3)->setIsObstacle(true);

	environment.getMapData(2, 0)->setIsObstacle(true);
	environment.getMapData(2, 1)->setIsWalkable(false);
	environment.getMapData(2, 2)->setIsObstacle(true);
	environment.getMapData(2, 3)->setIsWalkable(false);


	environment.getMapData(3, 0)->setIsWalkable(false);
	environment.getMapData(3, 1)->setIsObstacle(true);
	environment.getMapData(3, 2)->setIsWalkable(false);
	environment.getMapData(3, 3)->setIsObstacle(true);

	renderer.setColorator(colorator);

	while (window.isOpen())
	{
		float deltatime = deltaClock.restart().asSeconds();

		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
		}

		for (int i = 0; i < characters.size(); i++)
		{
			characters[i]->update(deltatime);

			if (!characters[i]->hasTargetPosition())
			{
				setNextPosition(characters, testDirection, XDirection, YDirection, &environment);
			}

			int x = characters[i]->getCurrentX();
			int y = characters[i]->getCurrentY();

			colorator->setPathZone(ZoneAndSightCalculator::getInstance()->generateZone(x, y, 0, 3, TypeZoneLaunch::DIAGONAL));
		}

		window.clear();
		renderer.render(&environment, characters, deltatime);
		window.display();
	}

	return 0;
}

