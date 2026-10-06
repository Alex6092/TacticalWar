#include "HelpPanel.h"

#include <Palette.h>

#include "ClientGameData.h"
#include "LinkToServer.h"
#include "UiScale.h"

using namespace tw;

namespace
{
	// Largeur à la taille de texte normale ; elle grandit avec le texte.
	const float BASE_WIDTH = 940;

	float panelWidth()
	{
		return BASE_WIDTH + (ui::scale() - 1.f) * 400.f;
	}

	float column()
	{
		return (panelWidth() - 60) / 2;
	}

	tgui::Label::Ptr text(const sf::Font & font, const sf::String & value, unsigned int size, const sf::Color & color)
	{
		tgui::Label::Ptr label = tgui::Label::create(value);
		label->setInheritedFont(font);
		label->setTextSize(size);
		label->setMaximumTextWidth(column());
		label->getRenderer()->setTextColor(color);
		return label;
	}
}

HelpPanel::HelpPanel(tgui::Gui * gui, const sf::Font & titleFont)
	: titleFont(titleFont)
{
	textFont.loadFromFile("./assets/font/OpenSans-Regular.ttf");

	panel = tgui::Panel::create();
	panel->getRenderer()->setBackgroundColor(sf::Color(15, 15, 25, 245));
	panel->getRenderer()->setBorders(2);
	panel->getRenderer()->setBorderColor(sf::Color(255, 215, 0));
	panel->setVisible(false);
	gui->add(panel);
	rebuild();
}

void HelpPanel::rebuild()
{
	panel->removeAllWidgets();
	const float WIDTH = panelWidth();
	const float COLUMN = column();
	const sf::String reachable = fromServerText(palette::name(palette::Role::REACHABLE));
	const sf::String castable = fromServerText(palette::name(palette::Role::CASTABLE));
	const battle::BattleRules & gameRules = ClientGameData::get().data().rules;

	const sf::Color gold(255, 215, 0);
	const sf::Color white(235, 235, 235);
	tgui::Label::Ptr commandsTitle = text(titleFont, L"Commandes", 20, gold);
	commandsTitle->setPosition(20, 16);
	panel->add(commandsTitle);
	tgui::Label::Ptr commands = text(textFont,
		L"- Clic sur une case " + reachable + L" : se déplacer (1 PM par case).\n"
		L"- 1 à 4, ou clic sur l'icône : choisir un sort. Clic sur une case " + castable + L" : le lancer. Échap : annuler.\n"
		L"- Survol d'un personnage : ses détails, et où il pourra aller à son prochain tour.\n"
		L"- Alt + clic : roue des signaux (Ici, Attaquez, Repli, Danger). Clic molette : signal Ici.\n"
		L"- F1 à F6 : émotes.\n"
		L"- Molette : zoom. Clic droit maintenu : déplacer la vue. F : suivre le personnage actif. C : recentrer.\n"
		L"- Passer le tour : quand il n'y a plus rien à faire.\n"
		L"- H : afficher ou fermer cette aide. Options : sons, mode daltonien (motifs, symboles), "
		L"taille du texte, alerte des 5 dernières secondes du tour.", ui::text(15), white);
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
		L"- Ligne de vue : rochers, arbres, hautes herbes et personnages bloquent les sorts à distance ; "
		L"les buissons et l'eau non (ils bloquent seulement le passage).\n"
		L"- Braises : 8 dégâts au début du tour. Source : +6 PV. Hautes herbes : on s'y cache.\n"
		L"- Combinaisons : une marque posée par une classe (gelé, entravé, provoqué, brûlé) renforce un sort "
		L"d'une autre classe. L'aperçu l'annonce.\n"
		L"- Murs (sort de terrain) : des blocs avec leurs PV. Tout sort de dégâts les abîme, même votre mur "
		L"pour passer. Survol : PV et tours restants.\n"
		L"- Orbes (si activés) : au centre ; passez dessus pour un soin, un PA ou un bouclier.\n"
		L"- Un tour dure " + std::to_wstring(gameRules.turnSeconds) + L" s, puis la réserve de temps ("
		+ std::to_wstring(gameRules.timeBankSeconds) + L" s pour tout le combat) s'entame.", ui::text(15), white);
	rules->setPosition(40 + COLUMN, 52);
	panel->add(rules);

	// Hauteur : la plus longue des deux colonnes, puis les boutons.
	const float HEIGHT = 52 + std::max(commands->getSize().y, rules->getSize().y) + 76;
	panel->setSize(WIDTH, HEIGHT);

	tgui::Button::Ptr options = tgui::Button::create(L"Options");
	options->setInheritedFont(titleFont);
	options->setTextSize(16);
	options->setSize(160, 40);
	options->setPosition(WIDTH / 2 - 170, HEIGHT - 56);
	options->connect("pressed", [this]() {
		if (onOptions)
			onOptions();
	});
	panel->add(options);

	tgui::Button::Ptr close = tgui::Button::create(L"Fermer");
	close->setInheritedFont(titleFont);
	close->setTextSize(16);
	close->setSize(160, 40);
	close->setPosition(WIDTH / 2 + 10, HEIGHT - 56);
	close->connect("pressed", [this]() { hide(); });
	panel->add(close);
	layout(windowSize);
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
	this->windowSize = windowSize;
	sf::Vector2f size = panel->getSize();
	panel->setPosition(std::max(0.f, (windowSize.x - size.x) / 2), std::max(0.f, (windowSize.y - size.y) / 2 - 40));
}
