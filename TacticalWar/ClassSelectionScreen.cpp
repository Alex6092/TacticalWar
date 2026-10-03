#include "ClassSelectionScreen.h"
#include "LinkToServer.h"
#include <Match.h>
#include "MatchView.h"
#include "PlayerStatusView.h"
#include "ScreenManager.h"
#include "LoginScreen.h"
#include <CharacterFactory.h>
#include "PictureCharacterView.h"
#include "BattleScreen.h"
#include "ClientConfig.h"
#include "ClientGameData.h"
#include <BattleRules.h>
#include <algorithm>
#include "WaitMatchScreen.h"



ClassSelectionScreen::ClassSelectionScreen(tgui::Gui * gui)
	: Screen()
{
	readyToLock = false;
	locked = false;
	ellapsedTime = 0;
	orientation = 0;
	this->gui = gui;
	gui->removeAllWidgets();
	font.loadFromFile("./assets/font/neuropol_x_rg.ttf");
	font2.loadFromFile("./assets/font/OpenSans-Regular.ttf");

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
	stats.setFont(font2);
	stats.setCharacterSize(100);
	stats.setString("STATS");
	stats.setFillColor(sf::Color(0, 255, 255));
	*/
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


	std::vector<int> classesIds = CharacterFactory::getInstance()->getClassesIds();

	
	for (int i = 0; i < classesIds.size(); i++)
	{
		classesInstances.push_back(CharacterFactory::getInstance()->constructCharacter(NULL, classesIds[i], 1, 0, 0));
	}

	indexClass = 0;
	characterView = NULL;
	
	
	tgui::Picture::Ptr Icon = tgui::Picture::create();

	tgui::Picture::Ptr card = tgui::Picture::create();
	std::shared_ptr<PictureCharacterView> classCharacterView = std::make_shared<PictureCharacterView>();


	
	
	tgui::Button::Ptr buttonSuivant = tgui::Button::create();
	buttonSuivant->setInheritedFont(font);
	buttonSuivant->setText("suivant");
	buttonSuivant->setSize(200, 100);

	tgui::Button::Ptr buttonPrecedent = tgui::Button::create();
	buttonPrecedent->setInheritedFont(font);
	buttonPrecedent->setText("precedent");
	buttonPrecedent->setSize(200, 100);

	tgui::Button::Ptr buttonLock = tgui::Button::create();
	buttonLock->setInheritedFont(font);
	buttonLock->setText("Verrouiller mon choix");
	buttonLock->setSize(200, 50);

	buttonSuivant->connect("pressed", [&]() {
		int currentValue = this->getIdxClass();
		currentValue++;
		this->setIdxClass(currentValue);
	});


	buttonPrecedent->connect("pressed", [&]() {
		int currentValue = this->getIdxClass();
		currentValue--;
		this->setIdxClass(currentValue);
	});

	buttonLock->connect("pressed", [&]() {
		readyToLock = true;
	});

/*	m_matchListpanel = tgui::ScrollablePanel::create();
	m_matchListpanel->setSize(1500, 700);
	m_matchListpanel->setPosition(230, 250);
	m_matchListpanel->setInheritedFont(font);
	m_matchListpanel->getRenderer()->setBackgroundColor(sf::Color(128, 128, 128, 128));
*/
	statsPanel = tgui::ScrollablePanel::create();
	statsPanel->setSize(500, 300);
	statsPanel->setPosition(PositionOfCardX + 500, PositionOfCardY-20);
	statsPanel->getRenderer()->setBackgroundColor(sf::Color(0, 0, 0));

	descriptionPanel = tgui::ScrollablePanel::create();
	descriptionPanel->setSize(500, 300);
	descriptionPanel->setPosition(DescriptionX, DescriptionY);
	descriptionPanel->getRenderer()->setBackgroundColor(sf::Color(0, 0, 0));

	tgui::Label::Ptr stats = tgui::Label::create();
	stats->setInheritedFont(font2);

	tgui::Label::Ptr classNameLabel = tgui::Label::create();
	classNameLabel->setInheritedFont(font2);

	tgui::Label::Ptr atkLabel = tgui::Label::create();
	atkLabel->setInheritedFont(font2);

	tgui::Label::Ptr atk = tgui::Label::create();
	atk->setInheritedFont(font2);

	tgui::Label::Ptr pmLabel = tgui::Label::create();
	pmLabel->setInheritedFont(font2);

	tgui::Label::Ptr paLabel = tgui::Label::create();
	paLabel->setInheritedFont(font2);

	tgui::Label::Ptr lifeLabel = tgui::Label::create();
	lifeLabel->setInheritedFont(font2);

	tgui::Label::Ptr defLabel = tgui::Label::create();
	defLabel->setInheritedFont(font2);

	tgui::Label::Ptr description = tgui::Label::create();
	description->setInheritedFont(font2);



	gui->add(statsPanel);
	gui->add(descriptionPanel);
	gui->add(card, "classPreview");
/*
	gui->add(m_matchListpanel);
	gui->add(warriorpanel);
	gui->add(statsPanel);
	*/
	gui->add(Icon, "classIcon");
	gui->add(classCharacterView, "classCharacterView");	
	gui->add(buttonSuivant, "buttonSuivant");
	gui->add(buttonPrecedent, "buttonPrecedent");

	gui->add(stats, "stats");
	gui->add(classNameLabel, "classNameLabel");
	gui->add(atkLabel, "atkLabel");
	gui->add(atk, "atk");
	gui->add(pmLabel, "pmLabel");
	gui->add(paLabel, "paLabel");
	gui->add(lifeLabel, "lifeLabel");
	gui->add(defLabel, "defLabel");
	gui->add(description, "description");
	// Les 6 sorts de la classe : un clic (sur l'icône ou le texte) ajoute ou retire le sort.
	for (int i = 0; i < 6; i++)
	{
		std::string index = std::to_string(i + 1);
		tgui::Picture::Ptr spell = tgui::Picture::create();
		spell->connect("Clicked", [this, i]() { toggleSpell(i); });
		gui->add(spell, "spell" + index);
		tgui::Label::Ptr spellDescription = tgui::Label::create();
		spellDescription->setInheritedFont(font2);
		spellDescription->connect("Clicked", [this, i]() { toggleSpell(i); });
		gui->add(spellDescription, "spell" + index + "Description");
	}
	tgui::Label::Ptr spellCounter = tgui::Label::create();
	spellCounter->setInheritedFont(font2);
	spellCounter->setTextSize(18);
	spellCounter->getRenderer()->setTextColor(sf::Color(255, 215, 0));
	spellCounter->setPosition(215, 868);
	gui->add(spellCounter, "spellCounter");
	
	gui->add(buttonLock, "buttonLock");



	setClassView();
}

