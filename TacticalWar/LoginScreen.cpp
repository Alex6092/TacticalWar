#include "LoginScreen.h"
#include "ScreenManager.h"
#include "BattleScreen.h"
#include "LinkToServer.h"
#include "SpectatorModeScreen.h"
#include "ClassSelectionScreen.h"
#include "AdminScreen.h"
#include "WaitMatchScreen.h"
#include "MusicManager.h"

using namespace tw;

float formFontSize = 16;
float formElementWidth = 200;
float formElementHeight = 25;

LoginScreen::LoginScreen(tgui::Gui * gui)
{
	readyForConnect = false;
	this->gui = gui;
	gui->removeAllWidgets();
	
	font.loadFromFile("./assets/font/neuropol_x_rg.ttf");
	
	title.setFont(font);
	title.setCharacterSize(128);
	title.setString("");
	title.setFillColor(sf::Color::White);
	//title.setStyle(sf::Text::Bold);
	title.setOutlineColor(sf::Color(255, 215, 0));
	title.setOutlineThickness(3);

	tgui::Label::Ptr loginLabel = tgui::Label::create();
	loginLabel->setInheritedFont(font);
	loginLabel->setTextSize(formFontSize);
	loginLabel->setText("Nom d'utilisateur :");
	loginLabel->setSize(formElementWidth, formElementHeight);
	
	tgui::EditBox::Ptr login = tgui::EditBox::create();
	login->setInheritedFont(font);
	login->setTextSize(formFontSize);
	login->setSize(formElementWidth, formElementHeight);
	login->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 180));

	tgui::Label::Ptr passwordLabel = tgui::Label::create();
	passwordLabel->setInheritedFont(font);
	passwordLabel->setText("Mot de passe :");
	passwordLabel->setTextSize(formFontSize);
	passwordLabel->setSize(formElementWidth, formElementHeight);

	tgui::EditBox::Ptr password = tgui::EditBox::create();
	password->setInheritedFont(font);
	password->setPasswordCharacter('*');
	password->setTextSize(formFontSize);
	password->setSize(formElementWidth, formElementHeight);
	password->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 180));

	tgui::Button::Ptr button = tgui::Button::create();
	button->setInheritedFont(font);
	button->setTextSize(formFontSize);
	button->setText("Connexion");
	button->setSize(login->getSize().x, button->getSize().y);
	button->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 180));

	button->connect("pressed", [&]() { 
		readyForConnect = true;
	});

	errorMsg = tgui::Label::create();
	errorMsg->setInheritedFont(font);
	errorMsg->setTextSize(formFontSize);
	errorMsg->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
	
	gui->add(loginLabel, "loginLabel");
	gui->add(login, "loginEdit");
	gui->add(passwordLabel, "passwordLabel");
	gui->add(password, "passwordEdit");

	gui->add(button, "connectBtn");

	gui->add(errorMsg, "errorMsg");

	LinkToServer::getInstance()->addListener(this);

	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/intro2.glsl");

	MusicManager::getInstance()->setMenuMusic();
}

LoginScreen::~LoginScreen()
{
	LinkToServer::getInstance()->removeListener(this);
}

void LoginScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	tgui::Label::Ptr loginLabel = gui->get<tgui::Label>("loginLabel");
	tgui::Label::Ptr passwordLabel = gui->get<tgui::Label>("passwordLabel");
	tgui::EditBox::Ptr login = gui->get<tgui::EditBox>("loginEdit");
	tgui::EditBox::Ptr password = gui->get<tgui::EditBox>("passwordEdit");
	tgui::Button::Ptr btn = gui->get<tgui::Button>("connectBtn");

	title.setPosition(window->getSize().x / 2 - title.getLocalBounds().width / 2, 10);

	float formX = window->getSize().x / 2 - login->getSize().x / 2;
	float formY = window->getSize().y / 2 - login->getSize().y / 2 - 50;

	loginLabel->setPosition(formX - 4, formY);
	login->setPosition(formX, formY + formElementHeight);

	passwordLabel->setPosition(formX - 4, formY + 2 * formElementHeight + 10);
	password->setPosition(formX, formY + 3 * formElementHeight + 10);

	btn->setPosition(formX, formY + 4 * formElementHeight + 20);

	errorMsg->setSize(window->getSize().x, 40);
	errorMsg->setPosition(0, formY + 5 * formElementHeight + 30);
	

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

	if (readyForConnect)
	{
		if (LinkToServer::getInstance()->Connect())
		{
			LinkToServer::getInstance()->Send("HG" + login->getText() + ";" + password->getText());
			// The sentence will be treated in onMessageReceived callback.
		}
		else
		{
			messageDuration = 5;
			errorMsg->setText("Serveur introuvable !");
		}

		readyForConnect = false;
	}
}

void LoginScreen::update(float deltatime)
{
	Screen::update(deltatime);

	// Reset du message d'erreur :
	if (messageDuration > 0)
	{
		messageDuration -= deltatime;

		if (messageDuration <= 0)
		{
			errorMsg->setText("");
			messageDuration = 0;
		}
	}

	LinkToServer::getInstance()->UpdateReceivedData();
}

void LoginScreen::render(sf::RenderWindow * window)
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
	
}

void LoginScreen::onMessageReceived(std::string msg)
{
	sf::String sentence = msg;

	if (sentence.substring(0, 2) == "HG")
	{
		readyForConnect = false;
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new BattleScreen(gui));
		delete this;
	}
	else if (sentence.substring(0, 2) == "HC")
	{
		readyForConnect = false;
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new ClassSelectionScreen(gui));
		delete this;
	}
	else if (sentence.substring(0, 2) == "HS")
	{
		readyForConnect = false;
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new SpectatorModeScreen(gui));
		delete this;
	}
	else if (sentence.substring(0, 2) == "AD")
	{
		readyForConnect = false;
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new AdminScreen(gui));
		delete this;
	}
	else if (sentence.substring(0, 2) == "HW")
	{
		readyForConnect = false;
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new WaitMatchScreen(gui));
		delete this;
	}
	else
	{
		LinkToServer::getInstance()->Disconnect();
		messageDuration = 5;
		errorMsg->setText("Login ou mot de passe incorrect ...");
	}
}

void tw::LoginScreen::onDisconnected()
{
}
