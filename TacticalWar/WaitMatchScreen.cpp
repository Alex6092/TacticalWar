#include "WaitMatchScreen.h"
#include "LinkToServer.h"
#include <Message.h>
#include "PlayerStatusView.h"
#include "ScreenManager.h"
#include "ClassSelectionScreen.h"
#include "LoginScreen.h"

#include <cmath>

namespace
{
	const float PANEL_WIDTH = 760;
	const float PANEL_HEIGHT = 420;
	const float PREFERENCES_WIDTH = 600;
	// Liste des équipes redemandée toutes les 5 secondes.
	const float LIST_PERIOD = 5;

	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}

	void sendJson(const std::string & op, const nlohmann::json & body)
	{
		LinkToServer::getInstance()->SendRaw(op + body.dump());
	}
}

WaitMatchScreen::WaitMatchScreen(tgui::Gui * gui)
	: Screen()
{
	this->gui = gui;
	gui->removeAllWidgets();
	font.loadFromFile("./assets/font/neuropol_x_rg.ttf");
	textFont.loadFromFile("./assets/font/OpenSans-Regular.ttf");

	title.setFont(font);
	title.setCharacterSize(128);
	title.setString("Tactical War");
	title.setFillColor(sf::Color::White);
	title.setOutlineColor(sf::Color(255, 215, 0));
	title.setOutlineThickness(3);

	subtitle.setFont(font);
	subtitle.setCharacterSize(32);
	subtitle.setString("En attente d'un match ...");
	subtitle.setFillColor(sf::Color::Red);
	subtitle.setOutlineColor(sf::Color(255, 215, 0));
	subtitle.setOutlineThickness(1.5);

	createChallengePanel();
	preferences.reset(new tw::PreferencesPanel(gui, font, textFont));
	gui->add(preferences->getPanel());

	LinkToServer::getInstance()->addListener(this);
	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/animatedBackground2.glsl");
	sendJson("DL", nlohmann::json::object());
}

WaitMatchScreen::~WaitMatchScreen()
{
	LinkToServer::getInstance()->removeListener(this);
}

void WaitMatchScreen::createChallengePanel()
{
	challengePanel = tgui::Panel::create();
	challengePanel->getRenderer()->setBackgroundColor(sf::Color(15, 15, 25, 225));
	challengePanel->getRenderer()->setBorders(2);
	challengePanel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	gui->add(challengePanel);

	challengeTitle = tgui::Label::create(L"Défier une équipe");
	challengeTitle->setInheritedFont(font);
	challengeTitle->setTextSize(22);
	challengeTitle->getRenderer()->setTextColor(sf::Color(255, 215, 0));
	challengeTitle->setPosition(20, 14);
	challengePanel->add(challengeTitle);

	challengeNote = tgui::Label::create(L"Match amical en attendant le tournoi : l'équipe défiée a 30 s pour accepter.");
	challengeNote->setInheritedFont(textFont);
	challengeNote->setTextSize(15);
	challengeNote->getRenderer()->setTextColor(sf::Color(200, 200, 210));
	challengeNote->setPosition(20, 50);
	challengePanel->add(challengeNote);

	teamList = tgui::ListView::create();
	teamList->setInheritedFont(textFont);
	teamList->setTextSize(16);
	teamList->setItemHeight(30);
	teamList->setHeaderHeight(28);
	teamList->addColumn(L"Équipe", 250);
	teamList->addColumn(L"Joueurs connectés", 250);
	teamList->addColumn(L"Disponible", 220);
	teamList->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 225));
	teamList->connect("DoubleClicked", [this]() { sendChallenge(); });
	challengePanel->add(teamList);

	challengeButton = tgui::Button::create(L"Défier");
	challengeButton->setInheritedFont(font);
	challengeButton->setTextSize(18);
	challengeButton->getRenderer()->setBackgroundColor(sf::Color(255, 215, 0, 210));
	challengeButton->connect("pressed", [this]() { sendChallenge(); });
	challengePanel->add(challengeButton);

	challengeStatus = tgui::Label::create();
	challengeStatus->setInheritedFont(textFont);
	challengeStatus->setTextSize(16);
	challengePanel->add(challengeStatus);

	invitePanel = tgui::Panel::create({ 560, 220 });
	invitePanel->getRenderer()->setBackgroundColor(sf::Color(25, 20, 10, 245));
	invitePanel->getRenderer()->setBorders(3);
	invitePanel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	invitePanel->setVisible(false);
	gui->add(invitePanel);

	inviteText = tgui::Label::create();
	inviteText->setInheritedFont(textFont);
	inviteText->setTextSize(20);
	inviteText->setMaximumTextWidth(520);
	inviteText->getRenderer()->setTextColor(sf::Color::White);
	inviteText->setPosition(20, 20);
	invitePanel->add(inviteText);

	acceptButton = tgui::Button::create(L"Accepter");
	acceptButton->setInheritedFont(font);
	acceptButton->setTextSize(18);
	acceptButton->setSize(220, 50);
	acceptButton->setPosition(40, 150);
	acceptButton->getRenderer()->setBackgroundColor(sf::Color(110, 210, 110));
	acceptButton->connect("pressed", [this]() { answer(true); });
	invitePanel->add(acceptButton);

	declineButton = tgui::Button::create(L"Refuser");
	declineButton->setInheritedFont(font);
	declineButton->setTextSize(18);
	declineButton->setSize(220, 50);
	declineButton->setPosition(300, 150);
	declineButton->getRenderer()->setBackgroundColor(sf::Color(230, 120, 110));
	declineButton->connect("pressed", [this]() { answer(false); });
	invitePanel->add(declineButton);
}

