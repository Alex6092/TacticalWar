#pragma once

#include "Screen.h"
#include "ServerMessageListener.h"
#include <Player.h>
#include "TeamsAdminPanel.h"
#include "TournamentAdminPanel.h"
#include "LiveSessionsPanel.h"
#include <memory>

class AdminScreen : public tw::Screen, ServerMessageListener
{
private:
	bool readyForCreate;

	sf::Shader shader;

	sf::Font font;
	sf::Text title;
	sf::Text subtitle;

	tgui::Label::Ptr matchPanelTitle;
	tgui::Label::Ptr versus;
	tgui::Label::Ptr nameMatch;
	tgui::Label::Ptr team1Choice;
	tgui::Label::Ptr team2Choice;
	tgui::Label::Ptr matchCreate;
	tgui::Label::Ptr matchEnd;
	tgui::ScrollablePanel::Ptr m_matchListpanel;
	tgui::ScrollablePanel::Ptr m_matchListCreate;
	tgui::ScrollablePanel::Ptr m_matchListEnd;
	tgui::ListBox::Ptr listTeam1;
	tgui::ListBox::Ptr listTeam2;
	tgui::ListBox::Ptr listMatchCreate;
	tgui::Button::Ptr createMatch;
	tgui::EditBox::Ptr matchName;
	//static void scrollPanel(tgui::Panel::Ptr panel, int value);
	//static int previousScrollbarValue;


	tgui::Gui * gui;

	// Onglets : "Matchs" (création manuelle de matchs) et "Équipes".
	tgui::Tabs::Ptr tabs;
	tgui::Group::Ptr matchesGroup;
	std::unique_ptr<TeamsAdminPanel> teamsPanel;
	std::unique_ptr<TournamentAdminPanel> tournamentPanel;
	std::unique_ptr<LiveSessionsPanel> livePanel;
	// Onglet affiché, conservé quand l'admin revient d'un combat regardé.
	static sf::String currentTab;
	void showTab(const sf::String & tab);

	void updateListTeam(tgui::ListBox::Ptr listTeam);

public:
	AdminScreen(tgui::Gui * gui);
	~AdminScreen();

	virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
	virtual void update(float deltatime);
	virtual void render(sf::RenderWindow * window);


	virtual void onMessageReceived(std::string msg);
	virtual void onDisconnected();
};

