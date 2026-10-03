#pragma once
#include "pch.h"
#include <AbstractSpellView.h>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Animation d'un sort (projectile, impact, effet durable…) : une planche d'images au format atlas
// libGDX ("<fichier>.png" et "<fichier>.txt"), chargée une seule fois et partagée entre les vues.
// La vue est placée en coordonnées de case (fractionnaires pour un projectile en vol) ; le rendu
// la dessine au sol, sous les personnages, ou au-dessus de tout.
class SpellView : public tw::AbstractSpellView<sf::Sprite*>
{
public:
	enum class Layer { GROUND, TOP };

	SpellView(int x, int y);
	virtual ~SpellView();

	// Retourne false si l'animation est introuvable ou vide.
	bool loadAnimation(const std::string & filename);
	// Oublie les planches chargées, pour relire les fichiers modifiés. Aucune vue ne doit plus les utiliser.
	static void clearCache();
	// Charge une planche à l'avance (pas d'à-coup à son premier affichage). Retourne false si elle est introuvable ou vide.
	static bool preload(const std::string & filename);

	virtual sf::Sprite* getImageToDraw();
	virtual void update(float deltatime);

	void setPosition(float cellX, float cellY) { posX = cellX; posY = cellY; }
	float getPositionX() const { return posX; }
	float getPositionY() const { return posY; }
	// Décalage vertical dans le monde, en pixels (négatif : vers le haut).
	void setHeight(float pixels) { height = pixels; }
	float getHeight() const { return height; }

	void setScale(float value) { scale = value; }
	void setRotation(float degrees) { rotation = degrees; }
	// Point de l'image (fractions de sa largeur et de sa hauteur) placé sur la position.
	void setAnchor(float x, float y) { anchorX = x; anchorY = y; }
	void setColor(const sf::Color & value) { color = value; }
	void setLayer(Layer value) { layer = value; }
	Layer getLayer() const { return layer; }
	void setAdditive(bool value) { additive = value; }
	bool isAdditive() const { return additive; }

	void setFrameRate(float framesPerSecond) { fps = framesPerSecond > 0 ? framesPerSecond : 24.f; }
	void setLoop(bool value) { loop = value; }
	// Animation jouée une fois en entier (toujours faux pour une boucle).
	bool isFinished() const;
	// Durée d'un passage de l'animation, en secondes.
	float getAnimationDuration() const;

private:
	struct Frame
	{
		sf::IntRect rect;
		// Position du rectangle dans l'image d'origine (planches rognées).
		float offsetX = 0;
		float offsetY = 0;
		float width = 0;
		float height = 0;
	};

	struct Sheet
	{
		sf::Texture texture;
		std::vector<Frame> frames;
	};

	static std::map<std::string, std::unique_ptr<Sheet>> sheets;
	static const Sheet * loadSheet(const std::string & filename);

	const Sheet * sheet;
	sf::Sprite sprite;
	float elapsed;
	float fps;
	bool loop;
	float posX;
	float posY;
	float height;
	float scale;
	float rotation;
	float anchorX;
	float anchorY;
	sf::Color color;
	Layer layer;
	bool additive;
};
