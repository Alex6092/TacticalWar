#include "TournamentAdminPanel.h"
#include "ClientConfig.h"
#include "LinkToServer.h"

#include <algorithm>
#include <map>
#include <Message.h>
#include <Windows.h>
#include <shellapi.h>

// Windows.h définit MessageBox comme une macro, en conflit avec tgui::MessageBox.
#undef MessageBox

namespace
{
	const unsigned int TEXT_SIZE = 14;

	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}

	sf::String text(const nlohmann::json & json, const char * key)
	{
		return fromServerText(json.value(key, std::string()));
	}

	std::string toUtf8(const sf::String & value)
	{
		std::basic_string<sf::Uint8> utf8 = value.toUtf8();
		return std::string(utf8.begin(), utf8.end());
	}

	sf::String statusLabel(const std::string & status)
	{
		if (status == "DRAFT") return L"Préparation";
		if (status == "RUNNING") return L"En cours";
		if (status == "FINISHED") return L"Terminé";
		if (status == "PENDING") return L"En attente";
		if (status == "READY") return L"Prêt";
		if (status == "IN_PROGRESS") return L"En cours";
		if (status == "DONE") return L"Terminé";
		return fromServerText(status);
	}

	sf::String formatLabel(const std::string & format)
	{
		if (format == "POOLS_THEN_BRACKET") return L"Poules + phase finale";
		if (format == "DOUBLE_ELIMINATION") return L"Double élimination";
		if (format == "SWISS") return L"Suisse";
		return fromServerText(format);
	}

	sf::String reasonLabel(const std::string & reason)
	{
		if (reason == "KO") return L"KO";
		if (reason == "ROUND_LIMIT") return L"décision PV";
		if (reason == "FORFEIT") return L"forfait";
		if (reason == "ADMIN") return L"arbitrage";
		if (reason == "BYE") return L"exempt";
		return fromServerText(reason);
	}
}

