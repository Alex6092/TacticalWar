#include "ClassSelectionScreen.h"

#include <algorithm>

#include <BattleRules.h>
#include <CharacterFactory.h>
#include <nlohmann/json.hpp>

#include "BattleScreen.h"
#include "ClientConfig.h"
#include "ClientGameData.h"
#include "LinkToServer.h"
#include "LoginScreen.h"
#include "PictureCharacterView.h"
#include "PlayerStatusView.h"
#include "ScreenManager.h"
#include "WaitMatchScreen.h"

namespace
{
	// Dimensions d'origine de la carte de classe (assets/classpreview).
	const float CARD_WIDTH = 400;
	const float CARD_HEIGHT = 450;

	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}

	tgui::Button::Ptr createButton(const sf::Font & font, const sf::String & text, unsigned int size)
	{
		tgui::Button::Ptr button = tgui::Button::create(text);
		button->setInheritedFont(font);
		button->setTextSize(size);
		return button;
	}

	tgui::Panel::Ptr createPanel()
	{
		tgui::Panel::Ptr panel = tgui::Panel::create();
		panel->getRenderer()->setBackgroundColor(sf::Color(0, 0, 0, 200));
		panel->getRenderer()->setBorders(1);
		panel->getRenderer()->setBorderColor(sf::Color(255, 215, 0, 160));
		return panel;
	}
}

ClassSelectionScreen::ClassSelectionScreen(tgui::Gui * gui, const std::string & selection)
	: Screen(), gui(gui), characterView(NULL), indexClass(0), orientationTime(0), orientation(0), readyToLock(false), locked(false)
{
	gui->removeAllWidgets();
	font.loadFromFile("./assets/font/neuropol_x_rg.ttf");
	textFont.loadFromFile("./assets/font/OpenSans-Regular.ttf");

	title.setFont(font);
	title.setString("Tactical War");
	title.setFillColor(sf::Color::White);
	title.setOutlineColor(sf::Color(255, 215, 0));
	title.setOutlineThickness(3);

	subtitle.setFont(font);
	subtitle.setCharacterSize(28);
	subtitle.setString(L"Sélection de la classe");
	subtitle.setFillColor(sf::Color::Red);
	subtitle.setOutlineColor(sf::Color(255, 215, 0));
	subtitle.setOutlineThickness(1.5);

	gui->add(PlayerStatusView::getInstance());
	LinkToServer::getInstance()->addListener(this);
	shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/animatedBackground2.glsl");

	for (int classId : CharacterFactory::getInstance()->getClassesIds())
		classesInstances.push_back(CharacterFactory::getInstance()->constructCharacter(NULL, classId, 1, 0, 0));

	// Centre : carte de la classe, nom, icône, personnage animé, flèches.
	className = tgui::Label::create();
	className->setInheritedFont(font);
	className->setTextSize(26);
	className->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
	className->getRenderer()->setTextColor(sf::Color(255, 215, 0));
	className->getRenderer()->setTextOutlineColor(sf::Color::Black);
	className->getRenderer()->setTextOutlineThickness(2);
	gui->add(className);

	preview = tgui::Picture::create();
	gui->add(preview);
	classIcon = tgui::Picture::create();
	gui->add(classIcon);
	characterPicture = std::make_shared<PictureCharacterView>();
	gui->add(characterPicture);

	previousButton = createButton(font, L"<", 30);
	previousButton->connect("pressed", [this]() { showClass(indexClass - 1); });
	gui->add(previousButton);
	nextButton = createButton(font, L">", 30);
	nextButton->connect("pressed", [this]() { showClass(indexClass + 1); });
	gui->add(nextButton);

	// Droite : caractéristiques, puis description et passif.
	statsPanel = createPanel();
	statsLabel = tgui::Label::create();
	statsLabel->setInheritedFont(textFont);
	statsLabel->setTextSize(19);
	statsLabel->getRenderer()->setTextColor(sf::Color::White);
	statsLabel->setPosition(14, 10);
	statsPanel->add(statsLabel);
	gui->add(statsPanel);

	descriptionPanel = createPanel();
	descriptionLabel = tgui::Label::create();
	descriptionLabel->setInheritedFont(textFont);
	descriptionLabel->setTextSize(18);
	descriptionLabel->getRenderer()->setTextColor(sf::Color(235, 235, 235));
	descriptionLabel->setPosition(14, 10);
	descriptionPanel->add(descriptionLabel);
	gui->add(descriptionPanel);

	// Gauche : sorts (sur un fond sombre, pour la lisibilité) et talents.
	spellsPanel = createPanel();
	gui->add(spellsPanel);
	spellPicker.reset(new tw::SpellPicker(textFont, tw::SpellPicker::Layout::LIST));
	spellPicker->onChange = [this]() { refreshLock(); };
	gui->add(spellPicker->getWidget());

	nlohmann::json message = nlohmann::json::parse(selection, nullptr, false);
	talentPicker.reset(new tw::TalentPicker(gui, font));
	talentPicker->setSlots(message.is_object() ? message.value("talents", 0) : 0);
	talentPicker->setChosen(ClientConfig::get().talentChoice);
	talentPicker->onChange = [this]() { refreshLock(); };
	gui->add(talentPicker->getButton());

	lockButton = createButton(font, L"Verrouiller mon choix", 20);
	lockButton->connect("pressed", [this]() { readyToLock = true; });
	gui->add(lockButton);

	showClass(0);
}

