#include "SpectatorModeScreen.h"
#include "BattleScreen.h"
#include "ClientConfig.h"
#include "LinkToServer.h"
#include "LoginScreen.h"
#include "ScreenManager.h"

#include <Message.h>

#include <iostream>

namespace
{
	// Un moment d'un combat en cours est proposé pendant 3 minutes.
	const int LIVE_MOMENT_MAX_AGE = 180;
	// Le réalisateur quitte un direct pour un moment d'un autre combat au plus une fois par minute.
	const float LIVE_SWITCH_SECONDS = 60.f;
}

bool SpectatorModeScreen::directorMode = false;
nlohmann::json SpectatorModeScreen::highlights = nlohmann::json::array();
float SpectatorModeScreen::highlightsAge = 1e9f;
std::set<std::string> SpectatorModeScreen::played;
int SpectatorModeScreen::liveSession = 0;
float SpectatorModeScreen::lastSwitch = -1e9f;
sf::Clock SpectatorModeScreen::directorClock;
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
	// Réalisateur : choix suivant dès l'arrivée des listes (enchaînement sans attente).
	liveSession = 0;
	if (directorMode)
	{
		decidePending = true;
		highlightRequest = 5.f;
		LinkToServer::getInstance()->SendRaw("HL{}");
	}

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

std::string SpectatorModeScreen::keyOf(const nlohmann::json & highlight)
{
	return highlight.value("live", false) ? "s" + std::to_string(highlight.value("session", 0)) + ":" + std::to_string(highlight.value("from", 0))
		: "r" + highlight.value("replay", std::string()) + ":" + std::to_string(highlight.value("from", 0));
}

void SpectatorModeScreen::direct()
{
	sinceRefresh = 0;
	decidePending = false;
	// Liste vieille de plus de 10 secondes : redemandée pour le choix suivant.
	if (highlightsAge > 10.f && highlightRequest <= 0)
	{
		highlightRequest = 5.f;
		LinkToServer::getInstance()->SendRaw("HL{}");
	}

	// 1. Le meilleur moment pas encore vu des combats en cours (léger différé).
	const nlohmann::json * best = nullptr;
	for (const nlohmann::json & highlight : highlights)
	{
		if (!highlight.value("live", false) || highlight.value("age", 0) >= LIVE_MOMENT_MAX_AGE || played.count(keyOf(highlight)) > 0)
			continue;
		if (best == nullptr || highlight.value("score", 0) > best->value("score", 0))
			best = &highlight;
	}
	if (best != nullptr)
	{
		playExtract(*best);
		return;
	}

	// 2. Le combat en cours le plus serré, en direct.
	int session = sessionsPanel->mostContestedSession();
	if (session > 0)
	{
		sessionsPanel->setStatus(L"Mode réalisateur : connexion au combat le plus serré...", sf::Color(200, 220, 255));
		std::cout << "Realisateur : combat " << session << " en direct" << std::endl;
		watch(session);
		liveSession = session;
		lastSwitch = directorClock.getElapsedTime().asSeconds();
		return;
	}

	// 3. Les temps forts des rediffusions, chacun une fois ; tous joués : on recommence.
	for (int pass = 0; pass < 2; pass++)
	{
		for (const nlohmann::json & highlight : highlights)
		{
			if (!highlight.value("live", false) && played.count(keyOf(highlight)) == 0)
			{
				playExtract(highlight);
				return;
			}
		}
		for (auto it = played.begin(); it != played.end();)
			it = (*it)[0] == 'r' ? played.erase(it) : std::next(it);
	}
	sessionsPanel->setStatus(L"Mode réalisateur : en attente d'un combat...", sf::Color(200, 220, 255));
}

void SpectatorModeScreen::playExtract(const nlohmann::json & highlight)
{
	played.insert(keyOf(highlight));
	liveSession = 0;
	bool live = highlight.value("live", false);
	std::string title = highlight.value("title", std::string());
	sessionsPanel->setStatus((live ? sf::String(L"Mode réalisateur : à l'instant, « ") : sf::String(L"Mode réalisateur : temps fort « "))
		+ fromServerText(title) + L" »", sf::Color(255, 215, 120));
	std::cout << "Realisateur : extrait " << (live ? "en direct du combat " + std::to_string(highlight.value("session", 0))
		+ " (il y a " + std::to_string(highlight.value("age", 0)) + " s)" : "de la rediffusion " + highlight.value("replay", std::string()))
		<< " : " << title << std::endl;
	watchPending = 5.f;
	nlohmann::json request = { { "from", highlight.value("from", 0) }, { "to", highlight.value("to", 0) } };
	if (live)
		request["session"] = highlight.value("session", 0);
	else
		request["id"] = highlight.value("replay", std::string());
	LinkToServer::getInstance()->SendRaw("RP" + request.dump());
}

bool SpectatorModeScreen::onLiveHighlights(const nlohmann::json & body)
{
	highlights = body.value("highlights", nlohmann::json::array());
	highlightsAge = 0;
	bool elsewhere = false;
	for (const nlohmann::json & highlight : highlights)
	{
		if (!highlight.value("live", false))
			continue;
		// Les moments du combat regardé ne seront pas rejoués.
		if (highlight.value("session", 0) == liveSession)
			played.insert(keyOf(highlight));
		else if (highlight.value("age", 0) < LIVE_MOMENT_MAX_AGE && played.count(keyOf(highlight)) == 0)
			elsewhere = true;
	}
	return elsewhere && directorClock.getElapsedTime().asSeconds() - lastSwitch >= LIVE_SWITCH_SECONDS;
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

	// Mode réalisateur : choix suivant dès l'arrivée des listes (au plus 1,5 s d'attente), puis
	// toutes les 2 secondes tant que rien n'est regardable.
	sinceRefresh += deltatime;
	if (directorMode && watchPending <= 0 && currentTab != L"Rediffusions"
		&& (decidePending ? (sessionsReceived && highlightsReceived) || sinceRefresh > 1.5f : sinceRefresh > 2.f))
		direct();

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
		sessionsReceived = true;
	}
	else if (message.op == "HL")
	{
		nlohmann::json body;
		if (message.parseJson(body))
		{
			highlights = body.value("highlights", nlohmann::json::array());
			highlightsAge = 0;
		}
		highlightsReceived = true;
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
