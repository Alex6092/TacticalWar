#include "LiveSessionsPanel.h"
#include "LinkToServer.h"

#include <algorithm>
#include <cmath>

namespace
{
	const unsigned int TEXT_SIZE = 16;

	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}

	sf::String text(const nlohmann::json & object, const char * key)
	{
		return fromServerText(object.value(key, std::string()));
	}

	sf::String phaseLabel(const std::string & phase)
	{
		if (phase == "BAN") return L"Bannissement";
		if (phase == "CLASS_SELECTION") return L"Choix des classes";
		if (phase == "PLACEMENT") return L"Placement";
		if (phase == "FIGHT") return L"Combat";
		if (phase == "ENDED") return L"Terminé";
		return fromServerText(phase);
	}

	bool watchable(const nlohmann::json & session)
	{
		std::string phase = session.value("phase", std::string());
		return phase == "PLACEMENT" || phase == "FIGHT";
	}
}

LiveSessionsPanel::LiveSessionsPanel(tgui::Gui * gui, const sf::Font & font)
	: gui(gui), font(font), sessions(nlohmann::json::array())
{
	group = tgui::Group::create();

	header = tgui::Label::create(L"Combats en cours");
	header->setInheritedFont(font);
	header->setTextSize(20);
	header->getRenderer()->setTextColor(sf::Color(255, 215, 0));
	group->add(header);

	list = tgui::ListView::create();
	list->setInheritedFont(font);
	list->setTextSize(TEXT_SIZE);
	list->setItemHeight(30);
	list->setHeaderHeight(30);
	list->addColumn(L"Match", 260);
	list->addColumn(L"Équipe A", 220);
	list->addColumn(L"Équipe B", 220);
	list->addColumn(L"Phase", 170);
	list->addColumn(L"Tour", 60);
	list->addColumn(L"PV A", 80);
	list->addColumn(L"PV B", 80);
	list->addColumn(L"Spectateurs", 150);
	list->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 220));
	list->connect("DoubleClicked", [this]() { watchSelected(); });
	group->add(list);

	watchButton = tgui::Button::create(L"Regarder");
	watchButton->setInheritedFont(font);
	watchButton->setTextSize(18);
	watchButton->connect("pressed", [this]() { watchSelected(); });
	group->add(watchButton);

	refreshButton = tgui::Button::create(L"Actualiser");
	refreshButton->setInheritedFont(font);
	refreshButton->setTextSize(18);
	refreshButton->connect("pressed", []() { LinkToServer::getInstance()->SendRaw("SL{}"); });
	group->add(refreshButton);

	shrinkButton = tgui::Button::create(L"Rétrécir la carte");
	shrinkButton->setInheritedFont(font);
	shrinkButton->setTextSize(16);
	shrinkButton->setVisible(false);
	shrinkButton->connect("pressed", [this]() {
		int index = list->getSelectedItemIndex();
		if (index < 0 || index >= (int)rowIds.size() || sessions[index].value("phase", std::string()) != "FIGHT")
		{
			setStatus(L"Sélectionnez un combat en cours (après le placement).");
			return;
		}
		if (onShrink)
			onShrink(rowIds[index]);
	});
	group->add(shrinkButton);

	status = tgui::Label::create();
	status->setInheritedFont(font);
	status->setTextSize(16);
	status->getRenderer()->setTextColor(sf::Color(255, 200, 120));
	group->add(status);

	gui->add(group);
	onSessionList({ { "sessions", nlohmann::json::array() } });
}

void LiveSessionsPanel::setVisible(bool visible)
{
	group->setVisible(visible);
}

void LiveSessionsPanel::layout(const sf::Vector2u & windowSize, float top)
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
	shrinkButton->setVisible((bool)onShrink);
	shrinkButton->setPosition(388, height - 48);
	shrinkButton->setSize(220, 44);
	status->setPosition(onShrink ? 622.f : 390.f, height - 38);
}

void LiveSessionsPanel::setStatus(const sf::String & value, const sf::Color & color)
{
	status->setText(value);
	status->getRenderer()->setTextColor(color);
}

void LiveSessionsPanel::onSessionList(const nlohmann::json & body)
{
	int selected = -1;
	int index = list->getSelectedItemIndex();
	if (index >= 0 && index < (int)rowIds.size())
		selected = rowIds[index];

	sessions = body.value("sessions", nlohmann::json::array());
	list->removeAllItems();
	rowIds.clear();

	for (const nlohmann::json & session : sessions)
	{
		const nlohmann::json & teams = session.value("teams", nlohmann::json::array());
		bool fighting = watchable(session);
		list->addItem({
			text(session, "name"),
			teams.size() > 0 ? fromServerText(teams[0].get<std::string>()) : sf::String(),
			teams.size() > 1 ? fromServerText(teams[1].get<std::string>()) : sf::String(),
			phaseLabel(session.value("phase", std::string())),
			fighting ? num(session.value("round", 0)) : sf::String(),
			fighting && session.contains("hp1") ? num((int)std::round(session.value("hp1", 0.0))) + L" %" : sf::String(),
			fighting && session.contains("hp2") ? num((int)std::round(session.value("hp2", 0.0))) + L" %" : sf::String(),
			num(session.value("spectators", 0))
		});
		rowIds.push_back(session.value("session", 0));
	}

	for (std::size_t i = 0; i < rowIds.size(); i++)
	{
		if (rowIds[i] == selected)
			list->setSelectedItem(i);
	}

	header->setText(sessions.empty() ? sf::String(L"Aucun combat en cours") : L"Combats en cours (" + num((int)sessions.size()) + L")");
}

void LiveSessionsPanel::watchSelected()
{
	int index = list->getSelectedItemIndex();
	if (index < 0 || index >= (int)rowIds.size())
	{
		setStatus(L"Sélectionnez un combat.");
		return;
	}

	if (!watchable(sessions[index]))
	{
		setStatus(L"Ce combat n'a pas encore commencé (choix des classes).");
		return;
	}

	setStatus(L"Connexion au combat...", sf::Color(200, 220, 255));
	if (onWatch)
		onWatch(rowIds[index]);
}

int LiveSessionsPanel::mostContestedSession() const
{
	// Combat le plus serré : écart de PV le plus faible, puis le plus avancé. Le placement passe
	// après les combats commencés.
	int best = 0;
	double bestScore = 0;
	for (const nlohmann::json & session : sessions)
	{
		if (!watchable(session))
			continue;

		double gap = std::abs(session.value("hp1", 100.0) - session.value("hp2", 100.0));
		double score = gap - session.value("round", 0) * 0.5;
		if (session.value("phase", std::string()) != "FIGHT")
			score += 1000;

		if (best == 0 || score < bestScore)
		{
			best = session.value("session", 0);
			bestScore = score;
		}
	}
	return best;
}
