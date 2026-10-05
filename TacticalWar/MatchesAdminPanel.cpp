#include "MatchesAdminPanel.h"
#include "LinkToServer.h"

#include <algorithm>

namespace
{
	const unsigned int TEXT_SIZE = 16;
	const float FORM_WIDTH = 360;
	// Colonnes de la liste (largeurs à 1060 pixels, ajustées à la place disponible).
	const float COLUMNS[6] = { 240, 170, 170, 150, 160, 170 };

	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}

	sf::String statusLabel(const std::string & status)
	{
		if (status == "planned") return L"Choix des classes";
		if (status == "playing") return L"En cours";
		if (status == "finished") return L"Terminé";
		if (status == "cancelled") return L"Annulé";
		return fromServerText(status);
	}

	tgui::Label::Ptr label(const sf::Font & font, const sf::String & text, unsigned int size, const sf::Color & color)
	{
		tgui::Label::Ptr result = tgui::Label::create(text);
		result->setInheritedFont(font);
		result->setTextSize(size);
		result->getRenderer()->setTextColor(color);
		return result;
	}

	tgui::ComboBox::Ptr combo(const sf::Font & font)
	{
		tgui::ComboBox::Ptr result = tgui::ComboBox::create();
		result->setInheritedFont(font);
		result->setTextSize(TEXT_SIZE);
		result->setItemsToDisplay(10);
		return result;
	}

	tgui::Button::Ptr button(const sf::Font & font, const sf::String & text)
	{
		tgui::Button::Ptr result = tgui::Button::create(text);
		result->setInheritedFont(font);
		result->setTextSize(18);
		return result;
	}
}

MatchesAdminPanel::MatchesAdminPanel(tgui::Gui * gui, const sf::Font & font)
	: font(font), matches(nlohmann::json::array())
{
	group = tgui::Group::create();
	const sf::Color gold(255, 215, 0);
	const sf::Color white(235, 235, 235);

	formTitle = label(font, L"Nouveau match amical", 20, gold);
	group->add(formTitle);
	nameLabel = label(font, L"Nom (facultatif)", TEXT_SIZE, white);
	group->add(nameLabel);
	nameEdit = tgui::EditBox::create();
	nameEdit->setInheritedFont(font);
	nameEdit->setTextSize(TEXT_SIZE);
	nameEdit->setMaximumCharacters(60);
	nameEdit->setDefaultText(L"Équipe A - Équipe B");
	group->add(nameEdit);
	teamALabel = label(font, L"Équipe A", TEXT_SIZE, white);
	group->add(teamALabel);
	teamACombo = combo(font);
	group->add(teamACombo);
	teamBLabel = label(font, L"Équipe B", TEXT_SIZE, white);
	group->add(teamBLabel);
	teamBCombo = combo(font);
	group->add(teamBCombo);
	mapLabel = label(font, L"Carte", TEXT_SIZE, white);
	group->add(mapLabel);
	mapCombo = combo(font);
	mapCombo->addItem(L"Au hasard", "0");
	mapCombo->setSelectedItemById("0");
	group->add(mapCombo);
	createButton = button(font, L"Créer le match");
	createButton->getRenderer()->setBackgroundColor(sf::Color(90, 182, 96, 220));
	createButton->connect("pressed", [this]() { create(); });
	group->add(createButton);

	listTitle = label(font, L"Matchs amicaux", 20, gold);
	group->add(listTitle);
	list = tgui::ListView::create();
	list->setInheritedFont(font);
	list->setTextSize(TEXT_SIZE);
	list->setItemHeight(30);
	list->setHeaderHeight(30);
	const sf::String headers[6] = { L"Match", L"Équipe A", L"Équipe B", L"Carte", L"État", L"Vainqueur" };
	for (int i = 0; i < 6; i++)
		list->addColumn(headers[i], COLUMNS[i]);
	list->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 220));
	list->connect("DoubleClicked", [this]() { watchSelected(); });
	group->add(list);

	watchButton = button(font, L"Regarder");
	watchButton->connect("pressed", [this]() { watchSelected(); });
	group->add(watchButton);
	cancelButton = button(font, L"Annuler le match");
	cancelButton->connect("pressed", [this]() { cancelSelected(); });
	group->add(cancelButton);
	refreshButton = button(font, L"Actualiser");
	refreshButton->connect("pressed", []() {
		LinkToServer::getInstance()->SendRaw("FL{}");
		LinkToServer::getInstance()->SendRaw("TL");
	});
	group->add(refreshButton);
	status = label(font, sf::String(), TEXT_SIZE, sf::Color(255, 200, 120));
	group->add(status);

	gui->add(group);
	onFriendlyList({ { "matches", nlohmann::json::array() } });
}

