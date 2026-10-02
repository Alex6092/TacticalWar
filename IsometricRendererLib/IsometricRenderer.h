#pragma once

#include <AbstractRenderer.h>
#include <AbstractSpellView.h>
#include <Environment.h>
#include <BaseCharacterModel.h>
#include "CellColorator.h"

#include <map>
#include <string>
#include <vector>
#include <SFML\Graphics.hpp>
#include <iostream>
#include <SFML/Graphics/Shader.hpp>

namespace tw
{
	class CharacterView;
	struct TileDef;

	// Rendu isométrique : cases de 120 x 60 pixels, la case (x, y) a son centre en
	// ((x - y) * 60 + 60, (x + y) * 30 + 30) dans le repère du monde.
	// Les tuiles et les personnages sont dessinés de l'arrière vers l'avant (diagonale x + y),
	// pour qu'un arbre ou un rocher situé devant un personnage le masque.
	class IsometricRenderer : public AbstractRenderer<sf::Sprite>
	{
		bool hasFocus;
		bool forcedFocus;

		sf::RenderWindow * window;
		CellColorator * colorator;

		std::map<BaseCharacterModel*, CharacterView*> characterViewsCache;
		CharacterView & getCharacterView(BaseCharacterModel * model);

		void manageEvents(Environment * environment, std::vector<BaseCharacterModel*> & characters);

		// Textures des tuiles (registre TileRegistry), chargées à la demande.
		std::map<std::string, sf::Texture> tileTextures;
		sf::Texture missingTexture;
		const sf::Texture & getTileTexture(const TileDef & tile);
		sf::Sprite tileSprite;

		void drawCell(Environment * environment, int x, int y);
		void drawCharacter(BaseCharacterModel * model, float deltatime);
		void drawCharacterSprite(BaseCharacterModel * model, sf::RenderTarget & target, bool mirrored);
		void drawCharacterOverlay(BaseCharacterModel * model);
		void drawSpell(AbstractSpellView<sf::Sprite*> * spell);

		// Liquides (eau, lave) : shader animé ; reflets des personnages et du décor voisin dans l'eau,
		// dessinés à l'envers dans une texture de la taille de la fenêtre.
		bool isLiquid(Environment * environment, int x, int y);
		bool renderReflections(Environment * environment, std::vector<BaseCharacterModel*> & characters);
		sf::Shader liquidShader;
		bool liquidShaderReady;
		sf::RenderTexture * reflections;
		bool reflectionsAvailable;
		bool reflectionsDrawn;

		sf::Vector2i screenCoordinatesToIsoGridCoordinates(float worldX, float worldY);

		sf::Shader shader;

		float ellapsedTime;

		// Caméra (voir Camera) : sans caméra, la carte est centrée dans la vue courante.
		bool hasCamera;
		sf::Vector2f cameraCenter;
		float cameraZoom;

	public:
		IsometricRenderer(sf::RenderWindow * window);
		~IsometricRenderer();
		inline void modifyWindow(sf::RenderWindow * newWindow) { this->window = newWindow; }
		virtual void render(Environment* environment, std::vector<BaseCharacterModel*> & characters, std::vector<AbstractSpellView<sf::Sprite*> *> spells, float deltatime);

		// Oublie les textures chargées (après une modification du jeu de tuiles).
		void reloadTiles();

		// Centre (repère du monde) et facteur de zoom de la vue (> 1 : vue plus large).
		void setCamera(const sf::Vector2f & center, float zoom)
		{
			hasCamera = true;
			cameraCenter = center;
			cameraZoom = zoom;
		}

		void setColorator(CellColorator * colorator)
		{
			this->colorator = colorator;
		}

		inline void forceFocus()
		{
			forcedFocus = true;
			hasFocus = true;
		}

		inline void forceUnfocus()
		{
			forcedFocus = true;
			hasFocus = false;
		}

		void ellapseTime(float deltatime)
		{
			ellapsedTime += deltatime;
		}
	};
}
