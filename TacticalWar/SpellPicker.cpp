#include "SpellPicker.h"

#include <algorithm>
#include <string>

#include <BattleRules.h>

#include "LinkToServer.h"

using namespace tw;

namespace
{
	// Sorts au plus par classe (7 aujourd'hui : 6 sorts et un sort de terrain).
	const int MAX_SPELLS = 8;
	const float ROW_ICON = 48;

	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}

	sf::String spellText(const battle::SpellDef & spell)
	{
		sf::String text = fromServerText(spell.name) + L" - " + num(spell.apCost) + L" PA";
		if (spell.launch != battle::LaunchShape::SELF)
			text += L", portée " + num(spell.minRange) + L"-" + num(spell.maxRange);
		if (spell.cooldown > 0)
			text += L", relance " + num(spell.cooldown);
		return text + L"\n" + fromServerText(spell.description);
	}
}

SpellPicker::SpellPicker(const sf::Font & font, Layout layout)
	: font(font), layout(layout), group(tgui::Group::create()), classDef(nullptr), locked(false), width(500), rowHeight(82)
{
	for (int i = 0; i < MAX_SPELLS; i++)
	{
		tgui::Picture::Ptr icon = tgui::Picture::create();
		icon->connect("Clicked", [this, i]() { toggle(i); });
		group->add(icon);
		icons.push_back(icon);

		tgui::Label::Ptr label = tgui::Label::create();
		label->setInheritedFont(font);
		label->setTextSize(15);
		label->getRenderer()->setTextOutlineColor(sf::Color::Black);
		label->getRenderer()->setTextOutlineThickness(1);
		label->connect("Clicked", [this, i]() { toggle(i); });
		label->setVisible(layout == Layout::LIST);
		group->add(label);
		labels.push_back(label);
	}

	counter = tgui::Label::create();
	counter->setInheritedFont(font);
	counter->setTextSize(layout == Layout::LIST ? 17 : 14);
	counter->getRenderer()->setTextOutlineColor(sf::Color::Black);
	counter->getRenderer()->setTextOutlineThickness(1);
	group->add(counter);
}

void SpellPicker::setClass(const battle::ClassDef * newClass, const std::vector<int> & requested)
{
	classDef = newClass;
	chosen = classDef != nullptr ? battle::validSpellChoice(*classDef, requested) : std::vector<int>();

	for (int i = 0; i < MAX_SPELLS; i++)
	{
		bool exists = classDef != nullptr && i < (int)classDef->spells.size();
		icons[i]->setVisible(exists);
		labels[i]->setVisible(exists && layout == Layout::LIST);
		if (!exists)
			continue;

		const battle::SpellDef & spell = classDef->spells[i];
		sf::Texture texture;
		if (texture.loadFromFile(spell.icon))
			icons[i]->getRenderer()->setTexture(texture);
		labels[i]->setText(spellText(spell));

		// Rangée d'icônes : la description s'affiche au survol.
		if (layout == Layout::ROW)
		{
			tgui::Label::Ptr tip = tgui::Label::create(spellText(spell));
			tip->setInheritedFont(font);
			tip->setTextSize(13);
			tip->setMaximumTextWidth(360);
			tip->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 235));
			tip->getRenderer()->setTextColor(sf::Color::White);
			tip->getRenderer()->setBorders(1);
			tip->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
			tip->getRenderer()->setPadding(6);
			icons[i]->setToolTip(tip);
		}
	}
	arrange();
	refresh();
}

bool SpellPicker::isComplete() const
{
	return classDef != nullptr && (int)chosen.size() == (int)battle::defaultSpells(*classDef).size();
}

void SpellPicker::setLocked(bool value)
{
	locked = value;
	refresh();
}

void SpellPicker::setGeometry(float newWidth, float newRowHeight)
{
	width = newWidth;
	rowHeight = newRowHeight;
	arrange();
}

float SpellPicker::getHeight() const
{
	if (layout == Layout::ROW)
		return ROW_ICON;
	return rows() * rowHeight + 30;
}

int SpellPicker::rows() const
{
	return classDef != nullptr ? std::min(MAX_SPELLS, (int)classDef->spells.size()) : 6;
}

void SpellPicker::toggle(int index)
{
	if (locked || classDef == nullptr)
		return;
	auto it = std::find(chosen.begin(), chosen.end(), index);
	if (it != chosen.end())
		chosen.erase(it);
	else if ((int)chosen.size() < (int)battle::defaultSpells(*classDef).size())
		chosen.push_back(index);
	// Barre de sorts dans l'ordre de la classe.
	std::sort(chosen.begin(), chosen.end());
	refresh();
	if (onChange)
		onChange();
}

void SpellPicker::arrange()
{
	if (layout == Layout::LIST)
	{
		float icon = std::min(72.f, rowHeight - 8);
		for (int i = 0; i < MAX_SPELLS; i++)
		{
			icons[i]->setSize(icon, icon);
			icons[i]->setPosition(0, i * rowHeight);
			labels[i]->setPosition(icon + 12, i * rowHeight);
			labels[i]->setSize(std::max(100.f, width - icon - 12), rowHeight - 4);
			labels[i]->setTextSize(rowHeight < 70 ? 12 : 14);
		}
		counter->setPosition(0, rows() * rowHeight + 2);
		counter->setSize(width, 26);
		group->setSize(width, getHeight());
	}
	else
	{
		for (int i = 0; i < MAX_SPELLS; i++)
		{
			icons[i]->setSize(ROW_ICON, ROW_ICON);
			icons[i]->setPosition(i * (ROW_ICON + 8), 0);
		}
		float left = rows() * (ROW_ICON + 8) + 10;
		counter->setPosition(left, 4);
		counter->setSize(std::max(100.f, width - left), ROW_ICON);
		group->setSize(width, ROW_ICON);
	}
}

void SpellPicker::refresh()
{
	if (classDef == nullptr)
	{
		counter->setText(L"");
		return;
	}

	int needed = (int)battle::defaultSpells(*classDef).size();
	for (int i = 0; i < MAX_SPELLS; i++)
	{
		bool picked = std::find(chosen.begin(), chosen.end(), i) != chosen.end();
		icons[i]->getRenderer()->setOpacity(picked ? 1.f : 0.35f);
		labels[i]->getRenderer()->setTextColor(picked ? sf::Color(255, 240, 200) : sf::Color(150, 150, 150));
	}

	bool complete = (int)chosen.size() == needed;
	sf::String text = L"Sorts emportés : " + num((int)chosen.size()) + L"/" + num(needed);
	if (!locked)
		text += layout == Layout::LIST ? (complete ? L" - cliquez sur un sort pour le retirer" : L" - cliquez sur un sort pour l'ajouter")
			: (complete ? L"\nClic : retirer un sort. Survol : description." : L"\nCliquez sur un sort pour l'ajouter.");
	counter->setText(text);
	counter->getRenderer()->setTextColor(complete ? sf::Color(255, 215, 0) : sf::Color(255, 120, 100));
}
