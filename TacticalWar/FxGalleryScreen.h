#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "LocalBattleScreen.h"

namespace tw
{
	// Galerie des effets de sorts, pour régler les animations (assets/spellsprites/effects.json et
	// objet "visual" des sorts dans assets/data/gamedata.json). L'écran de combat est alimenté par
	// un moteur de combat local, sans serveur : chaque sort est lancé en boucle par un personnage de
	// sa classe sur une cible (ennemie) ou un allié, avec les vrais événements du combat (effets
	// durables, glyphes, poussées, dégâts périodiques au tour suivant). Avant chaque lancer, la visée
	// est montrée : portée du sort, cases ciblables, une case non ciblable puis la cible survolées.
	//   TacticalWar.exe --fx-gallery [--fx-spell <id du sort>] [--fx-map <id de la carte>]
	// Gauche / droite : sort précédent / suivant, R : rejouer, F5 : relire les fichiers, Échap : quitter.
	// Les sorts de la barre restent utilisables à la souris : la boucle s'arrête jusqu'au prochain R.
	class FxGalleryScreen : public LocalBattleScreen
	{
	public:
		FxGalleryScreen(tgui::Gui * gui, const std::string & spellId, int environmentId);

		virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
		virtual void update(float deltatime);
		virtual void onEvent(void * e);
		virtual void onCellHover(int cellX, int cellY);

	protected:
		// Action à la souris : la démonstration automatique s'arrête (R pour la reprendre).
		virtual void onPlayerAction();
		virtual void onLocalEnd();

	private:
		// Personnages, dans l'ordre de création (identifiants du moteur).
		enum { CASTER = 0, ENEMY = 1, ALLY = 2 };

		enum class Step
		{
			AIM_ZONE,		// Sort sélectionné : portée, cases ciblables, une case non ciblable survolée
			AIM_TARGET,		// Cible survolée : zone d'impact
			CAST,			// Lancer le sort
			AFTER_CAST,		// Fin du tour du lanceur (effets durables)
			ROUND,			// Tours des autres personnages (dégâts périodiques, glyphes)
			PAUSE,			// Avant de rejouer
			MANUAL			// Sorts lancés à la souris : les autres personnages passent leur tour
		};

		enum class Command { NONE, PREVIOUS, NEXT, REPLAY, RELOAD, QUIT };

		struct Entry
		{
			int classId;
			int slot;
			std::string spellId;
		};

		struct Layout
		{
			battle::Cell caster;
			battle::Cell enemy;
			battle::Cell ally;
			battle::Cell target;

			bool operator==(const Layout & other) const
			{
				return caster == other.caster && enemy == other.enemy && ally == other.ally && target == other.target;
			}
		};

		static int existingEnvironment(int requested);

		void buildEntries();
		const battle::ClassDef * currentClass() const;
		const battle::SpellDef * currentSpell() const;
		void select(int index);
		void restart();
		void reload(bool announce);
		void advance();
		void aimAt(const battle::Cell & cell);
		bool findLayout(const battle::ClassDef & classDef, const battle::SpellDef & spell, Layout & layout, std::string & error);
		std::unique_ptr<battle::BattleEngine> createEngine(const battle::ClassDef & classDef, const battle::SpellDef & spell,
			const Layout & layout, std::string & error);
		void focusCamera();
		void refreshLabel();

		// Dossier qui contient assets/ : celui du dépôt quand le jeu est lancé depuis x64/<configuration>,
		// pour régler directement les fichiers suivis par git.
		std::string root;
		battle::BattleMap baseMap;

		std::vector<Entry> entries;
		int current;
		int shownClass;
		std::map<std::string, Layout> layouts;
		Layout layout;
		bool focusPending;
		Step step;
		float wait;
		Command command;
		tgui::Label::Ptr label;
	};
}
