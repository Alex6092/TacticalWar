#pragma once

#include <functional>
#include <vector>
#include <TGUI/TGUI.hpp>
#include <nlohmann/json.hpp>

// Liste des combats en cours (message SL du serveur) avec un bouton "Regarder".
// Utilisée par l'écran spectateur et par l'onglet "Combats" de l'administration.
class LiveSessionsPanel
{
public:
	LiveSessionsPanel(tgui::Gui * gui, const sf::Font & font);

	// Appelé avec l'identifiant de la session à regarder.
	std::function<void(int)> onWatch;
	// Administration : faire rétrécir la carte du combat choisi (bouton affiché si défini).
	std::function<void(int)> onShrink;

	void setVisible(bool visible);
	void layout(const sf::Vector2u & windowSize, float top);

	void onSessionList(const nlohmann::json & body);
	void setStatus(const sf::String & text, const sf::Color & color = sf::Color(255, 200, 120));

	// Combat le plus serré, pour le mode réalisateur (0 si aucun combat n'est regardable).
	int mostContestedSession() const;

	tgui::Group::Ptr getGroup() const { return group; }

private:
	void watchSelected();

	tgui::Gui * gui;
	const sf::Font & font;
	tgui::Group::Ptr group;
	tgui::Label::Ptr header;
	tgui::ListView::Ptr list;
	tgui::Button::Ptr watchButton;
	tgui::Button::Ptr refreshButton;
	tgui::Button::Ptr shrinkButton;
	tgui::Label::Ptr status;

	nlohmann::json sessions;
	std::vector<int> rowIds;
};
