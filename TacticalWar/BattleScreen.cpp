#include "BattleScreen.h"
#include "TestCharacterModel.h"
#include <Pathfinder.h>
#include <ZoneAndSightCalculator.h>
#include <EnvironmentManager.h>
#include <TypeZoneLaunch.h>
#include "ScreenManager.h"
#include "LoginScreen.h"
#include <StringUtils.h>
#include <CharacterFactory.h>
#include "MusicManager.h"
#include "PlayerStatusView.h"
#include "SpellSlot.h"
#include <AnimationManager.h>
#include <ChangeTurnAction.h>
#include <CharacterMoveAction.h>
#include <LaunchSpellAction.h>
#include <TakeDamage.h>
#include <TeleportAction.h>
#include <SynchroPAAction.h>
#include <SynchroPMAction.h>
#include <LaunchSpellAction.h>
#include <BattleEndAction.h>
#include "ClassSelectionScreen.h"
#include "WaitMatchScreen.h"


using namespace tw;


BattleScreen::BattleScreen(tgui::Gui * gui, int environmentId)
{
	redirectToBattlePreparation = false;
	hasInitSpellBar = false;
	turnToken = -1;
	readyToValidatePosition = false;
	this->gui = NULL;
	this->window = NULL;
	
	gui->removeAllWidgets();
	
	renderer = new IsometricRenderer(NULL);
	environment = EnvironmentManager::getInstance()->loadEnvironment(environmentId);

	
	colorator = new TWColorator(sf::Color(40, 200, 255), sf::Color(20, 100, 200));

	renderer->setColorator(colorator);
	renderer->addEventListener(this);

	activeCharacter = NULL;

	font.loadFromFile("./assets/font/neuropol_x_rg.ttf");
	FPS.setFont(font);


	tgui::Button::Ptr readyButton = tgui::Button::create();
	readyButton->setInheritedFont(font);
	readyButton->setText("Valider position");
	readyButton->setSize(200, 100);
	readyButton->setVisible(false);
	readyButton->connect("pressed", [&]() {
		if (activeCharacter != NULL && !activeCharacter->isPlayerReady() && !readyToValidatePosition)
		{
			readyToValidatePosition = true;
		}
	});

	gui->add(readyButton, "readyButton");

	gui->add(PlayerStatusView::getInstance());

	LinkToServer::getInstance()->addListener(this);
	MusicManager::getInstance()->setBattleMusic();
}

BattleScreen::~BattleScreen()
{
	LinkToServer::getInstance()->removeListener(this);
}

void BattleScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{	
	this->window = window;
	this->gui = gui;

	tgui::Button::Ptr readyButton = gui->get<tgui::Button>("readyButton");
	readyButton->setPosition(window->getSize().x / 2. - readyButton->getSize().x / 2., window->getSize().y - readyButton->getSize().y - 20);

	

	if (activeCharacter != NULL)
	{
		readyButton->setVisible(!activeCharacter->isPlayerReady());

		if(!hasInitSpellBar)
		{
			std::shared_ptr<SpellSlot> spellSlot1 = std::make_shared<SpellSlot>(activeCharacter, 1, activeCharacter->getSpell1IconPath());
			gui->add(spellSlot1, "spellSlot1");
			spellSlot1->setSize(tgui::Layout2d(100, 100));
			spellSlot1->setPosition(tgui::Layout2d(150, window->getSize().y - 110));
			spellSlot1->getSpellPicture()->connect("Clicked", [&]() {
				setSelectedSpell(1);
			});

			std::shared_ptr<SpellSlot> spellSlot2 = std::make_shared<SpellSlot>(activeCharacter, 2, activeCharacter->getSpell2IconPath());
			gui->add(spellSlot2, "spellSlot2");
			spellSlot2->setSize(tgui::Layout2d(100, 100));
			spellSlot2->setPosition(tgui::Layout2d(150 + 110, window->getSize().y - 110));
			spellSlot2->getSpellPicture()->connect("Clicked", [&]() {
				setSelectedSpell(2);
			});

			std::shared_ptr<SpellSlot> spellSlot3 = std::make_shared<SpellSlot>(activeCharacter, 3, activeCharacter->getSpell3IconPath());
			gui->add(spellSlot3, "spellSlot3");
			spellSlot3->setSize(tgui::Layout2d(100, 100));
			spellSlot3->setPosition(tgui::Layout2d(150 + 220, window->getSize().y - 110));
			spellSlot3->getSpellPicture()->connect("Clicked", [&]() {
				setSelectedSpell(3);
			});

			std::shared_ptr<SpellSlot> spellSlot4 = std::make_shared<SpellSlot>(activeCharacter, 4, activeCharacter->getSpell4IconPath());
			gui->add(spellSlot4, "spellSlot4");
			spellSlot4->setSize(tgui::Layout2d(100, 100));
			spellSlot4->setPosition(tgui::Layout2d(150 + 330, window->getSize().y - 110));
			spellSlot4->getSpellPicture()->connect("Clicked", [&]() {
				setSelectedSpell(4);
			});

			tgui::Button::Ptr skipTurnBtn = tgui::Button::create("Passer le tour");
			gui->add(skipTurnBtn, "skipTurnBtn");
			skipTurnBtn->setSize(200, 100);
			skipTurnBtn->setPosition(tgui::Layout2d(150 + 440, window->getSize().y - 110));
			skipTurnBtn->setVisible(false);
			skipTurnBtn->connect("pressed", [&]() {
				LinkToServer::getInstance()->Send("Ct");
			});

			hasInitSpellBar = true;
		}
		else
		{
			tgui::Button::Ptr skipTurnButton = gui->get<tgui::Button>("skipTurnBtn");
			// Le bouton n'est visible que pendant notre tour.
			skipTurnButton->setVisible(colorator->getBattleState() == BattleState::BATTLE_PHASE_ACTIVE_PLAYER_TURN);
		}
	}
}

void tw::BattleScreen::calculateAndSetSpellZone()
{
	int spellMinPO = -1;
	int spellMaxPO = -1;
	TypeZoneLaunch zoneType = TypeZoneLaunch::NORMAL;
	
	if (activeCharacter != NULL)
	{
		switch (selectedSpell)
		{
		case 1:
			spellMinPO = activeCharacter->getSpell1MinPO();
			spellMaxPO = activeCharacter->getSpell1MaxPO();
			zoneType = activeCharacter->getSpell1LaunchZoneType();
			break;

		case 2:
			spellMinPO = activeCharacter->getSpell2MinPO();
			spellMaxPO = activeCharacter->getSpell2MaxPO();
			zoneType = activeCharacter->getSpell2LaunchZoneType();
			break;

		case 3:
			spellMinPO = activeCharacter->getSpell3MinPO();
			spellMaxPO = activeCharacter->getSpell3MaxPO();
			zoneType = activeCharacter->getSpell3LaunchZoneType();
			break;

		case 4:
			spellMinPO = activeCharacter->getSpell4MinPO();
			spellMaxPO = activeCharacter->getSpell4MaxPO();
			zoneType = activeCharacter->getSpell4LaunchZoneType();
			break;
		}
	}

	if (activeCharacter != NULL && selectedSpell != -1)
	{
		std::vector<Point2D> targetZone = ZoneAndSightCalculator::getInstance()->generateZone(
			activeCharacter->getCurrentX(),
			activeCharacter->getCurrentY(),
			spellMinPO,
			spellMaxPO,
			zoneType);

		std::vector<Obstacle> obstacles;
		std::vector<Obstacle> environmentObstacles = environment->getObstacles();
		std::vector<Obstacle> dynamicsObstacles = getDynamicObstacles();
		obstacles.insert(obstacles.end(), environmentObstacles.begin(), environmentObstacles.end());
		obstacles.insert(obstacles.end(), dynamicsObstacles.begin(), dynamicsObstacles.end());

		std::vector<Point2D> targettable = ZoneAndSightCalculator::getInstance()->processLineOfSight(
			activeCharacter->getCurrentX(),
			activeCharacter->getCurrentY(),
			targetZone,
			obstacles
		);

		colorator->setSpellLaunchZone(targettable);
	}
}

