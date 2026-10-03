#pragma once

#include <functional>
#include <string>
#include <vector>
#include <TGUI/TGUI.hpp>

namespace tw
{
	// Choix des talents de tournoi : un bouton « Talents (1/2) » ouvre une fenêtre avec la liste des
	// talents (nom et description) ; un clic ajoute ou retire un talent, dans la limite du nombre gagné.
	// Utilisé par l'écran de choix de classe et les réglages de l'entraînement.
	class TalentPicker
	{
	public:
		TalentPicker(tgui::Gui * gui, const sf::Font & font);
		~TalentPicker();

		// Bouton à ajouter à l'interface (ou à un panneau) et à placer ; caché sans talent à choisir.
		tgui::Button::Ptr getButton() const { return button; }

		void setSlots(int slots);
		int getSlots() const { return slots; }
		// Talents proposés au départ (gardés s'ils sont connus et distincts, dans la limite).
		void setChosen(const std::vector<std::string> & ids);
		const std::vector<std::string> & getChosen() const { return chosen; }
		bool isComplete() const { return (int)chosen.size() >= slots; }
		void setLocked(bool locked);

		// Le choix a changé (clic du joueur).
		std::function<void()> onChange;

	private:
		void open();
		void toggle(const std::string & id);
		void refresh();

		tgui::Gui * gui;
		const sf::Font & font;
		tgui::Button::Ptr button;
		tgui::ChildWindow::Ptr window;
		tgui::Label::Ptr windowCounter;
		std::vector<tgui::Button::Ptr> talentButtons;
		std::vector<std::string> talentIds;
		int slots;
		std::vector<std::string> chosen;
		bool locked;
	};
}
