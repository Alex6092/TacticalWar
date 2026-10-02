#pragma once

#include "Parser.h"
#include "ParserEventListener.h"
#include "ThreadSafeQueue.h"
#include <Player.h>
#include <map>
#include <Battle.h>
#include <Match.h>
#include <Environment.h>
#include <Battle.h>

class TWParser : public Parser<ClientState>, tw::MatchEventListener, BattleEventListener
{
	std::vector<tw::Environment*> environments;

	void loadEnvironments();

	bool hasCompleteMessage(ClientState * client);
	std::string extractCompleteMessageFromBuffer(ClientState * client);
	std::vector<tw::Player*> players;
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


	bool initRandom;


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

public:
	TWParser();
	~TWParser();

	virtual void onClientConnected(ClientState * client);
	virtual void parse(ClientState * client, std::vector<unsigned char> & receivedPacket);
	virtual void parse(SOCKET sock, unsigned char * buf, int length);
	virtual void onClientDisconnected(SOCKET sock);
	virtual void onClientDisconnected(ClientState * client);

	void kick(ClientState * client);

	// MatchEventListener implementation :
	virtual void onMatchStatusChanged(tw::Match * match, tw::MatchStatus oldStatus, tw::MatchStatus newStatus);

	// BattleEventListener implementation :
	virtual void onBattleStateChanged(tw::Match * m, BattleState state);
	virtual void onPlayerTurnStart(tw::Match * match, tw::Player * player);
};

