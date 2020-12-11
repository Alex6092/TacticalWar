#pragma once

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
		float messageDuration;
		tgui::Label::Ptr errorMsg;
		tgui::Gui * gui;
		sf::Shader shader;

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