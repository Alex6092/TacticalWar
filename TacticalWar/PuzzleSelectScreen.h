#pragma once

#include <vector>

#include "Screen.h"

namespace tw
{
	// Liste des énigmes tactiques (une ligne par énigme, marquée « réussie » si elle l'a été), depuis
	// les réglages de l'entraînement. « Retour » y ramène.
	class PuzzleSelectScreen : public Screen
	{
	public:
		PuzzleSelectScreen(tgui::Gui * gui);

		virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
		virtual void update(float deltatime);
		virtual void render(sf::RenderWindow * window);

	private:
		tgui::Gui * gui;
		sf::Font font;
		sf::Font textFont;
		sf::Text title;
		sf::Shader shader;
		tgui::Panel::Ptr panel;
		// Écran demandé par un bouton (changé hors des fonctions des boutons) : énigme, ou -2 pour Retour.
		int request;
	};
}
