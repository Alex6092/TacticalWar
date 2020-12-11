#include "AdminScreen.h"
#include "LinkToServer.h"
#include <Match.h>
#include "MatchView.h"
#include "ScreenManager.h"
#include "LoginScreen.h"



AdminScreen::AdminScreen(tgui::Gui * gui)
	: Screen()
{
	readyForCreate = false;
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
	m_matchListpanel->getRenderer()->setBackgroundColor(sf::Color(128, 128, 128, 128));

	m_matchListCreate = tgui::ScrollablePanel::create();
	m_matchListCreate->setSize(300, 600);
	m_matchListCreate->setInheritedFont(font);
	m_matchListCreate->getRenderer()->setBackgroundColor(sf::Color(128, 128, 128, 128));

	listTeam1 = tgui::ListBox::create();
	listTeam1->setSize(200, 150);
	listTeam1->setInheritedFont(font);
	listTeam1->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 200));

	listTeam2 = tgui::ListBox::create();
	listTeam2->setSize(200, 150);
	listTeam2->setInheritedFont(font);
	listTeam2->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 200));

	versus = tgui::Label::create();
	versus->setInheritedFont(font);
	versus->setTextSize(50);
	versus->getRenderer()->setTextColor(sf::Color(240, 139, 27));
	versus->setText("VS");

	nameMatch = tgui::Label::create();
	nameMatch->setInheritedFont(font);
	nameMatch->setTextSize(15);
	nameMatch->getRenderer()->setTextColor(sf::Color(240, 139, 27));
	nameMatch->setText("nom du match :");

	team1Choice = tgui::Label::create();
	team1Choice->setInheritedFont(font);
	team1Choice->setTextSize(15);
	team1Choice->getRenderer()->setTextColor(sf::Color(240, 139, 27));
	team1Choice->setText("equipe 1 :");

	team2Choice = tgui::Label::create();
	team2Choice->setInheritedFont(font);
	team2Choice->setTextSize(15);
	team2Choice->getRenderer()->setTextColor(sf::Color(240, 139, 27));
	team2Choice->setText("equipe 2 :");

	matchCreate = tgui::Label::create();
	matchCreate->setInheritedFont(font);
	matchCreate->setTextSize(20);
	matchCreate->getRenderer()->setTextColor(sf::Color::Yellow);
	matchCreate->setText("Matchs créés :");

	createMatch = tgui::Button::create();
	createMatch->setSize(150, 75);
	createMatch->setInheritedFont(font);
	createMatch->getRenderer()->setBackgroundColor(sf::Color(90, 182, 96, 200)); // couleur verte
	
	//createMatch->getRenderer()->setBackgroundColor(sf::Color(226, 82, 32)); // couleur rouge
	//createMatch->getRenderer()->setBackgroundColor(sf::Color(90, 182, 96)); // couleur verte
	createMatch->setText("Creer");
	createMatch->connect("pressed", [&]() {
		readyForCreate = true;
	});

	matchName = tgui::EditBox::create();
	matchName->setSize(250, 30);
	matchName->setInheritedFont(font);
	matchName->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 200));



	gui->add(matchPanelTitle);
	gui->add(m_matchListpanel);
	gui->add(m_matchListCreate);
	gui->add(listTeam1);
	gui->add(listTeam2);
	gui->add(versus);
	gui->add(createMatch);
	gui->add(matchName);
	gui->add(nameMatch);
	gui->add(matchCreate);
	gui->add(team1Choice);
	gui->add(team2Choice);

	LinkToServer::getInstance()->addListener(this);

	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/animatedBackground2.glsl");
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
	m_matchListCreate->setPosition(window->getSize().x / 2.0 + 700 - m_matchListCreate->getSize().x / 2.0, 300);
	listTeam1->setPosition(window->getSize().x / 2.0 - 350 - listTeam1->getSize().x / 2.0, 450);
	listTeam2->setPosition(window->getSize().x / 2.0 + 350 - listTeam2->getSize().x / 2.0, 450);
	versus->setPosition(window->getSize().x / 2.0 - versus->getSize().x / 2.0, 500);
	createMatch->setPosition(window->getSize().x / 2.0 - createMatch->getSize().x / 2.0, 800);
	matchName->setPosition(window->getSize().x / 2.0 - matchName->getSize().x / 2.0, 350);
	nameMatch->setPosition(window->getSize().x / 2.0 - 230 - nameMatch->getSize().x / 2.0, 353);
	matchCreate->setPosition(window->getSize().x / 2.0 + 650 - matchCreate->getSize().x / 2.0, 270);
	team1Choice->setPosition(window->getSize().x / 2.0 - 350 - team1Choice->getSize().x / 2.0, 425);
	team2Choice->setPosition(window->getSize().x / 2.0 + 350 - team2Choice->getSize().x / 2.0, 425);

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

	if (readyForCreate)
	{
		sf::String matchNameStr = matchName->getText();
		sf::String teamAId = listTeam1->getSelectedItemId();
		sf::String teamBId = listTeam2->getSelectedItemId();

		if (matchNameStr.getSize() > 0 && teamAId.getSize() > 0 && teamBId.getSize() > 0)
		{
			sf::String request = "CM" + matchNameStr + ";" + teamAId + ";" + teamBId;
			LinkToServer::getInstance()->Send(request);
		}

		readyForCreate = false;
	}
}