ClassSelectionScreen::~ClassSelectionScreen()
{
	LinkToServer::getInstance()->removeListener(this);
	delete characterView;
}

int ClassSelectionScreen::currentClassId() const
{
	return classesInstances.empty() ? 0 : classesInstances[indexClass]->getClassId();
}

void ClassSelectionScreen::showClass(int index)
{
	if (classesInstances.empty())
		return;
	int count = (int)classesInstances.size();
	indexClass = (index % count + count) % count;
	tw::BaseCharacterModel * model = classesInstances[indexClass];

	// Textes et chiffres : données de jeu envoyées par le serveur (assets/data/gamedata.json).
	const tw::battle::ClassDef * classDef = ClientGameData::get().findClass(model->getClassId());
	className->setText(classDef != NULL ? fromServerText(classDef->name) : L"Classe " + num(model->getClassId()));

	sf::Texture texture;
	if (classDef != NULL && texture.loadFromFile(classDef->preview))
	{
		texture.setSmooth(true);
		preview->getRenderer()->setTexture(texture);
	}
	sf::Texture icon;
	if (classDef != NULL && icon.loadFromFile(classDef->icon))
	{
		icon.setSmooth(true);
		classIcon->getRenderer()->setTexture(icon);
	}

	sf::String stats;
	sf::String description;
	if (classDef != NULL)
	{
		const tw::battle::Stats & s = classDef->baseStats;
		stats = L"Points de vie : " + num(s.get(tw::battle::Stat::MAX_HP))
			+ L"\nPA : " + num(s.get(tw::battle::Stat::AP)) + L"     PM : " + num(s.get(tw::battle::Stat::MP))
			+ L"\nInitiative : " + num(s.get(tw::battle::Stat::INITIATIVE))
			+ L"\nPuissance : " + num(s.get(tw::battle::Stat::POWER)) + L" %"
			+ L"\nRésistance : " + num(s.get(tw::battle::Stat::RESISTANCE)) + L" %"
			+ L"\nTacle : " + num(s.get(tw::battle::Stat::LOCK)) + L"     Fuite : " + num(s.get(tw::battle::Stat::DODGE));
		description = fromServerText(classDef->description);
		if (classDef->passive.type != tw::battle::PassiveType::NONE)
			description += L"\n\nPassif - " + fromServerText(classDef->passive.name) + L" : " + fromServerText(classDef->passive.description);
	}
	statsLabel->setText(stats);
	descriptionLabel->setText(description);

	// Sorts proposés : le dernier choix fait pour cette classe, à défaut les 4 premiers.
	spellPicker->setClass(classDef, classDef != NULL ? ClientConfig::get().spellChoice(classDef->id) : std::vector<int>());

	delete characterView;
	characterView = new tw::CharacterView(model);
	characterView->setOrientation((tw::Orientation)(orientation % 4));
	characterPicture->setCharacterView(characterView);

	refreshLock();
	if (windowSize.x > 0)
		layout(windowSize);
}

void ClassSelectionScreen::refreshLock()
{
	bool complete = spellPicker->isComplete() && talentPicker->isComplete();
	lockButton->setEnabled(!locked && complete);
	lockButton->setText(locked ? L"Choix verrouillé" : complete ? L"Verrouiller mon choix"
		: !spellPicker->isComplete() ? L"Choisissez 4 sorts" : L"Choisissez vos talents");
}