void WaitMatchScreen::layout(const sf::Vector2u & size)
{
	bool compact = size.y < 900;
	float top = compact ? 140.f : 220.f;
	// Défis à gauche, préférences à droite.
	float available = (float)size.x - 60.f;
	float width = std::min(PANEL_WIDTH, available * 0.52f);
	float preferencesWidth = std::min(PREFERENCES_WIDTH, available - width);
	float left = (size.x - (width + 20.f + preferencesWidth)) / 2.f;
	float height = std::min(PANEL_HEIGHT, (float)size.y - top - 30.f);
	challengePanel->setSize(width, height);
	challengePanel->setPosition(left, top);
	preferences->getPanel()->setPosition(left + width + 20.f, top);
	preferences->layout(preferencesWidth, height);
	teamList->setPosition(20, 84);
	teamList->setSize(width - 40, height - 84 - 70);
	// Colonnes à la largeur de la liste (sans barre de défilement horizontale).
	float columns = width - 40 - 24;
	teamList->setColumnWidth(0, columns * 0.32f);
	teamList->setColumnWidth(1, columns * 0.40f);
	teamList->setColumnWidth(2, columns * 0.28f);
	challengeButton->setPosition(20, height - 58);
	challengeButton->setSize(180, 44);
	challengeStatus->setPosition(216, height - 48);
	challengeStatus->setMaximumTextWidth(width - 236);
	invitePanel->setPosition((size.x - 560.f) / 2.f, (size.y - 220.f) / 2.f);
}

void WaitMatchScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	bool compact = window->getSize().y < 900;
	title.setCharacterSize(compact ? 76 : 128);
	subtitle.setCharacterSize(compact ? 24 : 32);
	title.setPosition(window->getSize().x / 2 - title.getLocalBounds().width / 2, compact ? 4.f : 10.f);
	subtitle.setPosition(window->getSize().x / 2 - subtitle.getLocalBounds().width / 2, compact ? 90.f : 10 + 128 + 10);
	layout(window->getSize());

	sf::Event event;
	while (window->pollEvent(event))
	{
		if (event.type == sf::Event::Closed)
			window->close();
		else if (event.type == sf::Event::Resized)
		{
			sf::View view = window->getView();
			view.setSize(event.size.width, event.size.height);
			window->setView(view);
		}

		gui->handleEvent(event);
	}
}

void WaitMatchScreen::update(float deltatime)
{
	Screen::update(deltatime);

	listRefresh += deltatime;
	if (listRefresh >= LIST_PERIOD)
	{
		listRefresh = 0;
		sendJson("DL", nlohmann::json::object());
	}

	if (invitePanel->isVisible())
	{
		inviteRemaining -= deltatime;
		if (inviteRemaining <= 0)
		{
			invitePanel->setVisible(false);
			setStatus(L"Défi de " + inviteTeam + L" expiré.", sf::Color(255, 200, 120));
		}
		else
		{
			inviteText->setText(inviteTeam + L" vous défie pour un match amical !\nRéponse avant "
				+ num((int)std::ceil(inviteRemaining)) + L" s (le premier de l'équipe qui répond décide).");
		}
	}

	LinkToServer::getInstance()->UpdateReceivedData();
}

