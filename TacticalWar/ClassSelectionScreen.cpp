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
#include "SpellSlot.h"



ClassSelectionScreen::ClassSelectionScreen(tgui::Gui * gui)
	: Screen()
{
	readyToLock = false;
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
		classesInstances.push_back(CharacterFactory::getInstance()->constructCharacter(NULL, classesIds[i], 1, 0, 0, NULL));
	}

	indexClass = 0;
	characterView = NULL;
	
	
	tgui::Picture::Ptr Icon = tgui::Picture::create();	
	std::shared_ptr<SpellSlot> spell1 = std::make_shared<SpellSlot>(classesInstances[0], 1, "");
	std::shared_ptr<SpellSlot> spell2 = std::make_shared<SpellSlot>(classesInstances[0], 2, "");
	std::shared_ptr<SpellSlot> spell3 = std::make_shared<SpellSlot>(classesInstances[0], 3, "");
	std::shared_ptr<SpellSlot> spell4 = std::make_shared<SpellSlot>(classesInstances[0], 4, "");

	spell1->setCooldownVisible(false);
	spell2->setCooldownVisible(false);
	spell3->setCooldownVisible(false);
	spell4->setCooldownVisible(false);

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

	tgui::Label::Ptr spell1description = tgui::Label::create();
	spell1description->setInheritedFont(font2);

	tgui::Label::Ptr spell2description = tgui::Label::create();
	spell2description->setInheritedFont(font2);

	tgui::Label::Ptr spell3description = tgui::Label::create();
	spell3description->setInheritedFont(font2);

	tgui::Label::Ptr spell4description = tgui::Label::create();
	spell4description->setInheritedFont(font2);


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
	gui->add(spell1, "spell1");
	gui->add(spell2, "spell2");
	gui->add(spell3, "spell3");
	gui->add(spell4, "spell4");

	gui->add(spell1description, "spell1Description");
	gui->add(spell2description, "spell2Description");
	gui->add(spell3description, "spell3Description");
	gui->add(spell4description, "spell4Description");
	
	gui->add(buttonLock, "buttonLock");



	setClassView();
}