void MatchesAdminPanel::setVisible(bool visible)
{
	group->setVisible(visible);
}

void MatchesAdminPanel::layout(const sf::Vector2u & windowSize, float top)
{
	float width = std::min(1500.f, (float)windowSize.x - 40.f);
	float left = ((float)windowSize.x - width) / 2.f;
	float height = std::max(260.f, (float)windowSize.y - top - 30.f);
	group->setPosition(left, top);
	group->setSize(width, height);

	// Formulaire à gauche, liste à droite (le formulaire passe au-dessus dans une fenêtre étroite).
	bool stacked = width < 1100;
	float y = 0;
	formTitle->setPosition(0, y);
	y += 34;
	float field = stacked ? std::min(260.f, (width - 40) / 4) : FORM_WIDTH;
	if (stacked)
	{
		// Une ligne : nom, équipe A, équipe B, carte, bouton.
		float x = 0;
		for (auto pair : { std::make_pair(tgui::Widget::Ptr(nameLabel), tgui::Widget::Ptr(nameEdit)),
			std::make_pair(tgui::Widget::Ptr(teamALabel), tgui::Widget::Ptr(teamACombo)),
			std::make_pair(tgui::Widget::Ptr(teamBLabel), tgui::Widget::Ptr(teamBCombo)),
			std::make_pair(tgui::Widget::Ptr(mapLabel), tgui::Widget::Ptr(mapCombo)) })
		{
			pair.first->setPosition(x, y);
			pair.second->setPosition(x, y + 24);
			pair.second->setSize(field - 10, 32);
			x += field;
		}
		createButton->setPosition(0, y + 66);
		createButton->setSize(220, 40);
		y += 120;
	}
	else
	{
		for (auto pair : { std::make_pair(tgui::Widget::Ptr(nameLabel), tgui::Widget::Ptr(nameEdit)),
			std::make_pair(tgui::Widget::Ptr(teamALabel), tgui::Widget::Ptr(teamACombo)),
			std::make_pair(tgui::Widget::Ptr(teamBLabel), tgui::Widget::Ptr(teamBCombo)),
			std::make_pair(tgui::Widget::Ptr(mapLabel), tgui::Widget::Ptr(mapCombo)) })
		{
			pair.first->setPosition(0, y);
			pair.second->setPosition(0, y + 24);
			pair.second->setSize(field, 32);
			y += 70;
		}
		createButton->setPosition(0, y + 6);
		createButton->setSize(field, 44);
		y = 0;
	}

	float listX = stacked ? 0 : FORM_WIDTH + 30;
	float listWidth = width - listX;
	listTitle->setPosition(listX, y);
	list->setPosition(listX, y + 34);
	list->setSize(listWidth, std::max(120.f, height - y - 34 - 60));
	float total = 0;
	for (float column : COLUMNS)
		total += column;
	float factor = std::min(1.5f, (listWidth - 24) / total);
	for (int i = 0; i < 6; i++)
		list->setColumnWidth(i, COLUMNS[i] * factor);
	float buttonsY = height - 48;
	watchButton->setPosition(listX, buttonsY);
	watchButton->setSize(170, 44);
	cancelButton->setPosition(listX + 180, buttonsY);
	cancelButton->setSize(250, 44);
	refreshButton->setPosition(listX + 440, buttonsY);
	refreshButton->setSize(170, 44);
	status->setPosition(listX + 625, buttonsY + 10);
	status->setMaximumTextWidth(std::max(150.f, listWidth - 630));
}

void MatchesAdminPanel::setTeams(const nlohmann::json & teams)
{
	for (tgui::ComboBox::Ptr box : { teamACombo, teamBCombo })
	{
		sf::String selected = box->getSelectedItemId();
		box->removeAllItems();
		for (const nlohmann::json & team : teams)
		{
			if (!team.value("active", true))
				continue;
			sf::String players;
			for (const nlohmann::json & player : team.value("players", nlohmann::json::array()))
			{
				std::string login = player.value("login", std::string());
				if (!login.empty())
					players += (players.isEmpty() ? sf::String() : sf::String(L", ")) + fromServerText(login);
			}
			box->addItem(fromServerText(team.value("name", std::string())) + L" (" + players + L")", std::to_string(team.value("id", 0)));
		}
		box->setSelectedItemById(selected);
	}
}

