#pragma once

#include <TGUI/TGUI.hpp>
#include <nlohmann/json.hpp>
#include <string>

// Onglet "Équipes" de l'écran admin : liste des équipes, formulaire de création /
// modification, mots de passe, import de l'ancien fichier equipe.txt.
class TeamsAdminPanel
{
public:
	TeamsAdminPanel(tgui::Gui * gui, const sf::Font & font);

	tgui::Group::Ptr getGroup() { return group; }
	void setVisible(bool visible);
	void layout(const sf::Vector2u & windowSize, float top);

	// Messages TL (liste) et TR (résultat d'une opération) du serveur.
	void onTeamList(const nlohmann::json & body);
	void onTeamResult(const nlohmann::json & body);

	const nlohmann::json & getTeams() const { return teams; }

private:
	struct PlayerFields
	{
		tgui::Label::Ptr title;
		tgui::EditBox::Ptr login;
		tgui::EditBox::Ptr displayName;
		tgui::EditBox::Ptr password;
		tgui::Button::Ptr resetPassword;
	};

	tgui::EditBox::Ptr createEditBox(const sf::String & placeholder);
	tgui::Button::Ptr createButton(const sf::String & text);
	tgui::Label::Ptr createLabel(const sf::String & text);

	void refreshList();
	void fillForm(int teamIndex);
	void clearForm();
	nlohmann::json readForm() const;
	int selectedTeamId() const;
	void setStatus(const sf::String & text, bool ok);

	void onSave();
	void onDelete();
	void onToggleActive();
	void onResetPassword(int playerIndex);
	void onImport();
	void onOpenCredentialSheet();

	tgui::Gui * gui;
	const sf::Font & font;
	tgui::Group::Ptr group;

	tgui::ListView::Ptr list;
	tgui::Panel::Ptr form;
	tgui::Label::Ptr formTitle;
	tgui::EditBox::Ptr name;
	tgui::EditBox::Ptr tag;
	tgui::EditBox::Ptr seed;
	PlayerFields players[2];
	tgui::Button::Ptr newButton;
	tgui::Button::Ptr saveButton;
	tgui::Button::Ptr toggleActiveButton;
	tgui::Button::Ptr deleteButton;
	tgui::Button::Ptr importButton;
	tgui::Button::Ptr sheetButton;
	tgui::Label::Ptr status;

	nlohmann::json teams;
	std::string credentialSheetPath;
	int editedTeamId;
};
