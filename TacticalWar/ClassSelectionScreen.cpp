#include "ClassSelectionScreen.h"
#include "LinkToServer.h"
#include <Match.h>
#include "MatchView.h"
#include "PlayerStatusView.h"
#include "ScreenManager.h"
#include "LoginScreen.h"
#include <CharacterFactory.h>


ClassSelectionScreen::ClassSelectionScreen(tgui::Gui * gui)
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
	subtitle.setString("Sélection de la classe");
	subtitle.setFillColor(sf::Color::Red);
	subtitle.setOutlineColor(sf::Color(255, 215, 0));
	subtitle.setOutlineThickness(1.5);

	/*
	matchPanelTitle = tgui::Label::create();
	matchPanelTitle->setInheritedFont(font);
	matchPanelTitle->setTextSize(20);
	matchPanelTitle->setText("Match(s) en cours :");
	m_matchListpanel = tgui::ScrollablePanel::create();
	m_matchListpanel->setSize(1000, 600);
	m_matchListpanel->setInheritedFont(font);
	m_matchListpanel->getRenderer()->setBackgroundColor(sf::Color(128, 128, 128));
	gui->add(matchPanelTitle);
	gui->add(m_matchListpanel);
	*/

	gui->add(PlayerStatusView::getInstance());

	LinkToServer::getInstance()->addListener(this);

	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/animatedBackground2.glsl");


	/* MAGE */
	tw::BaseCharacterModel * mage = CharacterFactory::getInstance()->constructCharacter(NULL, 1, 1, 0, 0);
	std::string pathMage = mage->getClassIconPath();
	sf::Texture TextureMage;
	TextureMage.loadFromFile(pathMage);
	tgui::Picture::Ptr IconMage = tgui::Picture::create(TextureMage);
	IconMage->setSize(70, 75);
	IconMage->setPosition(630, 250);

	std::string graphicPathMage = mage->getGraphicsPath();
	sf::Texture TextureGraphicMage;
	TextureGraphicMage.loadFromFile(graphicPathMage);
	tgui::Picture::Ptr GraphicMage = tgui::Picture::create(TextureGraphicMage);
	
	


	/*ARCHER*/
	tw::BaseCharacterModel * archer = CharacterFactory::getInstance()->constructCharacter(NULL, 2, 1, 0, 0);
	std::string pathArcher = archer->getClassIconPath();
	sf::Texture TextureArcher;
	TextureArcher.loadFromFile(pathArcher);
	tgui::Picture::Ptr IconArcher = tgui::Picture::create(TextureArcher);
	IconArcher->setPosition(830, 350);
	/*PROTECTEUR*/
	tw::BaseCharacterModel * protecteur = CharacterFactory::getInstance()->constructCharacter(NULL, 3, 1, 0, 0);
	std::string pathProtecteur = protecteur->getClassIconPath();
	sf::Texture TextureProtecteur;
	TextureProtecteur.loadFromFile(pathProtecteur);
	tgui::Picture::Ptr IconProtecteur = tgui::Picture::create(TextureProtecteur);
	IconProtecteur->setPosition(830, 350);
	/*BARBARE*/
	tw::BaseCharacterModel * barbare = CharacterFactory::getInstance()->constructCharacter(NULL, 4, 1, 0, 0);
	std::string pathBarbare = barbare->getClassIconPath();
	sf::Texture TextureBarbare;
	TextureBarbare.loadFromFile(pathBarbare);
	tgui::Picture::Ptr IconBarbare = tgui::Picture::create(TextureBarbare);
	IconBarbare->setPosition(830, 350);
	
	tgui::Button::Ptr buttonSuivant = tgui::Button::create();
	buttonSuivant->setInheritedFont(font);
	buttonSuivant->setText("suivant");
	buttonSuivant->setSize(200, 100);

	tgui::Button::Ptr buttonPrecedent = tgui::Button::create();
	buttonPrecedent->setInheritedFont(font);
	buttonPrecedent->setText("precedent");
	buttonPrecedent->setSize(200, 100);

	m_matchListpanel = tgui::ScrollablePanel::create();
	m_matchListpanel->setSize(1000, 500);
	m_matchListpanel->setPosition(550, 250);
	m_matchListpanel->setInheritedFont(font);
	m_matchListpanel->getRenderer()->setBackgroundColor(sf::Color(128, 128, 128, 128));


	gui->add(m_matchListpanel);
	gui->add(IconMage, "mageIcon");
	gui->add(IconArcher, "archerIcon");
	gui->add(IconProtecteur, "protecteurIcon");
	gui->add(IconBarbare, "barbareIcon");

	gui->add(GraphicMage, "graphicMage");
	
	gui->add(buttonSuivant, "buttonSuivant");
	gui->add(buttonPrecedent, "buttonPrecedent");



}

