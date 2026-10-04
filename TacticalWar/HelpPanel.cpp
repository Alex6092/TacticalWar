#include "HelpPanel.h"

using namespace tw;

namespace
{
	const float WIDTH = 940;
	const float HEIGHT = 470;
	const float COLUMN = (WIDTH - 60) / 2;

	tgui::Label::Ptr text(const sf::Font & font, const sf::String & value, unsigned int size, const sf::Color & color)
	{
		tgui::Label::Ptr label = tgui::Label::create(value);
		label->setInheritedFont(font);
		label->setTextSize(size);
		label->setMaximumTextWidth(COLUMN);
		label->getRenderer()->setTextColor(color);
		return label;
	}
}

HelpPanel::HelpPanel(tgui::Gui * gui, const sf::Font & titleFont)
{
	textFont.loadFromFile("./assets/font/OpenSans-Regular.ttf");

	panel = tgui::Panel::create();
	panel->setSize(WIDTH, HEIGHT);
	panel->getRenderer()->setBackgroundColor(sf::Color(15, 15, 25, 245));
	panel->getRenderer()->setBorders(2);
	panel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	panel->setVisible(false);

	const sf::Color gold(255, 215, 0);
	const sf::Color white(235, 235, 235);
	tgui::Label::Ptr commandsTitle = text(titleFont, L"Commandes", 20, gold);
	commandsTitle->setPosition(20, 16);
	panel->add(commandsTitle);
	tgui::Label::Ptr commands = text(textFont,
		L"- Clic sur une case verte : se déplacer (1 PM par case).\n"
		L"- 1 à 4, ou clic sur l'icône : choisir un sort. Clic sur une case bleue : le lancer. Échap : annuler.\n"
		L"- Survol d'un personnage : ses détails, et où il pourra aller à son prochain tour.\n"
		L"- Alt + clic : roue des signaux (Ici, Attaquez, Repli, Danger). Clic molette : signal Ici.\n"
		L"- F1 à F6 : émotes.\n"
		L"- Molette : zoom. Clic droit maintenu : déplacer la vue. F : suivre le personnage actif. C : recentrer.\n"
		L"- Passer le tour : quand il n'y a plus rien à faire.\n"
		L"- H : afficher ou fermer cette aide.", 15, white);
	commands->setPosition(20, 52);
	panel->add(commands);

	tgui::Label::Ptr rulesTitle = text(titleFont, L"À retenir", 20, gold);
	rulesTitle->setPosition(40 + COLUMN, 16);
	panel->add(rulesTitle);
	tgui::Label::Ptr rules = text(textFont,
		L"- PA (étoile jaune) : lancer des sorts. PM (carré vert) : se déplacer. Ils reviennent à chaque tour.\n"
		L"- Un sort grisé avec un chiffre est en relance : il revient dans ce nombre de tours.\n"
		L"- Bouclier (écusson bleu) : il absorbe les dégâts avant les PV.\n"
		L"- Tacle : quitter le contact d'un ennemi peut coûter des PM et des PA.\n"
		L"- Ligne de vue : rochers, arbres, hautes herbes et personnages bloquent les sorts à distance.\n"
		L"- Braises : 8 dégâts au début du tour. Source : +6 PV. Hautes herbes : on s'y cache.\n"
		L"- Combinaisons : une marque posée par une classe (gelé, entravé, provoqué, brûlé) renforce un sort "
		L"d'une autre classe. L'aperçu l'annonce.\n"
		L"- Un tour dure 40 s, puis la réserve de temps (30 s pour tout le combat) s'entame.", 15, white);
	rules->setPosition(40 + COLUMN, 52);
	panel->add(rules);

	tgui::Button::Ptr close = tgui::Button::create(L"Fermer");
	close->setInheritedFont(titleFont);
	close->setTextSize(16);
	close->setSize(160, 40);
	close->setPosition((WIDTH - 160) / 2, HEIGHT - 56);
	close->connect("pressed", [this]() { hide(); });
	panel->add(close);

	gui->add(panel);
}

void HelpPanel::toggle()
{
	panel->setVisible(!panel->isVisible());
	if (panel->isVisible())
		panel->moveToFront();
}

void HelpPanel::hide()
{
	panel->setVisible(false);
}

bool HelpPanel::isVisible() const
{
	return panel->isVisible();
}

void HelpPanel::layout(const sf::Vector2u & windowSize)
{
	panel->setPosition(std::max(0.f, (windowSize.x - WIDTH) / 2), std::max(0.f, (windowSize.y - HEIGHT) / 2 - 40));
}
