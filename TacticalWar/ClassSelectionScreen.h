#pragma once

#include "Screen.h"
#include "ServerMessageListener.h"
#include <CharacterView.h>

class ClassSelectionScreen : public tw::Screen, ServerMessageListener
{
private:
	sf::Font font;
	sf::Font font2;
	sf::Text title;
	sf::Text subtitle;
	sf::Text subsubtitle;
	sf::Text stats;

	tgui::Label::Ptr matchPanelTitle;
	tgui::ScrollablePanel::Ptr m_matchListpanel;
	tgui::ScrollablePanel::Ptr warriorpanel;
	tgui::ScrollablePanel::Ptr statsPanel;
	tgui::ScrollablePanel::Ptr descriptionPanel;
	//static void scrollPanel(tgui::Panel::Ptr panel, int value);
	//static int previousScrollbarValue;

	sf::Shader shader;

	tgui::Gui * gui;

	std::vector<tw::BaseCharacterModel*> classesInstances;
	tw::CharacterView * characterView;
	int indexClass;

	tgui::Label::HorizontalAlignment Horizontale_AlignementRight = tgui::Label::HorizontalAlignment::Right;
	
	int frontsize2 = 20;
	int PositionOfPanelX = 230;
	int PositionOfPanelY = 250;
	int SizeOfPanelX = 1500;
	int SizeOfPanelY = 700;

	int PositionOfCardX = 800;
	int PositionOfCardY = 300;
	int SizeOfCardX = 400;
	int SizeOfCardY = 450;

	int DescriptionX = PositionOfCardX + 500;
	int DescriptionY = PositionOfCardY + 290;

	int sizeTextX = 450;
	int sizeTextY = 25;

	void setClassView();
	// Sorts emportés : clic sur un sort pour l'ajouter ou le retirer (4 sur les 6 de la classe).
	void toggleSpell(int index);
	void refreshSpells();

	std::vector<int> chosenSpells;
	bool locked;

	sf::Vector2u windowSize;
	float ellapsedTime;
	int orientation;

	bool readyToLock;

public:
	ClassSelectionScreen(tgui::Gui * gui);
	~ClassSelectionScreen();

	virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
	virtual void update(float deltatime);
	virtual void render(sf::RenderWindow * window);


	virtual void onMessageReceived(std::string msg);
	virtual void onDisconnected();

	
	void setIdxClass(int classId)
	{
		if (classId < 0)
		{
			classId = classesInstances.size() - 1;
		}
		else if (classId >= classesInstances.size())
		{
			classId = 0;
		}

		this->indexClass = classId;
		setClassView();
	}

	int getIdxClass()
	{
		return indexClass;
	}
};