TournamentAdminPanel::TournamentAdminPanel(tgui::Gui * gui, const sf::Font & font)
	: gui(gui), font(font), tournaments(nlohmann::json::array()), teams(nlohmann::json::array()), selectedId(0)
{
	group = tgui::Group::create({ "100%", "100%" });

	tournamentList = tgui::ListView::create();
	tournamentList->setInheritedFont(font);
	tournamentList->setTextSize(TEXT_SIZE);
	tournamentList->addColumn("Tournoi", 150);
	tournamentList->addColumn("Format", 120);
	tournamentList->addColumn(L"Statut", 90);
	tournamentList->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 220));
	tournamentList->connect("ItemSelected", [this](int index) {
		if (index >= 0 && index < (int)tournaments.size())
			selectTournament(tournaments[index].value("id", 0));
	});
	group->add(tournamentList);

	newButton = createButton(L"Nouveau tournoi");
	newButton->connect("pressed", [this]() { newTournament(); });
	group->add(newButton);

	// Formulaire de création / modification.
	form = tgui::Panel::create();
	form->getRenderer()->setBackgroundColor(sf::Color(30, 30, 30, 200));
	form->getRenderer()->setBorders(1);
	form->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	group->add(form);

	form->add(createLabel(L"Nom du tournoi"), "nameLabel");
	name = tgui::EditBox::create();
	name->setInheritedFont(font);
	name->setTextSize(TEXT_SIZE);
	form->add(name);

	form->add(createLabel(L"Format"), "formatLabel");
	format = tgui::ComboBox::create();
	format->setInheritedFont(font);
	format->setTextSize(TEXT_SIZE);
	format->addItem(L"Poules + phase finale", "POOLS_THEN_BRACKET");
	format->addItem(L"Double élimination", "DOUBLE_ELIMINATION");
	format->addItem(L"Suisse", "SWISS");
	format->setSelectedItemById("POOLS_THEN_BRACKET");
	format->connect("ItemSelected", [this]() { refreshFormatOptions(); });
	form->add(format);

	poolCountLabel = createLabel(L"Nombre de poules");
	poolCount = createNumberBox("2");
	qualifiersLabel = createLabel(L"Qualifiés par poule");
	qualifiers = createNumberBox("2");
	thirdPlace = tgui::CheckBox::create(L"Petite finale");
	thirdPlace->setInheritedFont(font);
	thirdPlace->setTextSize(TEXT_SIZE);
	thirdPlace->getRenderer()->setTextColor(sf::Color::White);
	thirdPlace->setChecked(true);
	grandFinalReset = tgui::CheckBox::create(L"Revanche en grande finale");
	grandFinalReset->setInheritedFont(font);
	grandFinalReset->setTextSize(TEXT_SIZE);
	grandFinalReset->getRenderer()->setTextColor(sf::Color::White);
	grandFinalReset->setChecked(true);
	swissRoundsLabel = createLabel(L"Rondes (0 = auto)");
	swissRounds = createNumberBox("0");
	topCutLabel = createLabel(L"Phase finale (0 = non)");
	topCut = createNumberBox("4");

	for (const tgui::Widget::Ptr & widget : std::vector<tgui::Widget::Ptr>{ poolCountLabel, poolCount, qualifiersLabel, qualifiers,
		thirdPlace, grandFinalReset, swissRoundsLabel, swissRounds, topCutLabel, topCut })
		form->add(widget);

	form->add(createLabel(L"Équipes inscrites (sélection multiple, ordre = têtes de série)"), "teamsLabel");
	teamList = tgui::ListView::create();
	teamList->setInheritedFont(font);
	teamList->setTextSize(TEXT_SIZE);
	teamList->setMultiSelect(true);
	teamList->addColumn(L"Équipe", 220);
	teamList->addColumn(L"Tête de série", 110);
	teamList->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 220));
	form->add(teamList);

	saveButton = createButton(L"Enregistrer");
	saveButton->getRenderer()->setBackgroundColor(sf::Color(90, 182, 96, 220));
	saveButton->connect("pressed", [this]() { save(); });
	startButton = createButton(L"Démarrer");
	startButton->connect("pressed", [this]() {
		if (selectedId != 0)
			send("UB", { { "id", selectedId } });
	});
	deleteButton = createButton(L"Supprimer");
	deleteButton->getRenderer()->setBackgroundColor(sf::Color(226, 82, 32, 220));
	deleteButton->connect("pressed", [this]() {
		if (selectedId == 0)
			return;
		int id = selectedId;
		tgui::MessageBox::Ptr box = tgui::MessageBox::create(L"Supprimer", L"Supprimer définitivement ce tournoi ?");
		box->setInheritedFont(this->font);
		box->addButton(L"Supprimer");
		box->addButton(L"Annuler");
		box->setPosition("(&.size - size) / 2");
		tgui::Gui * gui = this->gui;
		box->connect("ButtonPressed", [this, gui, box, id](const sf::String & button) {
			if (button == L"Supprimer")
				send("UD", { { "id", id } });
			gui->remove(box);
		});
		gui->add(box);
	});
	form->add(saveButton);
	form->add(startButton);
	form->add(deleteButton);

	// Suivi du tournoi.
	header = createLabel("", 18);
	header->getRenderer()->setTextColor(sf::Color::Yellow);
	group->add(header);

	matchList = tgui::ListView::create();
	matchList->setInheritedFont(font);
	matchList->setTextSize(TEXT_SIZE);
	matchList->addColumn("#", 40);
	matchList->addColumn("Phase", 220);
	matchList->addColumn(L"Équipe A", 170);
	matchList->addColumn(L"Équipe B", 170);
	matchList->addColumn(L"Statut", 100);
	matchList->addColumn(L"Résultat", 230);
	matchList->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 220));
	group->add(matchList);

	pauseButton = createButton(L"Suspendre");
	pauseButton->connect("pressed", [this]() {
		if (selectedId != 0 && state.is_object())
			send("UP", { { "id", selectedId }, { "paused", !state.value("paused", false) } });
	});
	winAButton = createButton(L"Victoire A");
	winAButton->connect("pressed", [this]() { forceWinner(true); });
	winBButton = createButton(L"Victoire B");
	winBButton->connect("pressed", [this]() { forceWinner(false); });
	stopButton = createButton(L"Arrêter (PV)");
	stopButton->connect("pressed", [this]() {
		if (selectedMatchId() != 0)
			send("US", { { "id", selectedId }, { "match", selectedMatchId() } });
	});
	replayButton = createButton(L"Rejouer");
	replayButton->connect("pressed", [this]() {
		if (selectedMatchId() != 0)
			send("UX", { { "id", selectedId }, { "match", selectedMatchId() } });
	});
	webButton = createButton(L"Vue projetée");
	webButton->connect("pressed", []() {
		std::string url = "http://" + ClientConfig::get().serverHost + ":8080/";
		ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
	});
	watchButton = createButton(L"Regarder");
	watchButton->connect("pressed", [this]() {
		int matchId = selectedMatchId();
		for (const nlohmann::json & session : liveSessions)
		{
			if (matchId != 0 && session.value("tournament", 0) == selectedId && session.value("match", 0) == matchId)
			{
				std::string phase = session.value("phase", std::string());
				if (phase == "PLACEMENT" || phase == "FIGHT")
					send("SW", { { "session", session.value("session", 0) } });
				else
					status->setText(L"Les joueurs choisissent encore leurs classes.");
				return;
			}
		}
		status->setText(L"Sélectionnez un match en cours.");
	});
	for (const tgui::Button::Ptr & button : { pauseButton, winAButton, winBButton, stopButton, replayButton, watchButton, webButton })
		group->add(button);

	standings = createLabel("", 13);
	standings->getRenderer()->setBackgroundColor(sf::Color(20, 20, 30, 200));
	standings->getRenderer()->setPadding(8);
	group->add(standings);

	status = createLabel("", 15);
	group->add(status);

	gui->add(group);
	newTournament();
	send("UL", nlohmann::json::object());
}

