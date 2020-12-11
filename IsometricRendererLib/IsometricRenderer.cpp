#include "pch.h"
#include "IsometricRenderer.h"
#include <CharacterView.h>
#include <math.h>
#include <iostream>

using namespace tw;

IsometricRenderer::IsometricRenderer(sf::RenderWindow * window)
{
	hasFocus = true;
	forcedFocus = false;
	if (!textureGrass.loadFromFile("assets/tiles/Grass_01.png")) { std::cout << "Impossible de charger Grass texture" << std::endl; }
	if (!textureWater.loadFromFile("assets/tiles/Water_01.png")) { std::cout << "Impossible de charger Water texture" << std::endl; }
	if (!textureStone.loadFromFile("assets/tiles/Stone_02.png")) { std::cout << "Impossible de charger Stone texture" << std::endl; }
	if (!textureTree.loadFromFile("assets/tiles/Tree_01.png")) { std::cout << "Impossible de charger Tree texture" << std::endl; }

	textureGrass.setSmooth(true);
	textureWater.setSmooth(true);
	textureStone.setSmooth(true);
	textureTree.setSmooth(true);

	this->window = window;
	this->colorator = NULL;
}

void IsometricRenderer::manageEvents(Environment * environment, std::vector<BaseCharacterModel*> & characters)
{
	sf::Event e;
	while (window->pollEvent(e))
	{
		// check the type of the event...
		switch (e.type)
		{
			/*
			case sf::Event::LostFocus:
				std::cout << "Lost focus" << std::endl;
				hasFocus = false;
				break;

			case sf::Event::GainedFocus:
				std::cout << "Gain focus" << std::endl;
				hasFocus = true;

				break;
			*/
			// window closed
		case sf::Event::Closed:
			window->close();
			break;

			// key pressed
		case sf::Event::KeyPressed:

			break;

		case sf::Event::MouseButtonPressed:
			if (e.mouseButton.button == sf::Mouse::Left)
			{
				int x = e.mouseButton.x;
				int y = e.mouseButton.y;

				sf::Vector2f unprojected = window->mapPixelToCoords(sf::Vector2i(x, y));
				sf::Vector2i isoCoords = screenCoordinatesToIsoGridCoordinates(unprojected.x, unprojected.y);


				int cellX = isoCoords.x;
				int cellY = isoCoords.y;

				if (cellX >= 0 && cellX < environment->getWidth()
					&&
					cellY >= 0 && cellY < environment->getHeight())
				{
					if (hasFocus)
						notifyCellClicked(cellX, cellY);
				}
			}
			break;
			// we don't process other types of events
		default:
			break;
		}

		notifyEvent(&e);
	}

	// Gestion du focus :
	if (!forcedFocus)
	{
		if (window->hasFocus())
		{
			if (!hasFocus)
			{
				hasFocus = true;
				std::cout << "Gain focus" << std::endl;
			}
		}
		else
		{
			if (hasFocus)
			{
				hasFocus = false;
				std::cout << "Lost focus" << std::endl;
			}
		}
	}

	sf::Vector2i position = sf::Mouse::getPosition(*window);
	if (position.x >= 0 && position.y >= 0)
	{
		sf::Vector2f unprojected = window->mapPixelToCoords(position);
		sf::Vector2i isoCoords = screenCoordinatesToIsoGridCoordinates(unprojected.x, unprojected.y);

		int cellX = isoCoords.x;
		int cellY = isoCoords.y;

		if (cellX >= 0 && cellX < environment->getWidth()
			&&
			cellY >= 0 && cellY < environment->getHeight())
		{
			if (hasFocus)
			{
				if (sf::Mouse::isButtonPressed(sf::Mouse::Left))
				{
					notifyCellMouseDown(cellX, cellY);
				}

				notifyCellHover(cellX, cellY);
			}
		}		
	}
}
	
