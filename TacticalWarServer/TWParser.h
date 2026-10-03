#pragma once

#include "ClientState.h"
#include "net/NetServer.h"
#include <Player.h>
#include <map>
#include <Match.h>
#include "BattleSession.h"
#include <Environment.h>
#include <CredentialSheet.h>
#include <ServerConfig.h>
#include <TeamStore.h>
#include <TournamentService.h>
#include <ReplayStore.h>
#include <deque>
#include <memory>
#include <nlohmann/json.hpp>

class HttpFrontend;

class TWParser : public tw::net::NetHandler, tw::MatchEventListener
{
	tw::net::NetServer * net;
	std::map<tw::net::ConnId, ClientState*> clients;

	std::vector<tw::Environment*> environments;
	// Cartes tirées pour les matchs de tournoi (option "tournament" des cartes ; toutes si aucune).
	std::vector<tw::Environment*> tournamentEnvironments;
	// Message MP (carte au format v2 avec ses règles) par identifiant de carte.
	std::map<int, std::string> mapMessages;

	void loadEnvironments();

	tw::ServerConfig config;

	// Équipes et comptes (source de vérité, persistée dans data/teams.json) :
	tw::TeamStore teamStore;
	tw::CredentialSheet credentials;
	// true si teams.json n'a pas pu être lu : aucune modification n'est alors enregistrée.
	bool teamStoreReadOnly;

	// Tous les joueurs déjà créés (login -> joueur), y compris ceux d'équipes désactivées
	// ou supprimées, encore référencés par des matchs.
	std::map<std::string, tw::Player*> allPlayers;
	// Joueurs des équipes actives (login -> joueur) :
	std::map<std::string, tw::Player*> playersMap;
	std::map<tw::Player*, ClientState*> connectedPlayerMap;
	std::map<int, std::vector<tw::Player*>> teamIdToPlayerList;

	// Liste des clients en mode spectateur (pour mettre à jour la liste des match en cours) :
	std::vector<ClientState*> spectatorModeClientDiffusionList;

	void notifyPlayingMatchList(ClientState * c = NULL);
	void notifyMatchConnectedPlayerChanged(tw::Match * match);
	void notifyTeamList(ClientState * c);
	void notifyMatchCreated(tw::Match * m);
	void notifySwitchToClassSelectionToConnectedPlayer(std::vector<tw::Player*> team);
	void notifyPlanifiedAndPlayingMatch(ClientState * c);
	void notifyFinishedMatch(ClientState * c);


	int isTeamAvailableForMatchCreation(int teamId);









	ClientState * getClientStateFromPlayer(tw::Player * p)
	{
		ClientState * c = NULL;
		if (connectedPlayerMap.find(p) != connectedPlayerMap.end())
		{
			c = connectedPlayerMap[p];
		}
		return c;
	}

	tw::Player * getPlayerFromClientState(ClientState * c)
	{
		tw::Player * p = NULL;

		if (c->getPseudo().size() > 0)
		{
			if (playersMap.find(c->getPseudo()) != playersMap.end())
			{
				p = playersMap[c->getPseudo()];
			}
		}

		return p;
	}


	ClientState * admin;

	void sendToMatch(tw::Match * match, std::string str);
	void send(ClientState * client, const std::string & data);

	void handleMessage(ClientState * client, const std::string & toParse);
	bool isAuthorized(ClientState * client, const std::string & op);
	bool isAdminLoginAllowed(ClientState * client);

	// Gestion des équipes (TWParserTeams.cpp) :
	void loadTeams();
	bool saveTeams(std::string * error = nullptr);
	void rebuildPlayers();
	bool teamHasPendingMatch(int teamId);
	bool teamHasAnyMatch(int teamId);
	// Combats (TWParserBattle.cpp) :
	tw::battle::GameData gameData;
	std::string gameDataMessage;
	std::map<int, BattleSession*> sessions;
	int nextSessionId;
	std::int64_t nowMs() const;
	void createSession(tw::Match * match);
	BattleSession * sessionOfMatch(tw::Match * match);
	BattleSession * sessionOfPlayer(tw::Player * player);
	void sendGameData(ClientState * client);
	void handlePickClass(ClientState * client, tw::Player * player, const std::string & body);
	void handleBan(ClientState * client, tw::Player * player, const std::string & body);
	// Bannissement : BB{banned: classe interdite par l'équipe du joueur, forbidden: classe qui lui est
	// interdite, done: phase terminée}. Envoyé au retour d'un joueur si le match a un bannissement.
	void sendBanState(BattleSession * session, ClientState * client, tw::Player * player);
	void finishBanPhase(BattleSession * session);
	// Passage au choix de classe : HC{"talents": nombre de talents de tournoi à choisir}.
	std::string classSelectionMessage(tw::Player * player);
	void handleBattleAction(ClientState * client, const std::string & op, const nlohmann::json & body);
	// Signal d'un joueur à ses coéquipiers (CG -> BG), limité en cadence.
	void handlePing(ClientState * client, const nlohmann::json & body);
	std::map<tw::Player*, std::deque<std::int64_t>> recentPings;
	void startBattle(BattleSession * session);
	void sendBattleState(BattleSession * session, ClientState * client, tw::Player * player, bool enterScreen);
	void broadcastBattleEvents(BattleSession * session);
	void finishBattle(BattleSession * session);
	void onPlayerConnectionChanged(tw::Player * player, bool connected);
	void tickBattles();
	void trackAbsences(BattleSession * session, std::int64_t now);

