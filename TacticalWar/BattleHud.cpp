#include "BattleHud.h"
#include "LinkToServer.h"

#include <BattleRules.h>
#include <Emotes.h>
#include <algorithm>

using namespace tw::battle;

namespace
{
	const float ROW_HEIGHT = 74;
	const float TIMELINE_WIDTH = 380;
	const float SPELL_SIZE = 72;

	std::map<std::string, tgui::Texture> textureCache;

	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}

	const tgui::Texture & cachedTexture(const std::string & path)
	{
		auto it = textureCache.find(path);
		if (it == textureCache.end())
		{
			tgui::Texture texture;
			try
			{
				texture.load(path);
			}
			catch (const tgui::Exception &)
			{
			}
			it = textureCache.insert(std::make_pair(path, texture)).first;
		}
		return it->second;
	}

	sf::String statLabel(Stat stat)
	{
		switch (stat)
		{
		case Stat::MAX_HP: return L"PV max";
		case Stat::AP: return L"PA";
		case Stat::MP: return L"PM";
		case Stat::INITIATIVE: return L"Initiative";
		case Stat::POWER: return L"% puissance";
		case Stat::RESISTANCE: return L"% résistance";
		case Stat::RANGE: return L"Portée";
		case Stat::HEAL_BONUS: return L"% soins";
		case Stat::LOCK: return L"Tacle";
		case Stat::DODGE: return L"Fuite";
		case Stat::EROSION: return L"% érosion";
		default: return L"";
		}
	}

	sf::String turnsLabel(const ActiveEffect & effect)
	{
		if (effect.remainingTurns <= 0)
			return L"ce tour";
		return num(effect.remainingTurns) + (effect.remainingTurns > 1 ? L" tours" : L" tour");
	}

	sf::String effectDescription(const ActiveEffect & effect)
	{
		sf::String name = fromServerText(effect.name);
		switch (effect.type)
		{
		case EffectType::STAT_MOD:
			return name + L" : " + (effect.value > 0 ? L"+" : L"") + num(effect.value) + L" " + statLabel(effect.stat) + L" (" + turnsLabel(effect) + L")";
		case EffectType::DOT:
			return name + L" : " + num(effect.minValue) + L" dégâts/tour (" + turnsLabel(effect) + L")";
		case EffectType::HOT:
			return name + L" : +" + num(effect.minValue) + L" PV/tour (" + turnsLabel(effect) + L")";
		case EffectType::SHIELD:
			return name + L" : bouclier " + num(effect.value) + L" (" + turnsLabel(effect) + L")";
		default:
			return name + L" (" + turnsLabel(effect) + L")";
		}
	}

	sf::String spellTooltip(const SpellDef & spell)
	{
		sf::String text = fromServerText(spell.name) + L"\n";
		text += num(spell.apCost) + L" PA";
		if (spell.launch == LaunchShape::SELF)
			text += L" - sur soi";
		else
			text += L" - portée " + num(spell.minRange) + L"-" + num(spell.maxRange) + (spell.rangeModifiable ? L" (modifiable)" : L"");
		if (spell.launch == LaunchShape::LINE)
			text += L" - en ligne";
		if (!spell.lineOfSight)
			text += L" - sans ligne de vue";
		if (spell.cooldown > 0)
			text += L"\nRelance : " + num(spell.cooldown) + L" tour(s)";
		if (spell.castsPerTurn > 0)
			text += L"\n" + num(spell.castsPerTurn) + L" lancer(s) par tour";
		text += L"\n\n" + fromServerText(spell.description);
		return text;
	}
}

