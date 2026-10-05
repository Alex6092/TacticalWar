#pragma once

#include "OptionsPanel.h"
#include "Screen.h"
#include "ServerMessageListener.h"

namespace tw
{
	class LoginScreen : public Screen, ServerMessageListener
	{
	private:
		sf::Font font;
		sf::Text title;
		bool readyForConnect;
		bool trainingRequested;
		bool tutorialRequested = false;
		float messageDuration;
		tgui::Label::Ptr errorMsg;
		std::unique_ptr<tw::OptionsPanel> optionsPanel;
		// Client d'une autre version que le serveur (HV) : page de téléchargement de la bonne version.
		tgui::Button::Ptr downloadButton;
		std::string downloadUrl;
		tgui::Gui * gui;
		sf::Shader shader;
		sf::RectangleShape rect;

	public:
		LoginScreen(tgui::Gui * gui);
		~LoginScreen();

		virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
		virtual void update(float deltatime);
		virtual void render(sf::RenderWindow * window);

		virtual void onMessageReceived(std::string msg);
		virtual void onDisconnected();
	};
}