	// Tournois (TWParserTournament.cpp) :
	tw::TournamentService tournaments;

	// Rediffusions (TWParserReplay.cpp) : enregistrement de chaque combat et relecture
	// pour les spectateurs (même flux de messages qu'un combat en direct).
	struct ReplayRecording
	{
		std::unique_ptr<tw::store::ReplayWriter> writer;
		std::int64_t startMs = 0;
	};
	struct ReplayPlayback
	{
		tw::store::Replay replay;
		std::vector<std::int64_t> due;	// Moment d'envoi de chaque lot (ms après le début)
		std::size_t next = 0;
		std::int64_t startMs = 0;
	};
	tw::store::ReplayLibrary replays;
	std::map<int, ReplayRecording> recordings;
	std::map<tw::net::ConnId, ReplayPlayback> playbacks;
	nlohmann::json battleSnapshot(BattleSession * session, int fighterId);
	void startRecording(BattleSession * session);
	void recordBatch(BattleSession * session, const nlohmann::json & batch);
	void stopRecording(BattleSession * session, const nlohmann::json & end, bool keep);
	void handleReplayMessage(ClientState * client, const std::string & op, const nlohmann::json & body);
	void stopPlayback(ClientState * client);
	void tickReplays(std::int64_t now);
	int adminWatchedTournament;
	void loadTournaments();
	std::string teamName(int teamId);
	nlohmann::json tournamentListJson();
	nlohmann::json tournamentStateJson(int id);
	void sendTournamentAck(ClientState * client, const std::string & error, const std::string & success, int id);
	void notifyTournamentsChanged();
	BattleSession * sessionOfTournamentMatch(int tournamentId, int matchId);
	void handleTournamentAdminMessage(ClientState * client, const std::string & op, const nlohmann::json & body);
	void dispatchTournamentMatches();
	void reportTournamentResult(BattleSession * session, int winnerSide, tw::tournament::ResultReason reason, double hpPercent1, double hpPercent2, int rounds,
		const std::vector<tw::tournament::PlayerRecord> & players = std::vector<tw::tournament::PlayerRecord>());
	void finishWithoutBattle(BattleSession * session, int winnerSide, tw::tournament::ResultReason reason);
	void cancelSession(BattleSession * session);

	// Vue projetée (TWParserPublic.cpp) :
	HttpFrontend * http;
	bool publicDirty;
	std::int64_t lastPublicPublish;
	std::string displayNameOf(tw::Player * player);
	nlohmann::json publicStateJson();
	// Derniers combats terminés (bilan et MVP), du plus récent au plus ancien.
	std::deque<nlohmann::json> recentBattles;
	void publishPublicState(bool force = false);

	// Mode spectateur (TWParserSpectator.cpp) :
	std::string lastSessionSignature;
	nlohmann::json sessionListJson();
	void notifySessionList(ClientState * only = NULL);
	void refreshSessionList();
	BattleSession * spectatedSession(ClientState * client);
	void removeSpectator(ClientState * client);
	void handleSpectatorMessage(ClientState * client, const std::string & op, const nlohmann::json & body);

	void handleTeamAdminMessage(ClientState * client, const std::string & op, const nlohmann::json & body);
	void sendTeamResult(ClientState * client, bool ok, const std::string & message, const std::map<std::string, std::string> & passwords = std::map<std::string, std::string>());

public:
	TWParser(const tw::ServerConfig & config);
	~TWParser();

	void setNetServer(tw::net::NetServer * net);
	void setHttpFrontend(HttpFrontend * http);

	// NetHandler implementation :
	virtual void onConnected(tw::net::ConnId id, const std::string & remoteAddress);
	virtual void onMessage(tw::net::ConnId id, const std::string & line);
	virtual void onDisconnected(tw::net::ConnId id);
	virtual void onTick(tw::net::Clock::time_point now);

	void kick(ClientState * client);

	// MatchEventListener implementation :
	virtual void onMatchStatusChanged(tw::Match * match, tw::MatchStatus oldStatus, tw::MatchStatus newStatus);

};