BattleHud::BattleHud(tgui::Gui * gui, const sf::Font & font)
	: gui(gui), font(font), messageRemaining(0), readyState(false), spectator(false)
{
	timelinePanel = tgui::Panel::create();
	timelinePanel->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 170));
	gui->add(timelinePanel);

	timerLabel = createLabel(24, sf::Color::White);
	timerLabel->getRenderer()->setTextOutlineColor(sf::Color::Black);
	timerLabel->getRenderer()->setTextOutlineThickness(2);
	gui->add(timerLabel);

	// Score de la zone à tenir, sous le minuteur.
	zoneLabel = createLabel(18, sf::Color(255, 210, 80));
	zoneLabel->getRenderer()->setTextOutlineColor(sf::Color::Black);
	zoneLabel->getRenderer()->setTextOutlineThickness(2);
	zoneLabel->setVisible(false);
	gui->add(zoneLabel);

	messageLabel = createLabel(30, sf::Color(255, 220, 80));
	messageLabel->getRenderer()->setTextOutlineColor(sf::Color::Black);
	messageLabel->getRenderer()->setTextOutlineThickness(2);
	// Textes seuls : ils ne doivent pas intercepter les clics (carte, barre de sorts).
	messageLabel->setEnabled(false);
	gui->add(messageLabel);

	hintLabel = createLabel(16, sf::Color(255, 200, 120));
	hintLabel->getRenderer()->setTextOutlineColor(sf::Color::Black);
	hintLabel->getRenderer()->setTextOutlineThickness(1);
	hintLabel->setEnabled(false);
	gui->add(hintLabel);

	detailsPanel = tgui::Panel::create();
	detailsPanel->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 190));
	detailsLabel = createLabel(15, sf::Color::White);
	detailsLabel->setPosition(10, 8);
	detailsPanel->add(detailsLabel);
	gui->add(detailsPanel);

	logBox = tgui::ChatBox::create();
	logBox->setInheritedFont(font);
	logBox->setTextSize(14);
	logBox->setLineLimit(200);
	logBox->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 170));
	logBox->getRenderer()->setBorderColor(sf::Color(80, 80, 100));
	gui->add(logBox);

	for (int i = 0; i < 4; i++)
	{
		SpellButton button;
		button.icon = tgui::Picture::create();
		button.icon->connect("Clicked", [this, i]() {
			if (onSpellClicked)
				onSpellClicked(i);
		});
		button.tooltip = createLabel(15, sf::Color::White);
		button.tooltip->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 230));
		button.tooltip->getRenderer()->setPadding(8);
		button.icon->setToolTip(button.tooltip);

		button.cost = createLabel(16, sf::Color(120, 200, 255));
		button.cost->getRenderer()->setTextOutlineColor(sf::Color::Black);
		button.cost->getRenderer()->setTextOutlineThickness(2);
		button.cooldown = createLabel(34, sf::Color::White);
		button.cooldown->getRenderer()->setTextOutlineColor(sf::Color::Black);
		button.cooldown->getRenderer()->setTextOutlineThickness(2);
		button.key = createLabel(14, sf::Color(255, 230, 150));
		button.key->setText(num(i + 1));
		button.key->getRenderer()->setTextOutlineColor(sf::Color::Black);
		button.key->getRenderer()->setTextOutlineThickness(1);

		gui->add(button.icon);
		gui->add(button.cost);
		gui->add(button.cooldown);
		gui->add(button.key);
		// Les étiquettes ne doivent pas intercepter les clics destinés à l'icône.
		button.cost->setEnabled(false);
		button.cooldown->setEnabled(false);
		button.key->setEnabled(false);
		spells.push_back(button);
	}

	endTurnButton = tgui::Button::create(L"Passer le tour");
	endTurnButton->setInheritedFont(font);
	endTurnButton->setTextSize(16);
	endTurnButton->connect("pressed", [this]() {
		if (onEndTurn)
			onEndTurn();
	});
	gui->add(endTurnButton);

	// Émotes prédéfinies : le bouton ouvre la liste (aussi au clavier, touches F1 à F6).
	emoteButton = tgui::Button::create(L"Émotes");
	emoteButton->setInheritedFont(font);
	emoteButton->setTextSize(16);
	emoteButton->connect("pressed", [this]() { emotePanel->setVisible(!emotePanel->isVisible()); });
	gui->add(emoteButton);

	emotePanel = tgui::Panel::create();
	emotePanel->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 225));
	emotePanel->setVisible(false);
	for (int i = 0; i < EMOTE_COUNT; i++)
	{
		tgui::Button::Ptr button = tgui::Button::create(L"F" + num(i + 1) + L"   " + fromServerText(EMOTE_TEXTS[i]));
		button->setInheritedFont(font);
		button->setTextSize(15);
		button->setPosition(6, 6 + i * 38.f);
		button->setSize(208, 34);
		button->connect("pressed", [this, i]() {
			emotePanel->setVisible(false);
			if (onEmote)
				onEmote(i);
		});
		emotePanel->add(button);
	}
	gui->add(emotePanel);

	readyButton = tgui::Button::create(L"Prêt !");
	readyButton->setInheritedFont(font);
	readyButton->setTextSize(20);
	readyButton->connect("pressed", [this]() {
		if (onReady)
			onReady(!readyState);
	});
	gui->add(readyButton);

	endPanel = tgui::Panel::create();
	endPanel->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 230));
	endPanel->getRenderer()->setBorders(2);
	endPanel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	endPanel->setVisible(false);
	gui->add(endPanel);

	bannerLabel = createLabel(20, sf::Color(255, 215, 0));
	bannerLabel->getRenderer()->setTextOutlineColor(sf::Color::Black);
	bannerLabel->getRenderer()->setTextOutlineThickness(2);
	bannerLabel->setVisible(false);
	gui->add(bannerLabel);

	leaveButton = tgui::Button::create(L"Quitter");
	leaveButton->setInheritedFont(font);
	leaveButton->setTextSize(18);
	leaveButton->setVisible(false);
	leaveButton->connect("pressed", [this]() {
		if (onClose)
			onClose();
	});
	gui->add(leaveButton);

	cameraHelp = createLabel(13, sf::Color(220, 220, 220));
	cameraHelp->setText(L"Molette : zoom   Clic droit : déplacer   F : suivre   C : recentrer");
	cameraHelp->getRenderer()->setTextOutlineColor(sf::Color::Black);
	cameraHelp->getRenderer()->setTextOutlineThickness(1);
	cameraHelp->setEnabled(false);
	gui->add(cameraHelp);
}

