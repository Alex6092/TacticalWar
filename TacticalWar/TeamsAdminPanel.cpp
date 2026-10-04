#include "TeamsAdminPanel.h"
#include "LinkToServer.h"

#include <algorithm>
#include <filesystem>
#include <Message.h>
#include <Windows.h>
#include <shellapi.h>

// Windows.h définit MessageBox comme une macro, en conflit avec tgui::MessageBox.
#undef MessageBox

namespace
{
	const float FORM_PADDING = 12;
	const float ROW_HEIGHT = 28;
	const unsigned int TEXT_SIZE = 14;

	std::string toUtf8(const sf::String & text)
	{
		std::basic_string<sf::Uint8> utf8 = text.toUtf8();
		return std::string(utf8.begin(), utf8.end());
	}

	sf::String jsonText(const nlohmann::json & json, const char * key)
	{
		return fromServerText(json.value(key, std::string()));
	}

	void sendAdminRequest(const std::string & op, const nlohmann::json & body)
	{
		LinkToServer::getInstance()->SendRaw(op + tw::protocol::dumpJson(body));
	}
}

TeamsAdminPanel::TeamsAdminPanel(tgui::Gui * gui, const sf::Font & font)
	: gui(gui), font(font), teams(nlohmann::json::array()), editedTeamId(0)
{
	group = tgui::Group::create({ "100%", "100%" });

	list = tgui::ListView::create();
	list->setInheritedFont(font);
	list->setTextSize(TEXT_SIZE);
	list->setHeaderHeight(30);
	list->setItemHeight(26);
	list->setShowVerticalGridLines(true);
	list->setExpandLastColumn(true);
	list->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 220));
	list->addColumn(L"Équipe", 190);
	list->addColumn("Tag", 60);
	list->addColumn("Joueur 1", 170);
	list->addColumn("Mot de passe", 110);
	list->addColumn("Joueur 2", 170);
	list->addColumn("Mot de passe", 110);
	list->addColumn(L"État", 120);
	list->connect("ItemSelected", [this](int index) {
		if (index >= 0)
			fillForm(index);
	});
	group->add(list);

	form = tgui::Panel::create();
	form->getRenderer()->setBackgroundColor(sf::Color(30, 30, 30, 200));
	form->getRenderer()->setBorders(1);
	form->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	group->add(form);

	formTitle = createLabel(L"Nouvelle équipe");
	formTitle->setTextSize(20);
	formTitle->getRenderer()->setTextColor(sf::Color::Yellow);
	form->add(formTitle);

	form->add(createLabel(L"Nom de l'équipe"), "nameLabel");
	name = createEditBox(L"ex : Les Éclairs");
	form->add(name);

	form->add(createLabel("Tag"), "tagLabel");
	tag = createEditBox("ex : ECL");
	tag->setMaximumCharacters(6);
	form->add(tag);

	form->add(createLabel(L"Tête de série"), "seedLabel");
	seed = createEditBox("0 = non classée");
	seed->setInputValidator("[0-9]*");
	form->add(seed);

	for (int i = 0; i < 2; i++)
	{
		PlayerFields & fields = players[i];
		// Le joueur 2 est facultatif : un joueur seul joue les deux personnages de l'équipe.
		fields.title = createLabel(i == 0 ? sf::String(L"Joueur 1") : sf::String(L"Joueur 2 (facultatif)"));
		fields.title->getRenderer()->setTextColor(sf::Color(240, 139, 27));
		fields.login = createEditBox("Login");
		fields.displayName = createEditBox(L"Nom affiché");
		fields.password = createEditBox(L"Mdp (vide = généré)");
		fields.resetPassword = createButton(L"Nouveau mdp");
		fields.resetPassword->connect("pressed", [this, i]() { onResetPassword(i); });

		form->add(fields.title);
		form->add(fields.login);
		form->add(fields.displayName);
		form->add(fields.password);
		form->add(fields.resetPassword);
	}

	saveButton = createButton("Enregistrer");
	saveButton->getRenderer()->setBackgroundColor(sf::Color(90, 182, 96, 220));
	saveButton->connect("pressed", [this]() { onSave(); });
	form->add(saveButton);

	newButton = createButton(L"Nouvelle équipe");
	newButton->connect("pressed", [this]() { clearForm(); });
	form->add(newButton);

	toggleActiveButton = createButton(L"Désactiver");
	toggleActiveButton->connect("pressed", [this]() { onToggleActive(); });
	form->add(toggleActiveButton);

	deleteButton = createButton("Supprimer");
	deleteButton->getRenderer()->setBackgroundColor(sf::Color(226, 82, 32, 220));
	deleteButton->connect("pressed", [this]() { onDelete(); });
	form->add(deleteButton);

	importButton = createButton("Importer equipe.txt");
	importButton->connect("pressed", [this]() { onImport(); });
	form->add(importButton);

	sheetButton = createButton("Ouvrir les fiches");
	sheetButton->connect("pressed", [this]() { onOpenCredentialSheet(); });
	form->add(sheetButton);

	status = createLabel("");
	status->setAutoSize(false);
	form->add(status);

	clearForm();
	gui->add(group);
}