void ClassSelectionScreen::setClassView()
{

	tw::BaseCharacterModel * model = classesInstances[indexClass];

	std::string pathClassPreview = model->getClassPreviewPath();
	sf::Texture TextureClassPreview;
	TextureClassPreview.loadFromFile(pathClassPreview);
	TextureClassPreview.setSmooth(true);
	tgui::Picture::Ptr previewClass = gui->get<tgui::Picture>("classPreview");
	previewClass->setPosition(PositionOfCardX, PositionOfCardY);
	previewClass->getRenderer()->setTexture(TextureClassPreview);
	
	std::string path = model->getClassIconPath();
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

	std::string className = model->getClassName();
	tgui::Label::Ptr classNameLabel = gui->get<tgui::Label>("classNameLabel");
	classNameLabel->setText(className);
	classNameLabel->setPosition(PositionOfCardX + 700, PositionOfCardY+ 300);
	classNameLabel->setTextSize(25);
	
	std::string attaque = std::to_string(model->getBaseAttack());
	tgui::Label::Ptr atkLabel = gui->get<tgui::Label>("atkLabel");
	atkLabel->setText("Attaque : " + attaque);
	atkLabel->setTextSize(frontsize2);
	atkLabel->setHorizontalAlignment(Horizontale_AlignementRight);
	
	std::string pm = std::to_string(model->getBasePm());
	tgui::Label::Ptr pmLabel = gui->get<tgui::Label>("pmLabel");
	pmLabel->setText("Point de déplacement: " + pm);
	pmLabel->setTextSize(frontsize2);
	pmLabel->setHorizontalAlignment(Horizontale_AlignementRight);


	std::string life = std::to_string(model->getBaseMaxLife());
	tgui::Label::Ptr lifeLabel = gui->get<tgui::Label>("lifeLabel");
	lifeLabel->setText("Point de vie : " + life);
	lifeLabel->setTextSize(frontsize2);
	lifeLabel->setHorizontalAlignment(Horizontale_AlignementRight);

	std::string pa = std::to_string(model->getBasePa());
	tgui::Label::Ptr paLabel = gui->get<tgui::Label>("paLabel");
	paLabel->setText("Points d'attaque : " + pa);
	paLabel->setTextSize(frontsize2);
	paLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Right);


	std::string def = std::to_string(model->getBaseDefense());
	tgui::Label::Ptr defLabel = gui->get<tgui::Label>("defLabel");
	defLabel->setText("Défense : " + def);
	defLabel->setTextSize(frontsize2);
	defLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Right);

	std::string description = model->getClassDescription();
	tgui::Label::Ptr descriptionLabel = gui->get<tgui::Label>("description");
	descriptionLabel->setText(description);
	descriptionLabel->setSize(500, 200);
	descriptionLabel->setTextSize(18);


	std::shared_ptr<tgui::Picture> baseSpell1 = gui->get<tgui::Picture>("spell1");
	std::shared_ptr<SpellSlot> spell1 = std::dynamic_pointer_cast<SpellSlot>(baseSpell1);
	spell1->setModel(model);
	spell1->setSize(tgui::Layout2d(100, 100));

	
	std::shared_ptr<tgui::Picture> baseSpell2 = gui->get<tgui::Picture>("spell2");
	std::shared_ptr<SpellSlot> spell2 = std::dynamic_pointer_cast<SpellSlot>(baseSpell2);
	spell2->setModel(model);
	spell2->setSize(tgui::Layout2d(100, 100));

	std::shared_ptr<tgui::Picture> baseSpell3 = gui->get<tgui::Picture>("spell3");
	std::shared_ptr<SpellSlot> spell3 = std::dynamic_pointer_cast<SpellSlot>(baseSpell3);
	spell3->setModel(model);
	spell3->setSize(tgui::Layout2d(100, 100));

	std::shared_ptr<tgui::Picture> baseSpell4 = gui->get<tgui::Picture>("spell4");
	std::shared_ptr<SpellSlot> spell4 = std::dynamic_pointer_cast<SpellSlot>(baseSpell4);
	spell4->setModel(model);
	spell4->setSize(tgui::Layout2d(100, 100));


	tgui::Color outlineColor = tgui::Color::White;
	float outlineWidth = 1;

	std::string spell1Name = model->getSpell1Name();
	std::string spell1Description = model->getSpell1Description();
	tgui::Label::Ptr labelSpell1Description = gui->get<tgui::Label>("spell1Description");
	labelSpell1Description->setText(spell1Name + "\n" + spell1Description);
	labelSpell1Description->setSize(sizeTextX, sizeTextY * 2 + 10);
	labelSpell1Description->setTextSize(18);
	labelSpell1Description->getRenderer()->setTextStyle(sf::Text::Bold);
	labelSpell1Description->getRenderer()->setTextOutlineColor(outlineColor);
	labelSpell1Description->getRenderer()->setTextOutlineThickness(outlineWidth);

	std::string spell2Name = model->getSpell2Name();
	std::string spell2Description = model->getSpell2Description();
	tgui::Label::Ptr labelSpell2Description = gui->get<tgui::Label>("spell2Description");
	labelSpell2Description->setText(spell2Name + "\n" + spell2Description);
	labelSpell2Description->setSize(sizeTextX, sizeTextY * 2 + 10);
	labelSpell2Description->setTextSize(18);
	labelSpell2Description->getRenderer()->setTextStyle(sf::Text::Bold);
	labelSpell2Description->getRenderer()->setTextOutlineColor(outlineColor);
	labelSpell2Description->getRenderer()->setTextOutlineThickness(outlineWidth);

	std::string spell3Name = model->getSpell3Name();
	std::string spell3Description = model->getSpell3Description();
	tgui::Label::Ptr labelSpell3Description = gui->get<tgui::Label>("spell3Description");
	labelSpell3Description->setText(spell3Name + "\n" + spell3Description);
	labelSpell3Description->setSize(sizeTextX, sizeTextY * 2 + 10);
	labelSpell3Description->setTextSize(18);
	labelSpell3Description->getRenderer()->setTextStyle(sf::Text::Bold);
	labelSpell3Description->getRenderer()->setTextOutlineColor(outlineColor);
	labelSpell3Description->getRenderer()->setTextOutlineThickness(outlineWidth);

	std::string spell4Name = model->getSpell4Name();
	std::string spell4Description = model->getSpell4Description();
	tgui::Label::Ptr labelSpell4Description = gui->get<tgui::Label>("spell4Description");
	labelSpell4Description->setText(spell4Name + "\n" + spell4Description);
	labelSpell4Description->setSize(sizeTextX, sizeTextY * 2 + 10);
	labelSpell4Description->setTextSize(18);
	labelSpell4Description->getRenderer()->setTextStyle(sf::Text::Bold);
	labelSpell4Description->getRenderer()->setTextOutlineColor(outlineColor);
	labelSpell4Description->getRenderer()->setTextOutlineThickness(outlineWidth);
	
	atkLabel->setPosition(PositionOfCardX + 510, PositionOfCardY +100);
	pmLabel->setPosition(PositionOfCardX + 510, PositionOfCardY + 130);
	lifeLabel->setPosition(PositionOfCardX + 510, PositionOfCardY + 160);
	paLabel->setPosition(PositionOfCardX + 510, PositionOfCardY + 190);
	defLabel->setPosition(PositionOfCardX + 510, PositionOfCardY + 220);
	descriptionLabel->setPosition(DescriptionX, DescriptionY+100);
	spell1->setPosition(tgui::Layout2d(215, 300));
	spell2->setPosition(tgui::Layout2d(215, 400));
	spell3->setPosition(tgui::Layout2d(215, 500));
	spell4->setPosition(tgui::Layout2d(215, 600));
	labelSpell1Description->setPosition(310, 315);
	labelSpell2Description->setPosition(310, 415);
	labelSpell3Description->setPosition(310, 515);
	labelSpell4Description->setPosition(310, 615);


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
		LinkToServer::getInstance()->Send("PC" + std::to_string(classesInstances[indexClass]->getClassId()));
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
}

void ClassSelectionScreen::onDisconnected()
{
	gui->removeAllWidgets();
	tw::ScreenManager::getInstance()->setCurrentScreen(new tw::LoginScreen(gui));
	delete this;
}
