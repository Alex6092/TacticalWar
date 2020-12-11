

#include <Environment.h>
#include <BaseCharacterModel.h>
#include <vector>
#include <IsometricRenderer.h>
#include <SFML/Graphics.hpp>
#include "TestColorator.h"
#include "MapClickListener.h"
#include <iostream>
using namespace tw;

int main(int argc, char** argv)
{
	int *textureLoad = 0;
	sf::RenderWindow window(sf::VideoMode(800, 600), "SightTestModule test module");
	sf::Clock deltaClock;

	IsometricRenderer renderer(&window);
	Environment environment(20, 20, 0);
	std::vector<BaseCharacterModel*> characters;


	environment.getMapData(5, 7)->setIsObstacle(true);
	environment.getMapData(6, 7)->setIsWalkable(false);
	environment.getMapData(7, 7)->setIsObstacle(true);
	environment.getMapData(8, 7)->setIsWalkable(false);


	environment.getMapData(10, 10)->setIsWalkable(false);
	environment.getMapData(11, 11)->setIsObstacle(true);
	environment.getMapData(12, 12)->setIsWalkable(false);
	environment.getMapData(13, 13)->setIsObstacle(true);



	TestColorator * colorator = new TestColorator();

	renderer.setColorator(colorator);
	renderer.addEventListener(new MapClickListener(&environment, colorator));

	while (window.isOpen())
	{
		float deltatime = deltaClock.restart().asSeconds();

		/*
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
		}
		*/

		window.clear();
		renderer.render(&environment, characters, deltatime);
		window.display();
	}

	return 0;
}