void BattleHud::setSpectator(const sf::String & banner)
{
	spectator = true;
	bannerLabel->setText(banner);
	bannerLabel->setVisible(true);
	leaveButton->setVisible(true);
	layout(windowSize);
}

void BattleHud::showLeaveButton(const sf::String & text)
{
	leaveButton->setText(text);
	leaveButton->setVisible(true);
	layout(windowSize);
}

void BattleHud::setEndButtonText(const sf::String & text)
{
	if (endButton != nullptr && endButton->getText() != text)
		endButton->setText(text);
}

tgui::Label::Ptr BattleHud::createLabel(unsigned int size, const sf::Color & color)
{
	tgui::Label::Ptr label = tgui::Label::create();
	label->setInheritedFont(font);
	label->setTextSize(size);
	label->getRenderer()->setTextColor(color);
	return label;
}

void BattleHud::layout(const sf::Vector2u & size)
{
	windowSize = size;
	float width = (float)size.x;
	float height = (float)size.y;

	timelinePanel->setPosition(width - TIMELINE_WIDTH - 15, 15);
	timerLabel->setPosition((width - timerLabel->getSize().x) / 2, 12);
	zoneLabel->setPosition((width - zoneLabel->getSize().x) / 2, spectator ? 76.f : 46.f);
	messageLabel->setPosition((width - messageLabel->getSize().x) / 2, height / 2 - 140);
	hintLabel->setPosition((width - hintLabel->getSize().x) / 2, height - SPELL_SIZE - 60);

	detailsPanel->setPosition(15, 15);
	detailsPanel->setSize(330, detailsLabel->getSize().y + 16);

	logBox->setPosition(15, height - 215);
	logBox->setSize(440, 200);

	float barWidth = 4 * (SPELL_SIZE + 10) + 190 + 120;
	float barX = (width - barWidth) / 2;
	float barY = height - SPELL_SIZE - 18;
	for (int i = 0; i < (int)spells.size(); i++)
	{
		float x = barX + i * (SPELL_SIZE + 10);
		spells[i].icon->setPosition(x, barY);
		spells[i].icon->setSize(SPELL_SIZE, SPELL_SIZE);
		spells[i].key->setPosition(x + 4, barY + 2);
		spells[i].cost->setPosition(x + SPELL_SIZE - spells[i].cost->getSize().x - 4, barY + SPELL_SIZE - spells[i].cost->getSize().y - 2);
		spells[i].cooldown->setPosition(x + (SPELL_SIZE - spells[i].cooldown->getSize().x) / 2, barY + (SPELL_SIZE - spells[i].cooldown->getSize().y) / 2);
	}

	endTurnButton->setPosition(barX + 4 * (SPELL_SIZE + 10) + 10, barY + 8);
	endTurnButton->setSize(170, SPELL_SIZE - 16);
	emoteButton->setPosition(barX + 4 * (SPELL_SIZE + 10) + 190, barY + 8);
	emoteButton->setSize(110, SPELL_SIZE - 16);
	emotePanel->setSize(220, EMOTE_COUNT * 38.f + 8);
	emotePanel->setPosition(barX + 4 * (SPELL_SIZE + 10) + 300 - 220, barY - (EMOTE_COUNT * 38.f + 8) - 8);
	readyButton->setSize(220, 60);
	readyButton->setPosition((width - 220) / 2, height - 90);

	endPanel->setSize(endPanelSize.x, endPanelSize.y);
	endPanel->setPosition((width - endPanelSize.x) / 2, (height - endPanelSize.y) / 2);

	bannerLabel->setPosition((width - bannerLabel->getSize().x) / 2, 46);
	if (spectator)
	{
		leaveButton->setSize(180, 50);
		leaveButton->setPosition((width - 180) / 2, height - 68);
	}
	else
	{
		leaveButton->setSize(150, 36);
		leaveButton->setPosition(width - 165, height - cameraHelp->getSize().y - 56);
	}
	cameraHelp->setPosition(width - cameraHelp->getSize().x - 15, height - cameraHelp->getSize().y - 10);
}