void BattleScreen::update(float deltatime)
{
	Screen::update(deltatime);

	for (int i = 0; i < characters.size(); i++)
	{
		characters[i]->update(deltatime);
	}

	renderer->ellapseTime(deltatime);	// Update renderer (water animation)

	double fps = 1.0 / deltatime;
	FPS.setString(std::to_string((int)fps));
	FPS.setFillColor(sf::Color::Red);
	FPS.setPosition(10, 10);

	if (readyToValidatePosition)
	{
		if (activeCharacter != NULL && !activeCharacter->isPlayerReady())
		{
			LinkToServer::getInstance()->Send("Cs");
		}
	}

	AnimationManager::getInstance()->update(deltatime);

	if (redirectToBattlePreparation)
	{
		AnimationManager::getInstance()->clear();
		gui->removeAllWidgets();
		window->setView(window->getDefaultView());
		tw::ScreenManager::getInstance()->setCurrentScreen(new WaitMatchScreen(gui));
		MusicManager::getInstance()->setMenuMusic();
		redirectToBattlePreparation = false;
		delete this;
	}
	else
	{
		LinkToServer::getInstance()->UpdateReceivedData();
	}
}

void BattleScreen::render(sf::RenderWindow * window)
{
	renderer->modifyWindow(window);
	std::vector<BaseCharacterModel*> aliveCharacters;
	
	for (std::map<int, BaseCharacterModel*>::iterator it = characters.begin(); it != characters.end(); it++)
	{
		aliveCharacters.push_back((*it).second);
	}

	renderer->render(environment, aliveCharacters, animationsToDisplay, getDeltatime());
	window->draw(FPS);

	animationsToDisplay.clear();
}

void BattleScreen::invalidatePathZone()
{
	colorator->setPathToHighlight(std::vector<Point2D>());
	lastStartPosition.setX(-1);
	lastStartPosition.setY(-1);
}

std::vector<Obstacle> tw::BattleScreen::getDynamicObstacles()
{
	std::vector<Obstacle> obstacles;

	for (auto it = characters.begin(); it != characters.end(); it++)
	{
		if(activeCharacter != (*it).second && (*it).second->isAlive())
			obstacles.push_back(Obstacle((*it).second));
	}

	return obstacles;
}

// Renderer Event Listener
void BattleScreen::onCellClicked(int cellX, int cellY)
{
	std::cout << "Cell x=" << cellX << ", y=" << cellY << " clicked !" << std::endl;
	BaseCharacterModel * m = activeCharacter;
	if (colorator->getBattleState() == BattleState::BATTLE_PHASE_ACTIVE_PLAYER_TURN)
	{
		std::vector<tw::Point2D> spellZone = colorator->getSpellLaunchZone();

		// Ciblage :
		if(spellZone.size() > 0)
		{
			bool isInSpellZone = false;

			for (int i = 0; i < spellZone.size(); i++)
			{
				if (spellZone[i].getX() == cellX && spellZone[i].getY() == cellY)
				{
					isInSpellZone = true;
					break;
				}
			}

			if (!isInSpellZone)
			{
				setSelectedSpell(-1);
			}
			else
			{
				// Send launch spell request to server ...
				std::string str = "CL" + std::to_string(selectedSpell) + ";" + std::to_string(cellX) + ";" + std::to_string(cellY);
				LinkToServer::getInstance()->Send(str);
				setSelectedSpell(-1);
			}
		}
		// Déplacement :
		else if (m != NULL && !m->hasTargetPosition())
		{
			bool isInPathZone = false;
			for (int i = 0; i < pathZone.size(); i++)
			{
				if (pathZone[i].getX() == cellX && pathZone[i].getY() == cellY)
				{
					isInPathZone = true;
					break;
				}
			}

			if (isInPathZone)
			{
				Point2D startPosition(m->getCurrentX(), m->getCurrentY());
				Point2D targetPosition(cellX, cellY);

				std::vector<Point2D> path = Pathfinder::getInstance()->getPath(startPosition, targetPosition, environment, getDynamicObstacles());
				//m->setPath(path);
				std::string str = "Cm" + Pathfinder::serializePath(path);
				LinkToServer::getInstance()->Send(str);
			}
		}
	}
	else if (colorator->getBattleState() == BattleState::BATTLE_PHASE)
	{
		
	}
	else if (colorator->getBattleState() == BattleState::PREPARATION_PHASE)
	{
		if (m != NULL && !m->isPlayerReady())
		{
			CellData * cell = environment->getMapData(cellX, cellY);
			if (cell->getTeamStartPointNumber() == m->getTeamId())
			{
				LinkToServer::getInstance()->Send("CP" + std::to_string(cellX) + ";" + std::to_string(cellY));
			}
		}
	}
}

