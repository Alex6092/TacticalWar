#include "SpectatorModeScreen.h"
#include "BattleScreen.h"
#include "ClientConfig.h"
#include "LinkToServer.h"
#include "LoginScreen.h"
#include "ScreenManager.h"

#include <Message.h>

bool SpectatorModeScreen::directorMode = false;
nlohmann::json SpectatorModeScreen::highlights = nlohmann::json::array();
float SpectatorModeScreen::highlightsAge = 1e9f;
std::size_t SpectatorModeScreen::nextHighlight = 0;
sf::String SpectatorModeScreen::currentTab = L"En direct";

SpectatorModeScreen::SpectatorModeScreen(tgui::Gui * gui)
	: Screen(), gui(gui), sinceRefresh(0), watchPending(0)
{
	gui->removeAllWidgets();
	font.loadFromFile("./assets/font/neuropol_x_rg.ttf");

	// Option --director : le mode réalisateur est actif dès le premier affichage.
	static bool configApplied = false;
	if (!configApplied)
	{
		directorMode = directorMode || ClientConfig::get().directorMode;
		configApplied = true;
	}

	title.setFont(font);
	title.setCharacterSize(128);
	title.setString("Tactical War");
	title.setFillColor(sf::Color::White);
	title.setOutlineColor(sf::Color(255, 215, 0));
	title.setOutlineThickness(3);

	subtitle.setFont(font);
	subtitle.setCharacterSize(32);
	subtitle.setString("Mode spectateur");
	subtitle.setFillColor(sf::Color::Red);
	subtitle.setOutlineColor(sf::Color(255, 215, 0));
	subtitle.setOutlineThickness(1.5);

	sessionsPanel.reset(new LiveSessionsPanel(gui, font));
	sessionsPanel->onWatch = [this](int session) { watch(session); };

	replaysPanel.reset(new ReplaysPanel(gui, font));
	replaysPanel->onWatch = [this](const std::string & id) {
		watchPending = 5.f;
		LinkToServer::getInstance()->SendRaw("RP" + nlohmann::json({ { "id", id } }).dump());
	};

	tabs = tgui::Tabs::create();
	tabs->setInheritedFont(font);
	tabs->setTextSize(18);
	tabs->setTabHeight(34);
	tabs->add(L"En direct", false);
	tabs->add(L"Rediffusions", false);
	tabs->connect("TabSelected", [this](const sf::String & tab) { showTab(tab); });
	gui->add(tabs);

	directorBox = tgui::CheckBox::create(L"Mode réalisateur");
	directorBox->setInheritedFont(font);
	directorBox->setTextSize(18);
	directorBox->getRenderer()->setTextColor(sf::Color::White);
	directorBox->setChecked(directorMode);
	directorBox->connect("Changed", [this](bool checked) {
		directorMode = checked;
		sinceRefresh = 100;
	});
	gui->add(directorBox);

	directorHelp = tgui::Label::create(L"Enchaîne automatiquement les combats les plus serrés, caméra sur le personnage actif.");
	directorHelp->setInheritedFont(font);
	directorHelp->setTextSize(14);
	directorHelp->getRenderer()->setTextColor(sf::Color(200, 200, 200));
	gui->add(directorHelp);

	LinkToServer::getInstance()->addListener(this);
	LinkToServer::getInstance()->SendRaw("SL{}");
	if (!tabs->select(currentTab))
		tabs->select(0);

	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/animatedBackground2.glsl");
}

SpectatorModeScreen::~SpectatorModeScreen()
{
	LinkToServer::getInstance()->removeListener(this);
}

void SpectatorModeScreen::showTab(const sf::String & tab)
{
	currentTab = tab;
	bool replays = tab == L"Rediffusions";
	sessionsPanel->setVisible(!replays);
	replaysPanel->setVisible(replays);
	if (replays)
		LinkToServer::getInstance()->SendRaw("RL{}");
}

void SpectatorModeScreen::watch(int session)
{
	if (session <= 0)
		return;
	watchPending = 5.f;
	LinkToServer::getInstance()->SendRaw("SW" + nlohmann::json({ { "session", session } }).dump());
}