void BattleHud::update(float deltatime)
{
	if (messageRemaining > 0)
	{
		messageRemaining -= deltatime;
		if (messageRemaining <= 0)
			messageLabel->setText("");
	}
}

void BattleHud::showMessage(const sf::String & text, const sf::Color & color, float seconds)
{
	messageLabel->setText(text);
	messageLabel->getRenderer()->setTextColor(color);
	messageRemaining = seconds;
	layout(windowSize);
}

void BattleHud::setHint(const sf::String & text)
{
	if (hintLabel->getText() != text)
	{
		hintLabel->setText(text);
		layout(windowSize);
	}
}

void BattleHud::log(const sf::String & line, const sf::Color & color)
{
	logBox->addLine(line, color);
}

void BattleHud::setSpellBar(const GameData & data, const Fighter & fighter)
{
	for (int i = 0; i < (int)spells.size(); i++)
	{
		SpellButton & button = spells[i];
		const SpellDef * slotSpell = spellOf(data, fighter, i);
		if (slotSpell == nullptr)
		{
			button.icon->setVisible(false);
			continue;
		}

		const SpellDef & spell = *slotSpell;
		button.spellId = spell.id;
		button.icon->getRenderer()->setTexture(cachedTexture(spell.icon));
		button.cost->setText(num(spell.apCost));
		button.tooltip->setText(spellTooltip(spell));
	}
}