tgui::Label::Ptr TournamentAdminPanel::createLabel(const sf::String & value, unsigned int size)
{
	tgui::Label::Ptr label = tgui::Label::create(value);
	label->setInheritedFont(font);
	label->setTextSize(size);
	label->getRenderer()->setTextColor(sf::Color::White);
	return label;
}

tgui::EditBox::Ptr TournamentAdminPanel::createNumberBox(const sf::String & value)
{
	tgui::EditBox::Ptr box = tgui::EditBox::create();
	box->setInheritedFont(font);
	box->setTextSize(TEXT_SIZE);
	box->setInputValidator("[0-9]*");
	box->setText(value);
	return box;
}

tgui::Button::Ptr TournamentAdminPanel::createButton(const sf::String & value)
{
	tgui::Button::Ptr button = tgui::Button::create(value);
	button->setInheritedFont(font);
	button->setTextSize(TEXT_SIZE);
	button->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 200));
	return button;
}

void TournamentAdminPanel::setVisible(bool visible)
{
	group->setVisible(visible);
}

void TournamentAdminPanel::layout(const sf::Vector2u & windowSize, float top)
{
	const float margin = 30;
	float width = (float)windowSize.x;
	float height = (float)windowSize.y;

	tournamentList->setPosition(margin, top);
	tournamentList->setSize(380, 200);
	newButton->setPosition(margin, top + 206);
	newButton->setSize(380, 32);

	float formTop = top + 250;
	form->setPosition(margin, formTop);
	form->setSize(380, height - formTop - margin);

	float x = 10;
	float w = 360;
	float half = (w - 10) / 2;
	float y = 8;
	form->get<tgui::Label>("nameLabel")->setPosition(x, y);
	name->setPosition(x, y + 18);
	name->setSize(w, 26);
	y += 50;
	form->get<tgui::Label>("formatLabel")->setPosition(x, y);
	format->setPosition(x, y + 18);
	format->setSize(w, 26);
	y += 52;

	poolCountLabel->setPosition(x, y);
	poolCount->setPosition(x, y + 18);
	poolCount->setSize(half, 26);
	qualifiersLabel->setPosition(x + half + 10, y);
	qualifiers->setPosition(x + half + 10, y + 18);
	qualifiers->setSize(half, 26);
	swissRoundsLabel->setPosition(x, y);
	swissRounds->setPosition(x, y + 18);
	swissRounds->setSize(half, 26);
	topCutLabel->setPosition(x + half + 10, y);
	topCut->setPosition(x + half + 10, y + 18);
	topCut->setSize(half, 26);
	y += 52;
	thirdPlace->setPosition(x, y);
	thirdPlace->setSize(18, 18);
	grandFinalReset->setPosition(x, y);
	grandFinalReset->setSize(18, 18);
	y += 30;

	form->get<tgui::Label>("teamsLabel")->setPosition(x, y);
	teamList->setPosition(x, y + 20);
	float buttonsTop = form->getSize().y - 46;
	teamList->setSize(w, std::max(80.f, buttonsTop - y - 30));
	float third = (w - 20) / 3;
	saveButton->setPosition(x, buttonsTop);
	saveButton->setSize(third, 34);
	startButton->setPosition(x + third + 10, buttonsTop);
	startButton->setSize(third, 34);
	deleteButton->setPosition(x + 2 * (third + 10), buttonsTop);
	deleteButton->setSize(third, 34);

	float right = margin + 380 + margin;
	float rightWidth = width - right - margin;
	header->setPosition(right, top);
	float buttonWidth = (rightWidth - 60) / 7;
	tgui::Button::Ptr buttons[] = { pauseButton, winAButton, winBButton, stopButton, replayButton, watchButton, webButton };
	for (int i = 0; i < 7; i++)
	{
		buttons[i]->setPosition(right + i * (buttonWidth + 10), top + 32);
		buttons[i]->setSize(buttonWidth, 32);
	}

	float listTop = top + 74;
	float standingsHeight = 210;
	matchList->setPosition(right, listTop);
	matchList->setSize(rightWidth, height - listTop - standingsHeight - margin - 40);
	standings->setPosition(right, height - margin - standingsHeight - 30);
	standings->setSize(rightWidth, standingsHeight);
	status->setPosition(right, height - margin - 24);
}

