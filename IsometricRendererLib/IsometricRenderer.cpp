#include "pch.h"
#include "IsometricRenderer.h"

#include <Palette.h>
#include <CharacterView.h>
#include <SpellView.h>
#include <TileRegistry.h>
#include "Camera.h"
#include <algorithm>
#include <math.h>
#include <iostream>

using namespace tw;

IsometricRenderer::IsometricRenderer(sf::RenderWindow * window)
{
	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/fragment.frag");
	liquidShaderReady = sf::Shader::isAvailable() && liquidShader.loadFromFile("./assets/shaders/liquid.vert", "./assets/shaders/liquid.frag");
	reflectionsAvailable = liquidShaderReady;
	seeThroughReady = sf::Shader::isAvailable() && seeThroughShader.loadFromFile("./assets/shaders/liquid.vert", "./assets/shaders/seethrough.frag");
	reflectionsDrawn = false;
	reflections = NULL;

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

IsometricRenderer::~IsometricRenderer()
{
	delete reflections;
}

namespace
{
	// Surface des liquides (texture Water_01 et ses variantes) : losange de 112 x 56 pixels,
	// centré un peu sous le centre de la case (l'eau est en contrebas).
	const sf::Vector2f LIQUID_SURFACE_OFFSET(-1.5f, 7.f);
	const sf::Vector2f LIQUID_HALF_SIZE(56.f, 28.f);
	// Les reflets sont symétriques par rapport à une ligne un peu sous le pied des personnages.
	const float REFLECTION_AXIS = 20.f;
	const float REFLECTION_STRENGTH = 0.45f;
	// Tuiles qui s'élèvent au-dessus du sol (point d'ancrage plus haut que celui d'un sol, 45) : elles
	// peuvent cacher un personnage placé derrière.
	const float TALL_TILE_ANCHOR = 55.f;
	const int MAX_HOLES = 8;
}

bool IsometricRenderer::drawSeeThrough(const sf::Sprite & sprite, int depth)
{
	sf::FloatRect bounds = sprite.getGlobalBounds();
	sf::Glsl::Vec2 centers[MAX_HOLES];
	sf::Glsl::Vec2 radii[MAX_HOLES];
	int count = 0;
	for (const Hole & hole : holes)
	{
		sf::FloatRect area(hole.center - hole.radius, hole.radius * 2.f);
		if (hole.depth >= depth || !bounds.intersects(area) || count >= MAX_HOLES)
			continue;
		centers[count] = hole.center;
		radii[count] = hole.radius;
		count++;
	}
	if (count == 0)
		return false;

	if (seeThroughReady)
	{
		seeThroughShader.setUniform("texture", sf::Shader::CurrentTexture);
		seeThroughShader.setUniformArray("u_holes", centers, MAX_HOLES);
		seeThroughShader.setUniformArray("u_radii", radii, MAX_HOLES);
		seeThroughShader.setUniform("u_count", count);
		window->draw(sprite, &seeThroughShader);
	}
	else
	{
		sf::Sprite faded(sprite);
		sf::Color color = sprite.getColor();
		color.a = (sf::Uint8)(color.a * 110 / 255);
		faded.setColor(color);
		window->draw(faded);
	}
	return true;
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

	if (tile != NULL && tile->category == TileCategory::LIQUID && liquidShaderReady && texture != &missingTexture)
	{
		bool lava = tile->shader == "lava";
		liquidShader.setUniform("texture", sf::Shader::CurrentTexture);
		liquidShader.setUniform("u_time", ellapsedTime);
		liquidShader.setUniform("u_surface", sf::Vector2f(centerX, centerY) + LIQUID_SURFACE_OFFSET);
		liquidShader.setUniform("u_half", LIQUID_HALF_SIZE);
		liquidShader.setUniform("u_shore", sf::Glsl::Vec4(
			isLiquid(environment, x + 1, y) ? 0.f : 1.f, isLiquid(environment, x - 1, y) ? 0.f : 1.f,
			isLiquid(environment, x, y + 1) ? 0.f : 1.f, isLiquid(environment, x, y - 1) ? 0.f : 1.f));
		liquidShader.setUniform("u_textureSize", sf::Vector2f(texture->getSize()));
		liquidShader.setUniform("u_resolution", sf::Vector2f(window->getSize()));
		liquidShader.setUniform("u_lava", lava ? 1.f : 0.f);
		liquidShader.setUniform("u_reflectionStrength", (reflectionsDrawn && !lava) ? REFLECTION_STRENGTH : 0.f);
		if (reflections != NULL)
			liquidShader.setUniform("u_reflection", reflections->getTexture());
		window->draw(tileSprite, &liquidShader);
	}
	else if (!(anchorY >= TALL_TILE_ANCHOR && drawSeeThrough(tileSprite, x + y)))
	{
		window->draw(tileSprite);
	}

	// Surbrillance par-dessus la case (visée d'un sort…) : losange semi-transparent et liseré.
	sf::Color overlay = colorator != NULL ? colorator->getOverlayForCell(cell) : sf::Color::Transparent;
	if (overlay.a > 0)
	{
		sf::ConvexShape diamond(4);
		diamond.setPoint(0, sf::Vector2f(centerX, centerY - 30.f));
		diamond.setPoint(1, sf::Vector2f(centerX + 60.f, centerY));
		diamond.setPoint(2, sf::Vector2f(centerX, centerY + 30.f));
		diamond.setPoint(3, sf::Vector2f(centerX - 60.f, centerY));
		diamond.setFillColor(overlay);
		diamond.setOutlineColor(sf::Color(overlay.r, overlay.g, overlay.b, (sf::Uint8)std::min(255, overlay.a * 2)));
		diamond.setOutlineThickness(-1.5f);
		window->draw(diamond);
	}
	if (colorator != NULL && colorator->isHatched(cell))
	{
		// Rayures parallèles au bord haut-gauche du losange, d'un bord à l'autre.
		sf::Vector2f left(centerX - 60.f, centerY);
		sf::Vector2f top(centerX, centerY - 30.f);
		sf::Vector2f right(centerX + 60.f, centerY);
		sf::Vector2f bottom(centerX, centerY + 30.f);
		sf::VertexArray stripes(sf::Quads);
		const sf::Color stripe(25, 20, 20, 170);
		for (int i = 1; i <= 7; i++)
		{
			float t = i / 8.f;
			sf::Vector2f from = left + (bottom - left) * t;
			sf::Vector2f to = top + (right - top) * t;
			stripes.append(sf::Vertex(from, stripe));
			stripes.append(sf::Vertex(to, stripe));
			stripes.append(sf::Vertex(to + sf::Vector2f(0.f, 2.5f), stripe));
			stripes.append(sf::Vertex(from + sf::Vector2f(0.f, 2.5f), stripe));
		}
		window->draw(stripes);
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

	reflectionsDrawn = renderReflections(environment, characters);

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

	// Animations de sorts au sol (glyphes…) : sous les personnages et le décor qui les précèdent.
	std::vector<std::vector<AbstractSpellView<sf::Sprite*>*>> groundByDepth(diagonals);
	std::vector<AbstractSpellView<sf::Sprite*>*> onTop;
	for (AbstractSpellView<sf::Sprite*> * spell : spells)
	{
		SpellView * view = dynamic_cast<SpellView*>(spell);
		if (view != NULL && view->getLayer() == SpellView::Layer::GROUND)
		{
			int depth = (int)std::lround(view->getPositionX()) + (int)std::lround(view->getPositionY());
			groundByDepth[std::max(0, std::min(diagonals - 1, depth))].push_back(spell);
		}
		else
		{
			onTop.push_back(spell);
		}
	}

	// Personnages à garder visibles derrière le décor : ellipse sur le corps (des pieds, au centre de
	// la case, jusqu'au haut du sprite).
	holes.clear();
	if (seeThrough)
	{
		for (int d = 0; d < diagonals; d++)
		{
			for (BaseCharacterModel * model : byDepth[d])
			{
				float height = std::max(60.f, getCharacterView(model).getHeight());
				sf::Vector2f feet = Camera::cellToWorld(model->getInterpolatedX(), model->getInterpolatedY());
				holes.push_back({ sf::Vector2f(feet.x, feet.y - height * 0.5f), sf::Vector2f(40.f, std::max(50.f, height * 0.6f)), d });
			}
		}
	}

	// Objets posés sur les cases (blocs de mur) : avec le décor de leur diagonale.
	std::vector<std::vector<const Prop*>> propsByDepth(diagonals);
	for (const Prop & prop : props)
	{
		int depth = (int)std::lround(prop.x) + (int)std::lround(prop.y);
		propsByDepth[std::max(0, std::min(diagonals - 1, depth))].push_back(&prop);
	}

	for (int d = 0; d < diagonals; d++)
	{
		for (int x = std::max(0, d - (height - 1)); x <= std::min(width - 1, d); x++)
			drawCell(environment, x, d - x);

		for (AbstractSpellView<sf::Sprite*> * spell : groundByDepth[d])
			drawSpell(spell);

		for (const Prop * prop : propsByDepth[d])
			drawProp(*prop);

		std::sort(byDepth[d].begin(), byDepth[d].end(), [](BaseCharacterModel * a, BaseCharacterModel * b) {
			return a->getInterpolatedX() < b->getInterpolatedX();
		});
		for (BaseCharacterModel * model : byDepth[d])
			drawCharacter(model, deltatime);
	}

	for (AbstractSpellView<sf::Sprite*> * spell : onTop)
		drawSpell(spell);

	// Noms, PV, PA et PM par-dessus le décor, et barres de vie des objets posés.
	for (int d = 0; d < diagonals; d++)
	{
		for (const Prop * prop : propsByDepth[d])
			drawPropBar(*prop);
		for (BaseCharacterModel * model : byDepth[d])
			drawCharacterOverlay(model);
	}
}

void IsometricRenderer::drawProp(const Prop & prop)
{
	if (prop.texture == NULL)
		return;
	float centerX = (prop.x - prop.y) * 60.f + 60.f;
	float centerY = (prop.x + prop.y) * 30.f + 30.f;
	sf::Sprite sprite(*prop.texture);
	sprite.setPosition(std::floor(centerX - prop.anchorX), std::floor(centerY - prop.anchorY));
	sprite.setColor(sf::Color(255, 255, 255, prop.alpha));
	if (!drawSeeThrough(sprite, (int)std::lround(prop.x) + (int)std::lround(prop.y)))
		window->draw(sprite);
}

void IsometricRenderer::drawPropBar(const Prop & prop)
{
	if (prop.maxHp <= 0)
		return;
	float centerX = (prop.x - prop.y) * 60.f + 60.f;
	float centerY = (prop.x + prop.y) * 30.f + 30.f;
	const float width = 54.f;
	const float barHeight = 7.f;
	float top = centerY - prop.barAbove;
	sf::RectangleShape back(sf::Vector2f(width + 2, barHeight + 2));
	back.setPosition(std::floor(centerX - width / 2 - 1), std::floor(top - 1));
	back.setFillColor(sf::Color(20, 20, 25, 210));
	window->draw(back);
	float ratio = std::max(0.f, std::min(1.f, (float)prop.hp / prop.maxHp));
	sf::RectangleShape fill(sf::Vector2f(width * ratio, barHeight));
	fill.setPosition(std::floor(centerX - width / 2), std::floor(top));
	fill.setFillColor(ratio > 0.5f ? sf::Color(200, 200, 210) : ratio > 0.25f ? sf::Color(240, 180, 70) : sf::Color(230, 80, 60));
	window->draw(fill);
}

bool IsometricRenderer::isLiquid(Environment * environment, int x, int y)
{
	CellData * cell = environment->getMapData(x, y);
	if (cell == NULL)
		return false;
	const TileDef * tile = TileRegistry::get().find(cell->getDisplayTile());
	return tile != NULL && tile->category == TileCategory::LIQUID;
}

bool IsometricRenderer::renderReflections(Environment * environment, std::vector<BaseCharacterModel*> & characters)
{
	if (!reflectionsAvailable)
		return false;

	// Obstacles voisins d'un liquide, à refléter. Pas de liquide : rien à faire.
	int width = environment->getWidth();
	int height = environment->getHeight();
	bool anyLiquid = false;
	std::vector<sf::Vector2i> tall;
	for (int x = 0; x < width; x++)
	{
		for (int y = 0; y < height; y++)
		{
			if (isLiquid(environment, x, y))
			{
				anyLiquid = true;
				continue;
			}
			CellData * cell = environment->getMapData(x, y);
			const TileDef * tile = TileRegistry::get().find(cell->getDisplayTile());
			if (tile == NULL || tile->category != TileCategory::OBSTACLE)
				continue;
			bool nearLiquid = false;
			for (int dx = -1; dx <= 1 && !nearLiquid; dx++)
				for (int dy = -1; dy <= 1 && !nearLiquid; dy++)
					nearLiquid = isLiquid(environment, x + dx, y + dy);
			if (nearLiquid)
				tall.push_back(sf::Vector2i(x, y));
		}
	}
	if (!anyLiquid)
		return false;

	sf::Vector2u size = window->getSize();
	if (reflections == NULL || reflections->getSize() != size)
	{
		delete reflections;
		reflections = new sf::RenderTexture();
		if (size.x == 0 || size.y == 0 || !reflections->create(size.x, size.y))
		{
			std::cout << "Reflets de l'eau désactivés (texture de rendu indisponible)." << std::endl;
			delete reflections;
			reflections = NULL;
			reflectionsAvailable = false;
			return false;
		}
	}

	reflections->setView(window->getView());
	reflections->clear(sf::Color::Transparent);

	// Décor : la tuile retournée autour d'une ligne sous le centre de sa case.
	for (const sf::Vector2i & cell : tall)
	{
		const TileDef * tile = TileRegistry::get().find(environment->getMapData(cell.x, cell.y)->getDisplayTile());
		const sf::Texture & texture = getTileTexture(*tile);
		if (&texture == &missingTexture)
			continue;
		sf::Sprite sprite(texture);
		sprite.setOrigin(tile->anchorX, tile->anchorY);
		float centerX = (cell.x - cell.y) * 60.f + 60.f;
		float centerY = (cell.x + cell.y) * 30.f + 30.f;
		sprite.setPosition(centerX, centerY + 2 * REFLECTION_AXIS);
		sprite.setScale(1.f, -1.f);
		reflections->draw(sprite);
	}

	for (BaseCharacterModel * model : characters)
	{
		if (model->isAlive())
			drawCharacterSprite(model, *reflections, true);
	}

	reflections->display();
	return true;
}

void IsometricRenderer::drawCharacter(BaseCharacterModel * m, float deltatime)
{
	CharacterView & v = getCharacterView(m);
	v.update(deltatime);
	drawCharacterSprite(m, *window, false);
}

void IsometricRenderer::drawCharacterSprite(BaseCharacterModel * m, sf::RenderTarget & target, bool mirrored)
{
	CharacterView & v = getCharacterView(m);
	sf::Sprite * s = v.getImageToDraw();
	sf::Texture * mask = v.getMaskToDraw();
	if (s == NULL || mask == NULL)
		return;

	int isoX = (m->getInterpolatedX() * 120 - m->getInterpolatedY() * 120) / 2;
	int isoY = (m->getInterpolatedX() * 60 + m->getInterpolatedY() * 60) / 2;

	// Reflet : symétrie par rapport à une ligne sous les pieds.
	s->setPosition(isoX + 60, isoY + 30 + (mirrored ? 2 * REFLECTION_AXIS : 0));
	bool flipped = s->getScale().x < 0;
	float scaleX = 0.4;
	float scaleY = 0.4;
	s->setScale(flipped ? -scaleX : scaleX, mirrored ? -scaleY : scaleY);

	int team1[3];
	int team2[3];
	palette::teamArmor(1, team1);
	palette::teamArmor(2, team2);
	sf::Color toApplyarmure1 = sf::Color(team1[0], team1[1], team1[2]);
	sf::Color toApplyarmure2 = sf::Color(team2[0], team2[1], team2[2]);
	sf::Color toApplycheveux = sf::Color(108, 70, 35);
	sf::Color toApplypeau = sf::Color(202, 165, 150);

	// Apparence choisie : variante de la couleur d'équipe et couleur des cheveux.
	if (m->hasAppearanceColors())
	{
		const int * armor = m->getArmorColor();
		const int * hair = m->getHairColor();
		toApplyarmure1 = toApplyarmure2 = sf::Color(armor[0], armor[1], armor[2]);
		toApplycheveux = sf::Color(hair[0], hair[1], hair[2]);
	}

	shader.setUniform("mask", *mask);
	shader.setUniform("color1", sf::Glsl::Vec4(((m->getColorNumber() == 1) ? toApplyarmure1 : toApplyarmure2)));
	shader.setUniform("color2", sf::Glsl::Vec4(toApplycheveux));
	shader.setUniform("color3", sf::Glsl::Vec4(toApplypeau));

	target.draw(*s, &shader);
	if (mirrored)
		s->setScale(flipped ? -scaleX : scaleX, scaleY);
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

	pseudoTxt->setCharacterSize((unsigned int)std::lround(16 * textScale));
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
	// PV centrés sur le cœur d'après leurs limites réelles (le « 1 » de Neuropol a une marge à gauche),
	// avec un contour sombre ; dessinés après l'étoile des PA et le carré des PM, qui ne les cachent plus.
	sf::FloatRect lifeBounds = lifeTxt->getLocalBounds();
	lifeTxt->setOutlineColor(sf::Color(40, 0, 0));
	lifeTxt->setOutlineThickness(1);
	lifeTxt->setPosition(std::round(isoX + 60 - lifeBounds.width / 2.0f - lifeBounds.left),
		std::round(lifeBgY + lifeBg->getGlobalBounds().height / 2.0f - 4 - lifeBounds.height / 2.0f - lifeBounds.top));
	window->draw(*lifeBg);

	// Étoile des PA un peu à gauche du cœur, pour ne pas toucher le premier chiffre des PV.
	const float paShift = 6;
	paBg->setScale(0.75, 0.75);
	paBg->setPosition(isoX + 60 - (lifeBg->getGlobalBounds().width / 2.0) - (paBg->getGlobalBounds().width / 2.0) + 2 - paShift, lifeBgY - 5 + (paBg->getGlobalBounds().height / 2.0));
	paTxt->setPosition(isoX + 60 - (lifeBg->getGlobalBounds().width / 2.0) - (paTxt->getGlobalBounds().width / 2.0) + 2 - paShift, lifeBgY - 5 + (paBg->getGlobalBounds().height / 2.0) + (paBg->getGlobalBounds().height / 2.0) - (paTxt->getGlobalBounds().height / 2.0));
	window->draw(*paBg);
	window->draw(*paTxt);

	pmBg->setScale(0.75, 0.75);
	pmBg->setPosition(isoX + 60 + (lifeBg->getGlobalBounds().width / 2.0) - (pmBg->getGlobalBounds().width / 2.0), lifeBgY + (pmBg->getGlobalBounds().height / 2.0));
	pmTxt->setPosition(isoX + 60 + (lifeBg->getGlobalBounds().width / 2.0) - (pmTxt->getGlobalBounds().width / 2.0), lifeBgY - 5 + (pmBg->getGlobalBounds().height / 2.0) + (pmBg->getGlobalBounds().height / 2.0) - (pmTxt->getGlobalBounds().height / 2.0));
	window->draw(*pmBg);
	window->draw(*pmTxt);
	window->draw(*lifeTxt);

	// Bouclier : écusson bleu avec sa valeur, posé sur le haut du cœur (il absorbe les dégâts en premier).
	if (m->getCurrentShield() > 0 && m->getCurrentLife() > 0)
	{
		sf::Sprite * shieldBg = v.getShieldBackground();
		sf::Text * shieldTxt = v.getShieldText();
		shieldTxt->setCharacterSize(12);
		shieldTxt->setFillColor(sf::Color::White);
		shieldTxt->setOutlineColor(sf::Color(20, 45, 95));
		shieldTxt->setOutlineThickness(1);
		shieldBg->setScale(0.62f, 0.62f);
		float shieldX = isoX + 60 - shieldBg->getGlobalBounds().width / 2.0f;
		float shieldY = lifeBgY - shieldBg->getGlobalBounds().height * 0.72f;
		shieldBg->setPosition(shieldX, shieldY);
		sf::FloatRect textBounds = shieldTxt->getLocalBounds();
		shieldTxt->setPosition(isoX + 60 - textBounds.width / 2.0f - textBounds.left,
			shieldY + shieldBg->getGlobalBounds().height * 0.46f - textBounds.height / 2.0f - textBounds.top);
		window->draw(*shieldBg);
		window->draw(*shieldTxt);
	}

	window->draw(*pseudoTxt);

	// Mode daltonien : symbole d'équipe devant le nom (rond pour l'équipe 1, triangle pour l'équipe 2).
	if (palette::colorblind() && (m->getColorNumber() == 1 || m->getColorNumber() == 2))
	{
		palette::Rgba color = palette::color(palette::teamRole(m->getColorNumber(), palette::Role::TEAM1_ARMOR));
		sf::CircleShape symbol(6.f, m->getColorNumber() == 1 ? 24 : 3);
		symbol.setOrigin(6.f, 6.f);
		symbol.setFillColor(sf::Color(color.r, color.g, color.b));
		symbol.setOutlineColor(sf::Color::Black);
		symbol.setOutlineThickness(1.5f);
		sf::FloatRect name = pseudoTxt->getGlobalBounds();
		symbol.setPosition(name.left - 11.f, name.top + name.height / 2.f);
		window->draw(symbol);
	}
}


void IsometricRenderer::drawSpell(AbstractSpellView<sf::Sprite*> * spell)
{
	sf::Sprite * sprite = spell->getImageToDraw();
	if (sprite == NULL)
		return;

	// Position en coordonnées de case (fractionnaires en vol), plus la hauteur dans le monde.
	SpellView * view = dynamic_cast<SpellView*>(spell);
	sf::RenderStates states;
	if (view != NULL)
	{
		sf::Vector2f world = Camera::cellToWorld(view->getPositionX(), view->getPositionY());
		sprite->setPosition(world.x, world.y + view->getHeight());
		if (view->isAdditive())
			states.blendMode = sf::BlendAdd;
	}
	else
	{
		sf::Vector2f world = Camera::cellToWorld((float)spell->getX(), (float)spell->getY());
		sprite->setPosition(world.x - sprite->getGlobalBounds().width / 2.f, world.y - sprite->getGlobalBounds().height / 2.f);
	}
	window->draw(*sprite, states);
}

CharacterView & IsometricRenderer::getCharacterView(BaseCharacterModel * model)
{
	if (characterViewsCache.find(model) == characterViewsCache.end())
	{
		characterViewsCache[model] = new CharacterView(model);
	}

	return *characterViewsCache[model];
}