void SpectatorModeScreen::playNextHighlight()
{
	// Liste vieille d'une minute (ou vide) : redemandée, au plus toutes les 10 secondes.
	if ((highlights.empty() || highlightsAge > 60.f) && highlightRequest <= 0)
	{
		highlightRequest = 10.f;
		LinkToServer::getInstance()->SendRaw("HL{}");
	}
	if (highlights.empty())
	{
		sessionsPanel->setStatus(L"Mode réalisateur : en attente d'un combat...", sf::Color(200, 220, 255));
		return;
	}

	const nlohmann::json & highlight = highlights[nextHighlight % highlights.size()];
	nextHighlight++;
	sessionsPanel->setStatus(L"Mode réalisateur : temps fort « " + fromServerText(highlight.value("title", std::string())) + L" »",
		sf::Color(255, 215, 120));
	watchPending = 5.f;
	LinkToServer::getInstance()->SendRaw("RP" + nlohmann::json({ { "id", highlight.value("replay", std::string()) },
		{ "from", highlight.value("from", 0) }, { "to", highlight.value("to", 0) } }).dump());
}

void SpectatorModeScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	title.setPosition(window->getSize().x / 2 - title.getLocalBounds().width / 2, 10);
	subtitle.setPosition(window->getSize().x / 2 - subtitle.getLocalBounds().width / 2, 10 + 128 + 10);
	sessionsPanel->layout(window->getSize(), 300);
	replaysPanel->layout(window->getSize(), 300);

	float panelLeft = sessionsPanel->getGroup()->getPosition().x;
	directorBox->setPosition(panelLeft, 214);
	directorBox->setSize(22, 22);
	directorHelp->setPosition(panelLeft + 260, 217);
	tabs->setPosition(panelLeft, 252);

	sf::Event event;
	while (window->pollEvent(event))
	{
		if (event.type == sf::Event::Closed)
			window->close();
		else if (event.type == sf::Event::Resized)
		{
			sf::View view = window->getView();
			view.setSize(event.size.width, event.size.height);
			view.setCenter(event.size.width / 2.f, event.size.height / 2.f);
			window->setView(view);
		}

		gui->handleEvent(event);
	}
}

void SpectatorModeScreen::update(float deltatime)
{
	Screen::update(deltatime);

	if (watchPending > 0)
		watchPending -= deltatime;
	highlightsAge += deltatime;
	if (highlightRequest > 0)
		highlightRequest -= deltatime;

	// Mode réalisateur : dès qu'un combat est regardable, on le rejoint.
	sinceRefresh += deltatime;
	if (directorMode && watchPending <= 0 && sinceRefresh > 2.f && currentTab != L"Rediffusions")
	{
		sinceRefresh = 0;
		int session = sessionsPanel->mostContestedSession();
		if (session > 0)
		{
			sessionsPanel->setStatus(L"Mode réalisateur : connexion au combat le plus serré...", sf::Color(200, 220, 255));
			watch(session);
		}
		else
		{
			// Aucun combat : les temps forts des derniers combats, en attendant le prochain.
			playNextHighlight();
		}
	}

	LinkToServer::getInstance()->UpdateReceivedData();
}

void SpectatorModeScreen::render(sf::RenderWindow * window)
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

void SpectatorModeScreen::onMessageReceived(std::string msg)
{
	tw::protocol::Message message;
	if (!tw::protocol::Message::decode(msg, message))
		return;

	if (message.op == "SL")
	{
		nlohmann::json body;
		if (message.parseJson(body))
			sessionsPanel->onSessionList(body);
	}
	else if (message.op == "HL")
	{
		nlohmann::json body;
		if (message.parseJson(body))
		{
			highlights = body.value("highlights", nlohmann::json::array());
			highlightsAge = 0;
			nextHighlight = 0;
		}
	}
	else if (message.op == "RL")
	{
		nlohmann::json body;
		if (message.parseJson(body))
			replaysPanel->onReplayList(body);
	}
	else if (message.op == "ER")
	{
		nlohmann::json body;
		watchPending = 0;
		if (message.parseJson(body))
		{
			sf::String error = fromServerText(body.value("message", std::string()));
			sessionsPanel->setStatus(error, sf::Color(255, 120, 100));
			replaysPanel->setStatus(error, sf::Color(255, 120, 100));
		}
	}
	else if (message.op == "HG")
	{
		// Le serveur envoie la carte du combat regardé, puis son état complet (BI).
		int environmentId = std::atoi(msg.substr(2).c_str());
		gui->removeAllWidgets();
		tw::ScreenManager::getInstance()->setCurrentScreen(new tw::BattleScreen(gui, environmentId, tw::BattleScreen::Mode::SPECTATOR));
		delete this;
	}
}

void SpectatorModeScreen::onDisconnected()
{
	gui->removeAllWidgets();
	tw::ScreenManager::getInstance()->setCurrentScreen(new tw::LoginScreen(gui));
	delete this;
}