void TournamentAdminPanel::send(const std::string & op, const nlohmann::json & body)
{
	LinkToServer::getInstance()->SendRaw(op + tw::protocol::dumpJson(body));
}

void TournamentAdminPanel::setTeams(const nlohmann::json & value)
{
	teams = value;
	refreshForm();
}

void TournamentAdminPanel::onTournamentList(const nlohmann::json & body)
{
	tournaments = body.value("tournaments", nlohmann::json::array());
	tournamentList->removeAllItems();

	int selectedIndex = -1;
	for (std::size_t i = 0; i < tournaments.size(); i++)
	{
		const nlohmann::json & tournament = tournaments[i];
		sf::String statusText = statusLabel(tournament.value("status", std::string()));
		if (tournament.value("paused", false))
			statusText += L" (pause)";
		tournamentList->addItem({ text(tournament, "name"), formatLabel(tournament.value("format", std::string())), statusText });
		if (tournament.value("id", 0) == selectedId)
			selectedIndex = (int)i;
	}

	if (selectedIndex >= 0)
		tournamentList->setSelectedItem(selectedIndex);
	else if (selectedId != 0)
		newTournament();

	// À l'ouverture : le tournoi en cours le plus récent (ou, à défaut, le plus récent).
	if (!initialSelectionDone && !tournaments.empty())
	{
		initialSelectionDone = true;
		int chosen = tournaments.back().value("id", 0);
		for (const nlohmann::json & tournament : tournaments)
		{
			if (tournament.value("status", std::string()) == "RUNNING")
				chosen = tournament.value("id", 0);
		}
		selectTournament(chosen);
	}
}

void TournamentAdminPanel::onTournamentState(const nlohmann::json & body)
{
	if (body.value("id", 0) != selectedId)
		return;

	state = body;
	refreshForm();
	refreshMatches();
}

void TournamentAdminPanel::onSessionList(const nlohmann::json & body)
{
	liveSessions = body.value("sessions", nlohmann::json::array());
}

void TournamentAdminPanel::onAck(const nlohmann::json & body)
{
	bool ok = body.value("ok", false);
	status->setText(text(body, "message"));
	status->getRenderer()->setTextColor(ok ? sf::Color(120, 230, 120) : sf::Color(255, 110, 90));

	// Après une création, le nouveau tournoi devient le tournoi sélectionné.
	if (ok && selectedId == 0 && body.value("id", 0) != 0)
		selectTournament(body.value("id", 0));
}

void TournamentAdminPanel::newTournament()
{
	selectedId = 0;
	state = nlohmann::json();
	tournamentList->deselectItems();
	name->setText(L"Tournoi");
	format->setSelectedItemById("POOLS_THEN_BRACKET");
	refreshForm();
	refreshMatches();
}

void TournamentAdminPanel::selectTournament(int id)
{
	if (id == selectedId && !state.is_null())
		return;

	selectedId = id;
	state = nlohmann::json();
	send("UG", { { "id", id } });
}

void TournamentAdminPanel::refreshFormatOptions()
{
	std::string selected = format->getSelectedItemId().toAnsiString();
	bool pools = selected == "POOLS_THEN_BRACKET";
	bool swiss = selected == "SWISS";
	bool doubleElimination = selected == "DOUBLE_ELIMINATION";

	poolCountLabel->setVisible(pools);
	poolCount->setVisible(pools);
	qualifiersLabel->setVisible(pools);
	qualifiers->setVisible(pools);
	thirdPlace->setVisible(pools || swiss);
	grandFinalReset->setVisible(doubleElimination);
	swissRoundsLabel->setVisible(swiss);
	swissRounds->setVisible(swiss);
	topCutLabel->setVisible(swiss);
	topCut->setVisible(swiss);
}

