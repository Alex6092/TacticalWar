#include "IsometricRenderer.h"
#include "BattleScreen.h"
#include "ClassSelectionScreen.h"
#include "FxGalleryScreen.h"
#include "LoginScreen.h"
#include "TrainingScreen.h"
#include "TrainingSetupScreen.h"
#include "TutorialScreen.h"
#include "PuzzleScreen.h"
#include "PuzzleSelectScreen.h"
#include "ScreenManager.h"
#include <TGUI/TGUI.hpp>
#include "ClientConfig.h"
#include <algorithm>
#include <iostream>

int main(int argc, char** argv)
{
	ClientConfig & config = ClientConfig::get();
	config.applyCommandLine(argc, argv);

	// Plein écran si client.json le demande (paquet de l'événement) ; une taille de fenêtre donnée en
	// ligne de commande (captures, débogage) garde la fenêtre.
	sf::VideoMode mode = sf::VideoMode::getDesktopMode();
	bool windowed = config.windowWidth > 0 && config.windowHeight > 0;
	if (windowed)
		mode = sf::VideoMode(config.windowWidth, config.windowHeight);
	sf::Uint32 style = config.fullscreen && !windowed ? sf::Style::Fullscreen : sf::Style::Default;

	sf::RenderWindow window(mode, "Tactical War", style);
	tgui::Gui gui{ window };
	window.setVerticalSyncEnabled(true);
	if (config.fxGallery)
	{
		tw::ScreenManager::getInstance()->setCurrentScreen(new tw::FxGalleryScreen(&gui, config.fxSpell, config.fxMap));
	}
	else if (config.classScreenTalents >= 0)
	{
		std::string selection = "{\"talents\": " + std::to_string(config.classScreenTalents) + ", \"ban\": " + std::to_string(config.classScreenBan)
			+ ", \"team\": " + std::to_string(config.classScreenTeam)
			+ (config.classScreenSeconds >= 0 ? ", \"seconds\": " + std::to_string(config.classScreenSeconds) : std::string()) + "}";
		ClassSelectionScreen * screen = new ClassSelectionScreen(&gui, selection);
		tw::ScreenManager::getInstance()->setCurrentScreen(screen);
		if (config.classScreenSolo)
		{
			screen->onMessageReceived("PT{\"name\": \"Camille\", \"class\": 0, \"viewing\": 0, \"locked\": false, \"present\": false}");
			screen->onMessageReceived("PO4");
			if (config.classScreenSoloDone)
				screen->onMessageReceived("PT{\"name\": \"Camille\", \"class\": 2, \"viewing\": 2, \"locked\": true, \"present\": false}");
		}
		if (config.classScreenChosenBy)
		{
			screen->onMessageReceived("PT{\"name\": \"Camille\", \"class\": 4, \"viewing\": 4, \"locked\": true, \"present\": true}");
			screen->onMessageReceived("PO{\"class\": 2, \"spells\": [3, 4, 5, 6], \"talents\": [], \"appearance\": \"braise\", \"by\": \"Camille\"}");
		}
		if (config.classScreenAlone > 0)
		{
			screen->onMessageReceived("PT{\"name\": \"Camille (2)\", \"class\": 0, \"viewing\": 0, \"locked\": false, \"present\": false, \"standIn\": true}");
			if (config.classScreenAlone >= 2)
				screen->onMessageReceived("PO3");
		}
		if (config.classScreenMate > 0)
		{
			screen->onMessageReceived("PT{\"name\": \"Camille\", \"class\": " + std::to_string(config.classScreenMate)
				+ ", \"viewing\": " + std::to_string(config.classScreenMate) + ", \"locked\": true, \"appearance\": \"braise\", \"present\": true}");
		}
		// Fin de bannissement simulée : la classe donnée est interdite, la suivante bannie par notre équipe.
		if (config.classScreenForbidden > 0)
		{
			screen->onMessageReceived("BB{\"banned\": " + std::to_string(config.classScreenForbidden % 4 + 1) + ", \"done\": true, \"forbidden\": "
				+ std::to_string(config.classScreenForbidden) + "}");
		}
	}
	else if (config.puzzle > 0)
	{
		tw::ScreenManager::getInstance()->setCurrentScreen(new tw::PuzzleScreen(&gui, config.puzzle - 1, config.puzzleDemo));
	}
	else if (config.puzzleList)
	{
		tw::ScreenManager::getInstance()->setCurrentScreen(new tw::PuzzleSelectScreen(&gui));
	}
	else if (config.tutorial)
	{
		tw::ScreenManager::getInstance()->setCurrentScreen(new tw::TutorialScreen(&gui, tw::TutorialScreen::Origin::LOGIN, std::max(0, config.tutorialStep - 1)));
	}
	else if (config.training)
	{
		tw::TrainingSettings & settings = tw::TrainingSettings::current();
		settings.duo = !config.trainingDuel;
		settings.controlAlly = config.trainingDuoControl;
		settings.playerClass = config.trainingClass;
		settings.mapId = config.trainingMap;
		settings.autoplay = config.trainingAutoplay;
		settings.zone = config.trainingZone;
		settings.bonuses = config.trainingBonuses;
		if (config.trainingDifficulty == "easy")
			settings.difficulty = tw::TrainingSettings::Difficulty::EASY;
		else if (config.trainingDifficulty == "normal")
			settings.difficulty = tw::TrainingSettings::Difficulty::NORMAL;
		else if (config.trainingDifficulty == "hard")
			settings.difficulty = tw::TrainingSettings::Difficulty::HARD;
		settings.talentCount = config.trainingTalents;
		settings.talents = config.talentChoice;
		if (config.trainingStart)
			tw::ScreenManager::getInstance()->setCurrentScreen(new tw::TrainingScreen(&gui, settings));
		else
			tw::ScreenManager::getInstance()->setCurrentScreen(new tw::TrainingSetupScreen(&gui));
	}
	else
		tw::ScreenManager::getInstance()->setCurrentScreen(new tw::LoginScreen(&gui));
	sf::Clock deltaClock;
	sf::Clock runningClock;
	bool firstFrame = true;
	// --frame-stats : pires durées sur les 5 dernières secondes.
	sf::Clock statsClock;
	float worstFrame = 0;
	float worstWork = 0;

	while (window.isOpen())
	{
		float frame = deltaClock.restart().asSeconds();
		sf::Clock workClock;
		tw::ScreenManager::getInstance()->getCurrentScreen()->handleEvents(&window, &gui);
		tw::ScreenManager::getInstance()->getCurrentScreen()->update(frame);
		window.clear();
		tw::ScreenManager::getInstance()->getCurrentScreen()->render(&window);
		gui.draw();
		if (config.frameStats && !firstFrame)
		{
			worstFrame = std::max(worstFrame, frame);
			worstWork = std::max(worstWork, workClock.getElapsedTime().asSeconds());
			if (statsClock.getElapsedTime().asSeconds() >= 5)
			{
				std::cout << "Images (5 s) : pire " << (int)(worstFrame * 1000) << " ms, calcul le plus long " << (int)(worstWork * 1000) << " ms" << std::endl;
				worstFrame = worstWork = 0;
				statsClock.restart();
			}
		}

		// Capture d'écran demandée en ligne de commande (outil de développement) :
		if (!config.screenshotPath.empty() && !firstFrame && runningClock.getElapsedTime().asSeconds() >= config.screenshotDelaySeconds)
		{
			sf::Texture capture;
			capture.create(window.getSize().x, window.getSize().y);
			capture.update(window);
			capture.copyToImage().saveToFile(config.screenshotPath);
			config.screenshotPath.clear();
			window.close();
		}

		window.display();

		// La première image charge les textures (plusieurs secondes en Debug) : le temps du jeu et le
		// délai avant la capture d'écran partent de la fin de ce chargement.
		if (firstFrame)
		{
			firstFrame = false;
			deltaClock.restart();
			runningClock.restart();
		}
	}

	delete tw::ScreenManager::getInstance()->getCurrentScreen();

	return 0;
}