tgui::EditBox::Ptr TeamsAdminPanel::createEditBox(const sf::String & placeholder)
{
	tgui::EditBox::Ptr edit = tgui::EditBox::create();
	edit->setInheritedFont(font);
	edit->setTextSize(TEXT_SIZE);
	edit->setDefaultText(placeholder);
	edit->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 220));
	return edit;
}

tgui::Button::Ptr TeamsAdminPanel::createButton(const sf::String & text)
{
	tgui::Button::Ptr button = tgui::Button::create(text);
	button->setInheritedFont(font);
	button->setTextSize(TEXT_SIZE);
	button->getRenderer()->setBackgroundColor(sf::Color(255, 255, 255, 200));
	return button;
}

tgui::Label::Ptr TeamsAdminPanel::createLabel(const sf::String & text)
{
	tgui::Label::Ptr label = tgui::Label::create(text);
	label->setInheritedFont(font);
	label->setTextSize(TEXT_SIZE);
	label->getRenderer()->setTextColor(sf::Color::White);
	return label;
}

void TeamsAdminPanel::setVisible(bool visible)
{
	group->setVisible(visible);
}

void TeamsAdminPanel::layout(const sf::Vector2u & windowSize, float top)
{
	const float margin = 30;
	float height = windowSize.y - top - margin;
	float formWidth = 480;
	float listWidth = windowSize.x - formWidth - 3 * margin;

	list->setPosition(margin, top);
	list->setSize(listWidth, height);

	// Répartit la largeur : nom, tag, joueur, mdp, joueur, mdp (l'état prend le reste).
	const float weights[] = { 0.19f, 0.06f, 0.17f, 0.11f, 0.17f, 0.11f };
	for (std::size_t i = 0; i < 6; i++)
		list->setColumnWidth(i, (listWidth - 20) * weights[i]);

	form->setPosition(2 * margin + listWidth, top);
	form->setSize(formWidth, height);

	float inner = formWidth - 2 * FORM_PADDING;
	float half = (inner - FORM_PADDING) / 2;
	float y = FORM_PADDING;

	formTitle->setPosition(FORM_PADDING, y);
	y += 36;

	form->get<tgui::Label>("nameLabel")->setPosition(FORM_PADDING, y);
	name->setPosition(FORM_PADDING, y + 20);
	name->setSize(inner, ROW_HEIGHT);
	y += 56;

	form->get<tgui::Label>("tagLabel")->setPosition(FORM_PADDING, y);
	tag->setPosition(FORM_PADDING, y + 20);
	tag->setSize(half, ROW_HEIGHT);
	form->get<tgui::Label>("seedLabel")->setPosition(2 * FORM_PADDING + half, y);
	seed->setPosition(2 * FORM_PADDING + half, y + 20);
	seed->setSize(half, ROW_HEIGHT);
	y += 64;

	for (int i = 0; i < 2; i++)
	{
		PlayerFields & fields = players[i];
		fields.title->setPosition(FORM_PADDING, y);
		y += 22;
		fields.login->setPosition(FORM_PADDING, y);
		fields.login->setSize(half, ROW_HEIGHT);
		fields.displayName->setPosition(2 * FORM_PADDING + half, y);
		fields.displayName->setSize(half, ROW_HEIGHT);
		y += ROW_HEIGHT + 6;
		fields.password->setPosition(FORM_PADDING, y);
		fields.password->setSize(half, ROW_HEIGHT);
		fields.resetPassword->setPosition(2 * FORM_PADDING + half, y);
		fields.resetPassword->setSize(half, ROW_HEIGHT);
		y += ROW_HEIGHT + 16;
	}

	tgui::Button::Ptr rows[3][2] = {
		{ saveButton, newButton },
		{ toggleActiveButton, deleteButton },
		{ importButton, sheetButton }
	};
	for (auto & row : rows)
	{
		row[0]->setPosition(FORM_PADDING, y);
		row[0]->setSize(half, 34);
		row[1]->setPosition(2 * FORM_PADDING + half, y);
		row[1]->setSize(half, 34);
		y += 42;
	}

	status->setPosition(FORM_PADDING, y + 6);
	status->setSize(inner, std::max(40.f, height - y - 12));
}

