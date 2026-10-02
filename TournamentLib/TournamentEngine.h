#pragma once

#include <set>
#include <string>
#include <vector>

#include "Tournament.h"

namespace tw
{
	namespace tournament
	{
		// Règles du tournoi : génération des poules et des tableaux, appariements,
		// propagation des résultats, classements. Aucune dépendance réseau ni graphique.
		// Les méthodes qui modifient l'état retournent un message d'erreur (vide si succès).
		class TournamentEngine
		{
		public:
			explicit TournamentEngine(const Tournament & tournament = Tournament());

			const Tournament & get() const { return tournament; }

			// --- Préparation (statut DRAFT) ---
			std::string setTeams(const std::vector<int> & teamIdsInSeedOrder);
			std::string setSettings(const Settings & settings);
			// Vérifie que les paramètres sont compatibles avec le nombre d'équipes.
			std::string validate() const;
			std::string start();

			// --- Déroulement ---
			// Matchs prêts à être joués, dans l'ordre de priorité de lancement.
			std::vector<int> readyMatches() const;
			std::string markInProgress(int matchId, int sessionId);
			std::string reportResult(int matchId, const MatchResult & result);
			// Remet un match en cours à l'état "prêt" (combat annulé, à rejouer).
			std::string resetMatch(int matchId);
			// Corrige le résultat d'un match terminé. Si des matchs qui en dépendent ont déjà
			// été joués, l'opération est refusée sauf si cascade est vrai : ils sont alors annulés.
			std::string amendResult(int matchId, const MatchResult & result, bool cascade);
			// Après un redémarrage du serveur : les matchs "en cours" redeviennent "prêts".
			void recoverAfterRestart();

			// --- Lecture ---
			const TMatch * findMatch(int matchId) const;
			std::vector<StandingRow> standings(int stageIndex, int poolIndex = 0) const;
			std::vector<RankingEntry> finalRanking() const;
			bool isFinished() const { return tournament.status == TournamentStatus::FINISHED; }
			// Clé de priorité de lancement (plus petite = plus prioritaire).
			double scheduleKey(const TMatch & match) const;

		private:
			// Génération (TournamentBrackets.cpp)
			void buildStage(int stageIndex);
			void buildPools(Stage & stage, int stageIndex);
			void buildSingleElimination(Stage & stage, int stageIndex);
			void buildDoubleElimination(Stage & stage, int stageIndex);
			void buildNextSwissRound(Stage & stage, int stageIndex);
			std::vector<int> poolQualifiers() const;
			std::vector<int> swissQualifiers(int stageIndex, int count) const;

			// Propagation (TournamentEngine.cpp)
			int addMatch(int stageIndex, const std::string & bracket, int round, int order, const SlotRef & a, const SlotRef & b);
			int resolveSlot(const SlotRef & slot) const;
			void refreshMatch(TMatch & match);
			void propagate();
			bool isStageComplete(int stageIndex) const;
			void onStageComplete(int stageIndex);
			void collectDependents(int matchId, std::set<int> & dependents) const;

			// Classements (TournamentStandings.cpp)
			std::vector<StandingRow> computeRows(const std::vector<int> & teams, int stageIndex, const std::string & bracket) const;
			void sortPoolRows(std::vector<StandingRow> & rows, int stageIndex, const std::string & bracket) const;
			void sortSwissRows(std::vector<StandingRow> & rows) const;
			std::vector<std::vector<int>> eliminationPlacement(int stageIndex) const;
			int seedIndex(int teamId) const;

			Tournament tournament;
		};
	}
}
