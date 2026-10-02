#pragma once

#include <TGUI/TGUI.hpp>
#include <nlohmann/json.hpp>

// Onglet "Tournoi" de l'écran admin : création et paramétrage d'un tournoi, démarrage,
// suivi des matchs (lancés automatiquement par le serveur) et actions d'arbitrage.
class TournamentAdminPanel
{
public:
	TournamentAdminPanel(tgui::Gui * gui, const sf::Font & font);

	void setVisible(bool visible);
	void layout(const sf::Vector2u & windowSize, float top);

	void onTournamentList(const nlohmann::json & body);
	void onTournamentState(const nlohmann::json & body);
	void onAck(const nlohmann::json & body);
	// Combats en cours (message SL) : pour "Regarder" le match sélectionné.
	void onSessionList(const nlohmann::json & body);
	// Équipes disponibles (liste TL de l'onglet Équipes).
	void setTeams(const nlohmann::json & teams);

private:
	tgui::Label::Ptr createLabel(const sf::String & text, unsigned int size = 14);
	tgui::EditBox::Ptr createNumberBox(const sf::String & value);
	tgui::Button::Ptr createButton(const sf::String & text);

	void newTournament();
	void selectTournament(int id);
	void refreshForm();
	void refreshMatches();
	void refreshFormatOptions();
	nlohmann::json readSettings() const;
	std::vector<int> selectedTeamIds() const;
	void save();
	void send(const std::string & op, const nlohmann::json & body);
	int selectedMatchId() const;
	void forceWinner(bool teamA);

	tgui::Gui * gui;
	const sf::Font & font;
	tgui::Group::Ptr group;

	tgui::ListView::Ptr tournamentList;
	tgui::Button::Ptr newButton;

	tgui::Panel::Ptr form;
	tgui::EditBox::Ptr name;
	tgui::ComboBox::Ptr format;
	tgui::Label::Ptr poolCountLabel;
	tgui::EditBox::Ptr poolCount;
	tgui::Label::Ptr qualifiersLabel;
	tgui::EditBox::Ptr qualifiers;
	tgui::CheckBox::Ptr thirdPlace;
	tgui::CheckBox::Ptr grandFinalReset;
	tgui::Label::Ptr swissRoundsLabel;
	tgui::EditBox::Ptr swissRounds;
	tgui::Label::Ptr topCutLabel;
	tgui::EditBox::Ptr topCut;
	tgui::ListView::Ptr teamList;
	tgui::Button::Ptr saveButton;
	tgui::Button::Ptr startButton;
	tgui::Button::Ptr deleteButton;

	tgui::Label::Ptr header;
	tgui::ListView::Ptr matchList;
	tgui::Button::Ptr pauseButton;
	tgui::Button::Ptr winAButton;
	tgui::Button::Ptr winBButton;
	tgui::Button::Ptr stopButton;
	tgui::Button::Ptr replayButton;
	tgui::Button::Ptr webButton;
	tgui::Button::Ptr watchButton;
	nlohmann::json liveSessions = nlohmann::json::array();
	tgui::Label::Ptr standings;
	tgui::Label::Ptr status;

	nlohmann::json tournaments;
	nlohmann::json state;
	nlohmann::json teams;
	int selectedId;
	bool initialSelectionDone = false;
	std::vector<int> matchIds;
	std::vector<int> teamRowIds;
};