void WaitMatchScreen::render(sf::RenderWindow * window)
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

void WaitMatchScreen::onTeamList(const nlohmann::json & body)
{
	int selected = -1;
	int index = teamList->getSelectedItemIndex();
	if (index >= 0 && index < (int)teams.size())
		selected = teams[index].value("id", 0);

	teams = body.value("teams", nlohmann::json::array());
	teamList->removeAllItems();
	for (const nlohmann::json & team : teams)
	{
		sf::String players;
		for (const nlohmann::json & player : team.value("online", nlohmann::json::array()))
			players += (players.isEmpty() ? sf::String() : sf::String(L", ")) + fromServerText(player.get<std::string>());
		bool allowed = team.value("allowed", false);
		teamList->addItem({ fromServerText(team.value("name", std::string())), players.isEmpty() ? sf::String(L"aucun") : players,
			allowed ? sf::String(L"oui") : fromServerText(team.value("reason", std::string())) });
	}
	for (std::size_t i = 0; i < teams.size(); i++)
	{
		if (teams[i].value("id", 0) == selected)
			teamList->setSelectedItem(i);
	}

	// Tournoi en cours ou défi impossible : le panneau l'explique.
	std::string closed = body.value("closed", std::string());
	challengeButton->setEnabled(closed.empty());
	challengeNote->setText(closed.empty() ? sf::String(L"Match amical en attendant le tournoi : l'équipe défiée a 30 s pour accepter.")
		: fromServerText(closed));
}

void WaitMatchScreen::sendChallenge()
{
	int index = teamList->getSelectedItemIndex();
	if (index < 0 || index >= (int)teams.size())
	{
		setStatus(L"Choisissez une équipe dans la liste.", sf::Color(255, 200, 120));
		return;
	}
	const nlohmann::json & team = teams[index];
	if (!team.value("allowed", false))
	{
		setStatus(fromServerText(team.value("reason", std::string())), sf::Color(255, 200, 120));
		return;
	}
	sendJson("DD", { { "team", team.value("id", 0) } });
	setStatus(L"Défi envoyé à " + fromServerText(team.value("name", std::string())) + L"...", sf::Color(200, 220, 255));
}

void WaitMatchScreen::answer(bool accept)
{
	invitePanel->setVisible(false);
	sendJson("DA", { { "from", inviteFrom }, { "accept", accept } });
	setStatus(accept ? sf::String(L"Défi accepté : le match se prépare...") : sf::String(L"Défi refusé."), sf::Color(200, 220, 255));
}

void WaitMatchScreen::setStatus(const sf::String & text, const sf::Color & color)
{
	challengeStatus->setText(text);
	challengeStatus->getRenderer()->setTextColor(color);
}

void WaitMatchScreen::onMessageReceived(std::string msg)
{
	sf::String m = msg;

	if (m.substring(0, 2) == "PA")
	{
		preferences->refreshAppearances();
		return;
	}
	if (m.substring(0, 2) == "HC")
	{
		gui->removeAllWidgets();
		ClassSelectionScreen * classScreen = new ClassSelectionScreen(gui, msg.substr(2));
		classScreen->setShaderEllapsedTime(getShaderEllapsedTime());
		tw::ScreenManager::getInstance()->setCurrentScreen(classScreen);
		delete this;
		return;
	}

	tw::protocol::Message message;
	nlohmann::json body;
	if (!tw::protocol::Message::decode(msg, message) || !message.parseJson(body))
		return;
	if (message.op == "DL")
	{
		onTeamList(body);
	}
	else if (message.op == "DI")
	{
		inviteFrom = body.value("from", 0);
		inviteTeam = fromServerText(body.value("name", std::string()));
		inviteRemaining = body.value("seconds", 30.f);
		invitePanel->setVisible(true);
		invitePanel->moveToFront();
	}
	else if (message.op == "DR")
	{
		bool ok = body.value("ok", false);
		// Défi retiré (expiré, un coéquipier a répondu) : la fenêtre se ferme.
		if (body.value("from", 0) == inviteFrom)
			invitePanel->setVisible(false);
		setStatus(fromServerText(body.value("message", std::string())), ok ? sf::Color(140, 255, 140) : sf::Color(255, 160, 120));
		sendJson("DL", nlohmann::json::object());
	}
}

void WaitMatchScreen::onDisconnected()
{
	gui->removeAllWidgets();
	tw::ScreenManager::getInstance()->setCurrentScreen(new tw::LoginScreen(gui));
	delete this;
}