void TournamentAdminPanel::refreshForm()
{
	// L'état est vide (null) entre la sélection d'un tournoi et la réponse du serveur.
	bool draft = selectedId == 0 || (state.is_object() && state.value("status", std::string()) == "DRAFT");

	if (selectedId != 0 && state.is_object())
	{
		name->setText(text(state, "name"));
		const nlohmann::json & settings = state.value("settings", nlohmann::json::object());
		format->setSelectedItemById(settings.value("format", std::string("POOLS_THEN_BRACKET")));
		poolCount->setText(num(settings.value("poolCount", 2)));
		qualifiers->setText(num(settings.value("qualifiersPerPool", 2)));
		thirdPlace->setChecked(settings.value("thirdPlaceMatch", true));
		grandFinalReset->setChecked(settings.value("grandFinalReset", true));
		swissRounds->setText(num(settings.value("swissRounds", 0)));
		topCut->setText(num(settings.value("swissTopCut", 0)));
	}
	refreshFormatOptions();

	// Équipes : actives, triées par tête de série (non classées à la fin) puis par nom.
	std::vector<nlohmann::json> active;
	for (const nlohmann::json & team : teams)
	{
		if (team.value("active", true))
			active.push_back(team);
	}
	std::stable_sort(active.begin(), active.end(), [](const nlohmann::json & a, const nlohmann::json & b) {
		int seedA = a.value("seed", 0) > 0 ? a.value("seed", 0) : 100000;
		int seedB = b.value("seed", 0) > 0 ? b.value("seed", 0) : 100000;
		if (seedA != seedB)
			return seedA < seedB;
		return a.value("name", std::string()) < b.value("name", std::string());
	});

	std::vector<int> registered;
	if (state.is_object())
		registered = state.value("teams", std::vector<int>());

	std::vector<int> previousIds = selectedTeamIds();

	teamList->removeAllItems();
	teamRowIds.clear();
	std::set<std::size_t> selection;
	for (std::size_t i = 0; i < active.size(); i++)
	{
		int id = active[i].value("id", 0);
		int seed = active[i].value("seed", 0);
		teamList->addItem({ text(active[i], "name"), seed > 0 ? num(seed) : sf::String("-") });
		teamRowIds.push_back(id);

		bool selected = std::find(registered.begin(), registered.end(), id) != registered.end();
		if (selectedId == 0)
			selected = std::find(previousIds.begin(), previousIds.end(), id) != previousIds.end();
		if (selected)
			selection.insert(i);
	}
	teamList->setSelectedItems(selection);

	name->setEnabled(draft);
	format->setEnabled(draft);
	teamList->setEnabled(draft);
	saveButton->setEnabled(draft);
	startButton->setEnabled(draft && selectedId != 0);
	deleteButton->setEnabled(selectedId != 0);
}

void TournamentAdminPanel::refreshMatches()
{
	matchList->removeAllItems();
	matchIds.clear();

	if (selectedId == 0 || !state.is_object())
	{
		header->setText(L"Nouveau tournoi : choisissez le format et les équipes, enregistrez puis démarrez.");
		standings->setText("");
		return;
	}

	bool paused = state.value("paused", false);
	header->setText(text(state, "name") + L" - " + statusLabel(state.value("status", std::string())) + (paused ? L" (lancement des matchs suspendu)" : L""));
	pauseButton->setText(paused ? L"Reprendre" : L"Suspendre");

	const nlohmann::json & names = state.value("teamNames", nlohmann::json::object());
	auto teamName = [&](int id) -> sf::String {
		if (id == -1) return L"(exempt)";
		if (id == 0) return L"?";
		return fromServerText(names.value(std::to_string(id), std::to_string(id)));
	};

	std::vector<nlohmann::json> matches = state.value("matches", std::vector<nlohmann::json>());
	std::sort(matches.begin(), matches.end(), [](const nlohmann::json & a, const nlohmann::json & b) {
		return a.value("id", 0) < b.value("id", 0);
	});

	const nlohmann::json & labels = state.value("labels", nlohmann::json::object());
	for (const nlohmann::json & match : matches)
	{
		int id = match.value("id", 0);
		sf::String result;
		if (match.contains("result"))
		{
			const nlohmann::json & r = match["result"];
			result = teamName(r.value("winner", 0)) + L" (" + reasonLabel(r.value("reason", std::string())) + L")";
		}

		matchList->addItem({
			num(id),
			fromServerText(labels.value(std::to_string(id), std::string())),
			teamName(match.value("teamA", 0)),
			teamName(match.value("teamB", 0)),
			statusLabel(match.value("status", std::string())),
			result
		});
		matchIds.push_back(id);
	}

	// Classements des poules / rondes et classement final.
	sf::String summary;
	for (const nlohmann::json & group : state.value("standings", nlohmann::json::array()))
	{
		summary += text(group, "title") + L" : ";
		int position = 1;
		for (const nlohmann::json & row : group.value("rows", nlohmann::json::array()))
		{
			if (position > 1)
				summary += L", ";
			summary += num(position++) + L". " + teamName(row.value("team", 0)) + L" " + num(row.value("points", 0)) + L" pts";
		}
		summary += L"\n";
	}

	const nlohmann::json & ranking = state.value("ranking", nlohmann::json::array());
	if (!ranking.empty())
	{
		summary += L"\nClassement final : ";
		for (std::size_t i = 0; i < ranking.size(); i++)
		{
			if (i > 0)
				summary += L", ";
			summary += num(ranking[i].value("rank", 0)) + L". " + teamName(ranking[i].value("team", 0));
		}
	}
	standings->setText(summary);
}

