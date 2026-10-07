#pragma once

#include <memory>
#include <string>
#include <vector>
#include <TGUI/TGUI.hpp>

#include "SpellPicker.h"
#include "TalentPicker.h"

namespace tw
{
	// « Mes préférences », sur l'écran d'attente : classe préférée, ses 4 sorts, talents de tournoi par
	// ordre de préférence et apparence. Enregistrées dans client.json à chaque changement. L'écran de
	// choix de classe s'ouvre ensuite sur la classe préférée avec ces choix, sans rien verrouiller : on
	// gagne du temps, et une classe interdite par le bannissement reste interdite.
	class PreferencesPanel
	{
	public:
		// Nombre de talents rangés par préférence (le match n'en garde que le nombre gagné).
		static const int TALENT_SLOTS = 3;

		PreferencesPanel(tgui::Gui * gui, const sf::Font & titleFont, const sf::Font & textFont);

		tgui::Panel::Ptr getPanel() const { return panel; }
		void layout(float width, float height);
		// Apparences débloquées mises à jour (message PA) : pastilles à jour.
		void refreshAppearances();

	private:
		void showClass(int classId);
		void save(const sf::String & what);

		tgui::Gui * gui;
		const sf::Font & textFont;
		tgui::Panel::Ptr panel;
		tgui::Label::Ptr title;
		tgui::Label::Ptr note;
		tgui::Label::Ptr classTitle;
		tgui::ComboBox::Ptr classBox;
		std::vector<int> classIds;	// Dans l'ordre de la liste (0 : aucune préférence)
		tgui::Label::Ptr spellsTitle;
		std::unique_ptr<SpellPicker> spellPicker;
		std::unique_ptr<TalentPicker> talentPicker;
		tgui::Label::Ptr appearanceTitle;
		std::vector<tgui::Button::Ptr> swatches;
		std::vector<std::string> swatchIds;
		tgui::Label::Ptr status;
		int shownClass = 0;
		bool refreshing = false;
	};
}
