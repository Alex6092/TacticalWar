#include "SpectatorModeScreen.h"
#include "BattleScreen.h"
#include "ClientConfig.h"
#include "LinkToServer.h"
#include "LoginScreen.h"
#include "ScreenManager.h"

#include <Message.h>

bool SpectatorModeScreen::directorMode = false;

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

	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/animatedBackground2.glsl");
}

SpectatorModeScreen::~SpectatorModeScreen()
{
	LinkToServer::getInstance()->removeListener(this);
}

void SpectatorModeScreen::watch(int session)
{
	if (session <= 0)
		return;
	watchPending = 5.f;
	LinkToServer::getInstance()->SendRaw("SW" + nlohmann::json({ { "session", session } }).dump());
}

void SpectatorModeScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	title.setPosition(window->getSize().x / 2 - title.getLocalBounds().width / 2, 10);
	subtitle.setPosition(window->getSize().x / 2 - subtitle.getLocalBounds().width / 2, 10 + 128 + 10);
	sessionsPanel->layout(window->getSize(), 270);

	float panelLeft = sessionsPanel->getGroup()->getPosition().x;
	directorBox->setPosition(panelLeft, 230);
	directorBox->setSize(22, 22);
	directorHelp->setPosition(panelLeft + 260, 233);

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

	// Mode réalisateur : dès qu'un combat est regardable, on le rejoint.
	sinceRefresh += deltatime;
	if (directorMode && watchPending <= 0 && sinceRefresh > 2.f)
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
			sessionsPanel->setStatus(L"Mode réalisateur : en attente d'un combat...", sf::Color(200, 220, 255));
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
	else if (message.op == "ER")
	{
		nlohmann::json body;
		watchPending = 0;
		if (message.parseJson(body))
			sessionsPanel->setStatus(fromServerText(body.value("message", std::string())), sf::Color(255, 120, 100));
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