void ClassSelectionScreen::layout(const sf::Vector2u & size)
{
	float width = (float)size.x;
	float height = (float)size.y;
	unsigned int titleSize = height > 1000 ? 110 : 80;
	title.setCharacterSize(titleSize);
	title.setPosition(width / 2 - title.getLocalBounds().width / 2, 6);
	subtitle.setPosition(width / 2 - subtitle.getLocalBounds().width / 2, 10.f + titleSize);

	float top = titleSize + 64.f;
	float margin = width * 0.03f;
	float lockY = height - 76;

	// Gauche : sorts (lignes ajustées à la hauteur disponible), puis talents.
	float leftWidth = width * 0.36f;
	float rowHeight = std::max(56.f, std::min(82.f, (lockY - 60 - top - 30) / 6));
	spellPicker->setGeometry(leftWidth, rowHeight);
	spellPicker->getWidget()->setPosition(margin, top);
	spellsPanel->setPosition(margin - 10, top - 8);
	spellsPanel->setSize(leftWidth + 20, spellPicker->getHeight() + 12);
	tgui::Button::Ptr talents = talentPicker->getButton();
	talents->setSize(std::min(leftWidth, 380.f), 40);
	talents->setPosition(margin, top + spellPicker->getHeight() + 8);

	// Centre : carte de la classe, à la taille disponible.
	float centerX = margin + leftWidth + width * 0.03f;
	float centerWidth = width * 0.24f;
	float scale = std::min(1.f, std::min(centerWidth / CARD_WIDTH, (lockY - top - 230) / CARD_HEIGHT));
	float cardWidth = CARD_WIDTH * scale;
	float cardHeight = CARD_HEIGHT * scale;
	float cardX = centerX + (centerWidth - cardWidth) / 2;
	float cardY = top + 36;
	className->setSize(centerWidth, 34);
	className->setPosition(centerX, top);
	preview->setSize(cardWidth, cardHeight);
	preview->setPosition(cardX, cardY);
	classIcon->setSize(70 * scale, 75 * scale);
	classIcon->setPosition(cardX, cardY);
	previousButton->setSize(48, 64);
	previousButton->setPosition(cardX - 56, cardY + cardHeight / 2 - 32);
	nextButton->setSize(48, 64);
	nextButton->setPosition(cardX + cardWidth + 8, cardY + cardHeight / 2 - 32);
	characterPicture->setPosition(centerX + centerWidth / 2 - 40, cardY + cardHeight + 12);

	// Droite : caractéristiques et description.
	float rightX = centerX + centerWidth + width * 0.03f;
	float rightWidth = width - rightX - margin;
	statsPanel->setPosition(rightX, top);
	statsPanel->setSize(rightWidth, 200);
	descriptionPanel->setPosition(rightX, top + 214);
	descriptionPanel->setSize(rightWidth, std::max(120.f, lockY - 16 - (top + 214)));
	descriptionLabel->setMaximumTextWidth(rightWidth - 28);

	lockButton->setSize(400, 54);
	lockButton->setPosition(width / 2 - 200, lockY);
}

void ClassSelectionScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	if (window->getSize() != windowSize)
	{
		windowSize = window->getSize();
		layout(windowSize);
	}

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
			gui->setView(window->getView());
		}
		gui->handleEvent(event);
	}
}

void ClassSelectionScreen::update(float deltatime)
{
	Screen::update(deltatime);

	// Le personnage tourne sur lui-même (une orientation par seconde).
	orientationTime += deltatime;
	if (orientationTime > 1 && characterView != NULL)
	{
		orientationTime = 0;
		characterView->setOrientation((tw::Orientation)((++orientation) % 4));
	}
	if (characterView != NULL)
		characterView->update(deltatime);

	if (readyToLock)
	{
		readyToLock = false;
		// Classe, sorts et talents choisis, retenus pour la prochaine fois (client.json).
		int classId = currentClassId();
		ClientConfig & config = ClientConfig::get();
		config.spellChoices[classId] = spellPicker->getChosen();
		if (talentPicker->getSlots() > 0)
			config.talentChoice = talentPicker->getChosen();
		config.save();
		LinkToServer::getInstance()->Send("PC" + nlohmann::json({ { "class", classId }, { "spells", spellPicker->getChosen() },
			{ "talents", talentPicker->getChosen() } }).dump());
	}

	LinkToServer::getInstance()->UpdateReceivedData();
}

void ClassSelectionScreen::render(sf::RenderWindow * window)
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
	window->draw(subtitle);
}

void ClassSelectionScreen::onMessageReceived(std::string msg)
{
	sf::String m = msg;

	// L'état des joueurs est géré par PlayerStatusView (widget autonome).
	if (m.substring(0, 2) == "PO")
	{
		// Choix verrouillé (aussi au retour après une déconnexion) : la classe retenue est montrée.
		int classId = std::atoi(m.substring(2).toAnsiString().c_str());
		for (int i = 0; i < (int)classesInstances.size(); i++)
		{
			if (classesInstances[i]->getClassId() == classId)
				showClass(i);
		}
		locked = true;
		spellPicker->setLocked(true);
		talentPicker->setLocked(true);
		previousButton->setVisible(false);
		nextButton->setVisible(false);
		refreshLock();
	}
	else if (m.substring(0, 2) == "HG")
	{
		int environmentId = std::atoi(m.substring(2).toAnsiString().c_str());
		gui->removeAllWidgets();
		tw::ScreenManager::getInstance()->setCurrentScreen(new tw::BattleScreen(gui, environmentId));
		delete this;
	}
	else if (m.substring(0, 2) == "HW")
	{
		// Match annulé ou gagné par forfait : retour à l'attente.
		gui->removeAllWidgets();
		tw::ScreenManager::getInstance()->setCurrentScreen(new WaitMatchScreen(gui));
		delete this;
	}
}

void ClassSelectionScreen::onDisconnected()
{
	gui->removeAllWidgets();
	tw::ScreenManager::getInstance()->setCurrentScreen(new tw::LoginScreen(gui));
	delete this;
}
