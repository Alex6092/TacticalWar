#pragma once

#include "Screen.h"
#include "ServerMessageListener.h"

class WaitMatchScreen : public tw::Screen, ServerMessageListener
{
private:
	sf::Font font;
	sf::Text title;
	sf::Text subtitle;

	tgui::Label::Ptr matchPanelTitle;
	tgui::ScrollablePanel::Ptr m_matchListpanel;
	//static void scrollPanel(tgui::Panel::Ptr panel, int value);
	//static int previousScrollbarValue;


	sf::Shader shader;

	tgui::Gui * gui;

public:
	WaitMatchScreen(tgui::Gui * gui);
	~WaitMatchScreen();

	virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
	virtual void update(float deltatime);
	virtual void render(sf::RenderWindow * window);


	virtual void onMessageReceived(std::string msg);
	virtual void onDisconnected();
};
