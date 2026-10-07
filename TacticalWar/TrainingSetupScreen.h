#pragma once

#include <memory>
#include <vector>

#include "Screen.h"
#include "SpellPicker.h"
#include "TalentPicker.h"

namespace tw
{
	// Réglages de l'entraînement hors ligne (depuis l'écran de connexion, sans serveur) :
	// format, classes, carte et difficulté, puis "Jouer".
	class TrainingSetupScreen : public Screen
	{
	public:
		TrainingSetupScreen(tgui::Gui * gui);

		virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
		virtual void update(float deltatime);
		virtual void render(sf::RenderWindow * window);

	private:
		enum class Request { NONE, PLAY, BACK, TUTORIAL, PUZZLES };

		tgui::ComboBox::Ptr addRow(const sf::String & text);
		tgui::ComboBox::Ptr addClassRow(const sf::String & text, int selected);
		static int selectedId(const tgui::ComboBox::Ptr & box);
		void refresh();
		void save();
		// Jouer : sorts complets (classe choisie) et talents complets.
		void refreshPlay();

		tgui::Gui * gui;
		sf::Font font;
		sf::Text title;
		sf::Shader shader;

		tgui::Panel::Ptr panel;
		std::vector<tgui::Label::Ptr> labels;
		tgui::ComboBox::Ptr format;
		tgui::ComboBox::Ptr playerClass;
		tgui::ComboBox::Ptr allyClass;
		tgui::ComboBox::Ptr enemyClasses[2];
		tgui::ComboBox::Ptr map;
		tgui::ComboBox::Ptr mode;
	tgui::ComboBox::Ptr bonuses;
	tgui::ComboBox::Ptr shrink;
		tgui::ComboBox::Ptr difficulty;
		tgui::ComboBox::Ptr talentCount;
		tgui::Label::Ptr description;
		std::unique_ptr<SpellPicker> spellPicker;
		tgui::Label::Ptr randomSpells;
		std::unique_ptr<TalentPicker> talentPicker;
		tgui::Button::Ptr playButton;
		int spellClassId;
		bool spellsChanged;
		bool talentsChanged;
		Request request;
	};
}
