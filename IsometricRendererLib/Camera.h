#pragma once

#include <SFML/Graphics.hpp>

namespace tw
{
	class IsometricRenderer;

	// Caméra de la vue isométrique : zoom à la molette (centré sur la souris), déplacement
	// au clic droit maintenu, suivi en douceur d'une case (personnage actif).
	class Camera
	{
	public:
		Camera();

		// Recentre la caméra sur la carte, au zoom 1.
		void reset(int mapWidth, int mapHeight);
		// Recentre la caméra et choisit le zoom pour que toute la carte tienne dans la vue.
		void fit(int mapWidth, int mapHeight, const sf::Vector2u & viewSize, float margin = 1.05f);

		// Traite un événement de la fenêtre. Renvoie true si la caméra l'a consommé.
		bool handleEvent(const sf::Event & event, const sf::RenderWindow & window);
		void update(float deltatime);

		// Suivi : la caméra glisse vers la case donnée (coordonnées de grille, interpolées).
		void setFollowing(bool following) { this->following = following; }
		bool isFollowing() const { return following; }
		void followCell(float cellX, float cellY);
		// Centre immédiatement la vue sur une case.
		void centerOn(float cellX, float cellY);

		void apply(IsometricRenderer & renderer) const;

		float getZoom() const { return zoom; }
		const sf::Vector2f & getCenter() const { return center; }

		// Centre d'une case de la grille dans le repère du monde.
		static sf::Vector2f cellToWorld(float cellX, float cellY);

		static const float MIN_ZOOM;
		static const float MAX_ZOOM;

	private:
		void clampCenter();

		int mapWidth;
		int mapHeight;
		sf::Vector2f center;
		sf::Vector2f target;
		float zoom;
		bool following;
		bool dragging;
		sf::Vector2i lastMouse;
	};
}
