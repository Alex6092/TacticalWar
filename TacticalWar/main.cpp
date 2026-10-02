#include "IsometricRenderer.h"
#include "BattleScreen.h"
#include "LoginScreen.h"
#include "ScreenManager.h"
#include <TGUI/TGUI.hpp>
#include "ClientConfig.h"

int main(int argc, char** argv)
{
	ClientConfig & config = ClientConfig::get();
	config.applyCommandLine(argc, argv);

	sf::VideoMode mode = sf::VideoMode::getDesktopMode();
	if (config.windowWidth > 0 && config.windowHeight > 0)
		mode = sf::VideoMode(config.windowWidth, config.windowHeight);

	sf::RenderWindow window(mode, "Tactical War"/*, sf::Style::Fullscreen*/);
	tgui::Gui gui{ window };
	window.setVerticalSyncEnabled(true);
	tw::ScreenManager::getInstance()->setCurrentScreen(new tw::LoginScreen(&gui));
	//tw::Screen * battle = new tw::BattleScreen(&gui);
	sf::Clock deltaClock;
	sf::Clock runningClock;

	while (window.isOpen())
	{
		tw::ScreenManager::getInstance()->getCurrentScreen()->handleEvents(&window, &gui);
		tw::ScreenManager::getInstance()->getCurrentScreen()->update(deltaClock.restart().asSeconds());
		window.clear();
		tw::ScreenManager::getInstance()->getCurrentScreen()->render(&window);
		gui.draw();

		// Capture d'écran demandée en ligne de commande (outil de développement) :
		if (!config.screenshotPath.empty() && runningClock.getElapsedTime().asSeconds() >= config.screenshotDelaySeconds)
		{
			sf::Texture capture;
			capture.create(window.getSize().x, window.getSize().y);
			capture.update(window);
			capture.copyToImage().saveToFile(config.screenshotPath);
			config.screenshotPath.clear();
			window.close();
		}

		window.display();
	}

	delete tw::ScreenManager::getInstance()->getCurrentScreen();

	return 0;
}
