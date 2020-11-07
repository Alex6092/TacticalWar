#include "TWParser.h"
#include <iostream>

#include "TcpServer.h"
#include "StringUtils.h"
#include <PlayerManager.h>
#include <Match.h>

TWParser::TWParser()
{
	players = tw::PlayerManager::loadPlayers();

	// Construct player pseudo to player data map :
	for (int i = 0; i < players.size(); i++)
	{
		playersMap[players[i]->getPseudo()] = players[i];
	}
}


TWParser::~TWParser()
{
}

bool TWParser::hasCompleteMessage(ClientState * client)
{
	bool result = false;
	std::deque<unsigned char> & buffer = client->getBuffer();

	for (int i = 0; i < buffer.size(); i++)
	{
		if (buffer[i] == '\n')
		{
			result = true;
			break;
		}
	}

	return result;
}

std::string TWParser::extractCompleteMessageFromBuffer(ClientState * client)
{
	std::string result;
	int i = 0;
	std::deque<unsigned char> & buffer = client->getBuffer();
	
	while (buffer.front() != '\n')
	{
		result += buffer.front();
		buffer.pop_front();
	}

	buffer.pop_front();	// To remove the '\n' from the buffer.

	return result;
}

void TWParser::parse(ClientState * client, std::vector<unsigned char> & receivedPacket)
{
	std::deque<unsigned char> & buffer = client->getBuffer();

	for (int i = 0; i < receivedPacket.size(); i++)
	{
		unsigned char c = receivedPacket[i];
		buffer.push_back(c);
	}

	// Impl�mentation du protocole :
	if (hasCompleteMessage(client))
	{
		std::string toParse = extractCompleteMessageFromBuffer(client);

		if (StringUtils::startsWith(toParse, "HG"))
		{
			std::string payload = toParse.substr(2);

			std::vector<std::string> data = StringUtils::explode(payload, ';');
			
			std::string pseudo = data[0];
			std::string password = data[1];

			if (playersMap.find(pseudo) != playersMap.end())
			{
				tw::Player * p = playersMap[pseudo];

				if (password == p->getPassword())
				{
					std::cout << "Connexion du joueur " << pseudo.c_str() << std::endl;

					// Si compte d�j� utilis� : d�connexion du client pr�c�dent
					if (connectedPlayerMap.find(p) != connectedPlayerMap.end())
					{
						std::cout << "Compte deja utilise, kick du client precedent" << std::endl;
						// Kick :
						kick(connectedPlayerMap[p]);
					}

					client->setPseudo(pseudo);
					connectedPlayerMap[p] = client;

					tw::Match * match = tw::PlayerManager::getCurrentOrNextMatchForPlayer(p);
					if (match->getStatus() == tw::MatchStatus::STARTED)
					{
						// Retour en jeu (reconnexion en combat)
						Battle * b = (Battle*)match->getBattlePayload();
						// TODO : Notify that the player is back.
					}
					else
					{
						// D�tail du match � venir : proposer de rejoindre le match
					}
				}
			}
			//TcpServer<TWParser, ClientState>::Send(client, (char*)"Hello\n", 6);
		}

		/*
		long sentenceLength = buffer.size();
		unsigned char * sentence = new unsigned char[sentenceLength];

		for (int i = 0; i < sentenceLength; i++)
		{
			sentence[i] = buffer[i];
		}

		TcpServer<TWParser, ClientState>::Send(client, (char*)sentence, sentenceLength);
		delete sentence;
		buffer.clear();
		*/
	}
}

void TWParser::parse(SOCKET sock, unsigned char * buf, int length)
{
	Parser<ClientState>::parse(sock, buf, length);
}

void TWParser::kick(ClientState * client)
{
	// TODO : Notify disconnection to other clients (if in battle)
	notifyKick(client);
}

void TWParser::onClientConnected(ClientState * client)
{
	std::cout << "Client connecte" << std::endl;
	Parser<ClientState>::onClientConnected(client);
}

void TWParser::onClientDisconnected(SOCKET sock)
{
	Parser<ClientState>::onClientDisconnected(sock);
}

void TWParser::onClientDisconnected(ClientState * client)
{
	// Clear connected player map :
	if (client->getPseudo().length() > 0 && playersMap.find(client->getPseudo()) != playersMap.end())
	{
		tw::Player * p = playersMap[client->getPseudo()];
		
		if (playerToBattleMap.find(p) != playerToBattleMap.end())
		{
			std::cout << "Notify battle that connection is lost with " << p->getPseudo().c_str() << std::endl;
			
			// TODO : Notify battle that the connection with the player is lost
			//playerToBattleMap[p]->connectionLostWith(p);
		}

		connectedPlayerMap.erase(p);
		std::cout << "Client " << p->getPseudo().c_str() << " disconnected ..." << std::endl;
	}
	Parser<ClientState>::onClientDisconnected(client);	
}