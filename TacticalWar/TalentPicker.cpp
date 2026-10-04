#include "TalentPicker.h"

#include <algorithm>

#include <BattleRules.h>

#include "ClientGameData.h"
#include "LinkToServer.h"

using namespace tw;

namespace
{
	const float WINDOW_WIDTH = 620;
	const float TALENT_ROW = 46;

	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}
}

TalentPicker::TalentPicker(tgui::Gui * gui, const sf::Font & font)
	: gui(gui), font(font), slots(0), locked(false)
{
	button = tgui::Button::create();
	button->setInheritedFont(font);
	button->setTextSize(17);
	button->connect("pressed", [this]() { open(); });
	button->setVisible(false);

	// Fenêtre de choix, créée une fois et cachée (fermer depuis un de ses boutons la détruirait).
	const battle::GameData & data = ClientGameData::get().data();
	window = tgui::ChildWindow::create(L"Talents de tournoi");
	window->setInheritedFont(font);
	window->setTitleTextSize(16);
	window->setSize(WINDOW_WIDTH, 96 + data.talents.size() * TALENT_ROW + 56);
	window->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 245));
	window->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	window->getRenderer()->setBorders(2);
	window->connect("Closed", [this]() { window->setVisible(false); });

	tgui::Label::Ptr intro = tgui::Label::create(L"Un talent gagné par match joué dans le tournoi. Ils se choisissent avant chaque match, "
		L"pour le combat qui vient.");
	intro->setInheritedFont(font);
	intro->setTextSize(14);
	intro->setMaximumTextWidth(WINDOW_WIDTH - 30);
	intro->getRenderer()->setTextColor(sf::Color(220, 220, 220));
	intro->setPosition(14, 10);
	window->add(intro);

	windowCounter = tgui::Label::create();
	windowCounter->setInheritedFont(font);
	windowCounter->setTextSize(16);
	windowCounter->setPosition(14, 56);
	window->add(windowCounter);

	for (std::size_t i = 0; i < data.talents.size(); i++)
	{
		const battle::TalentDef & talent = data.talents[i];
		float y = 90 + i * TALENT_ROW;

		tgui::Button::Ptr talentButton = tgui::Button::create(fromServerText(talent.name));
		talentButton->setInheritedFont(font);
		talentButton->setTextSize(15);
		talentButton->setSize(170, TALENT_ROW - 8);
		talentButton->setPosition(14, y);
		std::string id = talent.id;
		talentButton->connect("pressed", [this, id]() { toggle(id); });
		window->add(talentButton);
		talentButtons.push_back(talentButton);
		talentIds.push_back(talent.id);

		tgui::Label::Ptr description = tgui::Label::create(fromServerText(talent.description));
		description->setInheritedFont(font);
		description->setTextSize(13);
		description->setMaximumTextWidth(WINDOW_WIDTH - 214);
		description->getRenderer()->setTextColor(sf::Color(230, 230, 230));
		description->setPosition(198, y + 4);
		window->add(description);
	}

	tgui::Button::Ptr done = tgui::Button::create(L"Valider");
	done->setInheritedFont(font);
	done->setTextSize(16);
	done->setSize(160, 40);
	done->setPosition((WINDOW_WIDTH - 160) / 2, 90 + data.talents.size() * TALENT_ROW + 6);
	done->connect("pressed", [this]() { window->setVisible(false); });
	window->add(done);

	window->setVisible(false);
	gui->add(window);
	refresh();
}

TalentPicker::~TalentPicker()
{
	gui->remove(window);
}

void TalentPicker::setSlots(int value)
{
	slots = std::max(0, value);
	chosen = battle::validTalentChoice(ClientGameData::get().data(), chosen, slots);
	refresh();
}

void TalentPicker::setChosen(const std::vector<std::string> & ids)
{
	chosen = battle::validTalentChoice(ClientGameData::get().data(), ids, slots);
	refresh();
}

void TalentPicker::setLocked(bool value)
{
	locked = value;
	if (locked)
		window->setVisible(false);
	refresh();
}

void TalentPicker::open()
{
	if (locked || slots <= 0)
		return;
	sf::Vector2f view = gui->getView().getSize();
	window->setPosition((view.x - window->getSize().x) / 2, std::max(10.f, (view.y - window->getSize().y) / 2));
	window->setVisible(true);
	window->moveToFront();
}

void TalentPicker::toggle(const std::string & id)
{
	if (locked)
		return;
	auto it = std::find(chosen.begin(), chosen.end(), id);
	if (it != chosen.end())
		chosen.erase(it);
	else if ((int)chosen.size() < slots)
		chosen.push_back(id);
	refresh();
	if (onChange)
		onChange();
}

void TalentPicker::refresh()
{
	bool complete = isComplete();
	button->setVisible(slots > 0);
	button->setText(L"Talents (" + num((int)chosen.size()) + L"/" + num(slots) + L")" + (locked ? L"" : complete ? L" : modifier" : L" : à choisir"));
	button->getRenderer()->setTextColor(complete ? sf::Color(40, 30, 0) : sf::Color(140, 20, 10));
	button->getRenderer()->setBackgroundColor(complete ? sf::Color(255, 215, 0, 220) : sf::Color(255, 160, 120, 230));
	button->setEnabled(!locked);

	windowCounter->setText(L"Talents choisis : " + num((int)chosen.size()) + L"/" + num(slots));
	windowCounter->getRenderer()->setTextColor(complete ? sf::Color(255, 215, 0) : sf::Color(255, 140, 110));
	for (std::size_t i = 0; i < talentButtons.size(); i++)
	{
		bool picked = std::find(chosen.begin(), chosen.end(), talentIds[i]) != chosen.end();
		talentButtons[i]->getRenderer()->setBackgroundColor(picked ? sf::Color(255, 215, 0) : sf::Color(235, 235, 235));
		talentButtons[i]->getRenderer()->setBackgroundColorHover(picked ? sf::Color(255, 225, 80) : sf::Color(255, 255, 255));
	}
}
