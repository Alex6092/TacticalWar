#include "TrainingSetupScreen.h"

#include <algorithm>
#include <string>

#include "ClientGameData.h"
#include "LinkToServer.h"
#include "LoginScreen.h"
#include "ScreenManager.h"
#include "TrainingScreen.h"

using namespace tw;

namespace
{
	const float PANEL_WIDTH = 780;
	const float LABEL_WIDTH = 230;
	const float ROW_HEIGHT = 40;
	const float FIELD_HEIGHT = 28;
	const unsigned int TEXT_SIZE = 16;
}

TrainingSetupScreen::TrainingSetupScreen(tgui::Gui * gui)
	: gui(gui), request(Request::NONE)
{
	gui->removeAllWidgets();
	font.loadFromFile("./assets/font/neuropol_x_rg.ttf");

	title.setFont(font);
	title.setCharacterSize(72);
	title.setString(L"Entraînement");
	title.setFillColor(sf::Color::White);
	title.setOutlineColor(sf::Color(255, 215, 0));
	title.setOutlineThickness(3);

	panel = tgui::Panel::create();
	panel->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 220));
	panel->getRenderer()->setBorders(2);
	panel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	gui->add(panel);

	const TrainingSettings & settings = TrainingSettings::current();

	tgui::Label::Ptr help = tgui::Label::create(L"Combat contre l'ordinateur, sans serveur, avec les règles du tournoi. "
		L"Placez votre personnage, cliquez sur « Prêt », puis jouez votre tour.");
	help->setInheritedFont(font);
	help->setTextSize(14);
	help->getRenderer()->setTextColor(sf::Color(220, 220, 220));
	help->setPosition(20, 14);
	help->setSize(PANEL_WIDTH - 40, 60);
	panel->add(help);

	format = addRow(L"Format");
	format->addItem(L"2 contre 2 (avec un allié IA)", "2");
	format->addItem(L"1 contre 1", "1");
	format->setSelectedItemById(settings.duo ? "2" : "1");
	format->connect("ItemSelected", [this]() { refresh(); });

	playerClass = addClassRow(L"Votre classe", settings.playerClass);
	playerClass->connect("ItemSelected", [this]() { refresh(); });
	allyClass = addClassRow(L"Allié (IA)", settings.allyClass);
	enemyClasses[0] = addClassRow(L"Adversaire 1", settings.enemyClasses[0]);
	enemyClasses[1] = addClassRow(L"Adversaire 2", settings.enemyClasses[1]);

	map = addRow(L"Carte");
	map->addItem(L"Au hasard (cartes du tournoi)", "0");
	for (const auto & entry : TrainingScreen::maps())
		map->addItem(fromServerText(entry.second), std::to_string(entry.first));
	if (!map->setSelectedItemById(std::to_string(settings.mapId)))
		map->setSelectedItemById("0");

	difficulty = addRow(L"Difficulté");
	difficulty->addItem(L"Facile (l'IA fait des erreurs)", "easy");
	difficulty->addItem(L"Normal", "normal");
	difficulty->setSelectedItemById(settings.easy ? "easy" : "normal");

	float top = 90 + labels.size() * ROW_HEIGHT;
	description = tgui::Label::create();
	description->setInheritedFont(font);
	description->setTextSize(14);
	description->getRenderer()->setTextColor(sf::Color(255, 230, 150));
	description->setPosition(20, top);
	description->setSize(PANEL_WIDTH - 40, 70);
	panel->add(description);

	tgui::Button::Ptr back = tgui::Button::create(L"Retour");
	back->setInheritedFont(font);
	back->setTextSize(18);
	back->setSize(180, 44);
	back->setPosition(PANEL_WIDTH / 2 - 200, top + 84);
	back->connect("pressed", [this]() { request = Request::BACK; });
	panel->add(back);

	tgui::Button::Ptr play = tgui::Button::create(L"Jouer");
	play->setInheritedFont(font);
	play->setTextSize(18);
	play->setSize(180, 44);
	play->setPosition(PANEL_WIDTH / 2 + 20, top + 84);
	play->connect("pressed", [this]() { request = Request::PLAY; });
	panel->add(play);

	panel->setSize(PANEL_WIDTH, top + 148);

	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/animatedBackground2.glsl");
	refresh();
}

