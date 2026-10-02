#pragma once

#include "ClientState.h"
#include "net/NetServer.h"
#include <Player.h>
#include <map>
#include <Battle.h>
#include <Match.h>
#include <Environment.h>
#include <CredentialSheet.h>
#include <ServerConfig.h>
#include <TeamStore.h>
#include <nlohmann/json.hpp>

class TWParser : public tw::net::NetHandler, tw::MatchEventListener, BattleEventListener
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
	std::map<tw::Player*, Battle*> playerToBattleMap;
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

	void notifyClassChoiceLocked(ClientState * c);

	int isTeamAvailableForMatchCreation(int teamId);

	bool everybodyReadyForBattle(tw::Match * m);
	void synchronizeBattleState(tw::Match * m, ClientState * c);
	void enterBattleState(tw::Match * m, ClientState * c);

	void notifyBattleState(ClientState * c, Battle * battle);
	void notifyReadyState(ClientState * c, int playerId, tw::Player * p);
	void notifyCharacterPositionChanged(ClientState * toNotify, int playerId, tw::Player * characterWhosePositionChanged);
	void notifyPlayerTurnToken(Battle * b, ClientState * c);
	void notifyActivePlayerPANumber(Battle * b, ClientState * c);
	void notifyActivePlayerPMNumber(Battle * b, ClientState * c);

	// Retourne true si le combat est terminé (le combat et les personnages sont alors détruits).
	bool checkBattleEnd(tw::Match * m, tw::Player * actingPlayer);
	bool isValidMovePath(tw::Match * m, tw::Player * p, const std::vector<tw::Point2D> & path);


	std::vector<tw::Point2D> calculateSpellZone(tw::BaseCharacterModel * character, int selectedSpell, tw::Match * match, tw::Environment * environment);




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

	void switchParticipantToBattleState(Battle * b);

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
	void handleTeamAdminMessage(ClientState * client, const std::string & op, const nlohmann::json & body);
	void sendTeamResult(ClientState * client, bool ok, const std::string & message, const std::map<std::string, std::string> & passwords = std::map<std::string, std::string>());

public:
	TWParser(const tw::ServerConfig & config);
	~TWParser();

	void setNetServer(tw::net::NetServer * net);

	// NetHandler implementation :
	virtual void onConnected(tw::net::ConnId id, const std::string & remoteAddress);
	virtual void onMessage(tw::net::ConnId id, const std::string & line);
	virtual void onDisconnected(tw::net::ConnId id);
	virtual void onTick(tw::net::Clock::time_point now);

	void kick(ClientState * client);

	// MatchEventListener implementation :
	virtual void onMatchStatusChanged(tw::Match * match, tw::MatchStatus oldStatus, tw::MatchStatus newStatus);

	// BattleEventListener implementation :
	virtual void onBattleStateChanged(tw::Match * m, BattleState state);
	virtual void onPlayerTurnStart(tw::Match * match, tw::Player * player);
};

