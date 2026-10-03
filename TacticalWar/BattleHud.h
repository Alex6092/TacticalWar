#pragma once

#include <functional>
#include <map>
#include <vector>
#include <TGUI/TGUI.hpp>
#include <BattleState.h>
#include <GameData.h>

// Interface du combat : ordre de jeu, barre de sorts, minuteur, journal, détails.
class BattleHud
{
public:
	BattleHud(tgui::Gui * gui, const sf::Font & font);

	std::function<void(int)> onSpellClicked;
	std::function<void()> onEndTurn;
	std::function<void(bool)> onReady;
	std::function<void()> onClose;
	// Entraînement : bouton "Rejouer" à côté de "Retour" sur l'écran de fin.
	std::function<void()> onReplay;
	// Émote choisie dans la liste (identifiant de BattleEngineLib/Emotes.h).
	std::function<void(int)> onEmote;

	void layout(const sf::Vector2u & windowSize);
	void update(float deltatime);

	// Met à jour l'affichage depuis l'état affiché du combat.
	void refresh(const tw::battle::BattleState & state, const tw::battle::GameData & data, int you,
		int hoveredFighter, int selectedSpell, bool myTurn, float remainingSeconds);

	void showMessage(const sf::String & text, const sf::Color & color, float seconds);
	void setHint(const sf::String & text);
	void log(const sf::String & line, const sf::Color & color = sf::Color(230, 230, 230));
	// Ligne du bilan de fin de combat.
	struct EndRow
	{
		sf::String name;
		int team = 0;
		bool mvp = false;
		int dealt = 0;
		int healed = 0;
		int shielded = 0;
		int kills = 0;
		sf::String badges;			// Hauts faits (noms), sous la ligne
		sf::String badgeDetails;	// Leurs descriptions, au survol
	};
	void showEnd(const sf::String & title, const sf::String & details, bool victory, const std::vector<EndRow> & rows);

	// Mode spectateur : bandeau (équipes en présence) et bouton "Quitter" permanent.
	void setSpectator(const sf::String & banner);
	// Bouton permanent pour quitter le combat (entraînement), dans le coin en bas à droite.
	void showLeaveButton(const sf::String & text);
	// Texte du bouton de l'écran de fin (ex : compte à rebours du mode réalisateur).
	void setEndButtonText(const sf::String & text);

	static sf::String fighterSummary(const tw::battle::BattleState & state, const tw::battle::GameData & data, const tw::battle::Fighter & fighter);

private:
	struct TimelineRow
	{
		tgui::Panel::Ptr panel;
		tgui::Label::Ptr name;
		tgui::Label::Ptr life;
		tgui::Label::Ptr shield;	// "+20" en bleu après les PV
		tgui::Label::Ptr stats;		// PA et PM
		tgui::Label::Ptr details;
		// Barre de vie : PV en rouge, bouclier en bleu à la suite.
		tgui::Panel::Ptr barBack;
		tgui::Panel::Ptr barLife;
		tgui::Panel::Ptr barShield;
	};

	struct SpellButton
	{
		tgui::Picture::Ptr icon;
		tgui::Label::Ptr cost;
		tgui::Label::Ptr cooldown;
		tgui::Label::Ptr key;
		tgui::Label::Ptr tooltip;
		std::string iconPath;
		std::string spellId;
	};

	tgui::Label::Ptr createLabel(unsigned int size, const sf::Color & color);
	// Barre de sorts : les sorts emportés par le combattant.
	void setSpellBar(const tw::battle::GameData & data, const tw::battle::Fighter & fighter);

	tgui::Gui * gui;
	const sf::Font & font;
	sf::Vector2u windowSize;

	tgui::Panel::Ptr timelinePanel;
	std::vector<TimelineRow> rows;
	tgui::Label::Ptr timerLabel;
	tgui::Label::Ptr messageLabel;
	float messageRemaining;
	tgui::Label::Ptr hintLabel;
	tgui::Panel::Ptr detailsPanel;
	tgui::Label::Ptr detailsLabel;
	tgui::ChatBox::Ptr logBox;
	std::vector<SpellButton> spells;
	// Classe et sorts affichés dans la barre ("classe:indices").
	std::string spellBarKey;
	tgui::Button::Ptr endTurnButton;
	tgui::Button::Ptr emoteButton;
	tgui::Panel::Ptr emotePanel;
	tgui::Button::Ptr readyButton;
	bool readyState;
	tgui::Panel::Ptr endPanel;
	sf::Vector2f endPanelSize = sf::Vector2f(520, 220);
	tgui::Button::Ptr endButton;
	tgui::Button::Ptr replayButton;

	bool spectator;
	tgui::Label::Ptr bannerLabel;
	tgui::Label::Ptr zoneLabel;
	tgui::Button::Ptr leaveButton;
	tgui::Label::Ptr cameraHelp;
};