sf::String BattleHud::fighterSummary(const BattleState & state, const GameData & data, const Fighter & fighter)
{
	const ClassDef * classDef = data.findClass(fighter.classId);
	sf::String text = fromServerText(fighter.name);
	if (classDef != nullptr)
		text += L" (" + fromServerText(classDef->name) + L")";

	text += L"\nPV " + num(fighter.hp) + L"/" + num(fighter.maxHp);
	if (fighter.shield > 0)
		text += L"\nBouclier " + num(fighter.shield) + L" : absorbe les dégâts en premier";
	text += L"\nPA " + num(fighter.ap) + L"   PM " + num(fighter.mp);
	text += L"\nPuissance " + num(effectiveStat(state, data, fighter, Stat::POWER))
		+ L"%   Résistance " + num(effectiveStat(state, data, fighter, Stat::RESISTANCE)) + L"%";
	text += L"\nTacle " + num(effectiveStat(state, data, fighter, Stat::LOCK))
		+ L"   Fuite " + num(effectiveStat(state, data, fighter, Stat::DODGE))
		+ L"   Portée +" + num(effectiveStat(state, data, fighter, Stat::RANGE));

	if (classDef != nullptr && classDef->passive.type != PassiveType::NONE)
		text += L"\nPassif : " + fromServerText(classDef->passive.name);

	// Talents de tournoi (visibles de tous : adversaires et spectateurs compris).
	sf::String talents;
	for (const std::string & id : fighter.talents)
	{
		const TalentDef * talent = data.findTalent(id);
		talents += (talents.isEmpty() ? sf::String() : sf::String(L", ")) + fromServerText(talent != nullptr ? talent->name : id);
	}
	if (!talents.isEmpty())
		text += L"\nTalents : " + talents;

	for (const ActiveEffect & effect : fighter.effects)
		text += L"\n- " + effectDescription(effect);

	if (!fighter.connected)
		text += L"\n(déconnecté)";
	return text;
}

