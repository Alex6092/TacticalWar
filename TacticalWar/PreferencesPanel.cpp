#include "PreferencesPanel.h"

#include <algorithm>

#include <Appearances.h>

#include "AppearanceChoice.h"
#include "ClientConfig.h"
#include "ClientGameData.h"
#include "LinkToServer.h"

using namespace tw;

namespace
{
	tgui::Label::Ptr label(const sf::Font & font, const sf::String & text, unsigned int size, const sf::Color & color)
	{
		tgui::Label::Ptr created = tgui::Label::create(text);
		created->setInheritedFont(font);
		created->setTextSize(size);
		created->getRenderer()->setTextColor(color);
		return created;
	}
}

PreferencesPanel::PreferencesPanel(tgui::Gui * gui, const sf::Font & titleFont, const sf::Font & textFont)
	: gui(gui), textFont(textFont)
{
	panel = tgui::Panel::create();
	panel->getRenderer()->setBackgroundColor(sf::Color(15, 15, 25, 225));
	panel->getRenderer()->setBorders(2);
	panel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));

	title = label(titleFont, L"Mes préférences", 22, sf::Color(255, 215, 0));
	title->setPosition(20, 14);
	panel->add(title);
	note = label(textFont, L"Proposées au choix des classes de chaque match : gagnez du temps. Rien n'est verrouillé "
		L"d'avance, et une classe interdite par l'adversaire reste interdite.", 14, sf::Color(200, 200, 210));
	note->setPosition(20, 48);
	panel->add(note);

	const ClientConfig & config = ClientConfig::get();
	classTitle = label(textFont, L"Classe préférée", 16, sf::Color::White);
	panel->add(classTitle);
	classBox = tgui::ComboBox::create();
	classBox->setInheritedFont(textFont);
	classBox->setTextSize(15);
	classBox->setItemsToDisplay(6);
	classBox->addItem(L"Aucune (la première de la liste)");
	classIds.push_back(0);
	int selected = 0;
	for (const battle::ClassDef & classDef : ClientGameData::get().data().classes)
	{
		if (classDef.id == config.preferredClass)
			selected = (int)classIds.size();
		classBox->addItem(fromServerText(classDef.name));
		classIds.push_back(classDef.id);
	}
	classBox->setSelectedItemByIndex(selected);
	classBox->connect("ItemSelected", [this]() {
		if (refreshing)
			return;
		int index = classBox->getSelectedItemIndex();
		int classId = index >= 0 && index < (int)classIds.size() ? classIds[index] : 0;
		ClientConfig::get().preferredClass = classId;
		showClass(classId);
		save(L"Classe préférée enregistrée.");
	});
	panel->add(classBox);

	spellsTitle = label(textFont, L"Ses sorts (4 parmi 7, description au survol)", 16, sf::Color::White);
	panel->add(spellsTitle);
	spellPicker.reset(new SpellPicker(textFont, SpellPicker::Layout::ROW));
	spellPicker->onChange = [this]() {
		if (shownClass != 0 && spellPicker->isComplete())
		{
			ClientConfig::get().spellChoices[shownClass] = spellPicker->getChosen();
			save(L"Sorts enregistrés.");
		}
	};
	panel->add(spellPicker->getWidget());

	talentPicker.reset(new TalentPicker(gui, titleFont));
	talentPicker->setSlots(TALENT_SLOTS);
	talentPicker->setChosen(config.talentChoice);
	talentPicker->onChange = [this]() {
		ClientConfig::get().talentChoice = talentPicker->getChosen();
		save(L"Talents enregistrés, dans l'ordre de préférence.");
	};
	panel->add(talentPicker->getButton());

	appearanceTitle = label(textFont, L"Apparence", 16, sf::Color::White);
	panel->add(appearanceTitle);
	for (const battle::AppearanceDef & look : ClientGameData::get().data().appearances)
	{
		tgui::Button::Ptr swatch = tgui::Button::create();
		std::string id = look.id;
		tgui::Label::Ptr tip = label(textFont, fromServerText(look.name) + (look.unlockAchievement.empty() && look.unlockWins == 0 && look.unlockMvp == 0
			&& look.unlockPuzzles == 0 ? sf::String() : L"\nDébloquée par : " + fromServerText(battle::unlockCondition(look))), 13, sf::Color::White);
		tip->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 235));
		tip->getRenderer()->setBorders(1);
		tip->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
		tip->getRenderer()->setPadding(6);
		swatch->setToolTip(tip);
		swatch->connect("pressed", [this, id]() {
			std::vector<std::string> available = availableAppearances();
			if (std::find(available.begin(), available.end(), id) == available.end())
			{
				status->getRenderer()->setTextColor(sf::Color(255, 160, 120));
				status->setText(fromServerText(appearanceName(id)) + L" : pas encore débloquée.");
				return;
			}
			ClientConfig::get().appearance = id;
			refreshAppearances();
			save(L"Apparence enregistrée.");
		});
		panel->add(swatch);
		swatches.push_back(swatch);
		swatchIds.push_back(id);
	}

	status = label(textFont, L"", 15, sf::Color(140, 255, 140));
	panel->add(status);

	showClass(config.preferredClass);
	refreshAppearances();
}

