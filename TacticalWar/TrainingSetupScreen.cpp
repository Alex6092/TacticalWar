#include "TrainingSetupScreen.h"

#include <algorithm>
#include <string>

#include "ClientConfig.h"
#include "ClientGameData.h"
#include <BattleRules.h>
#include "LinkToServer.h"
#include "LoginScreen.h"
#include "ScreenManager.h"
#include "TrainingScreen.h"
#include "TutorialScreen.h"
#include "PuzzleSelectScreen.h"

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
	: gui(gui), spellClassId(-1), spellsChanged(false), talentsChanged(false), request(Request::NONE)
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
	format->addItem(L"2 contre 2 (vous jouez les deux)", "2c");
	format->addItem(L"1 contre 1", "1");
	format->setSelectedItemById(!settings.duo ? "1" : settings.controlAlly ? "2c" : "2");
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

	mode = addRow(L"Mode");
	mode->addItem(L"KO : éliminer l'équipe adverse", "ko");
	mode->addItem(L"Zone à tenir (premier à " + std::to_wstring(TrainingSettings::ZONE_POINTS) + L" points)", "zone");
	mode->setSelectedItemById(settings.zone ? "zone" : "ko");

	difficulty = addRow(L"Difficulté");
	difficulty->addItem(L"Facile (l'IA fait des erreurs)", "easy");
	difficulty->addItem(L"Normal", "normal");
	difficulty->setSelectedItemById(settings.easy ? "easy" : "normal");

	talentCount = addRow(L"Talents de tournoi");
	talentCount->addItem(L"Aucun", "0");
	for (int count = 1; count <= 3; count++)
		talentCount->addItem(std::to_wstring(count) + (count == 1 ? L" talent (après un match)" : L" talents (après " + std::to_wstring(count) + L" matchs)"),
			std::to_string(count));
	if (!talentCount->setSelectedItemById(std::to_string(settings.talentCount)))
		talentCount->setSelectedItemById("0");
	talentCount->connect("ItemSelected", [this]() { refresh(); });

	float top = 90 + labels.size() * ROW_HEIGHT;
	description = tgui::Label::create();
	description->setInheritedFont(font);
	description->setTextSize(14);
	description->getRenderer()->setTextColor(sf::Color(255, 230, 150));
	description->setPosition(20, top);
	description->setSize(PANEL_WIDTH - 40, 56);
	panel->add(description);

	// Sorts emportés : 4 parmi ceux de la classe (survol : description du sort).
	spellPicker.reset(new SpellPicker(font, SpellPicker::Layout::ROW));
	spellPicker->setGeometry(PANEL_WIDTH - 40, 0);
	spellPicker->getWidget()->setPosition(20, top + 60);
	spellPicker->onChange = [this]() { spellsChanged = true; refreshPlay(); };
	panel->add(spellPicker->getWidget());
	randomSpells = tgui::Label::create(L"Classe au hasard : sorts par défaut de la classe tirée, ou derniers sorts choisis pour elle.");
	randomSpells->setInheritedFont(font);
	randomSpells->setTextSize(14);
	randomSpells->getRenderer()->setTextColor(sf::Color(200, 200, 200));
	randomSpells->setPosition(20, top + 64);
	randomSpells->setSize(PANEL_WIDTH - 40, 44);
	panel->add(randomSpells);

	// Talents : le même choix qu'avant un match de tournoi.
	talentPicker.reset(new TalentPicker(gui, font));
	talentPicker->setSlots(settings.talentCount);
	talentPicker->setChosen(settings.talents.empty() ? ClientConfig::get().talentChoice : settings.talents);
	talentPicker->onChange = [this]() { talentsChanged = true; refreshPlay(); };
	talentPicker->getButton()->setSize(320, 38);
	talentPicker->getButton()->setPosition(20, top + 118);
	panel->add(talentPicker->getButton());

	tgui::Button::Ptr back = tgui::Button::create(L"Retour");
	back->setInheritedFont(font);
	back->setTextSize(18);
	back->setSize(160, 44);
	back->setPosition(PANEL_WIDTH / 2 - 340, top + 170);
	back->connect("pressed", [this]() { request = Request::BACK; });
	panel->add(back);

	tgui::Button::Ptr tutorial = tgui::Button::create(L"Tutoriel");
	tutorial->setInheritedFont(font);
	tutorial->setTextSize(18);
	tutorial->setSize(160, 44);
	tutorial->setPosition(PANEL_WIDTH / 2 - 170, top + 170);
	tutorial->connect("pressed", [this]() { request = Request::TUTORIAL; });
	panel->add(tutorial);

	tgui::Button::Ptr puzzles = tgui::Button::create(L"Énigmes");
	puzzles->setInheritedFont(font);
	puzzles->setTextSize(18);
	puzzles->setSize(160, 44);
	puzzles->setPosition(PANEL_WIDTH / 2, top + 170);
	puzzles->connect("pressed", [this]() { request = Request::PUZZLES; });
	panel->add(puzzles);

	playButton = tgui::Button::create(L"Jouer");
	playButton->setInheritedFont(font);
	playButton->setTextSize(18);
	playButton->setSize(160, 44);
	playButton->setPosition(PANEL_WIDTH / 2 + 170, top + 170);
	playButton->connect("pressed", [this]() { request = Request::PLAY; });
	panel->add(playButton);

	panel->setSize(PANEL_WIDTH, top + 234);

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
	bool duo = format->getSelectedItemId() != "1";
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

	// Nouvelle classe : son dernier choix de sorts (à défaut, les 4 premiers).
	int classId = classDef != nullptr ? classDef->id : 0;
	if (classId != spellClassId)
	{
		spellClassId = classId;
		spellPicker->setClass(classDef, ClientConfig::get().spellChoice(classId));
	}
	spellPicker->getWidget()->setVisible(classDef != nullptr);
	randomSpells->setVisible(classDef == nullptr);
	talentPicker->setSlots(selectedId(talentCount));
	refreshPlay();
}