void BattleHud::refresh(const BattleState & state, const GameData & data, int you, int hoveredFighter, int selectedSpell, bool myTurn, float remainingSeconds)
{
	// Ordre de jeu (ou liste des combattants pendant le placement).
	std::vector<int> order = state.turnOrder;
	if (order.empty())
	{
		for (const Fighter & fighter : state.fighters)
			order.push_back(fighter.id);
	}

	while (rows.size() < order.size())
	{
		TimelineRow row;
		row.panel = tgui::Panel::create();
		row.name = createLabel(15, sf::Color::White);
		row.life = createLabel(14, sf::Color(255, 120, 120));
		row.shield = createLabel(14, sf::Color(130, 195, 255));
		row.stats = createLabel(14, sf::Color(225, 225, 225));
		row.details = createLabel(13, sf::Color(210, 210, 210));
		row.name->setPosition(10, 4);
		row.life->setPosition(10, 26);
		row.details->setPosition(10, 46);
		row.barBack = tgui::Panel::create();
		row.barBack->getRenderer()->setBackgroundColor(sf::Color(15, 15, 20, 200));
		row.barLife = tgui::Panel::create();
		row.barLife->getRenderer()->setBackgroundColor(sf::Color(225, 70, 60));
		row.barShield = tgui::Panel::create();
		row.barShield->getRenderer()->setBackgroundColor(sf::Color(110, 180, 255));
		for (const tgui::Widget::Ptr & widget : std::vector<tgui::Widget::Ptr>{ row.name, row.life, row.shield, row.stats, row.details,
			row.barBack, row.barLife, row.barShield })
			row.panel->add(widget);
		timelinePanel->add(row.panel);
		rows.push_back(row);
	}

	int active = state.activeFighterId();
	for (std::size_t i = 0; i < order.size(); i++)
	{
		const Fighter * fighter = state.findFighter(order[i]);
		if (fighter == nullptr)
			continue;

		TimelineRow & row = rows[i];
		row.panel->setPosition(6, 6 + i * (ROW_HEIGHT + 6));
		row.panel->setSize(TIMELINE_WIDTH - 12, ROW_HEIGHT);

		sf::Color teamColor = fighter->team == 1 ? sf::Color(40, 80, 170, 210) : sf::Color(160, 40, 40, 210);
		if (!fighter->alive)
			teamColor = sf::Color(60, 60, 60, 200);
		row.panel->getRenderer()->setBackgroundColor(teamColor);
		row.panel->getRenderer()->setBorders(fighter->id == active ? 3 : 0);
		row.panel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));

		const ClassDef * classDef = data.findClass(fighter->classId);
		sf::String name = fromServerText(fighter->name);
		if (classDef != nullptr)
			name += L" - " + fromServerText(classDef->name);
		if (fighter->id == you)
			name += L" (vous)";
		if (state.phase == BattlePhase::PLACEMENT && fighter->ready)
			name += L"  PRÊT";
		row.name->setText(name);

		// PV en rouge, bouclier en bleu ("+20"), puis PA et PM.
		row.life->setText(fighter->alive ? L"PV " + num(fighter->hp) + L"/" + num(fighter->maxHp) : sf::String(L"Mort"));
		bool shielded = fighter->alive && fighter->shield > 0;
		row.shield->setVisible(shielded);
		row.shield->setText(shielded ? L"+" + num(fighter->shield) : sf::String());
		row.shield->setPosition(10 + row.life->getSize().x + 4, 26);
		row.stats->setVisible(fighter->alive);
		row.stats->setText(L"PA " + num(fighter->ap) + L"  PM " + num(fighter->mp));
		row.stats->setPosition((shielded ? row.shield->getPosition().x + row.shield->getSize().x : 10 + row.life->getSize().x) + 14, 26);

		// Barre de vie au bas de la ligne : le bouclier prolonge les PV (il est consommé en premier).
		// PV + bouclier au-delà du maximum : la barre représente ce total, pour que le bouclier reste visible.
		float barWidth = TIMELINE_WIDTH - 12 - 20;
		float total = (float)std::max(1, std::max(fighter->maxHp, fighter->hp + (shielded ? fighter->shield : 0)));
		float lifeWidth = fighter->alive ? barWidth * fighter->hp / total : 0.f;
		float shieldWidth = shielded ? barWidth * fighter->shield / total : 0.f;
		row.barBack->setPosition(10, ROW_HEIGHT - 9);
		row.barBack->setSize(barWidth, 5);
		row.barLife->setPosition(10, ROW_HEIGHT - 9);
		row.barLife->setSize(lifeWidth, 5);
		row.barLife->setVisible(lifeWidth > 0);
		row.barShield->setPosition(10 + lifeWidth, ROW_HEIGHT - 9);
		row.barShield->setSize(shieldWidth, 5);
		row.barShield->setVisible(shieldWidth > 0);

		sf::String effects;
		for (const ActiveEffect & effect : fighter->effects)
		{
			if (!effects.isEmpty())
				effects += L", ";
			effects += fromServerText(effect.name) + L" " + num(std::max(0, effect.remainingTurns));
		}
		if (!fighter->connected)
			effects = L"Déconnecté  " + effects;
		row.details->setText(effects);
	}
	timelinePanel->setSize(TIMELINE_WIDTH, 12 + order.size() * (ROW_HEIGHT + 6));

	// Minuteur.
	sf::String timer;
	sf::String seconds = timersShown ? L" - " + num((int)remainingSeconds) + L" s" : sf::String();
	if (state.phase == BattlePhase::PLACEMENT)
		timer = L"Placement" + seconds;
	else if (state.phase == BattlePhase::FIGHT)
	{
		const Fighter * current = state.findFighter(active);
		timer = L"Tour " + num(state.round) + L" - " + (current != nullptr ? fromServerText(current->name) : sf::String()) + seconds;
	}
	if (timerLabel->getText() != timer)
	{
		timerLabel->setText(timer);
		timerLabel->getRenderer()->setTextColor(myTurn ? sf::Color(120, 255, 120) : sf::Color::White);
		layout(windowSize);
	}

	// Zone à tenir : score de chaque équipe (la sienne d'abord pour un joueur).
	sf::String zoneText;
	if (state.zone.enabled)
	{
		const Fighter * me = state.findFighter(you);
		sf::String goal = L"  (premier à " + num(state.zone.pointsToWin) + L")";
		if (me != nullptr)
			zoneText = L"Zone à tenir : votre équipe " + num(state.zone.scores[me->team]) + L" - " + num(state.zone.scores[3 - me->team]) + L" adversaires" + goal;
		else
			zoneText = L"Zone à tenir : bleus " + num(state.zone.scores[1]) + L" - " + num(state.zone.scores[2]) + L" rouges" + goal;
	}
	zoneLabel->setVisible(!zoneText.isEmpty());
	if (zoneLabel->getText() != zoneText)
	{
		zoneLabel->setText(zoneText);
		layout(windowSize);
	}

	// Détails du combattant survolé (par défaut : le sien, ou le combattant actif pour un spectateur).
	int detailsId = hoveredFighter >= 0 ? hoveredFighter : (you >= 0 ? you : active);
	const Fighter * shown = state.findFighter(detailsId);
	detailsPanel->setVisible(shown != nullptr);
	if (shown != nullptr)
	{
		sf::String summary = fighterSummary(state, data, *shown);
		if (detailsLabel->getText() != summary)
		{
			detailsLabel->setText(summary);
			layout(windowSize);
		}
	}

	// Barre de sorts du joueur.
	const Fighter * me = state.findFighter(you);
	bool fighting = state.phase == BattlePhase::FIGHT;
	for (SpellButton & button : spells)
		button.icon->setVisible(me != nullptr && fighting);

	if (me != nullptr)
	{
		std::string key = std::to_string(me->classId) + ":";
		for (int index : me->spells)
			key += std::to_string(index) + ",";
		if (key != spellBarKey)
		{
			spellBarKey = key;
			setSpellBar(data, *me);
			layout(windowSize);
		}

		for (int i = 0; i < (int)spells.size() && spellOf(data, *me, i) != nullptr; i++)
		{
			SpellButton & button = spells[i];
			const SpellDef & spell = *spellOf(data, *me, i);
			auto cooldown = me->cooldowns.find(spell.id);
			int remaining = cooldown == me->cooldowns.end() ? 0 : cooldown->second;
			// Grisé : un sort en relance l'est en permanence ; un manque de PA (ou de lancers) seulement
			// pendant son tour, les PA revenant au tour suivant. Pendant une visée, les autres sorts
			// disponibles sont atténués.
			bool usable = remaining <= 0 && (!myTurn || checkSpellResources(*me, spell).empty());
			float opacity = !usable ? 0.3f : (selectedSpell >= 0 && selectedSpell != i) ? 0.65f : 1.0f;
			button.icon->getRenderer()->setOpacity(opacity);
			button.key->getRenderer()->setTextColor(selectedSpell == i ? sf::Color(120, 255, 120) : sf::Color(255, 230, 150));
			button.cooldown->setText(remaining > 0 ? num(remaining) : "");
			button.cooldown->setVisible(fighting);
			button.cost->setVisible(fighting);
			button.key->setVisible(fighting);
		}
	}
	else
	{
		for (SpellButton & button : spells)
		{
			button.cooldown->setVisible(false);
			button.cost->setVisible(false);
			button.key->setVisible(false);
		}
	}

	endTurnButton->setVisible(fighting && myTurn);
	bool canEmote = me != nullptr && !spectator && state.phase != BattlePhase::ENDED;
	emoteButton->setVisible(canEmote);
	if (!canEmote)
		emotePanel->setVisible(false);
	readyButton->setVisible(state.phase == BattlePhase::PLACEMENT && me != nullptr);
	if (me != nullptr && me->ready != readyState)
	{
		readyState = me->ready;
		readyButton->setText(readyState ? L"Annuler" : L"Prêt !");
	}
}

