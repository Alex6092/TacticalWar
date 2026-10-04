#include "PuzzleSelectScreen.h"

#include "ClientConfig.h"
#include "LinkToServer.h"
#include "PuzzleScreen.h"
#include "ScreenManager.h"
#include "TrainingSetupScreen.h"

using namespace tw;

namespace
{
	const float PANEL_WIDTH = 720;
	const float ROW_HEIGHT = 58;
	const int NONE = -1;
	const int BACK = -2;
}

PuzzleSelectScreen::PuzzleSelectScreen(tgui::Gui * gui)
	: gui(gui), request(NONE)
{
	gui->removeAllWidgets();
	font.loadFromFile("./assets/font/neuropol_x_rg.ttf");
	textFont.loadFromFile("./assets/font/OpenSans-Regular.ttf");

	title.setFont(font);
	title.setCharacterSize(64);
	title.setString(L"Énigmes tactiques");
	title.setFillColor(sf::Color::White);
	title.setOutlineColor(sf::Color(255, 215, 0));
	title.setOutlineThickness(3);

	panel = tgui::Panel::create();
	panel->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 220));
	panel->getRenderer()->setBorders(2);
	panel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	gui->add(panel);

	tgui::Label::Ptr intro = tgui::Label::create(L"Une position, un objectif : trouvez la bonne idée pour gagner pendant vos tours "
		L"(combinaisons, distance, recul...). Les adversaires ne jouent pas.");
	intro->setInheritedFont(textFont);
	intro->setTextSize(16);
	intro->setMaximumTextWidth(PANEL_WIDTH - 40);
	intro->getRenderer()->setTextColor(sf::Color(230, 230, 230));
	intro->setPosition(20, 16);
	panel->add(intro);

	const std::vector<battle::Puzzle> & list = PuzzleScreen::puzzles();
	const std::set<std::string> & solved = ClientConfig::get().solvedPuzzles;
	float top = 30 + intro->getSize().y;
	for (int i = 0; i < (int)list.size(); i++)
	{
		bool done = solved.count(list[i].id) > 0;
		tgui::Button::Ptr row = tgui::Button::create(std::to_wstring(i + 1) + L".  " + fromServerText(list[i].title).toWideString()
			+ (done ? L"   -   réussie" : L""));
		row->setInheritedFont(font);
		row->setTextSize(18);
		row->setSize(PANEL_WIDTH - 40, ROW_HEIGHT - 10);
		row->setPosition(20, top + i * ROW_HEIGHT);
		if (done)
		{
			row->getRenderer()->setBackgroundColor(sf::Color(170, 230, 160));
			row->getRenderer()->setBackgroundColorHover(sf::Color(195, 245, 185));
		}
		row->connect("pressed", [this, i]() { request = i; });
		panel->add(row);
	}
	if (list.empty())
	{
		tgui::Label::Ptr empty = tgui::Label::create(L"Aucune énigme trouvée (dossier assets/puzzles).");
		empty->setInheritedFont(textFont);
		empty->setTextSize(16);
		empty->getRenderer()->setTextColor(sf::Color(255, 160, 140));
		empty->setPosition(20, top);
		panel->add(empty);
	}

	float buttonTop = top + std::max(1, (int)list.size()) * ROW_HEIGHT + 10;
	tgui::Button::Ptr back = tgui::Button::create(L"Retour");
	back->setInheritedFont(font);
	back->setTextSize(18);
	back->setSize(180, 44);
	back->setPosition((PANEL_WIDTH - 180) / 2, buttonTop);
	back->connect("pressed", [this]() { request = BACK; });
	panel->add(back);
	panel->setSize(PANEL_WIDTH, buttonTop + 64);

	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/animatedBackground2.glsl");
}

void PuzzleSelectScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	float width = (float)window->getSize().x;
	float height = (float)window->getSize().y;
	title.setPosition(width / 2 - title.getLocalBounds().width / 2, 20);
	panel->setPosition((width - PANEL_WIDTH) / 2, std::max(120.f, (height - panel->getSize().y) / 2 + 40));

	sf::Event event;
	while (window->pollEvent(event))
	{
		if (event.type == sf::Event::Closed)
			window->close();
		else if (event.type == sf::Event::Resized)
			window->setView(sf::View(sf::FloatRect(0.f, 0.f, (float)event.size.width, (float)event.size.height)));
		else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
			request = BACK;
		gui->handleEvent(event);
	}
}

void PuzzleSelectScreen::update(float deltatime)
{
	Screen::update(deltatime);

	// Changement d'écran hors des fonctions appelées par les boutons (leurs widgets sont détruits ici).
	if (request == BACK)
	{
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new TrainingSetupScreen(gui));
		delete this;
	}
	else if (request >= 0)
	{
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new PuzzleScreen(gui, request));
		delete this;
	}
}

void PuzzleSelectScreen::render(sf::RenderWindow * window)
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