void ClassSelectionScreen::setClassView()
{
	auto num = [](int value) { return sf::String(std::to_string(value)); };
	tw::BaseCharacterModel * model = classesInstances[indexClass];

	// Textes et chiffres : données de jeu envoyées par le serveur (assets/data/gamedata.json).
	const tw::battle::ClassDef * classDef = ClientGameData::get().findClass(model->getClassId());

	std::string pathClassPreview = classDef != NULL ? classDef->preview : std::string();
	sf::Texture TextureClassPreview;
	TextureClassPreview.loadFromFile(pathClassPreview);
	TextureClassPreview.setSmooth(true);
	tgui::Picture::Ptr previewClass = gui->get<tgui::Picture>("classPreview");
	previewClass->setPosition(PositionOfCardX, PositionOfCardY);
	previewClass->getRenderer()->setTexture(TextureClassPreview);

	std::string path = classDef != NULL ? classDef->icon : std::string();
	sf::Texture TextureIconClass;
	TextureIconClass.loadFromFile(path);
	TextureIconClass.setSmooth(true);
	tgui::Picture::Ptr IconClass = gui->get<tgui::Picture>("classIcon");
	IconClass->getRenderer()->setTexture(TextureIconClass);
	IconClass->setSize(70, 75);
	IconClass->setPosition(PositionOfCardX, PositionOfCardY);

	tgui::Label::Ptr stats= gui->get<tgui::Label>("stats");
	stats->setText("STATS");
	stats->setPosition(PositionOfCardX + 700, PositionOfCardY);
	stats->setTextSize(35);

	tgui::Label::Ptr classNameLabel = gui->get<tgui::Label>("classNameLabel");
	classNameLabel->setText(classDef != NULL ? fromServerText(classDef->name) : L"Classe " + num(model->getClassId()));
	classNameLabel->setPosition(PositionOfCardX + 700, PositionOfCardY + 300);
	classNameLabel->setTextSize(25);

	// Caractéristiques regroupées dans un seul bloc de texte.
	sf::String statsText;
	if (classDef != NULL)
	{
		const tw::battle::Stats & s = classDef->baseStats;
		statsText = L"Points de vie : " + num(s.get(tw::battle::Stat::MAX_HP))
			+ L"\nPA : " + num(s.get(tw::battle::Stat::AP)) + L"     PM : " + num(s.get(tw::battle::Stat::MP))
			+ L"\nInitiative : " + num(s.get(tw::battle::Stat::INITIATIVE))
			+ L"\nPuissance : " + num(s.get(tw::battle::Stat::POWER)) + L" %"
			+ L"\nRésistance : " + num(s.get(tw::battle::Stat::RESISTANCE)) + L" %"
			+ L"\nTacle : " + num(s.get(tw::battle::Stat::LOCK)) + L"     Fuite : " + num(s.get(tw::battle::Stat::DODGE));
	}
	tgui::Label::Ptr atkLabel = gui->get<tgui::Label>("atkLabel");
	atkLabel->setText(statsText);
	atkLabel->setTextSize(frontsize2);
	atkLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Left);

	for (const char * unused : { "pmLabel", "lifeLabel", "paLabel", "defLabel" })
		gui->get<tgui::Label>(unused)->setText("");

	sf::String description;
	if (classDef != NULL)
	{
		description = fromServerText(classDef->description);
		if (classDef->passive.type != tw::battle::PassiveType::NONE)
			description += L"\n\nPassif - " + fromServerText(classDef->passive.name) + L" : " + fromServerText(classDef->passive.description);
	}
	tgui::Label::Ptr descriptionLabel = gui->get<tgui::Label>("description");
	descriptionLabel->setText(description);
	descriptionLabel->setSize(500, 260);
	descriptionLabel->setTextSize(18);

	// Sorts proposés : le dernier choix fait pour cette classe, à défaut les 4 premiers.
	chosenSpells.clear();
	if (classDef != NULL)
		chosenSpells = tw::battle::validSpellChoice(*classDef, ClientConfig::get().spellChoice(classDef->id));

	for (int i = 0; i < 6; i++)
	{
		std::string index = std::to_string(i + 1);
		tgui::Picture::Ptr icon = gui->get<tgui::Picture>("spell" + index);
		tgui::Label::Ptr label = gui->get<tgui::Label>("spell" + index + "Description");

		sf::String text;
		bool exists = classDef != NULL && i < (int)classDef->spells.size();
		if (exists)
		{
			const tw::battle::SpellDef & spell = classDef->spells[i];
			sf::Texture texture;
			if (texture.loadFromFile(spell.icon))
				icon->getRenderer()->setTexture(texture);

			text = fromServerText(spell.name) + L" - " + num(spell.apCost) + L" PA";
			if (spell.launch != tw::battle::LaunchShape::SELF)
				text += L", portée " + num(spell.minRange) + L"-" + num(spell.maxRange);
			if (spell.cooldown > 0)
				text += L", relance " + num(spell.cooldown);
			text += L"\n" + fromServerText(spell.description);
		}

		icon->setVisible(exists);
		label->setVisible(exists);
		icon->setSize(tgui::Layout2d(72, 72));
		icon->setPosition(tgui::Layout2d(215, 290 + i * 95));
		label->setText(text);
		label->setSize(sizeTextX + 100, 90);
		label->setTextSize(15);
		label->getRenderer()->setTextStyle(sf::Text::Bold);
		label->getRenderer()->setTextOutlineColor(sf::Color::Black);
		label->getRenderer()->setTextOutlineThickness(1);
		label->setPosition(300, 290 + i * 95);
	}
	refreshSpells();

	atkLabel->setPosition(PositionOfCardX + 510, PositionOfCardY + 60);
	descriptionLabel->setPosition(DescriptionX, DescriptionY + 60);

	std::shared_ptr<tgui::Picture> classCharacterView = gui->get<tgui::Picture>("classCharacterView");
	std::shared_ptr<PictureCharacterView> convertedCharacterView = std::dynamic_pointer_cast<PictureCharacterView>(classCharacterView);

	if (characterView != NULL)
	{
		delete characterView;
	}

	characterView = new tw::CharacterView(model);
	characterView->setOrientation((tw::Orientation)((orientation) % 4));

	if (convertedCharacterView != NULL)
	{
		convertedCharacterView->setCharacterView(characterView);
		sf::FloatRect size = convertedCharacterView->getSize();
		convertedCharacterView->setSize(size.width, size.height);
		convertedCharacterView->setPosition(600, 2000);
	}
}