void BattleScreen::onCellHover(int cellX, int cellY)
{
	CellData * cell = environment->getMapData(cellX, cellY);
	
	if (selectedSpell == -1)
	{
		if (!cell->getIsObstacle() && cell->getIsWalkable())
		{
			bool needToReprocess = false;
			bool isInPathZone = false;
			for (int i = 0; i < pathZone.size(); i++)
			{
				if (pathZone[i].getX() == cellX && pathZone[i].getY() == cellY)
				{
					Point2D startPosition(activeCharacter->getCurrentX(), activeCharacter->getCurrentY());
					Point2D targetPosition(cellX, cellY);

					if (startPosition != lastStartPosition || targetPosition != lastTargetPosition)
					{
						lastStartPosition = startPosition;
						lastTargetPosition = targetPosition;
						needToReprocess = true;
					}

					isInPathZone = true;
					break;
				}
			}

			if (!isInPathZone)
			{
				invalidatePathZone();
			}

			if (needToReprocess)
			{
				std::vector<Point2D> pathToHighlight = Pathfinder::getInstance()->getPath(lastStartPosition, lastTargetPosition, environment, getDynamicObstacles());
				if (pathToHighlight.size() <= activeCharacter->getCurrentPM())
				{
					colorator->setPathToHighlight(pathToHighlight);
				}
			}
		}
		else
		{
			invalidatePathZone();
		}
	}
	else
	{
		invalidatePathZone();

		colorator->setSpellImpactZone(std::vector<Point2D>());

		if (!cell->getIsObstacle() && cell->getIsWalkable())
		{
			std::vector<Point2D> launchZone = colorator->getSpellLaunchZone();

			bool isInZone = false;
			for (int i = 0; i < launchZone.size(); i++)
			{
				if (launchZone[i] == (*cell))
				{
					isInZone = true;
					break;
				}
			}

			if (isInZone)
			{
				calculateAndSetSpellImpactZone(cell->getX(), cell->getY());
			}
		}
	}
}

void BattleScreen::calculateAndSetSpellImpactZone(int targetX, int targetY)
{
	int spellMinPO = -1;
	int spellMaxPO = -1;
	TypeZoneLaunch zoneType = TypeZoneLaunch::NORMAL;

	if (activeCharacter != NULL)
	{
		switch (selectedSpell)
		{
		case 1:
			spellMinPO = activeCharacter->getSpell1ImpactZoneMinPO();
			spellMaxPO = activeCharacter->getSpell1ImpactZoneMaxPO();
			zoneType = activeCharacter->getSpell1ImpactZoneType();
			break;

		case 2:
			spellMinPO = activeCharacter->getSpell2ImpactZoneMinPO();
			spellMaxPO = activeCharacter->getSpell2ImpactZoneMaxPO();
			zoneType = activeCharacter->getSpell2ImpactZoneType();
			break;

		case 3:
			spellMinPO = activeCharacter->getSpell3ImpactZoneMinPO();
			spellMaxPO = activeCharacter->getSpell3ImpactZoneMaxPO();
			zoneType = activeCharacter->getSpell3ImpactZoneType();
			break;

		case 4:
			spellMinPO = activeCharacter->getSpell4ImpactZoneMinPO();
			spellMaxPO = activeCharacter->getSpell4ImpactZoneMaxPO();
			zoneType = activeCharacter->getSpell4ImpactZoneType();
			break;
		}
	}

	if (activeCharacter != NULL && selectedSpell != -1)
	{
		std::vector<Point2D> impactZone = ZoneAndSightCalculator::getInstance()->generateZone(
			targetX,
			targetY,
			spellMinPO,
			spellMaxPO,
			zoneType);

		/*
		std::vector<Obstacle> obstacles;
		std::vector<Obstacle> environmentObstacles = environment->getObstacles();
		std::vector<Obstacle> dynamicsObstacles = getDynamicObstacles();
		obstacles.insert(obstacles.end(), environmentObstacles.begin(), environmentObstacles.end());
		obstacles.insert(obstacles.end(), dynamicsObstacles.begin(), dynamicsObstacles.end());

		std::vector<Point2D> targettable = ZoneAndSightCalculator::getInstance()->processLineOfSight(
			activeCharacter->getCurrentX(),
			activeCharacter->getCurrentY(),
			targetZone,
			obstacles
		);
		*/

		colorator->setSpellImpactZone(impactZone);
	}
}

