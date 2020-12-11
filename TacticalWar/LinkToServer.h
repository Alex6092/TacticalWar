#pragma once

#include <SFML/Network.hpp>
#include "ServerMessageListener.h"
#include <vector>

class LinkToServer
{
private:
	LinkToServer();
	~LinkToServer();
	static LinkToServer * instance;

	sf::TcpSocket socket;

	char * buffer;

	std::size_t maxRecv;
	std::size_t effRecv;
	std::size_t bufIndex;

	std::vector<ServerMessageListener *> listeners;

	sf::Mutex mutex;
	bool isConnected;

	void notifyMessage(std::string msg)
	{
		mutex.lock();

		// Work on a copy, this way it avoid bugs when a listener
		// is removed during notification (if it unsubscribes from 
		// this objects event notifications)
		std::vector<ServerMessageListener*> cpy = listeners;
		mutex.unlock();	// Unlock happens here to avoid deadlock in previously described situation.

		for (int i = 0; i < cpy.size(); i++)
		{
			cpy[i]->onMessageReceived(msg);
		}
		
	}

	void notifyDisconnected()
	{
		mutex.lock();

		// Work on a copy, this way it avoid bugs when a listener
		// is removed during notification (if it unsubscribes from 
		// this objects event notifications)
		std::vector<ServerMessageListener*> cpy = listeners;
		mutex.unlock();	// Unlock happens here to avoid deadlock in previously described situation.

		for (int i = 0; i < cpy.size(); i++)
		{
			cpy[i]->onDisconnected();
		}
	}

public:
	static LinkToServer * getInstance();

	void addListener(ServerMessageListener * l)
	{
		mutex.lock();
		listeners.push_back(l);
		mutex.unlock();
	}

	void removeListener(ServerMessageListener * l)
	{
		mutex.lock();
		std::vector<ServerMessageListener *>::iterator it = std::find(listeners.begin(), listeners.end(), l);
		if (it != listeners.end())
		{
			listeners.erase(it);
		}
		mutex.unlock();
	}

	bool Connect();
	bool Disconnect();
	void Send(sf::String sContent);

	void UpdateReceivedData();

	//sf::String Receive();
};

