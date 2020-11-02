#pragma once

#include "Screen.h"
#include <IsometricRenderer.h>
#include <Environment.h>
#include <BaseCharacterModel.h>

namespace tw
{
	class BattleScreen : public Screen
	{
	private:
		IsometricRenderer * renderer;
		Environment * environment;
		std::vector<tw::BaseCharacterModel*> characters;
		sf::Font font;
		sf::Text FPS;

		bool testDirection;
		bool XDirection;
		bool YDirection;
		inline void setNextPosition()
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

	public:
		BattleScreen();

		virtual void handleEvents(sf::RenderWindow * window);
		virtual void update(float deltatime);
		virtual void render(sf::RenderWindow * window);
	};
}

