#include "AdminScreen.h"
#include "LinkToServer.h"
#include "ClientConfig.h"
#include "ScreenManager.h"
#include "LoginScreen.h"
#include <Message.h>
#include "BattleScreen.h"

sf::String AdminScreen::currentTab = L"Tournoi";



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

	matchesPanel.reset(new MatchesAdminPanel(gui, font));
	matchesPanel->onWatch = [](int session) {
		LinkToServer::getInstance()->SendRaw("SW" + nlohmann::json({ { "session", session } }).dump());
	};

	teamsPanel.reset(new TeamsAdminPanel(gui, font));
	tournamentPanel.reset(new TournamentAdminPanel(gui, font));
	livePanel.reset(new LiveSessionsPanel(gui, font));
	livePanel->onWatch = [](int session) {
		LinkToServer::getInstance()->SendRaw("SW" + nlohmann::json({ { "session", session } }).dump());
	};
	livePanel->onShrink = [](int session) {
		LinkToServer::getInstance()->SendRaw("SK" + nlohmann::json({ { "session", session } }).dump());
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
	LinkToServer::getInstance()->SendRaw("FL{}");
	LinkToServer::getInstance()->SendRaw("SL{}");

	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/animatedBackground2.glsl");
}

void AdminScreen::showTab(const sf::String & tab)
{
	matchesPanel->setVisible(tab == "Matchs");
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
	tabs->setPosition(window->getSize().x / 2.0 - tabs->getSize().x / 2.0, tabsTop);
	matchesPanel->layout(window->getSize(), tabsTop + 50);
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

	// Matchs amicaux (JSON)
	if (m.substring(0, 2) == "FL" || m.substring(0, 2) == "FR")
	{
		tw::protocol::Message message;
		nlohmann::json body;
		if (tw::protocol::Message::decode(msg, message) && message.parseJson(body))
		{
			if (message.op == "FL")
				matchesPanel->onFriendlyList(body);
			else
				matchesPanel->onResult(body);
		}
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
				matchesPanel->setTeams(body.value("teams", nlohmann::json::array()));
			}
			else
			{
				teamsPanel->onTeamResult(body);
			}
		}
	}
	else if (m.substring(0, 2) == "SK")
	{
		// Réponse au rétrécissement d'une carte (onglets Combats et Tournoi).
		tw::protocol::Message message;
		nlohmann::json body;
		if (tw::protocol::Message::decode(msg, message) && message.parseJson(body))
		{
			sf::String text = fromServerText(body.value("message", std::string()));
			bool ok = body.value("ok", false);
			livePanel->setStatus(text, ok ? sf::Color(140, 255, 140) : sf::Color(255, 140, 120));
			tournamentPanel->showMessage(text, ok);
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
}

void AdminScreen::onDisconnected()
{
	gui->removeAllWidgets();
	tw::ScreenManager::getInstance()->setCurrentScreen(new tw::LoginScreen(gui));
	delete this;
}