void BattleScreen::onCellMouseDown(int cellX, int cellY)
{

}

void BattleScreen::onEvent(void * e)
{
	if (e != NULL)
	{
		sf::Event * event = (sf::Event*)e;

		if (event->type == sf::Event::Resized)
		{
			if (window != NULL)
			{
				int sizeX = event->size.width;
				int sizeY = event->size.height;
				sf::View view = window->getView();
				view.setSize(event->size.width, event->size.height);
				window->setView(view);
			}
		}
		else if (event->type == sf::Event::KeyPressed)
		{
			if (event->key.code == sf::Keyboard::Num1)
			{
				setSelectedSpell(1);
			}
			else if (event->key.code == sf::Keyboard::Num2)
			{
				setSelectedSpell(2);
			}
			else if (event->key.code == sf::Keyboard::Num3)
			{
				setSelectedSpell(3);
			}
			else if (event->key.code == sf::Keyboard::Num4)
			{
				setSelectedSpell(4);
			}
			else if (event->key.code == sf::Keyboard::Escape)
			{
				setSelectedSpell(-1);
			}
		}

		if(gui != NULL)
			gui->handleEvent(*event);
	}
}
//----------------------------------------------------------



//----------------------------------------------------------
// CharacterEventListener :
//----------------------------------------------------------
void BattleScreen::onPositionChanged(BaseCharacterModel * c, int newPositionX, int newPositionY)
{
	sf::Clock test;
	// Refresh the position :
	if (activeCharacter != NULL && colorator->getBattleState() == BattleState::BATTLE_PHASE_ACTIVE_PLAYER_TURN)
	{
		int x = c->getCurrentX();
		int y = c->getCurrentY();

		Point2D startPoint(x, y);

		std::vector<Point2D> zone = ZoneAndSightCalculator::getInstance()->generateZone(x, y, 1, activeCharacter->getCurrentPM(), TypeZoneLaunch::NORMAL);
		std::vector<Point2D> realZone;
		for (int i = 0; i < zone.size(); i++)
		{
			std::vector<Point2D> path = Pathfinder::getInstance()->getPath(startPoint, zone[i], environment, getDynamicObstacles());
			if (path.size() > 0 && path.size() <= activeCharacter->getCurrentPM())
			{
				realZone.push_back(zone[i]);
			}
		}

		pathZone = realZone;
		colorator->setPathZone(pathZone);
	}
	sf::Time ellapsed = test.restart();
	std::cout << "Time to reprocess path zone = " << ellapsed.asMilliseconds() << "ms" << std::endl;

	// Reprocess the spell zone :
	int spellId = selectedSpell;
	setSelectedSpell(-1);
	setSelectedSpell(selectedSpell);
}

//----------------------------------------------------------