sf::Vector2i IsometricRenderer::screenCoordinatesToIsoGridCoordinates(int screenX, int screenY)
{
	screenY -= 30;

	int calcX = floor((screenY/60.0) + (screenX/120.0));
	int calcY = ceil((screenY/60.0) - (screenX/120.0));
	//return sf::Vector2i((screenX/120 + screenY/120)*2, (screenY / 60 - screenX/60)*2);
	//return sf::Vector2i((screenX/120 - screenY/120)*64, (screenX/60+screenY/60)*64);
	return sf::Vector2i(calcX, calcY);
}

void IsometricRenderer::render(Environment* environment, std::vector<BaseCharacterModel*> & characters, float deltatime)
{
	manageEvents(environment, characters);

	sf::View view = window->getView();
	float centerX = environment->getWidth() / 2;
	float centerY = environment->getHeight() / 2;
	int viewCenterX = (centerX * 120.0 - centerY * 120.0) / 2;
	int viewCenterY = (centerY * 60.0 + centerY * 60.0) / 2;
	view.setCenter(viewCenterX + 60.0, viewCenterY + 30.0);
	window->setView(view);

	sf::Sprite spriteGrass;
	sf::Sprite spriteStone;
	sf::Sprite spriteWater;
	sf::Sprite spriteTree;
	sf::Sprite spriteToDraw;

	spriteGrass.setTexture(textureGrass);
	spriteWater.setTexture(textureWater);
	spriteStone.setTexture(textureStone);
	spriteTree.setTexture(textureTree);
	
	spriteGrass.setPosition(-127*0.05, -309 * 0.05);
	spriteGrass.setScale(0.05, 0.05);

	int borderX;
	int borderY;
	
	for (int i = 0; i < environment->getWidth(); i++)
	{
		for (int j = 0; j < environment->getHeight(); j++)
		{
			CellData * cell = environment->getMapData(i, j);
			if (cell->getIsObstacle())
			{
				spriteToDraw = spriteStone;
				borderY = -590 * 0.05;
				borderX = -202 * 0.05;
			}
			else if (cell->getIsWalkable())
			{
				spriteToDraw = spriteGrass;
				borderX = -128 * 0.05;
				borderY = -310 * 0.05;
			}
			else
			{
				borderX = -194 * 0.05;
				borderY = -260 * 0.05;
				spriteToDraw = spriteWater;
				borderY += 10;
			}

			int isoX = (i*120 - j*120)/2; // Cordonnées
			int isoY = (i*60 + j*60)/2;

			sf::Color toApply = sf::Color::White;
			if (colorator != NULL)
			{
				toApply = colorator->getColorForCell(cell);
			}

			spriteToDraw.setColor(toApply);
			spriteToDraw.setScale(0.05, 0.05);
			spriteToDraw.setPosition(borderX+isoX, borderY+isoY); 
			window->draw(spriteToDraw);
		}
	}
	
	for (int i = 0; i < characters.size(); i++)
	{
		BaseCharacterModel * m = characters[i];
		CharacterView & v = getCharacterView(m);
		v.update(deltatime);
		sf::Sprite * s = v.getImageToDraw();

		int isoX = (m->getInterpolatedX() * 120 - m->getInterpolatedY() * 120) / 2;
		int isoY = (m->getInterpolatedX() * 60 + m->getInterpolatedY() * 60) / 2;

		s->setPosition(isoX + 60, isoY + 30);
	

		sf::IntRect rect = s->getTextureRect();
		bool flipped = s->getScale().x < 0;
		float scaleX = 0.4;
		float scaleY = 0.4;
		s->setScale(flipped ? -scaleX : scaleX, scaleY);
		window->draw(*s);
	}
}


CharacterView & IsometricRenderer::getCharacterView(BaseCharacterModel * model)
{
	if (characterViewsCache.find(model) == characterViewsCache.end())
	{
		characterViewsCache[model] = new CharacterView(model);
	}

	return *characterViewsCache[model];
}