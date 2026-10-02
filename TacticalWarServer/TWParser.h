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
#include <nlohmann/json.hpp>

class HttpFrontend;

class TWParser : public tw::net::NetHandler, tw::MatchEventListener
{
	tw::net::NetServer * net;
	std::map<tw::net::ConnId, ClientState*> clients;

	std::vector<tw::Environment*> environments;

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
	void handlePickClass(ClientState * client, tw::Player * player, int classId);
	void handleBattleAction(ClientState * client, const std::string & op, const nlohmann::json & body);
	void startBattle(BattleSession * session);
	void sendBattleState(BattleSession * session, ClientState * client, tw::Player * player, bool enterScreen);
	void broadcastBattleEvents(BattleSession * session);
	void finishBattle(BattleSession * session);
	void onPlayerConnectionChanged(tw::Player * player, bool connected);
	void tickBattles();
	void trackAbsences(BattleSession * session, std::int64_t now);

	// Tournois (TWParserTournament.cpp) :
	tw::TournamentService tournaments;
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
	void reportTournamentResult(BattleSession * session, int winnerSide, tw::tournament::ResultReason reason, double hpPercent1, double hpPercent2, int rounds);
	void finishWithoutBattle(BattleSession * session, int winnerSide, tw::tournament::ResultReason reason);
	void cancelSession(BattleSession * session);

	// Vue projetée (TWParserPublic.cpp) :
	HttpFrontend * http;
	bool publicDirty;
	std::int64_t lastPublicPublish;
	std::string displayNameOf(tw::Player * player);
	nlohmann::json publicStateJson();
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