void PreferencesPanel::showClass(int classId)
{
	shownClass = ClientGameData::get().findClass(classId) != nullptr ? classId : 0;
	const battle::ClassDef * classDef = ClientGameData::get().findClass(shownClass);
	spellPicker->setClass(classDef, classDef != nullptr ? ClientConfig::get().spellChoice(shownClass) : std::vector<int>());
	spellPicker->getWidget()->setVisible(classDef != nullptr);
	spellsTitle->setText(classDef != nullptr ? L"Sorts de " + fromServerText(classDef->name) + L" (4 parmi 7, description au survol)"
		: sf::String(L"Choisissez une classe pour régler ses sorts."));
}

void PreferencesPanel::refreshAppearances()
{
	std::vector<std::string> available = availableAppearances();
	std::string chosen = chosenAppearance();
	for (std::size_t i = 0; i < swatches.size(); i++)
	{
		int armor[3];
		int hair[3];
		appearanceColors(swatchIds[i], 1, armor, hair);
		bool unlocked = std::find(available.begin(), available.end(), swatchIds[i]) != available.end();
		bool selected = swatchIds[i] == chosen;
		tgui::ButtonRenderer * renderer = swatches[i]->getRenderer();
		renderer->setBackgroundColor(sf::Color(armor[0], armor[1], armor[2]));
		renderer->setBackgroundColorHover(sf::Color(armor[0], armor[1], armor[2]));
		renderer->setBorders(selected ? 5 : 3);
		renderer->setBorderColor(selected ? sf::Color(255, 215, 0) : sf::Color(hair[0], hair[1], hair[2]));
		renderer->setOpacity(unlocked ? 1.f : 0.35f);
	}
	appearanceTitle->setText(L"Apparence : " + fromServerText(appearanceName(chosen)));
}

void PreferencesPanel::save(const sf::String & what)
{
	ClientConfig::get().save();
	status->getRenderer()->setTextColor(sf::Color(140, 255, 140));
	status->setText(what);
}

void PreferencesPanel::layout(float width, float height)
{
	panel->setSize(width, height);
	float inner = width - 40;
	note->setMaximumTextWidth(inner);
	float y = 48 + note->getSize().y + 12;
	classTitle->setPosition(20, y + 6);
	classBox->setPosition(170, y);
	classBox->setSize(std::max(160.f, width - 190), 32);
	y += 46;
	spellsTitle->setPosition(20, y);
	spellPicker->setGeometry(inner, 0);
	spellPicker->getWidget()->setPosition(20, y + 26);
	y += 26 + spellPicker->getHeight() + 14;
	talentPicker->getButton()->setPosition(20, y);
	talentPicker->getButton()->setSize(std::min(inner, 330.f), 36);
	y += 50;
	appearanceTitle->setPosition(20, y);
	for (std::size_t i = 0; i < swatches.size(); i++)
	{
		swatches[i]->setSize(28, 28);
		swatches[i]->setPosition(20 + i * 36.f, y + 26);
	}
	status->setPosition(20, std::max(y + 66, height - 34));
}
