#pragma once

#include <functional>

#include <TGUI/TGUI.hpp>

namespace tw
{
	// Options de confort et d'accessibilité, enregistrées dans client.json : sons, mode daltonien,
	// taille du texte, alerte de fin de tour, personnages visibles à travers le décor. Ouvert depuis
	// l'écran de connexion et depuis l'aide en combat ; les changements s'appliquent tout de suite.
	class OptionsPanel
	{
	public:
		OptionsPanel(tgui::Gui * gui, const sf::Font & titleFont);

		void show();
		void hide();
		bool isVisible() const;
		// Centré dans la fenêtre.
		void layout(const sf::Vector2u & windowSize);

		// Appelé après chaque changement (l'écran met à jour ses couleurs et ses textes).
		std::function<void()> onChange;

	private:
		void refresh();
		void changed();

		tgui::Panel::Ptr panel;
		tgui::CheckBox::Ptr sound;
		tgui::CheckBox::Ptr colorblind;
		tgui::CheckBox::Ptr turnAlert;
		tgui::CheckBox::Ptr seeThrough;
		std::vector<tgui::Button::Ptr> scaleButtons;
		tgui::Label::Ptr preview;
		sf::Font textFont;
		bool refreshing = false;
	};
}