nlohmann::json TournamentAdminPanel::readSettings() const
{
	auto number = [](const tgui::EditBox::Ptr & box) {
		return std::atoi(box->getText().toAnsiString().c_str());
	};

	return {
		{ "format", format->getSelectedItemId().toAnsiString() },
		{ "poolCount", number(poolCount) },
		{ "qualifiersPerPool", number(qualifiers) },
		{ "thirdPlaceMatch", thirdPlace->isChecked() },
		{ "grandFinalReset", grandFinalReset->isChecked() },
		{ "swissRounds", number(swissRounds) },
		{ "swissTopCut", number(topCut) }
	};
}

std::vector<int> TournamentAdminPanel::selectedTeamIds() const
{
	std::vector<int> ids;
	for (std::size_t index : teamList->getSelectedItemIndices())
	{
		if (index < teamRowIds.size())
			ids.push_back(teamRowIds[index]);
	}
	return ids;
}

void TournamentAdminPanel::save()
{
	nlohmann::json body = {
		{ "name", toUtf8(name->getText()) },
		{ "settings", readSettings() },
		{ "teams", selectedTeamIds() }
	};

	if (selectedId == 0)
	{
		send("UC", body);
	}
	else
	{
		body["id"] = selectedId;
		send("UE", body);
	}
}

int TournamentAdminPanel::selectedMatchId() const
{
	int index = matchList->getSelectedItemIndex();
	return index >= 0 && index < (int)matchIds.size() ? matchIds[index] : 0;
}

void TournamentAdminPanel::forceWinner(bool teamA)
{
	int matchId = selectedMatchId();
	if (matchId == 0 || !state.is_object())
		return;

	nlohmann::json match;
	for (const nlohmann::json & candidate : state.value("matches", nlohmann::json::array()))
	{
		if (candidate.value("id", 0) == matchId)
			match = candidate;
	}
	int winner = match.value(teamA ? "teamA" : "teamB", 0);
	if (winner <= 0)
		return;

	int tournamentId = selectedId;
	if (match.value("status", std::string()) != "DONE")
	{
		send("UF", { { "id", tournamentId }, { "match", matchId }, { "winner", winner }, { "cascade", false } });
		return;
	}

	// Correction d'un match terminé : les matchs suivants déjà joués seront annulés.
	tgui::MessageBox::Ptr box = tgui::MessageBox::create(L"Corriger le résultat",
		L"Ce match est terminé. Corriger son résultat annulera les matchs qui en dépendent et qui ont déjà été joués.");
	box->setInheritedFont(font);
	box->addButton(L"Corriger");
	box->addButton(L"Annuler");
	box->setPosition("(&.size - size) / 2");
	tgui::Gui * gui = this->gui;
	box->connect("ButtonPressed", [this, gui, box, tournamentId, matchId, winner](const sf::String & button) {
		if (button == L"Corriger")
			send("UF", { { "id", tournamentId }, { "match", matchId }, { "winner", winner }, { "cascade", true } });
		gui->remove(box);
	});
	gui->add(box);
}
