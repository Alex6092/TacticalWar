#include "BattleHud.h"
#include "LinkToServer.h"

#include <BattleRules.h>
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
	: gui(gui), font(font), messageRemaining(0), spellBarClassId(0), readyState(false)
{
	timelinePanel = tgui::Panel::create();
	timelinePanel->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 170));
	gui->add(timelinePanel);

	timerLabel = createLabel(24, sf::Color::White);
	timerLabel->getRenderer()->setTextOutlineColor(sf::Color::Black);
	timerLabel->getRenderer()->setTextOutlineThickness(2);
	gui->add(timerLabel);

	messageLabel = createLabel(30, sf::Color(255, 220, 80));
	messageLabel->getRenderer()->setTextOutlineColor(sf::Color::Black);
	messageLabel->getRenderer()->setTextOutlineThickness(2);
	gui->add(messageLabel);

	hintLabel = createLabel(16, sf::Color(255, 200, 120));
	hintLabel->getRenderer()->setTextOutlineColor(sf::Color::Black);
	hintLabel->getRenderer()->setTextOutlineThickness(1);
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
	messageLabel->setPosition((width - messageLabel->getSize().x) / 2, height / 2 - 140);
	hintLabel->setPosition((width - hintLabel->getSize().x) / 2, height - SPELL_SIZE - 60);

	detailsPanel->setPosition(15, 15);
	detailsPanel->setSize(330, detailsLabel->getSize().y + 16);

	logBox->setPosition(15, height - 215);
	logBox->setSize(440, 200);

	float barWidth = 4 * (SPELL_SIZE + 10) + 190;
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
	readyButton->setSize(220, 60);
	readyButton->setPosition((width - 220) / 2, height - 90);

	endPanel->setSize(520, 220);
	endPanel->setPosition((width - 520) / 2, (height - 220) / 2);
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

void BattleHud::setSpellBar(const ClassDef & classDef)
{
	spellBarClassId = classDef.id;
	for (int i = 0; i < (int)spells.size(); i++)
	{
		SpellButton & button = spells[i];
		if (i >= (int)classDef.spells.size())
		{
			button.icon->setVisible(false);
			continue;
		}

		const SpellDef & spell = classDef.spells[i];
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
		text += L"  +" + num(fighter.shield) + L" bouclier";
	text += L"\nPA " + num(fighter.ap) + L"   PM " + num(fighter.mp);
	text += L"\nPuissance " + num(effectiveStat(state, data, fighter, Stat::POWER))
		+ L"%   Résistance " + num(effectiveStat(state, data, fighter, Stat::RESISTANCE)) + L"%";
	text += L"\nTacle " + num(effectiveStat(state, data, fighter, Stat::LOCK))
		+ L"   Fuite " + num(effectiveStat(state, data, fighter, Stat::DODGE))
		+ L"   Portée +" + num(effectiveStat(state, data, fighter, Stat::RANGE));

	if (classDef != nullptr && classDef->passive.type != PassiveType::NONE)
		text += L"\nPassif : " + fromServerText(classDef->passive.name);

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
		row.details = createLabel(13, sf::Color(210, 210, 210));
		row.name->setPosition(10, 4);
		row.life->setPosition(10, 26);
		row.details->setPosition(10, 46);
		row.panel->add(row.name);
		row.panel->add(row.life);
		row.panel->add(row.details);
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

		sf::String life = fighter->alive ? L"PV " + num(fighter->hp) + L"/" + num(fighter->maxHp) : sf::String(L"Mort");
		if (fighter->alive && fighter->shield > 0)
			life += L"  (+" + num(fighter->shield) + L")";
		if (fighter->alive)
			life += L"   PA " + num(fighter->ap) + L"  PM " + num(fighter->mp);
		row.life->setText(life);

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
	if (state.phase == BattlePhase::PLACEMENT)
		timer = L"Placement - " + num((int)remainingSeconds) + L" s";
	else if (state.phase == BattlePhase::FIGHT)
	{
		const Fighter * current = state.findFighter(active);
		timer = L"Tour " + num(state.round) + L" - " + (current != nullptr ? fromServerText(current->name) : sf::String()) + L" - " + num((int)remainingSeconds) + L" s";
	}
	if (timerLabel->getText() != timer)
	{
		timerLabel->setText(timer);
		timerLabel->getRenderer()->setTextColor(myTurn ? sf::Color(120, 255, 120) : sf::Color::White);
		layout(windowSize);
	}

	// Détails du combattant survolé (ou du sien par défaut).
	int detailsId = hoveredFighter >= 0 ? hoveredFighter : you;
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
		const ClassDef * classDef = data.findClass(me->classId);
		if (classDef != nullptr && classDef->id != spellBarClassId)
		{
			setSpellBar(*classDef);
			layout(windowSize);
		}

		for (int i = 0; i < (int)spells.size() && classDef != nullptr && i < (int)classDef->spells.size(); i++)
		{
			SpellButton & button = spells[i];
			const SpellDef & spell = classDef->spells[i];
			auto cooldown = me->cooldowns.find(spell.id);
			int remaining = cooldown == me->cooldowns.end() ? 0 : cooldown->second;
			bool usable = myTurn && checkSpellResources(*me, spell).empty();

			float opacity = usable || !myTurn ? 1.0f : 0.35f;
			if (selectedSpell >= 0 && selectedSpell != i)
				opacity *= 0.6f;
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
	readyButton->setVisible(state.phase == BattlePhase::PLACEMENT && me != nullptr);
	if (me != nullptr && me->ready != readyState)
	{
		readyState = me->ready;
		readyButton->setText(readyState ? L"Annuler" : L"Prêt !");
	}
}

void BattleHud::showEnd(const sf::String & title, const sf::String & details, bool victory)
{
	endPanel->removeAllWidgets();

	tgui::Label::Ptr titleLabel = createLabel(34, victory ? sf::Color(120, 255, 120) : sf::Color(255, 120, 120));
	titleLabel->setText(title);
	titleLabel->setPosition(20, 20);
	endPanel->add(titleLabel);

	tgui::Label::Ptr detailsText = createLabel(18, sf::Color::White);
	detailsText->setText(details);
	detailsText->setPosition(20, 80);
	endPanel->add(detailsText);

	tgui::Button::Ptr close = tgui::Button::create(L"Fermer");
	close->setInheritedFont(font);
	close->setTextSize(18);
	close->setSize(160, 44);
	close->setPosition(340, 160);
	close->connect("pressed", [this]() {
		if (onClose)
			onClose();
	});
	endPanel->add(close);

	endPanel->setVisible(true);
	endTurnButton->setVisible(false);
	for (SpellButton & button : spells)
		button.icon->setVisible(false);
}
