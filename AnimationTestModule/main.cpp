

#include <Environment.h>
#include <BaseCharacterModel.h>
#include <vector>
#include <IsometricRenderer.h>
#include <SFML/Graphics.hpp>
#include <iostream>
#include <AnimationManager.h>
#include <Mage.h>

#include <SpellView.h>
#include "ScreenCallbackTest.h"
#include <LaunchSpellAction.h>
#include <CharacterMoveAction.h>
#include <Pathfinder.h>

using namespace tw;

int main(int argc, char** argv)
{
	sf::RenderWindow window(sf::VideoMode(800, 600), "SightTestModule test module");
	window.setVerticalSyncEnabled(true);
	sf::Clock deltaClock;

	IsometricRenderer renderer(&window);
	Environment environment(20, 20, 0);
	std::vector<BaseCharacterModel*> characters;

	ScreenCallbackTest * screen = new ScreenCallbackTest(&environment);
	SpellView * Spell = new SpellView();
	characters.push_back(screen->getCharacter(1)->getCharacter());
	characters.push_back(screen->getCharacter(2)->getCharacter());

	environment.getMapData(5, 7)->setIsObstacle(true);
	environment.getMapData(6, 7)->setIsWalkable(false);
	environment.getMapData(7, 7)->setIsObstacle(true);
	environment.getMapData(8, 7)->setIsWalkable(false);


	environment.getMapData(10, 10)->setIsWalkable(false);
	
	environment.getMapData(12, 12)->setIsWalkable(false);
	environment.getMapData(13, 13)->setIsObstacle(true);

	std::vector<Point2D> path1 = Pathfinder::getInstance()->getPath(tw::Point2D(screen->getCharacter(1)->getCharacter()->getCurrentX(), screen->getCharacter(1)->getCharacter()->getCurrentY()),
																	tw::Point2D(screen->getCharacter(1)->getCharacter()->getCurrentX(), screen->getCharacter(1)->getCharacter()->getCurrentY() + 1),
																	&environment,
																	std::vector<Obstacle*>());

	std::vector<Point2D> path2 = Pathfinder::getInstance()->getPath(tw::Point2D(screen->getCharacter(1)->getCharacter()->getCurrentX(), screen->getCharacter(1)->getCharacter()->getCurrentY() + 1),
		tw::Point2D(screen->getCharacter(1)->getCharacter()->getCurrentX() - 1, screen->getCharacter(1)->getCharacter()->getCurrentY()),
		&environment,
		std::vector<Obstacle*>());

	std::vector<Point2D> path3 = Pathfinder::getInstance()->getPath(tw::Point2D(screen->getCharacter(1)->getCharacter()->getCurrentX() - 1, screen->getCharacter(1)->getCharacter()->getCurrentY()),
		tw::Point2D(screen->getCharacter(1)->getCharacter()->getCurrentX(), screen->getCharacter(1)->getCharacter()->getCurrentY()),
		&environment,
		std::vector<Obstacle*>());

	AnimationManager::getInstance()->addAnimation(new LaunchSpellAction(screen, 1, 1, 11, 11));
	AnimationManager::getInstance()->addAnimation(new CharacterMoveAction(screen, 1, path1));
	AnimationManager::getInstance()->addAnimation(new LaunchSpellAction(screen, 1, 2, 11, 11));
	AnimationManager::getInstance()->addAnimation(new CharacterMoveAction(screen, 1, path2));
	AnimationManager::getInstance()->addAnimation(new LaunchSpellAction(screen, 1, 3, 11, 11));
	AnimationManager::getInstance()->addAnimation(new LaunchSpellAction(screen, 1, 4, 11, 11));
	AnimationManager::getInstance()->addAnimation(new LaunchSpellAction(screen, 1, 1, 11, 11));
	AnimationManager::getInstance()->addAnimation(new CharacterMoveAction(screen, 1, path3));
	AnimationManager::getInstance()->addAnimation(new LaunchSpellAction(screen, 1, 2, 11, 11));
	AnimationManager::getInstance()->addAnimation(new LaunchSpellAction(screen, 1, 1, 11, 11));
	AnimationManager::getInstance()->addAnimation(new CharacterMoveAction(screen, 1, path1));
	AnimationManager::getInstance()->addAnimation(new LaunchSpellAction(screen, 1, 2, 11, 11));
	AnimationManager::getInstance()->addAnimation(new CharacterMoveAction(screen, 1, path2));
	AnimationManager::getInstance()->addAnimation(new LaunchSpellAction(screen, 1, 3, 11, 11));
	AnimationManager::getInstance()->addAnimation(new LaunchSpellAction(screen, 1, 4, 11, 11));
	AnimationManager::getInstance()->addAnimation(new LaunchSpellAction(screen, 1, 1, 11, 11));
	AnimationManager::getInstance()->addAnimation(new CharacterMoveAction(screen, 1, path3));
	Spell->loadAnimation("./assets/spellsprites/ballbig1_red");
	while (window.isOpen())
	{
		float deltatime = deltaClock.restart().asSeconds();

		AnimationManager::getInstance()->update(deltatime);

		for (int i = 0; i < characters.size(); i++)
			characters[i]->update(deltatime);

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