//----------------------------------------------------------
// ServerMessageListener :
//----------------------------------------------------------
void BattleScreen::onMessageReceived(std::string msg)
{
	sf::String str = msg;
	
	if (str.substring(0, 2) == "CA")	// Add character
	{
		std::string data = str.substring(2).toAnsiString();
		std::vector<std::string> splitedData = StringUtils::explode(data, ';');
		int i = 0;
		int characterId = std::atoi(splitedData[i++].c_str());
		int classId = std::atoi(splitedData[i++].c_str());
		int teamId = std::atoi(splitedData[i++].c_str());
		int currentX = std::atoi(splitedData[i++].c_str());
		int currentY = std::atoi(splitedData[i++].c_str());
		int currentLife = std::atoi(splitedData[i++].c_str());
		int currentPA = std::atoi(splitedData[i++].c_str());
		int currentPM = std::atoi(splitedData[i++].c_str());
		int cooldown1 = std::atoi(splitedData[i++].c_str());
		int cooldown2 = std::atoi(splitedData[i++].c_str());
		int cooldown3 = std::atoi(splitedData[i++].c_str());
		int cooldown4 = std::atoi(splitedData[i++].c_str());
		int colorNumber = std::atoi(splitedData[i++].c_str());
		std::string pseudo = splitedData[i++];

		BaseCharacterModel * c = CharacterFactory::getInstance()->constructCharacter(environment, classId, teamId, currentX, currentY, this);
		characters[characterId] = c;
		c->setCurrentLife(currentLife);
		c->setCurrentPA(currentPA);
		c->setCurrentPM(currentPM);
		c->setAttackCooldown(1, cooldown1);
		c->setAttackCooldown(2, cooldown2);
		c->setAttackCooldown(3, cooldown3);
		c->setAttackCooldown(4, cooldown4);
		c->setColorNumber(colorNumber);
		c->setPseudo(pseudo);
		c->addEventListener(this);

		// To reprocess the path zone with new obstacles :
		if (activeCharacter != NULL)
		{
			onPositionChanged(activeCharacter, activeCharacter->getCurrentX(), activeCharacter->getCurrentY());
		}
	}
	else if (str.substring(0, 2) == "CS")	// Set active character
	{
		int characterId = std::atoi(str.substring(2).toAnsiString().c_str());
		activeCharacter = characters[characterId];
		onPositionChanged(activeCharacter, activeCharacter->getCurrentX(), activeCharacter->getCurrentY());
	}
	else if (str.substring(0, 2) == "BS")	// Set battle state
	{
		int state = std::atoi(str.substring(2).toAnsiString().c_str());
		BattleState battleState = (BattleState)state;
		colorator->setBattleState(battleState);

		if (battleState == BattleState::BATTLE_PHASE)
		{
			if (activeCharacter != NULL)
			{
				onPositionChanged(activeCharacter, activeCharacter->getCurrentX(), activeCharacter->getCurrentY());
			}
		}
	}
	else if (str.substring(0, 2) == "Cs")	// Player ready status
	{
		std::string data = str.substring(2);
		std::vector<std::string> splited = StringUtils::explode(data, ';');
		int playerId = std::atoi(splited[0].c_str());
		int status = std::atoi(splited[1].c_str());

		characters[playerId]->setReadyStatus(status == 1);
	}
	else if (str.substring(0, 2) == "CP")	// Update character position 
	{
		std::string data = str.substring(2);
		std::vector<std::string> splited = StringUtils::explode(data, ';');
		int playerId = std::atoi(splited[0].c_str());
		int cellX = std::atoi(splited[1].c_str());
		int cellY = std::atoi(splited[2].c_str());

		AnimationManager::getInstance()->addAnimation(new TeleportAction(this, playerId, cellX, cellY));
	}
	else if (str.substring(0, 2) == "Ct")	// Changement de tour
	{
		std::string data = str.substring(2);
		int playerId = std::atoi(data.c_str());

		std::string msg = "";

		if (characters[playerId] == activeCharacter)
		{			
			msg = "C'est à votre tour de jouer !";
		}
		else
		{
			msg = "C'est au tour de " + characters[playerId]->getPseudo();
		}

		AnimationManager::getInstance()->addAnimation(new ChangeTurnAction(this, playerId, msg, 0));
	}
	else if (str.substring(0, 2) == "Cm")	// Mouvement d'un joueur
	{
		std::string data = str.substring(2);
		std::vector<std::string> splited = StringUtils::explode(data, ';');
		int playerId = std::atoi(splited[0].c_str());
		std::string pathStr = splited[1];
		std::vector<Point2D> path = Pathfinder::deserializePath(pathStr);

		AnimationManager::getInstance()->addAnimation(new CharacterMoveAction(this, playerId, path));
	}
	else if (str.substring(0, 2) == "Ca")	// Synchro nombre de PA
	{
		std::string data = str.substring(2);
		std::vector<std::string> splited = StringUtils::explode(data, ';');
		int playerId = std::atoi(splited[0].c_str());
		int pa = std::atoi(splited[1].c_str());
		
		AnimationManager::getInstance()->addAnimation(new SynchroPAAction(this, playerId, pa));
	}
	else if (str.substring(0, 2) == "Cp")	// Synchro nombre de PM
	{
		std::string data = str.substring(2);
		std::vector<std::string> splited = StringUtils::explode(data, ';');
		int playerId = std::atoi(splited[0].c_str());
		int pm = std::atoi(splited[1].c_str());
		
		AnimationManager::getInstance()->addAnimation(new SynchroPMAction(this, playerId, pm));
	}
	else if (str.substring(0, 2) == "CL")	// Lancer d'un sort
	{
		std::string data = str.substring(2);
		std::vector<std::string> splited = StringUtils::explode(data, ';');
		int playerId = std::atoi(splited[0].c_str());
		int spellId = std::atoi(splited[1].c_str());
		int cellX = std::atoi(splited[2].c_str());
		int cellY = std::atoi(splited[3].c_str());

		AnimationManager::getInstance()->addAnimation(new LaunchSpellAction(this, playerId, spellId, cellX, cellY));
	}
	else if (str.substring(0, 2) == "BE")	// Fin du combat
	{
		std::string data = str.substring(2);
		int winnerTeamId = std::atoi(data.c_str());

		AnimationManager::getInstance()->addAnimation(new BattleEndAction(this, winnerTeamId));
	}
	else if (str.substring(0, 2) == "HC")
	{
		AnimationManager::getInstance()->clear();
		gui->removeAllWidgets();
		window->setView(window->getDefaultView());
		tw::ScreenManager::getInstance()->setCurrentScreen(new ClassSelectionScreen(gui));
		MusicManager::getInstance()->setMenuMusic();
		delete this;
	}
}