void TrainingSetupScreen::refreshPlay()
{
	bool spells = ClientGameData::get().findClass(spellClassId) == nullptr || spellPicker->isComplete();
	playButton->setEnabled(spells && talentPicker->isComplete());
}

void TrainingSetupScreen::save()
{
	TrainingSettings & settings = TrainingSettings::current();
	settings.duo = format->getSelectedItemId() != "1";
	settings.controlAlly = format->getSelectedItemId() == "2c";
	settings.playerClass = selectedId(playerClass);
	settings.allyClass = selectedId(allyClass);
	settings.enemyClasses[0] = selectedId(enemyClasses[0]);
	settings.enemyClasses[1] = selectedId(enemyClasses[1]);
	settings.mapId = selectedId(map);
	settings.easy = difficulty->getSelectedItemId() == "easy";
	settings.zone = mode->getSelectedItemId() == "zone";

	settings.talentCount = selectedId(talentCount);
	settings.talents = talentPicker->getChosen();

	// Choix de sorts ou de talents modifié : retenu (client.json), comme à l'écran de choix de classe.
	ClientConfig & config = ClientConfig::get();
	const battle::ClassDef * classDef = ClientGameData::get().findClass(spellClassId);
	bool changed = false;
	if (spellsChanged && classDef != nullptr && spellPicker->isComplete())
	{
		config.spellChoices[spellClassId] = spellPicker->getChosen();
		changed = true;
	}
	if (talentsChanged && !settings.talents.empty())
	{
		config.talentChoice = settings.talents;
		changed = true;
	}
	if (changed)
		config.save();
	spellsChanged = talentsChanged = false;
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
	else if (request == Request::PUZZLES)
	{
		save();
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new PuzzleSelectScreen(gui));
		delete this;
	}
	else if (request == Request::TUTORIAL)
	{
		save();
		gui->removeAllWidgets();
		ScreenManager::getInstance()->setCurrentScreen(new TutorialScreen(gui, TutorialScreen::Origin::TRAINING));
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