void MatchesAdminPanel::onFriendlyList(const nlohmann::json & body)
{
	int selected = -1;
	int index = list->getSelectedItemIndex();
	if (index >= 0 && index < (int)rowIds.size())
		selected = rowIds[index];

	matches = body.value("matches", nlohmann::json::array());
	list->removeAllItems();
	rowIds.clear();
	for (const nlohmann::json & match : matches)
	{
		std::string state = match.value("status", std::string());
		int winner = match.value("winner", 0);
		const nlohmann::json & teamA = match.value("teamA", nlohmann::json::object());
		const nlohmann::json & teamB = match.value("teamB", nlohmann::json::object());
		sf::String winnerName = winner == 0 ? sf::String() : fromServerText((winner == teamA.value("id", 0) ? teamA : teamB).value("name", std::string()));
		list->addItem({
			fromServerText(match.value("name", std::string())),
			fromServerText(teamA.value("name", std::string())),
			fromServerText(teamB.value("name", std::string())),
			fromServerText(match.value("map", nlohmann::json::object()).value("name", std::string())),
			statusLabel(state),
			winnerName
		});
		rowIds.push_back(match.value("id", 0));
	}
	for (std::size_t i = 0; i < rowIds.size(); i++)
	{
		if (rowIds[i] == selected)
			list->setSelectedItem(i);
	}
	listTitle->setText(matches.empty() ? sf::String(L"Aucun match amical") : L"Matchs amicaux (" + num((int)matches.size()) + L")");

	if (body.contains("maps"))
	{
		sf::String chosen = mapCombo->getSelectedItemId();
		mapCombo->removeAllItems();
		mapCombo->addItem(L"Au hasard", "0");
		for (const nlohmann::json & map : body["maps"])
			mapCombo->addItem(num(map.value("id", 0)) + L" - " + fromServerText(map.value("name", std::string())), std::to_string(map.value("id", 0)));
		if (!mapCombo->setSelectedItemById(chosen))
			mapCombo->setSelectedItemById("0");
	}
}

void MatchesAdminPanel::onResult(const nlohmann::json & body)
{
	bool ok = body.value("ok", false);
	setStatus(fromServerText(body.value("message", std::string())), ok ? sf::Color(140, 255, 140) : sf::Color(255, 120, 100));
	if (ok)
		nameEdit->setText("");
}

void MatchesAdminPanel::setStatus(const sf::String & text, const sf::Color & color)
{
	status->setText(text);
	status->getRenderer()->setTextColor(color);
}

const nlohmann::json * MatchesAdminPanel::selectedMatch() const
{
	int index = list->getSelectedItemIndex();
	if (index < 0 || index >= (int)matches.size())
		return nullptr;
	return &matches[index];
}

void MatchesAdminPanel::create()
{
	std::string teamA = teamACombo->getSelectedItemId().toAnsiString();
	std::string teamB = teamBCombo->getSelectedItemId().toAnsiString();
	if (teamA.empty() || teamB.empty())
	{
		setStatus(L"Choisissez les deux équipes.", sf::Color(255, 120, 100));
		return;
	}
	if (teamA == teamB)
	{
		setStatus(L"Choisissez deux équipes différentes.", sf::Color(255, 120, 100));
		return;
	}
	std::basic_string<sf::Uint8> name = nameEdit->getText().toUtf8();
	nlohmann::json request = {
		{ "name", std::string(name.begin(), name.end()) },
		{ "teamA", std::atoi(teamA.c_str()) },
		{ "teamB", std::atoi(teamB.c_str()) },
		{ "map", std::atoi(mapCombo->getSelectedItemId().toAnsiString().c_str()) }
	};
	setStatus(L"Création...", sf::Color(200, 220, 255));
	LinkToServer::getInstance()->SendRaw("FC" + request.dump());
}

void MatchesAdminPanel::cancelSelected()
{
	const nlohmann::json * match = selectedMatch();
	if (match == nullptr)
	{
		setStatus(L"Sélectionnez un match.", sf::Color(255, 200, 120));
		return;
	}
	std::string state = match->value("status", std::string());
	if (state == "finished" || state == "cancelled")
	{
		setStatus(L"Ce match est déjà terminé.", sf::Color(255, 200, 120));
		return;
	}
	LinkToServer::getInstance()->SendRaw("FX" + nlohmann::json({ { "id", match->value("id", 0) } }).dump());
}

void MatchesAdminPanel::watchSelected()
{
	const nlohmann::json * match = selectedMatch();
	if (match == nullptr)
	{
		setStatus(L"Sélectionnez un match.", sf::Color(255, 200, 120));
		return;
	}
	if (match->value("status", std::string()) != "playing" || match->value("session", 0) == 0)
	{
		setStatus(L"Ce match n'est pas en cours (choix des classes, ou terminé).", sf::Color(255, 200, 120));
		return;
	}
	setStatus(L"Connexion au combat...", sf::Color(200, 220, 255));
	if (onWatch)
		onWatch(match->value("session", 0));
}
