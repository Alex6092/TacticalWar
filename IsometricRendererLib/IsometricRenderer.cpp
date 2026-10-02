#include "pch.h"
#include "IsometricRenderer.h"
#include <CharacterView.h>
#include <SpellView.h>
#include <TileRegistry.h>
#include <algorithm>
#include <math.h>
#include <iostream>

using namespace tw;

IsometricRenderer::IsometricRenderer(sf::RenderWindow * window)
{
	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/fragment.frag");
	waterShader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/water-animation.glsl");

	hasFocus = true;
	forcedFocus = false;

	// Texture de secours (tuile introuvable) : losange magenta.
	sf::Image missing;
	missing.create(120, 60, sf::Color::Transparent);
	for (int y = 0; y < 60; y++)
	{
		for (int x = 0; x < 120; x++)
		{
			if (std::abs(x - 60) / 60.f + std::abs(y - 30) / 30.f <= 1.f)
				missing.setPixel(x, y, sf::Color(255, 0, 255));
		}
	}
	missingTexture.loadFromImage(missing);

	this->window = window;
	this->colorator = NULL;
	this->ellapsedTime = 0;
	this->hasCamera = false;
	this->cameraZoom = 1.f;
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
				hasFocus = false;
				break;

			case sf::Event::GainedFocus:
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
			}
		}
		else
		{
			if (hasFocus)
			{
				hasFocus = false;
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
	
sf::Vector2i IsometricRenderer::screenCoordinatesToIsoGridCoordinates(float worldX, float worldY)
{
	// Inverse de la projection : la case dont le losange contient le point.
	float u = (worldX - 60.f) / 120.f + (worldY - 30.f) / 60.f;
	float v = (worldY - 30.f) / 60.f - (worldX - 60.f) / 120.f;
	return sf::Vector2i((int)std::floor(u + 0.5f), (int)std::floor(v + 0.5f));
}

void IsometricRenderer::reloadTiles()
{
	tileTextures.clear();
}

const sf::Texture & IsometricRenderer::getTileTexture(const TileDef & tile)
{
	auto it = tileTextures.find(tile.id);
	if (it == tileTextures.end())
	{
		sf::Texture texture;
		if (tile.texture.empty() || !texture.loadFromFile(tile.texture))
		{
			std::cout << "Texture introuvable pour la tuile " << tile.id << " : " << tile.texture << std::endl;
			texture = missingTexture;
		}
		texture.setSmooth(true);
		it = tileTextures.insert(std::make_pair(tile.id, texture)).first;
	}
	return it->second;
}

void IsometricRenderer::drawCell(Environment * environment, int x, int y)
{
	CellData * cell = environment->getMapData(x, y);
	if (cell == NULL)
		return;

	const TileDef * tile = TileRegistry::get().find(cell->getDisplayTile());
	if (tile != NULL && tile->category == TileCategory::EMPTY)
		return;

	const sf::Texture * texture = &missingTexture;
	float anchorX = 60;
	float anchorY = 30;
	if (tile != NULL)
	{
		texture = &getTileTexture(*tile);
		if (texture != &missingTexture)
		{
			anchorX = tile->anchorX;
			anchorY = tile->anchorY;
		}
	}

	tileSprite.setTexture(*texture, true);
	tileSprite.setColor(colorator != NULL ? colorator->getColorForCell(cell) : sf::Color::White);
	float centerX = (x - y) * 60.f + 60.f;
	float centerY = (x + y) * 30.f + 30.f;
	tileSprite.setPosition(std::floor(centerX - anchorX), std::floor(centerY - anchorY));

	if (tile != NULL && tile->shader == "water")
	{
		waterShader.setUniform("u_time", ellapsedTime);
		waterShader.setUniform("u_widthFactor", (float)1.0);
		waterShader.setUniform("u_textureHeight", (float)90.0);
		window->draw(tileSprite, &waterShader);
	}
	else
	{
		window->draw(tileSprite);
	}
}

void IsometricRenderer::render(Environment* environment, std::vector<BaseCharacterModel*> & characters, std::vector<AbstractSpellView<sf::Sprite*> *> spells, float deltatime)
{
	manageEvents(environment, characters);

	sf::View view = window->getView();
	if (hasCamera)
	{
		view.setSize(window->getSize().x * cameraZoom, window->getSize().y * cameraZoom);
		view.setCenter(cameraCenter);
	}
	else
	{
		// Centre de la carte (case centrale, y compris pour les cartes non carrées).
		float centerX = (environment->getWidth() - 1) / 2.f;
		float centerY = (environment->getHeight() - 1) / 2.f;
		view.setCenter((centerX - centerY) * 60.f + 60.f, (centerX + centerY) * 30.f + 30.f);
	}
	window->setView(view);

	int width = environment->getWidth();
	int height = environment->getHeight();
	int diagonals = width + height - 1;

	// Personnages vivants rangés par diagonale. Pendant un déplacement, un personnage compte
	// pour la case la plus en avant des deux (sinon la case de devant recouvrirait ses pieds).
	std::vector<std::vector<BaseCharacterModel*>> byDepth(diagonals);
	for (BaseCharacterModel * model : characters)
	{
		if (!model->isAlive())
			continue;
		int depth = (int)std::ceil(model->getInterpolatedX() + model->getInterpolatedY() - 0.001f);
		depth = std::max(0, std::min(diagonals - 1, depth));
		byDepth[depth].push_back(model);
	}

	for (int d = 0; d < diagonals; d++)
	{
		for (int x = std::max(0, d - (height - 1)); x <= std::min(width - 1, d); x++)
			drawCell(environment, x, d - x);

		std::sort(byDepth[d].begin(), byDepth[d].end(), [](BaseCharacterModel * a, BaseCharacterModel * b) {
			return a->getInterpolatedX() < b->getInterpolatedX();
		});
		for (BaseCharacterModel * model : byDepth[d])
			drawCharacter(model, deltatime);
	}

	for (int i = 0; i < spells.size(); i++)
	{
		SpellView * view = dynamic_cast<SpellView*>(spells[i]);
		sf::Sprite * spellSprite = view->getImageToDraw();
		int isoX = (view->getX() * 120 - view->getY() * 120) / 2;
		int isoY = (view->getX() * 60 + view->getY() * 60) / 2;
		spellSprite->setPosition(isoX + 60 - (spellSprite->getGlobalBounds().width / 2.0), isoY + 30 - (spellSprite->getGlobalBounds().height / 2.0));

		window->draw(*spellSprite);
	}

	// Noms, PV, PA et PM par-dessus le décor.
	for (int d = 0; d < diagonals; d++)
	{
		for (BaseCharacterModel * model : byDepth[d])
			drawCharacterOverlay(model);
	}
}

void IsometricRenderer::drawCharacter(BaseCharacterModel * m, float deltatime)
{
	CharacterView & v = getCharacterView(m);
	v.update(deltatime);
	sf::Sprite * s = v.getImageToDraw();
	sf::Texture * mask = v.getMaskToDraw();

	int isoX = (m->getInterpolatedX() * 120 - m->getInterpolatedY() * 120) / 2;
	int isoY = (m->getInterpolatedX() * 60 + m->getInterpolatedY() * 60) / 2;

	s->setPosition(isoX + 60, isoY + 30);
	bool flipped = s->getScale().x < 0;
	float scaleX = 0.4;
	float scaleY = 0.4;
	s->setScale(flipped ? -scaleX : scaleX, scaleY);

	sf::Color toApplyarmure1 = sf::Color(0, 166, 214);
	sf::Color toApplyarmure2 = sf::Color(120, 17, 17);
	sf::Color toApplycheveux = sf::Color(108, 70, 35);
	sf::Color toApplypeau = sf::Color(202, 165, 150);

	shader.setUniform("mask", *mask);
	shader.setUniform("color1", sf::Glsl::Vec4(((m->getColorNumber() == 1) ? toApplyarmure1 : toApplyarmure2)));
	shader.setUniform("color2", sf::Glsl::Vec4(toApplycheveux));
	shader.setUniform("color3", sf::Glsl::Vec4(toApplypeau));

	window->draw(*s, &shader);
}

void IsometricRenderer::drawCharacterOverlay(BaseCharacterModel * m)
{
	CharacterView & v = getCharacterView(m);
	sf::Text * pseudoTxt = v.getPseudoText();
	if (pseudoTxt->getString().getSize() == 0)
		return;

	sf::Text * paTxt = v.getPaText();
	sf::Text * pmTxt = v.getPmText();
	sf::Text * lifeTxt = v.getLifeText();
	sf::Sprite * lifeBg = v.getLifeBackground();
	sf::Sprite * paBg = v.getPaBackground();
	sf::Sprite * pmBg = v.getPmBackground();

	pseudoTxt->setCharacterSize(16);
	paTxt->setCharacterSize(12);
	pmTxt->setCharacterSize(12);
	lifeTxt->setCharacterSize(12);

	int isoX = (m->getInterpolatedX() * 120 - m->getInterpolatedY() * 120) / 2;
	int isoY = (m->getInterpolatedX() * 60 + m->getInterpolatedY() * 60) / 2;
	float height = v.getHeight();

	pseudoTxt->setPosition(isoX + 60 - (pseudoTxt->getGlobalBounds().width / 2.0), isoY + 30 - height + 5);
	pseudoTxt->setOutlineColor(sf::Color::Black);
	pseudoTxt->setOutlineThickness(1);

	float lifeBgY = isoY + 30 - height - pseudoTxt->getGlobalBounds().height - lifeBg->getGlobalBounds().height + 20;
	lifeBg->setPosition(isoX + 60 - (lifeBg->getGlobalBounds().width / 2.0), lifeBgY);
	lifeTxt->setPosition(isoX + 60 - (lifeTxt->getGlobalBounds().width / 2.0), lifeBgY + lifeBg->getGlobalBounds().height / 2.0 - lifeTxt->getGlobalBounds().height / 2.0 - 8);
	window->draw(*lifeBg);
	window->draw(*lifeTxt);

	paBg->setScale(0.75, 0.75);
	paBg->setPosition(isoX + 60 - (lifeBg->getGlobalBounds().width / 2.0) - (paBg->getGlobalBounds().width / 2.0) + 2, lifeBgY - 5 + (paBg->getGlobalBounds().height / 2.0));
	paTxt->setPosition(isoX + 60 - (lifeBg->getGlobalBounds().width / 2.0) - (paTxt->getGlobalBounds().width / 2.0) + 2, lifeBgY - 5 + (paBg->getGlobalBounds().height / 2.0) + (paBg->getGlobalBounds().height / 2.0) - (paTxt->getGlobalBounds().height / 2.0));
	window->draw(*paBg);
	window->draw(*paTxt);

	pmBg->setScale(0.75, 0.75);
	pmBg->setPosition(isoX + 60 + (lifeBg->getGlobalBounds().width / 2.0) - (pmBg->getGlobalBounds().width / 2.0), lifeBgY + (pmBg->getGlobalBounds().height / 2.0));
	pmTxt->setPosition(isoX + 60 + (lifeBg->getGlobalBounds().width / 2.0) - (pmTxt->getGlobalBounds().width / 2.0), lifeBgY - 5 + (pmBg->getGlobalBounds().height / 2.0) + (pmBg->getGlobalBounds().height / 2.0) - (pmTxt->getGlobalBounds().height / 2.0));
	window->draw(*pmBg);
	window->draw(*pmTxt);

	window->draw(*pseudoTxt);
}


CharacterView & IsometricRenderer::getCharacterView(BaseCharacterModel * model)
{
	if (characterViewsCache.find(model) == characterViewsCache.end())
	{
		characterViewsCache[model] = new CharacterView(model);
	}

	return *characterViewsCache[model];
}
