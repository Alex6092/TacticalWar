#pragma once

#include <TGUI/TGUI.hpp>

namespace tw
{
	// Aide des commandes, en combat (bouton « ? » ou touche H) : les commandes d'un côté, les règles à
	// retenir de l'autre. Se ferme par son bouton, par H ou par Échap.
	class HelpPanel
	{
	public:
		HelpPanel(tgui::Gui * gui, const sf::Font & titleFont);

		void toggle();
		void hide();
		bool isVisible() const;
		// Centré dans la fenêtre.
		void layout(const sf::Vector2u & windowSize);

	private:
		tgui::Panel::Ptr panel;
		sf::Font textFont;
	};
}
