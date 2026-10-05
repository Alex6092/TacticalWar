#pragma once

#include "Screen.h"
#include "ServerMessageListener.h"
#include <Player.h>
#include "TeamsAdminPanel.h"
#include "TournamentAdminPanel.h"
#include "LiveSessionsPanel.h"
#include "MatchesAdminPanel.h"
#include <memory>

class AdminScreen : public tw::Screen, ServerMessageListener
{
private:
	sf::Shader shader;

	sf::Font font;
	sf::Text title;
	sf::Text subtitle;

	tgui::Gui * gui;

	// Onglets : Matchs (matchs amicaux), Équipes, Tournoi, Combats.
	tgui::Tabs::Ptr tabs;
	std::unique_ptr<MatchesAdminPanel> matchesPanel;
	std::unique_ptr<TeamsAdminPanel> teamsPanel;
	std::unique_ptr<TournamentAdminPanel> tournamentPanel;
	std::unique_ptr<LiveSessionsPanel> livePanel;
	// Onglet affiché, conservé quand l'admin revient d'un combat regardé.
	static sf::String currentTab;
	void showTab(const sf::String & tab);

public:
	AdminScreen(tgui::Gui * gui);
	~AdminScreen();

	virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
	virtual void update(float deltatime);
	virtual void render(sf::RenderWindow * window);


	virtual void onMessageReceived(std::string msg);
	virtual void onDisconnected();
};