void TeamsAdminPanel::onTeamList(const nlohmann::json & body)
{
	teams = body.value("teams", nlohmann::json::array());
	credentialSheetPath = body.value("credentialSheet", std::string());

	if (body.value("readOnly", false))
		setStatus(L"data/teams.json n'a pas pu être lu par le serveur : modifications impossibles.", false);

	refreshList();
}

void TeamsAdminPanel::refreshList()
{
	list->removeAllItems();

	int selected = -1;
	for (std::size_t i = 0; i < teams.size(); i++)
	{
		const nlohmann::json & team = teams[i];
		std::vector<sf::String> row;
		row.push_back(jsonText(team, "name"));
		row.push_back(jsonText(team, "tag"));

		for (int p = 0; p < 2; p++)
		{
			nlohmann::json player = team["players"].size() > (std::size_t)p ? team["players"][p] : nlohmann::json::object();
			sf::String login = jsonText(player, "login");
			if (login.isEmpty())
			{
				// Équipe d'un seul joueur.
				row.push_back(L"(aucun)");
				row.push_back("");
				continue;
			}
			if (player.value("connected", false))
				login += " [en ligne]";
			row.push_back(login);
			std::string password = player.value("password", std::string());
			row.push_back(password.empty() ? sf::String("?") : fromServerText(password));
		}

		sf::String state = team.value("active", true) ? sf::String("Active") : sf::String(L"Désactivée");
		if (team.value("busy", false))
			state += " (en match)";
		row.push_back(state);

		list->addItem(row);

		if (team.value("id", 0) == editedTeamId)
			selected = (int)i;
	}

	// Garde l'équipe éditée sélectionnée après un rafraîchissement.
	if (selected >= 0)
	{
		list->setSelectedItem(selected);
		fillForm(selected);
	}
	else if (editedTeamId != 0)
	{
		clearForm();
	}
}

void TeamsAdminPanel::fillForm(int teamIndex)
{
	if (teamIndex < 0 || teamIndex >= (int)teams.size())
		return;

	const nlohmann::json & team = teams[teamIndex];
	editedTeamId = team.value("id", 0);

	formTitle->setText("Modifier : " + jsonText(team, "name"));
	name->setText(jsonText(team, "name"));
	tag->setText(jsonText(team, "tag"));
	int seedValue = team.value("seed", 0);
	seed->setText(seedValue > 0 ? std::to_string(seedValue) : "");

	for (int i = 0; i < 2; i++)
	{
		nlohmann::json player = team["players"].size() > (std::size_t)i ? team["players"][i] : nlohmann::json::object();
		players[i].login->setText(jsonText(player, "login"));
		players[i].displayName->setText(jsonText(player, "displayName"));
		players[i].password->setText("");
		players[i].password->setDefaultText(L"Mdp (vide = inchangé)");
		players[i].resetPassword->setEnabled(true);
	}

	bool active = team.value("active", true);
	toggleActiveButton->setText(active ? L"Désactiver" : L"Réactiver");
	toggleActiveButton->setEnabled(true);
	deleteButton->setEnabled(true);
}

void TeamsAdminPanel::clearForm()
{
	editedTeamId = 0;
	list->deselectItems();

	formTitle->setText(L"Nouvelle équipe");
	name->setText("");
	tag->setText("");
	seed->setText("");

	for (int i = 0; i < 2; i++)
	{
		players[i].login->setText("");
		players[i].displayName->setText("");
		players[i].password->setText("");
		players[i].password->setDefaultText(L"Mdp (vide = généré)");
		players[i].resetPassword->setEnabled(false);
	}

	toggleActiveButton->setText(L"Désactiver");
	toggleActiveButton->setEnabled(false);
	deleteButton->setEnabled(false);
}

