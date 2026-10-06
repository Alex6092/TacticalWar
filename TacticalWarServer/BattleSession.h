#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <BattleEngine.h>
#include <Environment.h>
#include <Match.h>
#include <Player.h>
#include "net/NetServer.h"

// Un combat entre deux équipes : bannissement (certains matchs de tournoi), choix des classes, puis
// combat géré par le BattleEngine.
// Les identifiants de combattants sont stables : 0 et 1 pour l'équipe 1, 2 et 3 pour l'équipe 2.
class BattleSession
{
public:
	enum class Phase
	{
		BAN,
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
	// Combattant que le joueur fait agir : le combattant actif s'il s'agit du sien, ou de celui de
	// son coéquipier absent qu'il pilote ; sinon le sien.
	int actingFighter(tw::Player * player) const;
	// Équipe du joueur dans ce combat : 1 ou 2 (0 : il n'y participe pas).
	int teamOf(tw::Player * player) const;
	tw::Player * playerOfFighter(int fighterId) const;

	// Bannissement : chaque équipe interdit une classe à l'autre ; le premier choix d'un joueur de
	// l'équipe compte. À la fin de la phase, le choix des classes commence.
	void startBanPhase(std::int64_t deadline);
	bool hasBanPhase() const { return banPhase; }
	std::int64_t getBanDeadline() const { return banDeadline; }
	bool ban(tw::Player * player, int classId);
	int bannedBy(int team) const { return team == 1 || team == 2 ? bans[team] : 0; }	// 0 : aucune
	int forbiddenClass(int team) const { return bannedBy(3 - team); }
	bool allTeamsBanned() const { return bans[1] != 0 && bans[2] != 0; }
	void endBanPhase(std::int64_t classSelectionDeadline);

	// Choix des classes et des sorts (une seule fois par joueur). Un choix de sorts non valable
	// donne les sorts par défaut de la classe ; la classe interdite par l'adversaire est refusée.
	// chooser : joueur qui a fait le choix (son coéquipier absent, par exemple ; nul : lui-même).
	bool chooseClass(tw::Player * player, int classId, const std::vector<int> & spells = std::vector<int>(),
		const std::vector<std::string> & talents = std::vector<std::string>(), tw::Player * chooser = NULL);
	// Motif du refus d'un choix de classe (vide : le choix est possible).
	std::string choiceRefusal(tw::Player * player, int classId) const;
	// Talents de tournoi à choisir par chaque équipe (0 hors tournoi). Les emplacements laissés vides
	// sont remplis au hasard au début du combat.
	void setTalentSlots(int team1, int team2) { talentSlotsByTeam[1] = team1; talentSlotsByTeam[2] = team2; }
	int talentSlots(tw::Player * player) const;
	int chosenClass(tw::Player * player) const;
	std::vector<int> chosenSpells(tw::Player * player) const;
	std::vector<std::string> chosenTalents(tw::Player * player) const;
	// Joueur qui a choisi la classe de player (lui-même, ou un coéquipier pendant son absence).
	tw::Player * chooserOf(tw::Player * player) const;
	// Apparence du personnage du joueur (vérifiée par l'appelant), avant le début du combat.
	void setAppearance(tw::Player * player, const std::string & appearance) { appearances[player] = appearance; }
	std::string appearanceOf(tw::Player * player) const
	{
		auto found = appearances.find(player);
		return found != appearances.end() ? found->second : std::string();
	}
	// Brouillon d'un joueur non verrouillé : classe affichée sur son écran de choix (montrée à son
	// coéquipier), sorts, talents et apparence en cours (apparence vérifiée par l'appelant).
	struct Draft
	{
		int classId = 0;
		std::vector<int> spells;
		std::vector<std::string> talents;
		std::string appearance;
	};
	// Renvoie true si la classe affichée a changé.
	bool setDraft(tw::Player * player, const Draft & draft);
	int viewingClass(tw::Player * player) const;
	// Délai écoulé : chaque joueur non verrouillé reçoit la classe affichée sur son écran, avec ses
	// sorts, ses talents et son apparence, si elle est permise. Renvoie le nombre de choix retenus.
	int lockViewedClasses();
	bool allClassesChosen() const;
	std::int64_t getClassSelectionDeadline() const { return classSelectionDeadline; }
	void postponeClassSelection(std::int64_t deadline) { classSelectionDeadline = deadline; }

	// Crée le moteur et démarre le placement. Classe au hasard pour les joueurs qui n'ont rien
	// choisi ni regardé (jamais connectés), voir lockViewedClasses.
	void startBattle(std::int64_t nowMs, const std::map<tw::Player*, bool> & connected, const std::map<tw::Player*, std::string> & names);
	tw::battle::BattleEngine * getEngine() { return engine.get(); }

	void markEnded() { phase = Phase::ENDED; }

	// Zone à tenir : score à atteindre (0 : combat au KO). À régler avant le début du combat.
	void setZonePoints(int points) { zonePoints = points; }
	// Bonus sur la carte (orbes). À régler avant le début du combat.
	void setMapBonuses(bool enabled) { mapBonuses = enabled; }

	// Match de tournoi joué par cette session (0 : match amical).
	void setTournamentMatch(int tournamentId, int matchId) { this->tournamentId = tournamentId; this->tournamentMatchId = matchId; }
	int getTournamentId() const { return tournamentId; }
	int getTournamentMatchId() const { return tournamentMatchId; }
	std::uint32_t getSeed() const { return seed; }

	// Depuis quand toute une équipe (1 ou 2) est absente (0 : présente).
	std::int64_t absentSince[3] = { 0, 0, 0 };

	// Connexions des spectateurs de ce combat.
	std::set<tw::net::ConnId> spectators;

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
	std::map<tw::Player*, Draft> drafts;
	std::map<tw::Player*, tw::Player*> choosers;
	std::map<tw::Player*, std::vector<int>> spellChoices;
	std::map<tw::Player*, std::vector<std::string>> talentChoices;
	std::map<tw::Player*, std::string> appearances;
	int talentSlotsByTeam[3] = { 0, 0, 0 };
	bool banPhase = false;
	std::int64_t banDeadline = 0;
	int bans[3] = { 0, 0, 0 };	// Classe interdite par chaque équipe (1 et 2) à l'autre
	std::int64_t classSelectionDeadline;
	std::unique_ptr<tw::battle::BattleEngine> engine;
	std::uint32_t seed;
	int tournamentId = 0;
	int tournamentMatchId = 0;
	int zonePoints = 0;
	bool mapBonuses = false;
};
