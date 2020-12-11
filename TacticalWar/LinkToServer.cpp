#include "LinkToServer.h"
#include <stdlib.h>
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
	buffer = new char[10240];

	maxRecv = 1024;
	effRecv = 0;
	bufIndex = 0;

	isConnected = false;
}

LinkToServer::~LinkToServer()
{
	delete buffer;
}

bool LinkToServer::Connect()
{
	sf::Socket::Status status = socket.connect("127.0.0.1", 12345);
	if (status != sf::Socket::Done)
	{
		// erreur...
		std::cout << "Erreur de connexion" << std::endl;
		isConnected = false;
		return false;
	}

	isConnected = true;
	return true;
}

bool LinkToServer::Disconnect()
{
	if(isConnected)
		socket.disconnect();
	isConnected = false;
	return true;
}

void LinkToServer::Send(sf::String sContent)
{
	sContent += "\n";
	std::basic_string<sf::Uint8> utf8Content = sContent.toUtf8();
	socket.send(utf8Content.c_str(), utf8Content.length());
}

/*
sf::String LinkToServer::Receive()
{
	sf::String receivedData = "";
	bool bReceivedSomething = false;

	while (!bReceivedSomething)
	{
		int recvEndIndex = -1;
		for (int i = 0; i < bufIndex; i++)
		{
			if (buffer[i] == '\n')
			{
				bReceivedSomething = true;
				recvEndIndex = i;
				break;
			}
			else
			{
				receivedData += buffer[i];
			}
		}

		if (bReceivedSomething)
		{
			std::cout << "Received " << receivedData.toAnsiString() << std::endl;
			std::cout << receivedData.getSize() << std::endl;

			for (int i = recvEndIndex + 1; i < 10240 && i < bufIndex; i++)
			{
				buffer[i - (recvEndIndex + 1)] = buffer[i];
			}

			bufIndex -= (recvEndIndex + 1);
		}
		else
		{
			receivedData = "";

			int bufCapability = 10240 - bufIndex;
			std::size_t maxToRecv = bufCapability < maxRecv ? bufCapability : maxRecv;
			socket.receive(&buffer[bufIndex], maxToRecv, effRecv);
			if (effRecv == 0)
				return "";
			bufIndex += effRecv;
		}
	}

	return receivedData;
}
*/

void LinkToServer::UpdateReceivedData()
{
	socket.setBlocking(false);
	
	sf::String receivedData = "";
	bool bReceivedSomething = false;

	int bufCapability = 10240 - bufIndex;
	std::size_t maxToRecv = bufCapability < maxRecv ? bufCapability : maxRecv;
	sf::Socket::Status status = socket.receive(&buffer[bufIndex], maxToRecv, effRecv);
	if (status == sf::Socket::Status::Disconnected)
	{
		notifyDisconnected();
	}
	else if (status == sf::Socket::Status::Done)
	{
		bufIndex += effRecv;

		do
		{
			receivedData = "";
			bReceivedSomething = false;
			int recvEndIndex = -1;
			for (int i = 0; i < bufIndex; i++)
			{
				if (buffer[i] == '\n')
				{
					bReceivedSomething = true;
					recvEndIndex = i;
					break;
				}
				else
				{
					receivedData += buffer[i];
				}
			}

			if (bReceivedSomething)
			{
				std::cout << "Received " << receivedData.toAnsiString() << std::endl;
				std::cout << receivedData.getSize() << std::endl;

				for (int i = recvEndIndex + 1; i < 10240 && i < bufIndex; i++)
				{
					buffer[i - (recvEndIndex + 1)] = buffer[i];
				}

				bufIndex -= (recvEndIndex + 1);

				notifyMessage(receivedData.toAnsiString());
			}
		} while (bReceivedSomething);
	}

	socket.setBlocking(true);
}