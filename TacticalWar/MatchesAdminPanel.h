#pragma once

#include <functional>
#include <vector>
#include <TGUI/TGUI.hpp>
#include <nlohmann/json.hpp>

// Onglet « Matchs » de l'administration : création d'un match amical (nom, équipes, carte) et liste
// des matchs amicaux (messages FL, FC, FX, FR) avec « Regarder », « Annuler » et « Actualiser ».
class MatchesAdminPanel
{
public:
	MatchesAdminPanel(tgui::Gui * gui, const sf::Font & font);

	// Appelé avec l'identifiant de la session à regarder.
	std::function<void(int)> onWatch;

	void setVisible(bool visible);
	void layout(const sf::Vector2u & windowSize, float top);

	// Équipes (message TL), matchs amicaux et cartes (FL), réponse à une demande (FR).
	void setTeams(const nlohmann::json & teams);
	void onFriendlyList(const nlohmann::json & body);
	void onResult(const nlohmann::json & body);

private:
	void create();
	void cancelSelected();
	void watchSelected();
	void setStatus(const sf::String & text, const sf::Color & color);
	const nlohmann::json * selectedMatch() const;

	const sf::Font & font;
	tgui::Group::Ptr group;
	tgui::Label::Ptr formTitle;
	tgui::Label::Ptr nameLabel;
	tgui::EditBox::Ptr nameEdit;
	tgui::Label::Ptr teamALabel;
	tgui::ComboBox::Ptr teamACombo;
	tgui::Label::Ptr teamBLabel;
	tgui::ComboBox::Ptr teamBCombo;
	tgui::Label::Ptr mapLabel;
	tgui::ComboBox::Ptr mapCombo;
	tgui::Button::Ptr createButton;
	tgui::Label::Ptr listTitle;
	tgui::ListView::Ptr list;
	tgui::Button::Ptr watchButton;
	tgui::Button::Ptr cancelButton;
	tgui::Button::Ptr refreshButton;
	tgui::Label::Ptr status;

	nlohmann::json matches;
	std::vector<int> rowIds;
};
