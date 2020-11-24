#include "SpectatorModeScreen.h"
#include "LinkToServer.h"
#include <Match.h>

SpectatorModeScreen::SpectatorModeScreen(tgui::Gui * gui)
	: Screen()
{
	this->gui = gui;
	gui->removeAllWidgets();
	font.loadFromFile("./assets/font/neuropol_x_rg.ttf");

	title.setFont(font);
	title.setCharacterSize(128);
	title.setString("Tactical War");
	title.setFillColor(sf::Color::White);
	//title.setStyle(sf::Text::Bold);
	title.setOutlineColor(sf::Color(255, 215, 0));
	title.setOutlineThickness(3);

	subtitle.setFont(font);
	subtitle.setCharacterSize(32);
	subtitle.setString("Mode sp�ctateur");
	subtitle.setFillColor(sf::Color::Red);
	subtitle.setOutlineColor(sf::Color(255, 215, 0));
	subtitle.setOutlineThickness(1.5);

	matchPanelTitle = tgui::Label::create();
	matchPanelTitle->setInheritedFont(font);
	matchPanelTitle->setTextSize(20);
	//matchPanelTitle->getRenderer()->setTextColor(sf::Color::White);
	matchPanelTitle->setText("Match(s) en cours :");

	m_matchListpanel = tgui::ScrollablePanel::create();
	m_matchListpanel->setSize(1000, 600);
	m_matchListpanel->setInheritedFont(font);
	m_matchListpanel->getRenderer()->setBackgroundColor(sf::Color(128, 128, 128));
	
	//m_matchListpanel->setBackgroundColor(sf::Color(128, 128, 128, 0));
	gui->add(matchPanelTitle);
	gui->add(m_matchListpanel);

	LinkToServer::getInstance()->addListener(this);
}

SpectatorModeScreen::~SpectatorModeScreen()
{
	LinkToServer::getInstance()->removeListener(this);
}

void SpectatorModeScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	title.setPosition(window->getSize().x / 2 - title.getLocalBounds().width / 2, 10);
	subtitle.setPosition(window->getSize().x / 2 - subtitle.getLocalBounds().width / 2, 10 + 128 + 10);
	matchPanelTitle->setPosition(window->getSize().x / 2.0 - m_matchListpanel->getSize().x / 2.0, 270);
	m_matchListpanel->setPosition(window->getSize().x / 2.0 - m_matchListpanel->getSize().x / 2.0, 300);

	sf::Event event;
	while (window->pollEvent(event))
	{
		if (event.type == sf::Event::Closed)
			window->close();
		else if (event.type == sf::Event::Resized)
		{
			int sizeX = event.size.width;
			int sizeY = event.size.height;
			sf::View view = window->getView();
			view.setSize(event.size.width, event.size.height);
			window->setView(view);
		}

		gui->handleEvent(event);
	}
}

void SpectatorModeScreen::update(float deltatime)
{
	Screen::update(deltatime);
	LinkToServer::getInstance()->UpdateReceivedData();
}

void SpectatorModeScreen::render(sf::RenderWindow * window)
{
	window->draw(title);
	window->draw(subtitle);
}

void SpectatorModeScreen::onMessageReceived(std::string msg)
{
	sf::String m = msg;

	// Match list
	if (m.substring(0, 2) == "ML")
	{
		std::vector<tw::Match*> matchs;
		std::vector<std::string> str = StringUtils::explode(m.substring(2), ';');

		for (int i = 0; i < str.size(); i++)
		{
			matchs.push_back(tw::Match::deserialize(str[i]));
		}
		

		// TODO : Afficher les matchs
	}
}