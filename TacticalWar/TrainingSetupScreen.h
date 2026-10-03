#pragma once

#include "Screen.h"

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
		enum class Request { NONE, PLAY, BACK };

		tgui::ComboBox::Ptr addRow(const sf::String & text);
		tgui::ComboBox::Ptr addClassRow(const sf::String & text, int selected);
		static int selectedId(const tgui::ComboBox::Ptr & box);
		void refresh();
		void save();

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
		tgui::ComboBox::Ptr difficulty;
		tgui::Label::Ptr description;
		Request request;
	};
}
