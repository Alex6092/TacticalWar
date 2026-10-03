#pragma once

#include <functional>
#include <vector>
#include <TGUI/TGUI.hpp>
#include <GameData.h>

namespace tw
{
	// Choix des sorts emportés : 4 parmi les 6 de la classe, un clic sur un sort l'ajoute ou le retire.
	// LIST : une ligne par sort avec sa description (écran de choix de classe) ;
	// ROW : les icônes sur une ligne, description au survol (réglages de l'entraînement).
	class SpellPicker
	{
	public:
		enum class Layout { LIST, ROW };

		SpellPicker(const sf::Font & font, Layout layout);

		// Groupe à ajouter à l'interface (ou à un panneau) et à placer.
		tgui::Group::Ptr getWidget() const { return group; }

		// Classe montrée (nullptr : aucune) et sorts proposés au départ ; un choix non valable donne les
		// premiers sorts de la classe.
		void setClass(const battle::ClassDef * classDef, const std::vector<int> & requested);
		const std::vector<int> & getChosen() const { return chosen; }
		bool isComplete() const;
		// Choix verrouillé : plus de clic.
		void setLocked(bool locked);
		// LIST : largeur et hauteur d'une ligne ; ROW : largeur seulement.
		void setGeometry(float width, float rowHeight);
		float getHeight() const;

		// Le choix a changé (clic du joueur).
		std::function<void()> onChange;

	private:
		void toggle(int index);
		void refresh();
		void arrange();

		const sf::Font & font;
		Layout layout;
		tgui::Group::Ptr group;
		std::vector<tgui::Picture::Ptr> icons;
		std::vector<tgui::Label::Ptr> labels;
		tgui::Label::Ptr counter;
		const battle::ClassDef * classDef;
		std::vector<int> chosen;
		bool locked;
		float width;
		float rowHeight;
	};
}
