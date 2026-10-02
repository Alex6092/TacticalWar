#pragma once

#include <functional>
#include <string>
#include <vector>
#include <TGUI/TGUI.hpp>
#include <nlohmann/json.hpp>

// Liste des rediffusions (combats terminés, message RL du serveur) avec un bouton "Revoir".
class ReplaysPanel
{
public:
	ReplaysPanel(tgui::Gui * gui, const sf::Font & font);

	// Appelé avec l'identifiant de la rediffusion à revoir.
	std::function<void(const std::string &)> onWatch;

	void setVisible(bool visible);
	void layout(const sf::Vector2u & windowSize, float top);

	void onReplayList(const nlohmann::json & body);
	void setStatus(const sf::String & text, const sf::Color & color = sf::Color(255, 200, 120));

private:
	void watchSelected();

	tgui::Group::Ptr group;
	tgui::Label::Ptr header;
	tgui::ListView::Ptr list;
	tgui::Button::Ptr watchButton;
	tgui::Button::Ptr refreshButton;
	tgui::Label::Ptr status;

	std::vector<std::string> rowIds;
};
