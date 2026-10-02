#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include <BattleEngine.h>
#include <Environment.h>
#include <Match.h>
#include <Player.h>

// Un combat entre deux équipes : choix des classes, puis combat géré par le BattleEngine.
// Les identifiants de combattants sont stables : 0 et 1 pour l'équipe 1, 2 et 3 pour l'équipe 2.
class BattleSession
{
public:
	enum class Phase
	{
		CLASS_SELECTION,
		BATTLE,
		ENDED
	};

	BattleSession(int id, tw::Match * match, const tw::battle::GameData & data, tw::Environment * environment, std::int64_t classSelectionDeadline);

	int getId() const { return id; }
	Phase getPhase() const { return phase; }
	tw::Match * getMatch() const { return match; }
	int getMapId() const { return mapId; }

	const std::vector<tw::Player*> & getParticipants() const { return participants; }
	int fighterIdOf(tw::Player * player) const;
	tw::Player * playerOfFighter(int fighterId) const;

	// Choix des classes (une seule fois par joueur).
	bool chooseClass(tw::Player * player, int classId);
	int chosenClass(tw::Player * player) const;
	bool allClassesChosen() const;
	std::int64_t getClassSelectionDeadline() const { return classSelectionDeadline; }
	void postponeClassSelection(std::int64_t deadline) { classSelectionDeadline = deadline; }

	// Crée le moteur (classe au hasard pour les joueurs qui n'ont pas choisi) et démarre le placement.
	void startBattle(std::int64_t nowMs, const std::map<tw::Player*, bool> & connected, const std::map<tw::Player*, std::string> & names);
	tw::battle::BattleEngine * getEngine() { return engine.get(); }

	void markEnded() { phase = Phase::ENDED; }

	// Match de tournoi joué par cette session (0 : match amical).
	void setTournamentMatch(int tournamentId, int matchId) { this->tournamentId = tournamentId; this->tournamentMatchId = matchId; }
	int getTournamentId() const { return tournamentId; }
	int getTournamentMatchId() const { return tournamentMatchId; }
	std::uint32_t getSeed() const { return seed; }

	// Depuis quand toute une équipe (1 ou 2) est absente (0 : présente).
	std::int64_t absentSince[3] = { 0, 0, 0 };

	static tw::battle::BattleMap toBattleMap(tw::Environment * environment);

private:
	int id;
	tw::Match * match;
	const tw::battle::GameData & data;
	tw::battle::BattleMap map;
	int mapId;
	Phase phase;
	std::vector<tw::Player*> participants;
	std::map<tw::Player*, int> classes;
	std::int64_t classSelectionDeadline;
	std::unique_ptr<tw::battle::BattleEngine> engine;
	std::uint32_t seed;
	int tournamentId = 0;
	int tournamentMatchId = 0;
};