void ClassSelectionScreen::toggleSpell(int index)
{
	if (locked)
		return;
	auto it = std::find(chosenSpells.begin(), chosenSpells.end(), index);
	if (it != chosenSpells.end())
		chosenSpells.erase(it);
	else if ((int)chosenSpells.size() < tw::battle::SPELL_SLOTS)
		chosenSpells.push_back(index);
	// Barre de sorts dans l'ordre de la classe.
	std::sort(chosenSpells.begin(), chosenSpells.end());
	refreshSpells();
}

void ClassSelectionScreen::refreshSpells()
{
	const tw::battle::ClassDef * classDef = ClientGameData::get().findClass(classesInstances[indexClass]->getClassId());
	int needed = classDef != NULL ? (int)tw::battle::defaultSpells(*classDef).size() : 0;
	for (int i = 0; i < 6; i++)
	{
		std::string index = std::to_string(i + 1);
		bool chosen = std::find(chosenSpells.begin(), chosenSpells.end(), i) != chosenSpells.end();
		gui->get<tgui::Picture>("spell" + index)->getRenderer()->setOpacity(chosen ? 1.f : 0.35f);
		gui->get<tgui::Label>("spell" + index + "Description")->getRenderer()->setTextColor(chosen ? sf::Color(255, 240, 200) : sf::Color(150, 150, 150));
	}

	sf::String counter = L"Sorts emportés : " + sf::String(std::to_string(chosenSpells.size())) + L"/" + sf::String(std::to_string(needed));
	if (!locked)
		counter += (int)chosenSpells.size() < needed ? L" - cliquez sur un sort pour l'ajouter" : L" - cliquez sur un sort pour le retirer";
	tgui::Label::Ptr counterLabel = gui->get<tgui::Label>("spellCounter");
	counterLabel->setText(counter);
	counterLabel->getRenderer()->setTextColor((int)chosenSpells.size() == needed ? sf::Color(255, 215, 0) : sf::Color(255, 120, 100));

	tgui::Button::Ptr lockButton = gui->get<tgui::Button>("buttonLock");
	if (lockButton != nullptr && !locked)
		lockButton->setEnabled((int)chosenSpells.size() == needed);
}