void tw::BattleScreen::onDisconnected()
{
	gui->removeAllWidgets();
	AnimationManager::getInstance()->clear();

	window->setView(window->getDefaultView());

	tw::ScreenManager::getInstance()->setCurrentScreen(new tw::LoginScreen(gui));
	delete this;
}

//----------------------------------------------------------

//----------------------------------------------------------
// IScreenActionCallback implementation :
//----------------------------------------------------------
void tw::BattleScreen::applyEndOfBattle(int winnerTeam)
{
	std::vector<BaseCharacterModel*> winners;

	std::string msg = "";
	int i = 0;

	auto it = characters.begin();
	for (; it != characters.end(); it++)
	{
		if ((*it).second->getTeamId() == winnerTeam)
		{
			if (i > 0)
				msg += " et ";

			msg += (*it).second->getPseudo();
			winners.push_back((*it).second);
			i++;
		}
	}

	msg += " ont gagné !";

	tgui::Label::Ptr endLabel = tgui::Label::create(msg);
	endLabel->setInheritedFont(font);
	endLabel->setTextSize(30);
	endLabel->getRenderer()->setTextColor(tgui::Color::Green);
	endLabel->getRenderer()->setTextOutlineColor(tgui::Color::Black);
	endLabel->getRenderer()->setTextOutlineThickness(1.0);
	endLabel->setPosition(window->getSize().x / 2.0 - endLabel->getSize().x / 2.0, window->getSize().y / 2.0 - endLabel->getSize().y / 2.0);
	gui->add(endLabel, "endLabel");

	PlayerStatusView::getInstance()->setVisible(false);

	// Return to wait screen button
	tgui::Button::Ptr backButton = tgui::Button::create("Fermer");
	backButton->setInheritedFont(font);
	backButton->setPosition(window->getSize().x / 2.0 - backButton->getSize().x / 2.0, window->getSize().y / 2.0 - backButton->getSize().y / 2.0 + endLabel->getSize().y + 10);
	backButton->connect("pressed", [&]() {
		redirectToBattlePreparation = true;
	});
	gui->add(backButton, "backButton");

	colorator->setBattleState(BattleState::END_PHASE);
}

