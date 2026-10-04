#include "ClassSelectionScreen.h"

#include <algorithm>
#include <cmath>

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
	if (!message.is_object())
		message = nlohmann::json::object();
	talentPicker.reset(new tw::TalentPicker(gui, font));
	talentPicker->setSlots(message.value("talents", 0));
	talentPicker->setChosen(ClientConfig::get().talentChoice);
	talentPicker->onChange = [this]() { refreshLock(); };
	gui->add(talentPicker->getButton());

	lockButton = createButton(font, L"Verrouiller mon choix", 20);
	lockButton->connect("pressed", [this]() {
		if (banMode)
			banRequested = true;
		else
			readyToLock = true;
	});
	gui->add(lockButton);

	// Bannissement : consigne et compte à rebours, puis classes interdites.
	banRemaining = (float)message.value("ban", 0);
	banMode = banRemaining > 0;
	banLabel = tgui::Label::create();
	banLabel->setInheritedFont(textFont);
	banLabel->setTextSize(20);
	banLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
	banLabel->getRenderer()->setTextColor(sf::Color(255, 225, 120));
	banLabel->getRenderer()->setTextOutlineColor(sf::Color::Black);
	banLabel->getRenderer()->setTextOutlineThickness(2);
	banLabel->setVisible(banMode);
	gui->add(banLabel);

	// Coéquipier : affiché dès que le serveur en donne l'état.
	matePanel = createPanel();
	mateTitle = tgui::Label::create();
	mateTitle->setInheritedFont(textFont);
	mateTitle->setTextSize(18);
	mateTitle->getRenderer()->setTextColor(sf::Color(255, 215, 0));
	mateTitle->setPosition(14, 8);
	matePanel->add(mateTitle);
	mateStatus = tgui::Label::create();
	mateStatus->setInheritedFont(textFont);
	mateStatus->setTextSize(16);
	mateStatus->setPosition(14, 36);
	matePanel->add(mateStatus);
	mateCombos = tgui::Label::create();
	mateCombos->setInheritedFont(textFont);
	mateCombos->setTextSize(15);
	mateCombos->getRenderer()->setTextColor(sf::Color(235, 235, 235));
	mateCombos->setPosition(14, 64);
	matePanel->add(mateCombos);
	matePanel->setVisible(false);
	gui->add(matePanel);

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

	refreshBan();
	refreshLock();
	refreshMate();
	if (windowSize.x > 0)
		layout(windowSize);

	// Le coéquipier voit la classe regardée (tant que le choix n'est pas verrouillé).
	if (!locked && classDef != NULL && classDef->id != viewSent)
	{
		viewSent = classDef->id;
		LinkToServer::getInstance()->Send("PV" + nlohmann::json({ { "class", classDef->id } }).dump());
	}
}

void ClassSelectionScreen::refreshMate()
{
	matePanel->setVisible(mateKnown);
	if (!mateKnown)
		return;

	mateTitle->setText(L"Votre coéquipier : " + mateName);
	sf::String status = !matePresent ? sf::String(L"Absent pour le moment.")
		: mateLocked ? L"A choisi : " + classLabel(mateClass) + L" (verrouillé)"
		: mateViewing != 0 ? L"Regarde : " + classLabel(mateViewing)
		: sf::String(L"Choisit sa classe...");
	mateStatus->setText(status);
	mateStatus->getRenderer()->setTextColor(mateLocked ? sf::Color(130, 255, 130) : matePresent ? sf::Color::White : sf::Color(255, 160, 140));

	// Combinaisons entre la classe affichée et celle du coéquipier (verrouillée, sinon regardée).
	auto join = [](const std::vector<std::string> & names) {
		sf::String text;
		for (std::size_t i = 0; i < names.size(); i++)
			text += (i == 0 ? sf::String() : i + 1 == names.size() ? sf::String(L" ou ") : sf::String(L", ")) + fromServerText(names[i]);
		return text;
	};
	int other = mateLocked ? mateClass : mateViewing;
	sf::String combos;
	if (other != 0)
	{
		for (const tw::battle::ComboLink & link : tw::battle::combosBetween(ClientGameData::get().data(), currentClassId(), other))
		{
			bool mine = link.setterClass == currentClassId();
			combos += L"\n- " + fromServerText(link.name) + L" (+" + num(link.percent) + L" %) : "
				+ (mine ? L"marquez avec " + join(link.setters) + L", puis votre coéquipier frappe avec " + join(link.finishers)
					: L"votre coéquipier marque avec " + join(link.setters) + L", puis frappez avec " + join(link.finishers)) + L".";
		}
		combos = combos.isEmpty() ? sf::String(L"Pas de combinaison entre vos deux classes.") : L"Combinaisons possibles :" + combos;
	}
	mateCombos->setText(combos);
	if (windowSize.x > 0)
		layout(windowSize);
}

sf::String ClassSelectionScreen::classLabel(int classId) const
{
	const tw::battle::ClassDef * classDef = ClientGameData::get().findClass(classId);
	return classDef != NULL ? fromServerText(classDef->name) : L"Classe " + num(classId);
}

