#include "LoginScreen.h"
#include "ScreenManager.h"
#include "BattleScreen.h"
#include "LinkToServer.h"
#include "SpectatorModeScreen.h"
#include "ClassSelectionScreen.h"
#include "AdminScreen.h"
#include "WaitMatchScreen.h"
#include "MusicManager.h"
#include "ClientConfig.h"
#include <nlohmann/json.hpp>
#include "ClientConfig.h"
#include "TrainingSetupScreen.h"
#include "TutorialScreen.h"
#include "SystemBrowser.h"
#include <Message.h>
#include <algorithm>

using namespace tw;

float formFontSize = 16;
float formElementWidth = 200;
float formElementHeight = 25;

LoginScreen::LoginScreen(tgui::Gui * gui)
{
	readyForConnect = false;
	trainingRequested = false;
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

	tgui::Label::Ptr serverLabel = tgui::Label::create();
	serverLabel->setInheritedFont(font);
	serverLabel->setText("Serveur :");
	serverLabel->setTextSize(formFontSize);
	serverLabel->setSize(formElementWidth, formElementHeight);

	tgui::EditBox::Ptr server = tgui::EditBox::create();
	server->setInheritedFont(font);
	server->setTextSize(formFontSize);
	server->setSize(formElementWidth, formElementHeight);
	server->setText(ClientConfig::get().getServerAddress());
	server->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 180));

	tgui::Button::Ptr button = tgui::Button::create();
	button->setInheritedFont(font);
	button->setTextSize(formFontSize);
	button->setText("Connexion");
	button->setSize(login->getSize().x, button->getSize().y);
	button->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 180));

	button->connect("pressed", [&]() { 
		readyForConnect = true;
	});

	// Entraînement contre l'ordinateur, sans serveur.
	tgui::Button::Ptr trainingButton = tgui::Button::create();
	trainingButton->setInheritedFont(font);
	trainingButton->setTextSize(formFontSize);
	trainingButton->setText(L"Entraînement");
	trainingButton->setSize(login->getSize().x, trainingButton->getSize().y);
	trainingButton->getRenderer()->setBackgroundColor(sf::Color(255, 215, 0, 180));
	trainingButton->connect("pressed", [this]() {
		trainingRequested = true;
	});

	// Tutoriel guidé, sans serveur : à faire avant le jour du tournoi.
	tgui::Button::Ptr tutorialButton = tgui::Button::create();
	tutorialButton->setInheritedFont(font);
	tutorialButton->setTextSize(formFontSize);
	tutorialButton->setText(L"Tutoriel");
	tutorialButton->setSize(login->getSize().x, tutorialButton->getSize().y);
	tutorialButton->getRenderer()->setBackgroundColor(sf::Color(120, 220, 120, 180));
	tutorialButton->connect("pressed", [this]() {
		tutorialRequested = true;
	});

	// Options : sons, mode daltonien, taille du texte, alerte de fin de tour.
	tgui::Button::Ptr optionsButton = tgui::Button::create();
	optionsButton->setInheritedFont(font);
	optionsButton->setTextSize(formFontSize);
	optionsButton->setText(L"Options");
	optionsButton->setSize(login->getSize().x, optionsButton->getSize().y);
	optionsButton->getRenderer()->setBackgroundColor(sf::Color(170, 190, 255, 180));
	optionsButton->connect("pressed", [this]() { optionsPanel->show(); });

	downloadButton = tgui::Button::create(L"Ouvrir la page de téléchargement");
	downloadButton->setInheritedFont(font);
	downloadButton->setTextSize(formFontSize);
	downloadButton->getRenderer()->setBackgroundColor(sf::Color(255, 215, 0, 220));
	downloadButton->setVisible(false);
	downloadButton->connect("pressed", [this]() { openInBrowser(downloadUrl); });

	errorMsg = tgui::Label::create();
	errorMsg->setInheritedFont(font);
	errorMsg->setTextSize(formFontSize);
	errorMsg->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
	// Lisible sur le fond animé : texte clair et contour sombre.
	errorMsg->getRenderer()->setTextColor(sf::Color(255, 215, 140));
	errorMsg->getRenderer()->setTextOutlineColor(sf::Color::Black);
	errorMsg->getRenderer()->setTextOutlineThickness(2);
	
	gui->add(loginLabel, "loginLabel");
	gui->add(login, "loginEdit");
	gui->add(passwordLabel, "passwordLabel");
	gui->add(password, "passwordEdit");
	gui->add(serverLabel, "serverLabel");
	gui->add(server, "serverEdit");

	gui->add(button, "connectBtn");
	gui->add(trainingButton, "trainingBtn");
	gui->add(tutorialButton, "tutorialBtn");
	gui->add(optionsButton, "optionsBtn");

	gui->add(errorMsg, "errorMsg");
	gui->add(downloadButton);

	optionsPanel.reset(new tw::OptionsPanel(gui, font));
	if (ClientConfig::get().openOptions)
		optionsPanel->show();

	LinkToServer::getInstance()->addListener(this);

	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/intro2.glsl");

	MusicManager::getInstance()->setMenuMusic();

	// Connexion automatique demandée en ligne de commande (une seule fois) :
	ClientConfig & config = ClientConfig::get();
	if (config.autoConnect)
	{
		config.autoConnect = false;
		login->setText(fromServerText(config.autoLogin));
		password->setText(fromServerText(config.autoPassword));
		readyForConnect = true;
	}
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
	tgui::Label::Ptr serverLabel = gui->get<tgui::Label>("serverLabel");
	tgui::EditBox::Ptr server = gui->get<tgui::EditBox>("serverEdit");
	tgui::Button::Ptr btn = gui->get<tgui::Button>("connectBtn");
	tgui::Button::Ptr trainingBtn = gui->get<tgui::Button>("trainingBtn");
	tgui::Button::Ptr tutorialBtn = gui->get<tgui::Button>("tutorialBtn");
	tgui::Button::Ptr optionsBtn = gui->get<tgui::Button>("optionsBtn");

	title.setPosition(window->getSize().x / 2 - title.getLocalBounds().width / 2, 10);

	float formX = window->getSize().x / 2 - login->getSize().x / 2;
	float formY = window->getSize().y / 2 - login->getSize().y / 2 - 50;

	loginLabel->setPosition(formX - 4, formY);
	login->setPosition(formX, formY + formElementHeight);

	passwordLabel->setPosition(formX - 4, formY + 2 * formElementHeight + 10);
	password->setPosition(formX, formY + 3 * formElementHeight + 10);

	serverLabel->setPosition(formX - 4, formY + 4 * formElementHeight + 20);
	server->setPosition(formX, formY + 5 * formElementHeight + 20);

	btn->setPosition(formX, formY + 6 * formElementHeight + 30);
	trainingBtn->setPosition(formX, formY + 7 * formElementHeight + 50);
	tutorialBtn->setPosition(formX, formY + 8 * formElementHeight + 60);
	optionsBtn->setPosition(formX, formY + 9 * formElementHeight + 70);
	optionsPanel->layout(window->getSize());

	errorMsg->setSize(window->getSize().x, 60);
	errorMsg->setPosition(0, formY + 10 * formElementHeight + 80);
	downloadButton->setSize(std::max(400.f, login->getSize().x), 32);
	downloadButton->setPosition(window->getSize().x / 2.f - downloadButton->getSize().x / 2.f, formY + 10 * formElementHeight + 128);
	

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
		std::string serverAddress = server->getText().toAnsiString();
		if (!ClientConfig::get().setServerAddress(serverAddress))
		{
			messageDuration = 5;
			errorMsg->setText("Adresse du serveur invalide (exemple : 192.168.1.10 ou 192.168.1.10:12345)");
		}
		else
		{
			ClientConfig::get().save();

			if (LinkToServer::getInstance()->Connect())
			{
				std::basic_string<sf::Uint8> loginText = login->getText().toUtf8();
				std::basic_string<sf::Uint8> passwordText = password->getText().toUtf8();
				LinkToServer::getInstance()->SendRaw("HG" + tw::protocol::loginPayload(std::string(loginText.begin(), loginText.end()),
					std::string(passwordText.begin(), passwordText.end())));
				// The sentence will be treated in onMessageReceived callback.
			}
			else
			{
				messageDuration = 5;
				errorMsg->setText("Serveur introuvable : " + ClientConfig::get().getServerAddress());
			}
		}

		readyForConnect = false;
	}

	if (trainingRequested)
	{
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new TrainingSetupScreen(gui));
		delete this;
	}
	else if (tutorialRequested)
	{
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new TutorialScreen(gui, TutorialScreen::Origin::LOGIN));
		delete this;
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

	// Joueur connecté : ses énigmes réussies sur ce poste débloquent des apparences sur son compte.
	std::string op = msg.substr(0, 2);
	if ((op == "HG" || op == "HC" || op == "HW") && !ClientConfig::get().solvedPuzzles.empty())
		LinkToServer::getInstance()->Send("PZ" + nlohmann::json({ { "solved", ClientConfig::get().solvedPuzzles } }).dump());

	if (sentence.substring(0, 2) == "HG")
	{
		int environmentId = std::atoi(sentence.substring(2).toAnsiString().c_str());

		readyForConnect = false;
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new BattleScreen(gui, environmentId));
		delete this;
	}
	else if (sentence.substring(0, 2) == "HC")
	{
		readyForConnect = false;
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new ClassSelectionScreen(gui, msg.substr(2)));
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
	else if (op == "HV")
	{
		// Ce client n'a pas la version du serveur : la connexion est refusée, la bonne version se
		// télécharge sur la page web du serveur.
		nlohmann::json version = nlohmann::json::parse(msg.substr(2), nullptr, false);
		if (version.is_object())
		{
			downloadUrl = "http://" + ClientConfig::get().serverHost + ":" + std::to_string(version.value("httpPort", 8080))
				+ version.value("page", std::string("/telecharger.html"));
			LinkToServer::getInstance()->Disconnect();
			messageDuration = 0;
			errorMsg->setText(L"Ce jeu (version " + std::to_wstring(version.value("client", 0)) + L") ne correspond pas au serveur (version "
				+ std::to_wstring(version.value("server", 0)) + L").\nTéléchargez la nouvelle version : " + fromServerText(downloadUrl));
			downloadButton->setVisible(true);
		}
	}
	else if (sentence.substring(0, 2) == "HK")
	{
		LinkToServer::getInstance()->Disconnect();
		messageDuration = 5;
		errorMsg->setText("Login ou mot de passe incorrect ...");
	}
	// Les autres messages ne concernent pas cet écran.
}

void tw::LoginScreen::onDisconnected()
{
}
