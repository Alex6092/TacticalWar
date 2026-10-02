#include "LinkToServer.h"
#include "ClientConfig.h"
#include <Opcodes.h>
#include <iostream>

LinkToServer * LinkToServer::instance = NULL;

LinkToServer * LinkToServer::getInstance()
{
	if (instance == NULL)
		instance = new LinkToServer();

	return instance;
}

LinkToServer::LinkToServer()
{
	isConnected = false;
}

LinkToServer::~LinkToServer()
{
}

bool LinkToServer::Connect()
{
	const ClientConfig & config = ClientConfig::get();

	socket.setBlocking(true);
	sf::Socket::Status status = socket.connect(config.serverHost, config.serverPort, sf::seconds(3));
	if (status != sf::Socket::Done)
	{
		std::cout << "Erreur de connexion a " << config.serverHost << ":" << config.serverPort << std::endl;
		isConnected = false;
		return false;
	}

	framer.clear();
	isConnected = true;
	return true;
}

bool LinkToServer::Disconnect()
{
	if(isConnected)
		socket.disconnect();

	framer.clear();
	isConnected = false;
	return true;
}

void LinkToServer::Send(sf::String sContent)
{
	std::basic_string<sf::Uint8> utf8Content = sContent.toUtf8();
	SendRaw(std::string(utf8Content.begin(), utf8Content.end()));
}

void LinkToServer::SendRaw(const std::string & utf8Line)
{
	std::string data = utf8Line + "\n";
	socket.send(data.c_str(), data.length());
}

void LinkToServer::UpdateReceivedData()
{
	if (!isConnected)
		return;

	bool disconnected = false;
	char chunk[16 * 1024];
	std::size_t received = 0;

	socket.setBlocking(false);
	while (true)
	{
		sf::Socket::Status status = socket.receive(chunk, sizeof(chunk), received);
		if (status == sf::Socket::Done)
		{
			if (!framer.feed(chunk, received))
			{
				std::cout << "Message trop long recu du serveur : deconnexion." << std::endl;
				disconnected = true;
				break;
			}
		}
		else
		{
			if (status == sf::Socket::Disconnected || status == sf::Socket::Error)
				disconnected = true;
			break;
		}
	}
	socket.setBlocking(true);

	std::string line;
	while (framer.nextLine(line))
	{
		// Keepalive : répondu ici, jamais transmis aux écrans.
		if (line == tw::protocol::KEEPALIVE_PING)
		{
			SendRaw(tw::protocol::KEEPALIVE_PONG);
			continue;
		}

		notifyMessage(line);
	}

	if (disconnected)
	{
		Disconnect();
		notifyDisconnected();
	}
}
