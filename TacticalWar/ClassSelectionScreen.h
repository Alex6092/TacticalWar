#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Screen.h"
#include "ServerMessageListener.h"
#include "SpellPicker.h"
#include "TalentPicker.h"
#include <CharacterView.h>

class PictureCharacterView;

// Choix de la classe, des 4 sorts emportés et des talents de tournoi avant un match. L'écran est fait
// de blocs placés selon la taille de la fenêtre (layout) :
// - à gauche : les sorts de la classe (SpellPicker) et le bouton des talents (TalentPicker) ;
// - au centre : la carte de la classe, son personnage animé et les flèches pour changer de classe ;
// - à droite : les caractéristiques, la description et le passif, puis le coéquipier (classe
//   regardée ou verrouillée, combinaisons possibles entre vos deux classes) ;
// - en bas : le bouton de verrouillage, actif quand les sorts et les talents sont complets.
// Certains matchs de tournoi commencent par un bannissement : le même écran sert à choisir la classe
// interdite à l'adversaire (bandeau au-dessus du bouton, qui devient « Bannir cette classe »).
// Coéquipier absent : après son propre choix, le joueur choisit aussi le personnage de son coéquipier,
// qu'il jouera pendant le combat (seconde étape, PC{..., "teammate": true}), puis l'écran revient à
// sa propre classe.
// Brouillon : la classe affichée, les sorts, les talents et l'apparence en cours sont envoyés au
// serveur à chaque changement (PV) ; sans verrouillage, ils sont retenus à la fin du délai, dont le
// compte à rebours est affiché à côté du bouton.
class ClassSelectionScreen : public tw::Screen, ServerMessageListener
{
public:
	// selection : contenu du message HC du serveur ({"talents": nombre de talents à choisir,
	// "ban": secondes de bannissement restantes, "seconds": secondes restantes pour choisir}).
	ClassSelectionScreen(tgui::Gui * gui, const std::string & selection = std::string());
	~ClassSelectionScreen();

	virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
	virtual void update(float deltatime);
	virtual void render(sf::RenderWindow * window);

	virtual void onMessageReceived(std::string msg);
	virtual void onDisconnected();

private:
	void showClass(int index);
	void layout(const sf::Vector2u & size);
	void refreshLock();
	void setLockText(const sf::String & text);
	void refreshBan();
	void refreshMate();
	void updateMatePick();
	// Fin du choix pour le coéquipier : retour à sa propre classe, ses sorts, talents et apparence.
	void showOwnChoice();
	// Envoie le brouillon (PV) s'il a changé.
	void sendDraft();
	void refreshTimer();
	sf::String classLabel(int classId) const;
	int currentClassId() const;

	tgui::Gui * gui;
	sf::Font font;
	sf::Font textFont;
	sf::Text title;
	sf::Text subtitle;
	sf::Shader shader;
	sf::Vector2u windowSize;

	std::vector<tw::BaseCharacterModel*> classesInstances;
	tw::CharacterView * characterView;
	int indexClass;
	float orientationTime;
	int orientation;

	std::unique_ptr<tw::SpellPicker> spellPicker;
	std::unique_ptr<tw::TalentPicker> talentPicker;
	tgui::Button::Ptr previousButton;
	tgui::Button::Ptr nextButton;
	tgui::Button::Ptr lockButton;
	tgui::Picture::Ptr preview;
	tgui::Picture::Ptr classIcon;
	std::shared_ptr<PictureCharacterView> characterPicture;
	tgui::Label::Ptr className;
	tgui::Panel::Ptr spellsPanel;
	tgui::Panel::Ptr statsPanel;
	tgui::Label::Ptr statsLabel;
	tgui::Panel::Ptr descriptionPanel;
	tgui::Label::Ptr descriptionLabel;
	tgui::Label::Ptr banLabel;
	// Compte à rebours du choix, ou message d'un refus du serveur (ER) pendant quelques secondes.
	tgui::Label::Ptr timerLabel;
	float selectionRemaining = -1;	// Négatif : inconnu
	sf::String notice;
	float noticeTime = 0;
	tgui::Panel::Ptr matePanel;
	tgui::Label::Ptr mateTitle;
	tgui::Label::Ptr mateStatus;
	tgui::Label::Ptr mateCombos;
	// Apparence : une pastille par apparence (verrouillées en grisé), nom ou condition de déblocage.
	std::vector<tgui::Button::Ptr> appearanceButtons;
	tgui::Label::Ptr appearanceLabel;
	std::string selectedAppearance;
	sf::String appearanceHint;
	int myTeam = 1;
	void refreshAppearances();

	bool readyToLock;
	bool locked;

	// Bannissement : phase en cours, puis classes interdites (0 : aucune).
	bool banMode = false;
	bool banDone = false;
	bool banRequested = false;
	bool banSent = false;
	float banRemaining = 0;
	int banSecondsShown = -1;
	int bannedClass = 0;		// Interdite par notre équipe à l'adversaire
	int forbiddenClass = 0;		// Interdite à notre équipe par l'adversaire

	// Coéquipier (message PT du serveur) et dernière classe regardée envoyée (PV).
	bool mateKnown = false;
	sf::String mateName;
	int mateClass = 0;
	int mateViewing = 0;
	bool mateLocked = false;
	bool matePresent = false;
	// Le « coéquipier » est le second personnage d'un joueur seul dans son équipe.
	bool mateStandIn = false;
	std::string mateAppearance;
	// Choix verrouillé du joueur (PO), rappelé pendant le choix pour le second personnage, et nom du
	// coéquipier qui l'a fait pendant son absence (vide : lui-même).
	int myClass = 0;
	std::vector<int> mySpells;
	std::vector<std::string> myTalents;
	std::string myAppearance;
	sf::String chosenBy;
	// Dernier brouillon envoyé (PV).
	std::string draftSent;
	// Seconde étape : choix pour le coéquipier absent (envoyé, puis verrouillé).
	bool forMate = false;
	bool mateSent = false;
};
