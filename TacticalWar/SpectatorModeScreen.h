#pragma once

#include <memory>

#include "Screen.h"
#include "ServerMessageListener.h"
#include "LiveSessionsPanel.h"
#include "ReplaysPanel.h"

// Écran du mode spectateur : liste des combats en cours, "Regarder", et mode réalisateur
// (enchaîne automatiquement les combats les plus serrés, pour un écran projeté).
class SpectatorModeScreen : public tw::Screen, ServerMessageListener
{
private:
	sf::Font font;
	sf::Text title;
	sf::Text subtitle;

	tgui::Gui * gui;
	std::unique_ptr<LiveSessionsPanel> sessionsPanel;
	std::unique_ptr<ReplaysPanel> replaysPanel;
	tgui::Tabs::Ptr tabs;
	// Onglet affiché ("En direct" ou "Rediffusions"), conservé au retour d'un combat.
	static sf::String currentTab;
	void showTab(const sf::String & tab);
	tgui::CheckBox::Ptr directorBox;
	tgui::Label::Ptr directorHelp;

	sf::Shader shader;

	float sinceRefresh;
	float watchPending;		// Délai d'attente de la réponse à SW (secondes), 0 si aucune demande

	static bool directorMode;

	void watch(int session);

public:
	SpectatorModeScreen(tgui::Gui * gui);
	~SpectatorModeScreen();

	static bool isDirectorMode() { return directorMode; }

	virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
	virtual void update(float deltatime);
	virtual void render(sf::RenderWindow * window);

	virtual void onMessageReceived(std::string msg);
	virtual void onDisconnected();
};