ClassSelectionScreen::~ClassSelectionScreen()
{
	LinkToServer::getInstance()->removeListener(this);
}

void ClassSelectionScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	title.setPosition(window->getSize().x / 2 - title.getLocalBounds().width / 2, 10);
	subtitle.setPosition(window->getSize().x / 2 - subtitle.getLocalBounds().width / 2, 10 + 128 + 10);
	//matchPanelTitle->setPosition(window->getSize().x / 2.0 - m_matchListpanel->getSize().x / 2.0, 270);
	//m_matchListpanel->setPosition(window->getSize().x / 2.0 - m_matchListpanel->getSize().x / 2.0, 300);

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

void ClassSelectionScreen::update(float deltatime)
{
	Screen::update(deltatime);
	LinkToServer::getInstance()->UpdateReceivedData();
}

void ClassSelectionScreen::render(sf::RenderWindow * window)
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

	tgui::Button::Ptr btnSuivant = gui->get<tgui::Button>("buttonSuivant");
	btnSuivant->setPosition(1250, 800);

	tgui::Button::Ptr btnPrecedent = gui->get<tgui::Button>("buttonPrecedent");
	btnPrecedent->setPosition(400, 800);


	

	tgui::Picture::Ptr archer = gui->get<tgui::Picture>("archerIcon");
	archer->setVisible(false);

	tgui::Picture::Ptr protecteur = gui->get<tgui::Picture>("protecteurIcon");
	protecteur->setVisible(false);

	tgui::Picture::Ptr barbare = gui->get<tgui::Picture>("barbareIcon");
	barbare->setVisible(false);

	tgui::Picture::Ptr graphicMage = gui->get<tgui::Picture>("graphicMage");
	graphicMage->setPosition(630, 350);
	

	// TODO envoyer une tram contenant PC + L'id de la classe ex (PC2 pour archer) lorsque le joueur verouille son choix !

	btnSuivant->connect("pressed", [&]() {

		tgui::Picture::Ptr mage = gui->get<tgui::Picture>("mageIcon");
		mage->setVisible(false);

		tgui::Picture::Ptr archer = gui->get<tgui::Picture>("archerIcon");
		archer->setVisible(true);
	});

	/*tgui::Picture::Ptr mage = gui->get<tgui::Picture>("mageIcon");
	mage->setVisible(false);

	tgui::Picture::Ptr archer = gui->get<tgui::Picture>("archerIcon");
	archer->setVisible(true);

	tgui::Picture::Ptr protecteur = gui->get<tgui::Picture>("protecteurIcon");
	protecteur->setVisible(false);

	tgui::Picture::Ptr barbare = gui->get<tgui::Picture>("barbareIcon");
	barbare->setVisible(false);
	*/
	
}

void ClassSelectionScreen::onMessageReceived(std::string msg)
{
	sf::String m = msg;

	// Le status des joueurs est géré dans PlayerStatusView (widget autonome)
}

void ClassSelectionScreen::onDisconnected()
{
	gui->removeAllWidgets();
	tw::ScreenManager::getInstance()->setCurrentScreen(new tw::LoginScreen(gui));
	delete this;
}
