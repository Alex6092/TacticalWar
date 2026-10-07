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
		// En 2 contre 2 : le joueur joue aussi son allié (comme un joueur dont le coéquipier est absent).
		bool controlAlly = false;
		// Classes (0 : au hasard).
		int playerClass = 0;
		int allyClass = 0;
		int enemyClasses[2] = { 0, 0 };
		// Carte (0 : au hasard parmi les cartes du tournoi).
		int mapId = 0;
		// Facile : l'IA fait parfois une erreur volontaire ; Difficile : elle prépare ses coups sur une
		// copie du combat (voir BotOptions::planner).
		enum class Difficulty { EASY, NORMAL, HARD };
		Difficulty difficulty = Difficulty::EASY;
		// Mode "zone à tenir" (premier à ZONE_POINTS points), sinon au KO.
		bool zone = false;
		static const int ZONE_POINTS = 5;
		// Bonus sur la carte (orbes au centre).
		bool bonuses = false;
		// Carte qui rétrécit à partir de ce tour (0 : jamais), comme en tournoi.
		int shrinkRound = 12;
		// Talents de tournoi : autant pour chaque combattant ; ceux du joueur sont choisis, ceux de
		// l'ordinateur tirés au hasard.
		int talentCount = 0;
		std::vector<std::string> talents;
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
		// Démonstration : l'IA choisit l'action du personnage joué (le sien ou l'allié piloté), envoyée
		// comme par un joueur (mêmes messages qu'un clic).
		void playAsPlayer(float deltatime);

		TrainingSettings settings;
		bool replay;
		// Démonstration : délai avant le combat suivant.
		float replayRemaining;
		float autoWait = 0;
		int autoTurn = -1;
		int autoActions = 0;
		std::mt19937 autoRng;
		AsyncBotDecision autoDecision;
	};
}