nlohmann::json TeamsAdminPanel::readForm() const
{
	nlohmann::json playersJson = nlohmann::json::array();
	for (int i = 0; i < 2; i++)
	{
		nlohmann::json player = {
			{ "login", toUtf8(players[i].login->getText()) },
			{ "displayName", toUtf8(players[i].displayName->getText()) }
		};
		std::string password = toUtf8(players[i].password->getText());
		if (!password.empty())
			player["password"] = password;
		playersJson.push_back(player);
	}

	return {
		{ "name", toUtf8(name->getText()) },
		{ "tag", toUtf8(tag->getText()) },
		{ "seed", std::atoi(seed->getText().toAnsiString().c_str()) },
		{ "players", playersJson }
	};
}

int TeamsAdminPanel::selectedTeamId() const
{
	return editedTeamId;
}

void TeamsAdminPanel::setStatus(const sf::String & text, bool ok)
{
	status->setText(text);
	status->getRenderer()->setTextColor(ok ? sf::Color(120, 230, 120) : sf::Color(255, 110, 90));
}

void TeamsAdminPanel::onSave()
{
	nlohmann::json body = readForm();
	if (editedTeamId == 0)
	{
		sendAdminRequest("TC", body);
	}
	else
	{
		body["id"] = editedTeamId;
		sendAdminRequest("TU", body);
	}
}

void TeamsAdminPanel::onToggleActive()
{
	for (const nlohmann::json & team : teams)
	{
		if (team.value("id", 0) == editedTeamId)
		{
			sendAdminRequest("TA", { { "id", editedTeamId }, { "active", !team.value("active", true) } });
			return;
		}
	}
}

void TeamsAdminPanel::onDelete()
{
	if (editedTeamId == 0)
		return;

	int teamId = editedTeamId;
	tgui::MessageBox::Ptr box = tgui::MessageBox::create("Supprimer",
		L"Supprimer l'équipe \"" + name->getText() + L"\" ?\n(Si elle a déjà joué, elle sera seulement désactivée.)");
	box->setInheritedFont(font);
	box->setTextSize(TEXT_SIZE);
	box->addButton("Supprimer");
	box->addButton("Annuler");
	box->setPosition("(&.size - size) / 2");

	tgui::Gui * gui = this->gui;
	box->connect("ButtonPressed", [gui, box, teamId](const sf::String & button) {
		if (button == "Supprimer")
			sendAdminRequest("TD", { { "id", teamId } });
		gui->remove(box);
	});
	gui->add(box);
}

void TeamsAdminPanel::onResetPassword(int playerIndex)
{
	std::string login = toUtf8(players[playerIndex].login->getText());
	if (editedTeamId != 0 && !login.empty())
		sendAdminRequest("TK", { { "login", login } });
}

void TeamsAdminPanel::onImport()
{
	sendAdminRequest("TI", nlohmann::json::object());
}

void TeamsAdminPanel::onOpenCredentialSheet()
{
	// La fiche est écrite par le serveur : elle n'est accessible que si le client
	// tourne sur le même poste (même dossier de lancement).
	std::filesystem::path path = std::filesystem::u8path(credentialSheetPath);
	std::error_code ec;
	if (credentialSheetPath.empty() || !std::filesystem::exists(path, ec))
	{
		setStatus(L"Fiche introuvable sur ce poste. Sur le PC serveur : " + fromServerText(credentialSheetPath), false);
		return;
	}

	std::wstring absolute = std::filesystem::absolute(path, ec).wstring();
	ShellExecuteW(NULL, L"open", absolute.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void TeamsAdminPanel::onTeamResult(const nlohmann::json & body)
{
	bool ok = body.value("ok", false);
	sf::String text = jsonText(body, "message");

	if (body.contains("passwords") && body["passwords"].is_object() && !body["passwords"].empty())
	{
		text += L"\nMots de passe :";
		for (auto it = body["passwords"].begin(); it != body["passwords"].end(); it++)
			text += "\n  " + fromServerText(it.key()) + " : " + fromServerText(it.value().get<std::string>());
	}

	setStatus(text, ok);

	// Après une création réussie, le formulaire est vidé pour la saisie suivante.
	if (ok && editedTeamId == 0)
		clearForm();
}
