#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace tw
{
	namespace tournament
	{
		// Identifiant d'équipe particulier : place vide d'un tableau (exempt).
		const int BYE_TEAM = -1;
		// Équipe pas encore connue (dépend d'un match non joué).
		const int UNKNOWN_TEAM = 0;

		enum class Format
		{
			POOLS_THEN_BRACKET,		// Poules en round-robin puis élimination directe
			DOUBLE_ELIMINATION,
			SWISS					// Rondes suisses, avec phase finale optionnelle
		};

		enum class StageType
		{
			ROUND_ROBIN_POOLS,
			SINGLE_ELIMINATION,
			DOUBLE_ELIMINATION,
			SWISS
		};

		enum class TournamentStatus
		{
			DRAFT,		// Inscriptions : la liste des équipes peut encore changer
			RUNNING,
			FINISHED
		};

		enum class MatchStatus
		{
			PENDING,		// Au moins une équipe n'est pas encore connue
			READY,			// Les deux équipes sont connues : le match peut être lancé
			IN_PROGRESS,
			DONE
		};

		enum class ResultReason
		{
			KO,				// Toute l'équipe adverse est morte
			ROUND_LIMIT,	// Décision aux points de vie après la limite de tours
			FORFEIT,		// Équipe absente ou déconnectée
			ADMIN,			// Résultat saisi par l'admin
			BYE				// Exempt : victoire automatique
		};

		// Origine d'une équipe dans un match.
		struct SlotRef
		{
			enum class Kind
			{
				NONE,
				TEAM,			// value = id de l'équipe
				WINNER_OF,		// value = id du match
				LOSER_OF,		// value = id du match
				BYE
			};

			Kind kind = Kind::NONE;
			int value = 0;

			static SlotRef team(int teamId) { return { Kind::TEAM, teamId }; }
			static SlotRef winnerOf(int matchId) { return { Kind::WINNER_OF, matchId }; }
			static SlotRef loserOf(int matchId) { return { Kind::LOSER_OF, matchId }; }
			static SlotRef bye() { return { Kind::BYE, 0 }; }
		};

		struct MatchResult
		{
			int winnerTeamId = UNKNOWN_TEAM;
			ResultReason reason = ResultReason::KO;
			// Pourcentage de points de vie restants de chaque camp en fin de combat (0 à 100).
			double hpPercentA = 0;
			double hpPercentB = 0;
			int rounds = 0;
		};

		struct TMatch
		{
			int id = 0;
			int stageIndex = 0;
			// Partie du tableau : "P0".."Pn" (poules), "W" (gagnants / élimination directe),
			// "L" (perdants), "GF", "GF2" (grande finale et revanche), "3P" (petite finale), "S" (suisse).
			std::string bracket;
			int round = 1;		// Tour dans la partie du tableau (à partir de 1)
			int order = 0;		// Position dans le tour (à partir de 0), pour l'affichage
			SlotRef slotA;
			SlotRef slotB;
			int teamA = UNKNOWN_TEAM;
			int teamB = UNKNOWN_TEAM;
			MatchStatus status = MatchStatus::PENDING;
			std::optional<MatchResult> result;
			int mapId = 0;
			int sessionId = 0;	// Combat en cours pour ce match (0 : aucun)

			bool isBye() const { return teamA == BYE_TEAM || teamB == BYE_TEAM; }
			int loserTeamId() const;
		};

		struct Settings
		{
			Format format = Format::POOLS_THEN_BRACKET;

			// Poules :
			int poolCount = 2;
			int qualifiersPerPool = 2;
			bool thirdPlaceMatch = true;

			// Double élimination : revanche si le vainqueur du tableau des perdants gagne la finale.
			bool grandFinalReset = true;

			// Suisse : 0 = automatique (log2 du nombre d'équipes, arrondi au supérieur).
			int swissRounds = 0;
			// Nombre d'équipes qualifiées pour la phase finale après les rondes suisses (0 : aucune).
			int swissTopCut = 0;

			int pointsForWin = 3;
			int pointsForLoss = 0;
		};

		struct Stage
		{
			StageType type = StageType::ROUND_ROBIN_POOLS;
			std::string name;
			bool built = false;
			bool finished = false;

			// Poules : équipes de chaque poule.
			std::vector<std::vector<int>> pools;

			// Élimination : équipes dans l'ordre des têtes de série, et taille du tableau (puissance de 2).
			std::vector<int> seeds;
			int bracketSize = 0;

			// Suisse :
			int totalRounds = 0;
			int currentRound = 0;
		};

		struct StandingRow
		{
			int teamId = 0;
			int played = 0;
			int wins = 0;
			int losses = 0;
			int points = 0;
			double hpDifference = 0;	// Somme des (PV% de l'équipe - PV% adverses)
			double buchholz = 0;		// Suisse : somme des points des adversaires
			bool hadBye = false;
		};

		struct RankingEntry
		{
			int rank = 0;		// Les équipes à égalité partagent le même rang
			int teamId = 0;
		};

		struct Tournament
		{
			int id = 0;
			std::string name;
			Settings settings;
			TournamentStatus status = TournamentStatus::DRAFT;

			// Équipes inscrites, dans l'ordre des têtes de série.
			std::vector<int> teamIds;

			std::vector<Stage> stages;
			std::map<int, TMatch> matches;
			int nextMatchId = 1;
			std::uint32_t rngSeed = 0;
		};

		const char * toString(Format format);
		const char * toString(StageType type);
		const char * toString(TournamentStatus status);
		const char * toString(MatchStatus status);
		const char * toString(ResultReason reason);
	}
}