void BattleHud::showEnd(const sf::String & title, const sf::String & details, bool victory, const std::vector<EndRow> & rows)
{
	endPanel->removeAllWidgets();
	endPanelSize.x = rows.empty() ? 660.f : 760.f;

	tgui::Label::Ptr titleLabel = createLabel(34, victory ? sf::Color(120, 255, 120) : sf::Color(255, 120, 120));
	titleLabel->setText(title);
	titleLabel->setPosition(20, 20);
	endPanel->add(titleLabel);

	tgui::Label::Ptr detailsText = createLabel(18, sf::Color::White);
	detailsText->setMaximumTextWidth(endPanelSize.x - 40);
	detailsText->setText(details);
	detailsText->setPosition(20, 76);
	endPanel->add(detailsText);

	// Bilan : une ligne par combattant (couleur de son équipe), le MVP en doré, et ses hauts faits
	// en dessous (descriptions au survol).
	// Colonne des noms assez large pour "Prénom (Classe)   MVP".
	const float columns[5] = { 20, 400, 495, 580, 700 };
	const sf::String headers[5] = { L"Combattant", L"Dégâts", L"Soins", L"Boucliers", L"KO" };
	// Sous le texte (3 ou 4 lignes selon le mode de combat).
	float tableTop = std::max(160.f, 76.f + detailsText->getSize().y + 12.f);
	float rowTop = tableTop + 28;
	if (!rows.empty())
	{
		for (int column = 0; column < 5; column++)
		{
			tgui::Label::Ptr header = createLabel(15, sf::Color(170, 170, 190));
			header->setText(headers[column]);
			header->setPosition(columns[column], tableTop);
			endPanel->add(header);
		}
		for (std::size_t i = 0; i < rows.size(); i++)
		{
			const EndRow & row = rows[i];
			sf::Color color = row.mvp ? sf::Color(255, 215, 70) : row.team == 1 ? sf::Color(150, 200, 255) : sf::Color(255, 160, 150);
			const sf::String cells[5] = { row.name + (row.mvp ? sf::String(L"   MVP") : sf::String()), num(row.dealt), num(row.healed), num(row.shielded), num(row.kills) };
			for (int column = 0; column < 5; column++)
			{
				tgui::Label::Ptr cell = createLabel(17, color);
				cell->setText(cells[column]);
				cell->setPosition(columns[column], rowTop);
				endPanel->add(cell);
			}
			rowTop += 28;
			if (!row.badges.isEmpty())
			{
				tgui::Label::Ptr badges = createLabel(14, sf::Color(255, 205, 90));
				badges->setMaximumTextWidth(endPanelSize.x - 60);
				badges->setText(L"Hauts faits : " + row.badges);
				badges->setPosition(columns[0] + 18, rowTop - 4);
				tgui::Label::Ptr tip = createLabel(14, sf::Color::White);
				tip->setMaximumTextWidth(420);
				tip->setText(row.badgeDetails);
				tip->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 235));
				tip->getRenderer()->setBorders(1);
				tip->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
				tip->getRenderer()->setPadding(6);
				badges->setToolTip(tip);
				endPanel->add(badges);
				rowTop += 22;
			}
		}
	}
	float buttonTop = rows.empty() ? tableTop : rowTop + 20;
	endPanelSize.y = buttonTop + 44 + 16;

	endButton = tgui::Button::create(spectator ? L"Retour à la liste" : onReplay ? L"Retour" : L"Fermer");
	endButton->setInheritedFont(font);
	endButton->setTextSize(18);
	endButton->setSize(spectator ? 240 : 160, 44);
	endButton->setPosition((endPanelSize.x - (spectator ? 240 : 160)) / 2, buttonTop);
	endButton->connect("pressed", [this]() {
		if (onClose)
			onClose();
	});
	endPanel->add(endButton);
	if (onReplay)
	{
		// Rejouer (à gauche) et Retour (à droite).
		replayButton = tgui::Button::create(L"Rejouer");
		replayButton->setInheritedFont(font);
		replayButton->setTextSize(18);
		replayButton->setSize(160, 44);
		replayButton->setPosition(endPanelSize.x / 2 - 170, buttonTop);
		replayButton->connect("pressed", [this]() { onReplay(); });
		endPanel->add(replayButton);
		endButton->setPosition(endPanelSize.x / 2 + 10, buttonTop);
	}
	leaveButton->setVisible(false);

	endPanel->setVisible(true);
	layout(windowSize);
	endTurnButton->setVisible(false);
	emoteButton->setVisible(false);
	emotePanel->setVisible(false);
	for (SpellButton & button : spells)
		button.icon->setVisible(false);
}
