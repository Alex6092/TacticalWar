#pragma once

#include <string>
#include <utility>
#include <vector>

#include "LocalBattleScreen.h"

namespace tw
{
	// Réglages de l'entraînement hors ligne (gardés d'une partie à l'autre pendant la session).
	struct TrainingSettings
	{
		// 2 contre 2 (le joueur et un allié joué par l'IA), sinon 1 contre 1.
		bool duo = true;
		// Classes (0 : au hasard).
		int playerClass = 0;
		int allyClass = 0;
		int enemyClasses[2] = { 0, 0 };
		// Carte (0 : au hasard parmi les cartes du tournoi).
		int mapId = 0;
		// Facile : l'IA fait parfois une erreur volontaire.
		bool easy = true;
		// Le personnage du joueur est aussi joué par l'IA, et les combats s'enchaînent (démonstration
		// sur un écran projeté, captures automatiques).
		bool autoplay = false;

		static TrainingSettings & current();
	};

	// Entraînement sans serveur : le joueur affronte des adversaires joués par l'IA (avec un allié
	// joué par l'IA en 2 contre 2), avec les règles et les minuteurs du tournoi. À la fin du combat :
	// "Rejouer" (mêmes réglages) ou "Retour" (écran des réglages).
	class TrainingScreen : public LocalBattleScreen
	{
	public:
		TrainingScreen(tgui::Gui * gui, const TrainingSettings & settings);

		virtual void update(float deltatime);

		// Cartes proposées (identifiant, nom) : celles du tournoi, à défaut toutes les cartes.
		static const std::vector<std::pair<int, std::string>> & maps();

	protected:
		virtual void onLocalEnd();
		virtual void leave();

	private:
		static int chooseMap(int requested);

		TrainingSettings settings;
		bool replay;
		// Démonstration : délai avant le combat suivant.
		float replayRemaining;
	};
}
