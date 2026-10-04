#include "AdminScreen.h"
#include "LinkToServer.h"
#include "ClientConfig.h"
#include <Match.h>
#include "MatchView.h"
#include "ScreenManager.h"
#include "LoginScreen.h"
#include <Message.h>
#include "BattleScreen.h"

sf::String AdminScreen::currentTab = L"Tournoi";



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

	m_matchListEnd = tgui::ScrollablePanel::create();
	m_matchListEnd->setSize(300, 600);
	m_matchListEnd->setInheritedFont(font);
	m_matchListEnd->getRenderer()->setBackgroundColor(sf::Color(128, 128, 128, 128));

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

	matchEnd = tgui::Label::create();
	matchEnd->setInheritedFont(font);
	matchEnd->setTextSize(20);
	matchEnd->getRenderer()->setTextColor(sf::Color::Yellow);
	matchEnd->setText("Matchs terminés :");

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



	// Les widgets existants (création manuelle de matchs) sont regroupés dans l'onglet "Matchs" :
	matchesGroup = tgui::Group::create({ "100%", "100%" });
	matchesGroup->add(matchPanelTitle);
	matchesGroup->add(m_matchListpanel);
	matchesGroup->add(m_matchListCreate);
	matchesGroup->add(m_matchListEnd);
	matchesGroup->add(listTeam1);
	matchesGroup->add(listTeam2);
	matchesGroup->add(versus);
	matchesGroup->add(createMatch);
	matchesGroup->add(matchName);
	matchesGroup->add(nameMatch);
	matchesGroup->add(matchCreate);
	matchesGroup->add(team1Choice);
	matchesGroup->add(team2Choice);
	matchesGroup->add(matchEnd);
	gui->add(matchesGroup);

	teamsPanel.reset(new TeamsAdminPanel(gui, font));
	tournamentPanel.reset(new TournamentAdminPanel(gui, font));
	livePanel.reset(new LiveSessionsPanel(gui, font));
	livePanel->onWatch = [](int session) {
		LinkToServer::getInstance()->SendRaw("SW" + nlohmann::json({ { "session", session } }).dump());
	};

	tabs = tgui::Tabs::create();
	tabs->setInheritedFont(font);
	tabs->setTextSize(18);
	tabs->setTabHeight(36);
	tabs->add("Matchs", false);
	tabs->add(L"Équipes", false);
	tabs->add(L"Tournoi", false);
	tabs->add(L"Combats", false);
	tabs->connect("TabSelected", [this](const sf::String & tab) { showTab(tab); });
	gui->add(tabs);
	int startTab = ClientConfig::get().adminTab;
	if (startTab >= 0 && startTab < 4)
		tabs->select(startTab);
	else if (!tabs->select(currentTab))
		tabs->select(2);

	LinkToServer::getInstance()->addListener(this);

	// Listes à jour (utile au retour d'un combat regardé : le serveur ne les renvoie pas seul).
	LinkToServer::getInstance()->SendRaw("TL");
	LinkToServer::getInstance()->SendRaw("MC");
	LinkToServer::getInstance()->SendRaw("SL{}");

	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/animatedBackground2.glsl");
}

void AdminScreen::showTab(const sf::String & tab)
{
	matchesGroup->setVisible(tab == "Matchs");
	teamsPanel->setVisible(tab == L"Équipes");
	tournamentPanel->setVisible(tab == "Tournoi");
	livePanel->setVisible(tab == "Combats");
	currentTab = tab;
}

AdminScreen::~AdminScreen()
{
	LinkToServer::getInstance()->removeListener(this);
}

void AdminScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	// Fenêtre basse (portable en 1280x720 ou 1366x768) : titre réduit, pour laisser la place aux panneaux.
	bool compact = window->getSize().y < 900;
	float titleTop = compact ? 4.f : 10.f;
	float titleSize = compact ? 76.f : 128.f;
	title.setCharacterSize((unsigned int)titleSize);
	subtitle.setCharacterSize(compact ? 24 : 32);
	title.setPosition(window->getSize().x / 2 - title.getLocalBounds().width / 2, titleTop);
	subtitle.setPosition(window->getSize().x / 2 - subtitle.getLocalBounds().width / 2, titleTop + titleSize + 10);
	float tabsTop = compact ? 130.f : 200.f;
	matchPanelTitle->setPosition(window->getSize().x / 2.0 - m_matchListpanel->getSize().x / 2.0, 270);
	m_matchListpanel->setPosition(window->getSize().x / 2.0 - m_matchListpanel->getSize().x / 2.0, 300);
	m_matchListCreate->setPosition(window->getSize().x / 2.0 + 700 - m_matchListCreate->getSize().x / 2.0, 300);
	m_matchListEnd->setPosition(window->getSize().x / 2.0 - 700 - m_matchListCreate->getSize().x / 2.0, 300);
	listTeam1->setPosition(window->getSize().x / 2.0 - 350 - listTeam1->getSize().x / 2.0, 450);
	listTeam2->setPosition(window->getSize().x / 2.0 + 350 - listTeam2->getSize().x / 2.0, 450);
	versus->setPosition(window->getSize().x / 2.0 - versus->getSize().x / 2.0, 500);
	createMatch->setPosition(window->getSize().x / 2.0 - createMatch->getSize().x / 2.0, 800);
	matchName->setPosition(window->getSize().x / 2.0 - matchName->getSize().x / 2.0, 350);
	nameMatch->setPosition(window->getSize().x / 2.0 - 230 - nameMatch->getSize().x / 2.0, 353);
	matchCreate->setPosition(window->getSize().x / 2.0 + 650 - matchCreate->getSize().x / 2.0, 270);
	team1Choice->setPosition(window->getSize().x / 2.0 - 350 - team1Choice->getSize().x / 2.0, 425);
	team2Choice->setPosition(window->getSize().x / 2.0 + 350 - team2Choice->getSize().x / 2.0, 425);
	matchEnd->setPosition(window->getSize().x / 2.0 - 750 - matchCreate->getSize().x / 2.0, 270);

	tabs->setPosition(window->getSize().x / 2.0 - tabs->getSize().x / 2.0, tabsTop);
	teamsPanel->layout(window->getSize(), tabsTop + 50);
	tournamentPanel->layout(window->getSize(), tabsTop + 50);
	livePanel->layout(window->getSize(), tabsTop + 50);

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

	LinkToServer::getInstance()->UpdateReceivedData();
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
	// Finished matchs list
	else if (m.substring(0, 2) == "MF")
	{
		std::vector<tw::Match*> matchs;
		std::vector<std::string> str = StringUtils::explode(m.substring(2), ';');

		for (int i = 0; i < str.size(); i++)
		{
			matchs.push_back(tw::Match::deserialize(str[i]));
		}

		m_matchListEnd->removeAllWidgets();

		// Afficher les matchs
		for (int i = 0; i < matchs.size(); i++)
		{
			std::shared_ptr<MatchView> m = std::make_shared<MatchView>(*matchs[i], false);
			m->setSize(tgui::Layout("97%"), 120);
			m->setPosition(tgui::Layout("1.5%"), 10 * (i + 1) + (120 * i));
			m->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 200));

			m_matchListEnd->add(m);
		}

		m_matchListEnd->getRenderer()->setScrollbarWidth(10);
	}
	// Team list (JSON)
	else if (m.substring(0, 2) == "UL" || m.substring(0, 2) == "UT" || m.substring(0, 2) == "UA")
	{
		tw::protocol::Message message;
		nlohmann::json body;
		if (tw::protocol::Message::decode(msg, message) && message.parseJson(body))
		{
			if (message.op == "UL")
				tournamentPanel->onTournamentList(body);
			else if (message.op == "UT")
				tournamentPanel->onTournamentState(body);
			else
				tournamentPanel->onAck(body);
		}
	}
	else if (m.substring(0, 2) == "TL" || m.substring(0, 2) == "TR")
	{
		tw::protocol::Message message;
		nlohmann::json body;
		if (tw::protocol::Message::decode(msg, message) && message.parseJson(body))
		{
			if (message.op == "TL")
			{
				teamsPanel->onTeamList(body);
				tournamentPanel->setTeams(body.value("teams", nlohmann::json::array()));
				updateListTeam(listTeam1);
				updateListTeam(listTeam2);
			}
			else
			{
				teamsPanel->onTeamResult(body);
			}
		}
	}
	else if (m.substring(0, 2) == "SL" || m.substring(0, 2) == "ER")
	{
		tw::protocol::Message message;
		nlohmann::json body;
		if (tw::protocol::Message::decode(msg, message) && message.parseJson(body))
		{
			if (message.op == "SL")
			{
				livePanel->onSessionList(body);
				tournamentPanel->onSessionList(body);
			}
			else
			{
				livePanel->setStatus(fromServerText(body.value("message", std::string())), sf::Color(255, 120, 100));
			}
		}
	}
	else if (m.substring(0, 2) == "HG")
	{
		// Combat regardé : la carte, puis l'état complet (BI).
		int environmentId = std::atoi(msg.substr(2).c_str());
		gui->removeAllWidgets();
		tw::ScreenManager::getInstance()->setCurrentScreen(new tw::BattleScreen(gui, environmentId, tw::BattleScreen::Mode::ADMIN));
		delete this;
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
	sf::String selected = listTeam->getSelectedItemId();
	listTeam->removeAllItems();

	for (const nlohmann::json & team : teamsPanel->getTeams())
	{
		if (!team.value("active", true))
			continue;

		sf::String item = fromServerText(team.value("name", std::string())) + " (";
		const nlohmann::json & players = team["players"];
		for (std::size_t i = 0; i < players.size(); i++)
		{
			if (i > 0)
				item += ", ";
			item += fromServerText(players[i].value("login", std::string()));
		}
		item += ")";

		listTeam->addItem(item, std::to_string(team.value("id", 0)));
	}

	listTeam->setSelectedItemById(selected);
}

void AdminScreen::onDisconnected()
{
	gui->removeAllWidgets();
	tw::ScreenManager::getInstance()->setCurrentScreen(new tw::LoginScreen(gui));
	delete this;
}
