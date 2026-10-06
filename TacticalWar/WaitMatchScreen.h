#pragma once

#include <memory>
#include <nlohmann/json.hpp>

#include "Screen.h"
#include "ServerMessageListener.h"
#include "PreferencesPanel.h"

// Attente d'un match. Hors tournoi en cours, l'équipe peut défier une équipe libre (match amical) :
// liste des équipes (DL), défi (DD), défi reçu avec compte à rebours (DI), réponse (DA), résultat (DR).
// À droite, « Mes préférences » (PreferencesPanel) prépare le prochain choix de classe.
class WaitMatchScreen : public tw::Screen, ServerMessageListener
{
private:
	sf::Font font;
	sf::Font textFont;
	sf::Text title;
	sf::Text subtitle;

	sf::Shader shader;

	tgui::Gui * gui;

	std::unique_ptr<tw::PreferencesPanel> preferences;

	// Panneau « Défier une équipe ».
	tgui::Panel::Ptr challengePanel;
	tgui::Label::Ptr challengeTitle;
	tgui::Label::Ptr challengeNote;
	tgui::ListView::Ptr teamList;
	tgui::Button::Ptr challengeButton;
	tgui::Label::Ptr challengeStatus;
	nlohmann::json teams = nlohmann::json::array();
	float listRefresh = 0;

	// Fenêtre du défi reçu.
	tgui::Panel::Ptr invitePanel;
	tgui::Label::Ptr inviteText;
	tgui::Button::Ptr acceptButton;
	tgui::Button::Ptr declineButton;
	int inviteFrom = 0;
	sf::String inviteTeam;
	float inviteRemaining = 0;

	void createChallengePanel();
	void onTeamList(const nlohmann::json & body);
	void sendChallenge();
	void answer(bool accept);
	void setStatus(const sf::String & text, const sf::Color & color);
	void layout(const sf::Vector2u & size);

public:
	WaitMatchScreen(tgui::Gui * gui);
	~WaitMatchScreen();

	virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
	virtual void update(float deltatime);
	virtual void render(sf::RenderWindow * window);


	virtual void onMessageReceived(std::string msg);
	virtual void onDisconnected();
};