ClassSelectionScreen::~ClassSelectionScreen()
{
	LinkToServer::getInstance()->removeListener(this);
}

void ClassSelectionScreen::handleEvents(sf::RenderWindow * window, tgui::Gui * gui)
{
	windowSize = window->getSize();

	title.setPosition(window->getSize().x / 2 - title.getLocalBounds().width / 2, 10);
	subtitle.setPosition(window->getSize().x / 2 - subtitle.getLocalBounds().width / 2, 10 + 128 + 10);
	stats.setPosition(400+700, 450);
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
	ellapsedTime += deltatime;

	bool changeOrientation = false;
	if (ellapsedTime > 1)
	{
		changeOrientation = true;
		ellapsedTime = 0;
	}

	if (changeOrientation)
	{
		characterView->setOrientation((tw::Orientation)((++orientation) % 4));
	}

	if (characterView != NULL)
	{
		characterView->update(deltatime);
	}
		
	if (readyToLock)
	{
		// Classe et sorts choisis, retenus pour la prochaine fois (client.json).
		int classId = classesInstances[indexClass]->getClassId();
		ClientConfig::get().spellChoices[classId] = chosenSpells;
		ClientConfig::get().save();
		LinkToServer::getInstance()->Send("PC" + nlohmann::json({ { "class", classId }, { "spells", chosenSpells } }).dump());
		readyToLock = false;
	}

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
	btnSuivant->setPosition(windowSize.x- 200, windowSize.y/2);

	tgui::Button::Ptr btnPrecedent = gui->get<tgui::Button>("buttonPrecedent");
	btnPrecedent->setPosition(0, windowSize.y/2);

	tgui::Button::Ptr btnLock = gui->get<tgui::Button>("buttonLock");
	btnLock->setPosition(window->getSize().x / 2. - btnLock->getSize().x / 2, 900);
	
	std::shared_ptr<tgui::Picture> classCharacterView = gui->get<tgui::Picture>("classCharacterView");
	std::shared_ptr<PictureCharacterView> convertedCharacterView = std::dynamic_pointer_cast<PictureCharacterView>(classCharacterView);

	if (convertedCharacterView != NULL)
	{
		convertedCharacterView->setCharacterView(characterView);
		sf::FloatRect size = convertedCharacterView->getSize();
		convertedCharacterView->setSize(size.width, size.height);
		convertedCharacterView->setPosition(/*windowSize.x / 2. - 600, windowSize.y / 2. - convertedCharacterView->getSize().height / 2. + 80*/500,750);
	}

}

void ClassSelectionScreen::onMessageReceived(std::string msg)
{
	sf::String m = msg;

	// Le status des joueurs est géré dans PlayerStatusView (widget autonome)

	// Choix classe verrouillé :
	if (m.substring(0, 2) == "PO")
	{
		locked = true;
		tgui::Button::Ptr lockButton = gui->get<tgui::Button>("buttonLock");
		lockButton->setEnabled(false);
		lockButton->setText("Choix verrouillé");
		tgui::Button::Ptr previousButton = gui->get<tgui::Button>("buttonPrecedent");
		previousButton->setEnabled(false);
		previousButton->setVisible(false);
		tgui::Button::Ptr nextButton = gui->get<tgui::Button>("buttonSuivant");
		nextButton->setEnabled(false);
		nextButton->setVisible(false);

		int idClass = std::atoi(m.substring(2).toAnsiString().c_str());
		for (int i = 0; i < classesInstances.size(); i++)
		{
			if (classesInstances[i]->getClassId() == idClass)
			{
				indexClass = i;
				setClassView();
				break;
			}
		}
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
