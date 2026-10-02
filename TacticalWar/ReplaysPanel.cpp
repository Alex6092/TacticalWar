#include "ReplaysPanel.h"
#include "LinkToServer.h"

#include <algorithm>

namespace
{
	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}

	sf::String text(const nlohmann::json & object, const char * key)
	{
		return fromServerText(object.value(key, std::string()));
	}

	sf::String reasonLabel(const std::string & reason)
	{
		if (reason == "KO") return L"KO";
		if (reason == "ROUND_LIMIT") return L"aux PV";
		if (reason == "FORFEIT") return L"forfait";
		if (reason == "ADMIN") return L"arbitrage";
		return fromServerText(reason);
	}
}

ReplaysPanel::ReplaysPanel(tgui::Gui * gui, const sf::Font & font)
{
	group = tgui::Group::create();

	header = tgui::Label::create(L"Rediffusions");
	header->setInheritedFont(font);
	header->setTextSize(20);
	header->getRenderer()->setTextColor(sf::Color(255, 215, 0));
	group->add(header);

	list = tgui::ListView::create();
	list->setInheritedFont(font);
	list->setTextSize(16);
	list->setItemHeight(30);
	list->setHeaderHeight(30);
	list->addColumn(L"Date", 180);
	list->addColumn(L"Match", 300);
	list->addColumn(L"Équipe A", 220);
	list->addColumn(L"Équipe B", 220);
	list->addColumn(L"Vainqueur", 260);
	list->addColumn(L"Tours", 80);
	list->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 220));
	list->connect("DoubleClicked", [this]() { watchSelected(); });
	group->add(list);

	watchButton = tgui::Button::create(L"Revoir");
	watchButton->setInheritedFont(font);
	watchButton->setTextSize(18);
	watchButton->connect("pressed", [this]() { watchSelected(); });
	group->add(watchButton);

	refreshButton = tgui::Button::create(L"Actualiser");
	refreshButton->setInheritedFont(font);
	refreshButton->setTextSize(18);
	refreshButton->connect("pressed", []() { LinkToServer::getInstance()->SendRaw("RL{}"); });
	group->add(refreshButton);

	status = tgui::Label::create();
	status->setInheritedFont(font);
	status->setTextSize(16);
	status->getRenderer()->setTextColor(sf::Color(255, 200, 120));
	group->add(status);

	gui->add(group);
}

void ReplaysPanel::setVisible(bool visible)
{
	group->setVisible(visible);
}

void ReplaysPanel::layout(const sf::Vector2u & windowSize, float top)
{
	float width = std::min(1300.f, (float)windowSize.x - 40.f);
	float left = ((float)windowSize.x - width) / 2.f;
	float height = std::max(200.f, (float)windowSize.y - top - 30.f);

	group->setPosition(left, top);
	group->setSize(width, height);
	header->setPosition(0, 0);
	list->setPosition(0, 34);
	list->setSize(width, height - 34 - 60);
	watchButton->setPosition(0, height - 48);
	watchButton->setSize(200, 44);
	refreshButton->setPosition(214, height - 48);
	refreshButton->setSize(160, 44);
	status->setPosition(390, height - 38);
}

void ReplaysPanel::setStatus(const sf::String & value, const sf::Color & color)
{
	status->setText(value);
	status->getRenderer()->setTextColor(color);
}

void ReplaysPanel::onReplayList(const nlohmann::json & body)
{
	list->removeAllItems();
	rowIds.clear();

	const nlohmann::json & replays = body.value("replays", nlohmann::json::array());
	for (const nlohmann::json & replay : replays)
	{
		const nlohmann::json & teams = replay.value("teams", nlohmann::json::array());
		sf::String teamA = teams.size() > 0 ? fromServerText(teams[0].get<std::string>()) : sf::String();
		sf::String teamB = teams.size() > 1 ? fromServerText(teams[1].get<std::string>()) : sf::String();

		sf::String winner = L"(interrompu)";
		if (replay.value("complete", false))
		{
			int team = replay.value("winner", 0);
			winner = (team == 1 ? teamA : team == 2 ? teamB : sf::String(L"?")) + L" (" + reasonLabel(replay.value("reason", std::string())) + L")";
		}

		list->addItem({
			text(replay, "date"),
			text(replay, "title"),
			teamA,
			teamB,
			winner,
			replay.value("complete", false) ? num(replay.value("rounds", 0)) : sf::String()
		});
		rowIds.push_back(replay.value("id", std::string()));
	}

	header->setText(replays.empty() ? sf::String(L"Aucune rediffusion pour l'instant") : L"Rediffusions (" + num((int)replays.size()) + L")");
}

void ReplaysPanel::watchSelected()
{
	int index = list->getSelectedItemIndex();
	if (index < 0 || index >= (int)rowIds.size())
	{
		setStatus(L"Sélectionnez un combat.");
		return;
	}

	setStatus(L"Chargement de la rediffusion...", sf::Color(200, 220, 255));
	if (onWatch)
		onWatch(rowIds[index]);
}
