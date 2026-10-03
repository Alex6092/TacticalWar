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
	: gui(gui), spellClassId(-1), spellsChanged(false), request(Request::NONE)
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

	mode = addRow(L"Mode");
	mode->addItem(L"KO : éliminer l'équipe adverse", "ko");
	mode->addItem(L"Zone à tenir (premier à " + std::to_wstring(TrainingSettings::ZONE_POINTS) + L" points)", "zone");
	mode->setSelectedItemById(settings.zone ? "zone" : "ko");

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
	description->setSize(PANEL_WIDTH - 40, 56);
	panel->add(description);

	// Sorts emportés : les 6 de la classe, 4 choisis (survol : description du sort).
	for (int i = 0; i < 6; i++)
	{
		spellIcons[i] = tgui::Picture::create();
		spellIcons[i]->setSize(48, 48);
		spellIcons[i]->setPosition(20 + i * 56.f, top + 60);
		spellIcons[i]->connect("Clicked", [this, i]() { toggleSpell(i); });
		panel->add(spellIcons[i]);
	}
	spellLabel = tgui::Label::create();
	spellLabel->setInheritedFont(font);
	spellLabel->setTextSize(14);
	spellLabel->setPosition(20 + 6 * 56.f + 10, top + 64);
	spellLabel->setSize(PANEL_WIDTH - (20 + 6 * 56.f + 10) - 20, 44);
	panel->add(spellLabel);

	tgui::Button::Ptr back = tgui::Button::create(L"Retour");
	back->setInheritedFont(font);
	back->setTextSize(18);
	back->setSize(180, 44);
	back->setPosition(PANEL_WIDTH / 2 - 200, top + 124);
	back->connect("pressed", [this]() { request = Request::BACK; });
	panel->add(back);

	playButton = tgui::Button::create(L"Jouer");
	playButton->setInheritedFont(font);
	playButton->setTextSize(18);
	playButton->setSize(180, 44);
	playButton->setPosition(PANEL_WIDTH / 2 + 20, top + 124);
	playButton->connect("pressed", [this]() { request = Request::PLAY; });
	panel->add(playButton);

	panel->setSize(PANEL_WIDTH, top + 188);

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

	// Nouvelle classe : son dernier choix de sorts (à défaut, les 4 premiers).
	int classId = classDef != nullptr ? classDef->id : 0;
	if (classId != spellClassId)
	{
		spellClassId = classId;
		chosenSpells = classDef != nullptr ? battle::validSpellChoice(*classDef, ClientConfig::get().spellChoice(classId)) : std::vector<int>();
		for (int i = 0; i < 6; i++)
		{
			bool exists = classDef != nullptr && i < (int)classDef->spells.size();
			spellIcons[i]->setVisible(exists);
			if (!exists)
				continue;
			const battle::SpellDef & spell = classDef->spells[i];
			sf::Texture texture;
			if (texture.loadFromFile(spell.icon))
				spellIcons[i]->getRenderer()->setTexture(texture);
			tgui::Label::Ptr tip = tgui::Label::create(fromServerText(spell.name) + L" (" + std::to_wstring(spell.apCost) + L" PA)\n"
				+ fromServerText(spell.description));
			tip->setInheritedFont(font);
			tip->setTextSize(13);
			tip->setMaximumTextWidth(360);
			tip->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 235));
			tip->getRenderer()->setTextColor(sf::Color::White);
			tip->getRenderer()->setBorders(1);
			tip->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
			tip->getRenderer()->setPadding(6);
			spellIcons[i]->setToolTip(tip);
		}
	}
	refreshSpells();
}

void TrainingSetupScreen::toggleSpell(int index)
{
	auto it = std::find(chosenSpells.begin(), chosenSpells.end(), index);
	if (it != chosenSpells.end())
		chosenSpells.erase(it);
	else if ((int)chosenSpells.size() < battle::SPELL_SLOTS)
		chosenSpells.push_back(index);
	std::sort(chosenSpells.begin(), chosenSpells.end());
	spellsChanged = true;
	refreshSpells();
}

void TrainingSetupScreen::refreshSpells()
{
	const battle::ClassDef * classDef = ClientGameData::get().findClass(spellClassId);
	spellLabel->setPosition(classDef != nullptr ? 20 + 6 * 56.f + 10 : 20.f, spellLabel->getPosition().y);
	if (classDef == nullptr)
	{
		spellLabel->setText(L"Classe au hasard : sorts par défaut de la classe tirée, ou derniers sorts choisis pour elle.");
		spellLabel->getRenderer()->setTextColor(sf::Color(200, 200, 200));
		playButton->setEnabled(true);
		return;
	}

	int needed = (int)battle::defaultSpells(*classDef).size();
	for (int i = 0; i < 6; i++)
	{
		bool chosen = std::find(chosenSpells.begin(), chosenSpells.end(), i) != chosenSpells.end();
		spellIcons[i]->getRenderer()->setOpacity(chosen ? 1.f : 0.35f);
	}
	bool complete = (int)chosenSpells.size() == needed;
	spellLabel->setText(L"Sorts emportés : " + std::to_wstring(chosenSpells.size()) + L"/" + std::to_wstring(needed)
		+ (complete ? L"\nClic : retirer un sort. Survol : description." : L"\nCliquez sur un sort pour l'ajouter."));
	spellLabel->getRenderer()->setTextColor(complete ? sf::Color(255, 215, 0) : sf::Color(255, 120, 100));
	playButton->setEnabled(complete);
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
	settings.zone = mode->getSelectedItemId() == "zone";

	// Choix de sorts modifié : retenu pour cette classe (client.json), comme à l'écran de choix.
	const battle::ClassDef * classDef = ClientGameData::get().findClass(spellClassId);
	if (spellsChanged && classDef != nullptr && battle::validSpellChoice(*classDef, chosenSpells) == chosenSpells)
	{
		ClientConfig::get().spellChoices[spellClassId] = chosenSpells;
		ClientConfig::get().save();
		spellsChanged = false;
	}
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