tgui::ComboBox::Ptr TrainingSetupScreen::addRow(const sf::String & text)
{
	float top = 90 + labels.size() * ROW_HEIGHT;

	tgui::Label::Ptr label = tgui::Label::create(text);
	label->setInheritedFont(font);
	label->setTextSize(TEXT_SIZE);
	label->getRenderer()->setTextColor(sf::Color::White);
	label->setPosition(20, top + 4);
	panel->add(label);
	labels.push_back(label);

	tgui::ComboBox::Ptr box = tgui::ComboBox::create();
	box->setInheritedFont(font);
	box->setTextSize(TEXT_SIZE);
	box->setItemsToDisplay(8);
	box->setSize(PANEL_WIDTH - LABEL_WIDTH - 40, FIELD_HEIGHT);
	box->setPosition(20 + LABEL_WIDTH, top);
	panel->add(box);
	return box;
}

tgui::ComboBox::Ptr TrainingSetupScreen::addClassRow(const sf::String & text, int selected)
{
	tgui::ComboBox::Ptr box = addRow(text);
	box->addItem(L"Au hasard", "0");
	for (const battle::ClassDef & classDef : ClientGameData::get().data().classes)
		box->addItem(fromServerText(classDef.name), std::to_string(classDef.id));
	if (!box->setSelectedItemById(std::to_string(selected)))
		box->setSelectedItemById("0");
	return box;
}

int TrainingSetupScreen::selectedId(const tgui::ComboBox::Ptr & box)
{
	std::string id = box->getSelectedItemId().toAnsiString();
	return id.empty() || (id[0] != '-' && (id[0] < '0' || id[0] > '9')) ? 0 : std::atoi(id.c_str());
}

void TrainingSetupScreen::refresh()
{
	// En 1 contre 1, pas d'allié ni de second adversaire.
	bool duo = format->getSelectedItemId() == "2";
	allyClass->setEnabled(duo);
	enemyClasses[1]->setEnabled(duo);
	enemyClasses[0]->setEnabled(true);
	for (std::size_t i = 0; i < labels.size(); i++)
	{
		bool disabled = !duo && (i == 2 || i == 4);
		labels[i]->getRenderer()->setTextColor(disabled ? sf::Color(120, 120, 120) : sf::Color::White);
	}

	// Description de la classe choisie (aide pour qui découvre le jeu).
	const battle::ClassDef * classDef = ClientGameData::get().findClass(selectedId(playerClass));
	if (classDef != nullptr)
		description->setText(fromServerText(classDef->name) + L" : " + fromServerText(classDef->description));
	else
		description->setText(L"Une classe tirée au sort à chaque combat.");
}

void TrainingSetupScreen::save()
{
	TrainingSettings & settings = TrainingSettings::current();
	settings.duo = format->getSelectedItemId() == "2";
	settings.playerClass = selectedId(playerClass);
	settings.allyClass = selectedId(allyClass);
	settings.enemyClasses[0] = selectedId(enemyClasses[0]);
	settings.enemyClasses[1] = selectedId(enemyClasses[1]);
	settings.mapId = selectedId(map);
	settings.easy = difficulty->getSelectedItemId() == "easy";
}

void TrainingSetupScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	float width = (float)window->getSize().x;
	float height = (float)window->getSize().y;
	title.setPosition(width / 2 - title.getLocalBounds().width / 2, 20);
	float top = std::max(130.f, (height - panel->getSize().y) / 2 + 40);
	panel->setPosition((width - PANEL_WIDTH) / 2, top);

	sf::Event event;
	while (window->pollEvent(event))
	{
		if (event.type == sf::Event::Closed)
		{
			window->close();
		}
		else if (event.type == sf::Event::Resized)
		{
			window->setView(sf::View(sf::FloatRect(0.f, 0.f, (float)event.size.width, (float)event.size.height)));
		}
		else if (event.type == sf::Event::KeyPressed)
		{
			if (event.key.code == sf::Keyboard::Return)
				request = Request::PLAY;
			else if (event.key.code == sf::Keyboard::Escape)
				request = Request::BACK;
		}

		gui->handleEvent(event);
	}
}

void TrainingSetupScreen::update(float deltatime)
{
	Screen::update(deltatime);

	// Changement d'écran hors des fonctions appelées par les boutons (leurs widgets sont détruits ici).
	if (request == Request::PLAY)
	{
		save();
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new TrainingScreen(gui, TrainingSettings::current()));
		delete this;
	}
	else if (request == Request::BACK)
	{
		save();
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new LoginScreen(gui));
		delete this;
	}
}

void TrainingSetupScreen::render(sf::RenderWindow * window)
{
	shader.setUniform("time", getShaderEllapsedTime());
	shader.setUniform("resolution", sf::Glsl::Vec2(window->getSize()));

	sf::Shader::bind(&shader);
	sf::RectangleShape rect;
	rect.setSize(sf::Vector2f(window->getSize()));
	rect.setFillColor(sf::Color::Black);
	window->draw(rect);
	sf::Shader::bind(NULL);

	window->draw(title);
}
