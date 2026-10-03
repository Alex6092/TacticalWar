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
// - à droite : les caractéristiques, la description et le passif ;
// - en bas : le bouton de verrouillage, actif quand les sorts et les talents sont complets.
class ClassSelectionScreen : public tw::Screen, ServerMessageListener
{
public:
	// selection : contenu du message HC du serveur ({"talents": nombre de talents à choisir}).
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

	bool readyToLock;
	bool locked;
};