void tw::BattleScreen::applyChangeTurn(float remaining, int idPerso, std::string message)
{
	turnToken = idPerso;
	characters[idPerso]->turnStart();

	if (characters[idPerso] == activeCharacter)
	{
		colorator->setBattleState(BattleState::BATTLE_PHASE_ACTIVE_PLAYER_TURN);
		invalidatePathZone();

		// To reprocess the path zone with new obstacles configuration :
		if (activeCharacter != NULL)
		{
			onPositionChanged(activeCharacter, activeCharacter->getCurrentX(), activeCharacter->getCurrentY());
		}
	}
	else
	{
		colorator->setBattleState(BattleState::BATTLE_PHASE);
	}
}

void tw::BattleScreen::applyCharacterDie(int idPerso)
{

}

void tw::BattleScreen::applyCharacterLaunchSpell(int persoId, int x, int y, int spellId)
{
	// TODO : Ajouter un log des événements de combat ...
}

tw::BaseCharacterModel* tw::BattleScreen::getCharacter(int persoId)
{
	return characters[persoId];
}

std::vector<tw::BaseCharacterModel*> tw::BattleScreen::getAliveCharacters()
{
	std::vector<tw::BaseCharacterModel*> aliveCharacters;
	auto it = characters.begin();
	for(;it != characters.end(); it++)
	{
		if ((*it).second->isAlive())
			aliveCharacters.push_back((*it).second);
	}

	return aliveCharacters;
}

void tw::BattleScreen::addAnimationToDisplay(SpellView * s)
{
	animationsToDisplay.push_back(s);
}

void tw::BattleScreen::applyCharacterMove(int persoId, std::vector<tw::Point2D> path, MoveActionAnimationEventListener * callback)
{
	characters[persoId]->setPath(path, callback);
}

void tw::BattleScreen::applyCharacterDisconnected(int persoId)
{

}

void tw::BattleScreen::applyCharacterConnected(int persoId)
{

}

void tw::BattleScreen::applyTakeDamage(int persoId)
{

}

void tw::BattleScreen::applyCharacterPosition(int persoId, int x, int y)
{

}

void tw::BattleScreen::applyEnterBattlePhase()
{

}

void tw::BattleScreen::applyTeleport(int playerId, int cellX, int cellY)
{
	characters[playerId]->setCurrentX(cellX);
	characters[playerId]->setCurrentY(cellY);
}

void tw::BattleScreen::applySynchroPA(int playerId, int pa)
{
	characters[playerId]->setCurrentPA(pa);
}

void tw::BattleScreen::applySynchroPM(int playerId, int pm)
{
	characters[playerId]->setCurrentPM(pm);

	// To reprocess the path zone with new obstacles configuration :
	if (activeCharacter != NULL && characters[playerId] == activeCharacter)
	{
		onPositionChanged(activeCharacter, activeCharacter->getCurrentX(), activeCharacter->getCurrentY());
	}
}

void tw::BattleScreen::playTakeDamageSound()
{
	MusicManager::getInstance()->playTakeDamageSound();
}
//----------------------------------------------------------




//----------------------------------------------------------
// IMapKnowledge :
//----------------------------------------------------------

std::vector<tw::BaseCharacterModel*> BattleScreen::getAliveCharactersInZone(std::vector<tw::Point2D> zone)
{
	std::vector<tw::BaseCharacterModel*> result;

	auto it = characters.begin();
	for ( ; it != characters.end(); it++)
	{
		BaseCharacterModel * m = (*it).second;
		if (m->isAlive())
		{
			for (int i = 0; i < zone.size(); i++)
			{
				if (m->getCurrentX() == zone[i].getX() && m->getCurrentY() == zone[i].getY())
				{
					result.push_back(m);
					break;
				}
			}
		}
	}

	return result;
}

//----------------------------------------------------------