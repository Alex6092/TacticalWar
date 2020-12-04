#include "AdminScreen.h"
#include "LinkToServer.h"
#include <Match.h>
#include "MatchView.h"

AdminScreen::AdminScreen(tgui::Gui * gui)
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
	subtitle.setString("Administration");
	subtitle.setFillColor(sf::Color::Red);
	subtitle.setOutlineColor(sf::Color(255, 215, 0));
	subtitle.setOutlineThickness(1.5);

	matchPanelTitle = tgui::Label::create();
	matchPanelTitle->setInheritedFont(font);
	matchPanelTitle->setTextSize(20);
	matchPanelTitle->getRenderer()->setTextColor(sf::Color::Yellow);
	matchPanelTitle->setText("Creer un match :");

	m_matchListpanel = tgui::ScrollablePanel::create();
	m_matchListpanel->setSize(1000, 600);
	m_matchListpanel->setInheritedFont(font);
	m_matchListpanel->getRenderer()->setBackgroundColor(sf::Color(128, 128, 128));

	listTeam1 = tgui::ListBox::create();
	listTeam1->setSize(200, 150);
	listTeam1->setInheritedFont(font);
	listTeam1->getRenderer()->setBackgroundColor(sf::Color::White);

	listTeam2 = tgui::ListBox::create();
	listTeam2->setSize(200, 150);
	listTeam2->setInheritedFont(font);
	listTeam2->getRenderer()->setBackgroundColor(sf::Color::White);

	versus = tgui::Label::create();
	versus->setInheritedFont(font);
	versus->setTextSize(50);
	versus->getRenderer()->setTextColor(sf::Color(240, 139, 27));
	versus->setText("VS");

	createMatch = tgui::Button::create();
	createMatch->setSize(150, 75);
	createMatch->setInheritedFont(font);
	createMatch->getRenderer()->setBackgroundColor(sf::Color(90, 182, 96)); // couleur verte
	
	//createMatch->getRenderer()->setBackgroundColor(sf::Color(226, 82, 32)); // couleur rouge
	//createMatch->getRenderer()->setBackgroundColor(sf::Color(90, 182, 96)); // couleur verte
	createMatch->setText("Creer");



	gui->add(matchPanelTitle);
	gui->add(m_matchListpanel);
	gui->add(listTeam1);
	gui->add(listTeam2);
	gui->add(versus);
	gui->add(createMatch);

	LinkToServer::getInstance()->addListener(this);
}

AdminScreen::~AdminScreen()
{
	LinkToServer::getInstance()->removeListener(this);
}

void AdminScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	title.setPosition(window->getSize().x / 2 - title.getLocalBounds().width / 2, 10);
	subtitle.setPosition(window->getSize().x / 2 - subtitle.getLocalBounds().width / 2, 10 + 128 + 10);
	matchPanelTitle->setPosition(window->getSize().x / 2.0 - m_matchListpanel->getSize().x / 2.0, 270);
	m_matchListpanel->setPosition(window->getSize().x / 2.0 - m_matchListpanel->getSize().x / 2.0, 300);
	listTeam1->setPosition(window->getSize().x / 2.0 - 350 - listTeam1->getSize().x / 2.0, 450);
	listTeam2->setPosition(window->getSize().x / 2.0 - 350 - listTeam2->getSize().x / 2.0, 750);
	versus->setPosition(window->getSize().x / 2.0 - versus->getSize().x / 2.0, 500);
	createMatch->setPosition(window->getSize().x / 2.0 - createMatch->getSize().x / 2.0, 800);

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

void AdminScreen::update(float deltatime)
{
	Screen::update(deltatime);
	LinkToServer::getInstance()->UpdateReceivedData();
}

void AdminScreen::render(sf::RenderWindow * window)
{
	window->draw(title);
	window->draw(subtitle);
}

void AdminScreen::onMessageReceived(std::string msg)
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

		m_matchListpanel->removeAllWidgets();

		// Afficher les matchs
		for (int i = 0; i < matchs.size(); i++)
		{
			std::shared_ptr<MatchView> m = std::make_shared<MatchView>(*matchs[i]);
			m->setSize(tgui::Layout("97%"), 120);
			m->setPosition(tgui::Layout("1.5%"), 10 * (i + 1) + (120 * i));

			m_matchListpanel->add(m);
		}

		m_matchListpanel->getRenderer()->setScrollbarWidth(10);
	}
}