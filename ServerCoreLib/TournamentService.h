#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <TournamentEngine.h>

namespace tw
{
	// Tournois du serveur : persistance (data/tournaments/<id>.json, réécrit après chaque
	// changement), journal des résultats (data/results.jsonl) et choix des matchs à lancer.
	class TournamentService
	{
	public:
		struct DispatchOptions
		{
			int maxConcurrentMatches = 8;
			// Repos minimal d'une équipe entre deux matchs (le temps de lire son résultat).
			std::int64_t restMs = 20 * 1000;
		};

		struct LaunchRequest
		{
			int tournamentId = 0;
			int matchId = 0;
			int teamA = 0;
			int teamB = 0;
		};

		explicit TournamentService(const std::string & dataDirectory);

		// Charge les tournois existants ; les matchs "en cours" redeviennent "prêts".
		bool load(std::string & log);

		std::string create(const std::string & name, const tournament::Settings & settings, const std::vector<int> & teamIds, int & newId);
		std::string update(int id, const std::string & name, const tournament::Settings & settings, const std::vector<int> & teamIds);
		std::string start(int id);
		std::string remove(int id);
		std::string setPaused(int id, bool paused);

		// Résultat d'un combat terminé.
		std::string reportResult(int tournamentId, int matchId, const tournament::MatchResult & result, std::uint32_t battleSeed);
		// Résultat saisi par l'admin (match prêt, en cours ou terminé : correction).
		std::string forceResult(int tournamentId, int matchId, const tournament::MatchResult & result, bool cascade);
		// Remet un match en cours à l'état "prêt" (combat annulé, à rejouer).
		std::string resetMatch(int tournamentId, int matchId);

		// Matchs à lancer maintenant. isTeamBusy indique si une équipe joue déjà ;
		// runningMatches est le nombre de combats en cours sur le serveur.
		std::vector<LaunchRequest> nextLaunches(const std::function<bool(int)> & isTeamBusy, int runningMatches, std::int64_t nowMs, const DispatchOptions & options);
		void markLaunched(const LaunchRequest & request, int sessionId);
		void markTeamFinished(int teamId, std::int64_t nowMs);

		const tournament::TournamentEngine * find(int id) const;
		std::vector<int> ids() const;
		bool isPaused(int id) const;
		// Les données ont changé depuis le dernier appel (pour rafraîchir l'admin et la vue web).
		bool takeChanged();

	private:
		struct Entry
		{
			tournament::TournamentEngine engine;
			bool paused = false;
		};

		Entry * findEntry(int id);
		bool save(int id, std::string * error = nullptr);
		std::string path(int id) const;

		std::string dataDirectory;
		std::map<int, Entry> tournaments;
		std::map<int, std::int64_t> lastMatchEnd;
		int nextId;
		bool changed;
	};
}
