#pragma once

#include "Parser.h"
#include "ParserEventListener.h"
#include "ThreadSafeQueue.h"
#include <Player.h>
#include <map>
#include <Battle.h>
#include <Match.h>
#include <Environment.h>

class ModbusOperation;

class TWParser : public Parser<ClientState>, tw::MatchEventListener
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

	void notifyClassChoiceLocked(ClientState * c);

	int isTeamAvailableForMatchCreation(int teamId);

	ClientState * admin;

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
};