void AdminScreen::render(sf::RenderWindow * window)
{
	shader.setUniform("time", getShaderEllapsedTime());
	shader.setUniform("resolution", sf::Glsl::Vec2(window->getSize()));

	sf::Shader::bind(&shader);
	sf::RectangleShape rect;
	rect.setPosition(0, 0);
	rect.setSize(sf::Vector2f(window->getSize()));
	rect.setFillColor(sf::Color::Black);
	window->draw(rect);
	sf::Shader::bind(NULL);

	window->draw(title);
	window->draw(subtitle);
}

void AdminScreen::onMessageReceived(std::string msg)
{
	sf::String m = msg;

	// Match list
	if (m.substring(0, 2) == "MC")
	{
		std::vector<tw::Match*> matchs;
		std::vector<std::string> str = StringUtils::explode(m.substring(2), ';');

		for (int i = 0; i < str.size(); i++)
		{
			matchs.push_back(tw::Match::deserialize(str[i]));
		}

		m_matchListCreate->removeAllWidgets();

		// Afficher les matchs
		for (int i = 0; i < matchs.size(); i++)
		{
			std::shared_ptr<MatchView> m = std::make_shared<MatchView>(*matchs[i], false);
			m->setSize(tgui::Layout("97%"), 120);
			m->setPosition(tgui::Layout("1.5%"), 10 * (i + 1) + (120 * i));
			m->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 200));

			m_matchListCreate->add(m);
		}

		m_matchListCreate->getRenderer()->setScrollbarWidth(10);
	}
	// Team list
	else if (m.substring(0, 2) == "TL")
	{
		teamIdToPlayer.clear();
		std::vector<std::string> data = StringUtils::explode(m.substring(2), ';');

		for (int i = 0; i < data.size(); i++)
		{
			std::vector<std::string> teamData = StringUtils::explode(data[i], ',');
			int teamId = std::atoi(teamData[0].c_str());
			std::string teamInfo = teamData[1];

			std::vector<tw::Player> team = tw::Match::deserializeTeam(teamInfo, '¨', '^');

			teamIdToPlayer[teamId].clear();
			teamIdToPlayer[teamId] = team;
		}
		
		updateListTeam(listTeam1);
		updateListTeam(listTeam2);
	}
	else if (m.substring(0, 2) == "CO")
	{
		matchName->setText("");
		listTeam1->setSelectedItem("");
		listTeam2->setSelectedItem("");
	}
}

void AdminScreen::updateListTeam(tgui::ListBox::Ptr listTeam)
{
	listTeam->removeAllItems();

	for (std::map<int, std::vector<tw::Player>>::iterator it = teamIdToPlayer.begin(); it != teamIdToPlayer.end(); it++)
	{
		sf::String item = "Equipe " + sf::String(std::to_string((*it).first)) + " (" + (*it).second[0].getPseudo() + ", " + (*it).second[1].getPseudo() + ")";
		listTeam->addItem(item, std::to_string((*it).first));
	}
}

void AdminScreen::onDisconnected()
{
	gui->removeAllWidgets();
	tw::ScreenManager::getInstance()->setCurrentScreen(new tw::LoginScreen(gui));
	delete this;
}
