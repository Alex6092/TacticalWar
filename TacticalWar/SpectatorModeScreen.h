#pragma once

#include "Screen.h"
#include "ServerMessageListener.h"

class SpectatorModeScreen : public tw::Screen, ServerMessageListener
{
private:
	sf::Font font;
	sf::Text title;
	sf::Text subtitle;

	tgui::Label::Ptr matchPanelTitle;
	tgui::ScrollablePanel::Ptr m_matchListpanel;
	//static void scrollPanel(tgui::Panel::Ptr panel, int value);
	//static int previousScrollbarValue;


	tgui::Gui * gui;

	sf::Shader shader;

public:
	SpectatorModeScreen(tgui::Gui * gui);
	~SpectatorModeScreen();

	virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
	virtual void update(float deltatime);
	virtual void render(sf::RenderWindow * window);


	virtual void onMessageReceived(std::string msg);
	virtual void onDisconnected();
};