void ClassSelectionScreen::refreshLock()
{
	if (banMode)
	{
		// Le premier bannissement de l'équipe compte : celui du coéquipier aussi.
		lockButton->setEnabled(!banSent && bannedClass == 0);
		lockButton->setText(bannedClass != 0 ? L"Bannissement fait" : banSent ? L"Bannissement envoyé" : L"Bannir cette classe");
		return;
	}
	bool forbidden = forbiddenClass != 0 && currentClassId() == forbiddenClass;
	bool complete = spellPicker->isComplete() && talentPicker->isComplete();
	lockButton->setEnabled(!locked && complete && !forbidden);
	lockButton->setText(locked ? L"Choix verrouillé" : forbidden ? L"Interdite par l'adversaire" : complete ? L"Verrouiller mon choix"
		: !spellPicker->isComplete() ? L"Choisissez 4 sorts" : L"Choisissez vos talents");
}

void ClassSelectionScreen::refreshBan()
{
	// Classe affichée interdite par l'adversaire : grisée et signalée.
	int classId = currentClassId();
	bool forbidden = !banMode && forbiddenClass != 0 && classId == forbiddenClass;
	className->setText(classLabel(classId) + (forbidden ? sf::String(L" - interdite") : sf::String()));
	className->getRenderer()->setTextColor(forbidden ? sf::Color(255, 110, 90) : sf::Color(255, 215, 0));
	preview->getRenderer()->setOpacity(forbidden ? 0.3f : 1.f);
	classIcon->getRenderer()->setOpacity(forbidden ? 0.3f : 1.f);
	characterPicture->setVisible(!forbidden);

	sf::String subtitleText = banMode ? L"Bannissement" : L"Sélection de la classe";
	if (subtitle.getString() != subtitleText)
	{
		subtitle.setString(subtitleText);
		if (windowSize.x > 0)
			layout(windowSize);
	}

	if (banMode)
	{
		sf::String seconds = L" (" + num(std::max(0, (int)std::ceil(banRemaining))) + L" s)";
		banLabel->setText(bannedClass != 0
			? L"Votre équipe interdit : " + classLabel(bannedClass) + L". En attente de l'adversaire..." + seconds
			: L"Choisissez une classe que l'équipe adverse ne pourra pas jouer" + seconds);
	}
	else if (banDone)
	{
		banLabel->setText((forbiddenClass != 0 ? L"Interdite par l'adversaire : " + classLabel(forbiddenClass) : sf::String(L"L'adversaire n'a interdit aucune classe"))
			+ (bannedClass != 0 ? L"     -     Votre équipe a interdit : " + classLabel(bannedClass) : sf::String()));
	}
	banLabel->setVisible(banMode || banDone);
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
	// Bas des blocs : au-dessus de la consigne de bannissement, si elle est affichée.
	float bottom = banLabel->isVisible() ? lockY - 40 : lockY;
	banLabel->setSize(width - 2 * margin, 32);
	banLabel->setPosition(margin, lockY - 38);

	// Gauche : sorts (lignes ajustées à la hauteur disponible), puis talents.
	float leftWidth = width * 0.36f;
	float rowHeight = std::max(56.f, std::min(82.f, (bottom - 60 - top - 30) / 6));
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
	float scale = std::min(1.f, std::min(centerWidth / CARD_WIDTH, (bottom - top - 230) / CARD_HEIGHT));
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
	descriptionLabel->setMaximumTextWidth(rightWidth - 28);
	// Coéquipier, sous la description.
	float mateHeight = 0;
	if (matePanel->isVisible())
	{
		mateCombos->setMaximumTextWidth(rightWidth - 28);
		mateHeight = 64 + mateCombos->getSize().y + 12;
		matePanel->setSize(rightWidth, mateHeight);
		matePanel->setPosition(rightX, bottom - 16 - mateHeight);
		mateHeight += 12;
	}
	descriptionPanel->setPosition(rightX, top + 214);
	descriptionPanel->setSize(rightWidth, std::max(120.f, bottom - 16 - mateHeight - (top + 214)));

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

	if (banMode)
	{
		banRemaining = std::max(0.f, banRemaining - deltatime);
		int seconds = (int)std::ceil(banRemaining);
		if (seconds != banSecondsShown)
		{
			banSecondsShown = seconds;
			refreshBan();
		}
	}
	if (banRequested)
	{
		banRequested = false;
		banSent = true;
		LinkToServer::getInstance()->Send("PB" + nlohmann::json({ { "class", currentClassId() } }).dump());
		refreshLock();
	}

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
	else if (m.substring(0, 2) == "PT")
	{
		// État du coéquipier : nom, classe regardée ou verrouillée, présence.
		nlohmann::json mate = nlohmann::json::parse(msg.substr(2), nullptr, false);
		if (mate.is_object())
		{
			mateKnown = true;
			mateName = fromServerText(mate.value("name", std::string()));
			mateClass = mate.value("class", 0);
			mateViewing = mate.value("viewing", 0);
			mateLocked = mate.value("locked", false);
			matePresent = mate.value("present", true);
			refreshMate();
		}
	}
	else if (m.substring(0, 2) == "BB")
	{
		// Bannissement de notre équipe enregistré, puis fin de la phase avec la classe interdite.
		nlohmann::json ban = nlohmann::json::parse(msg.substr(2), nullptr, false);
		if (ban.is_object())
		{
			bannedClass = ban.value("banned", 0);
			if (ban.value("done", false))
			{
				banMode = false;
				banDone = true;
				forbiddenClass = ban.value("forbidden", 0);
			}
			// La classe affichée est interdite : on montre la suivante.
			if (banDone && !locked && forbiddenClass != 0 && currentClassId() == forbiddenClass)
				showClass(indexClass + 1);
			refreshBan();
			refreshLock();
			if (windowSize.x > 0)
				layout(windowSize);
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
