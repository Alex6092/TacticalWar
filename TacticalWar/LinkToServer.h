#pragma once
#include <SFML/Network.hpp>
#include "ServerMessageListener.h"
#include <LineFramer.h>
#include <vector>
#include <string>

class LinkToServer
{
private:
	LinkToServer();
	~LinkToServer();

	static LinkToServer * instance;

	sf::TcpSocket socket;
	tw::protocol::LineFramer framer;

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

	// Se connecte au serveur configuré dans client.json (timeout de 3 secondes).
	bool Connect();
	bool Disconnect();

	// Envoie un message (le '\n' est ajouté). Le texte est envoyé en UTF-8.
	void Send(sf::String sContent);
	// Envoie une ligne déjà encodée en UTF-8 (le '\n' est ajouté).
	void SendRaw(const std::string & utf8Line);

	// Lit les données reçues et transmet chaque message complet (en UTF-8) aux listeners.
	void UpdateReceivedData();
};

// Convertit un texte UTF-8 reçu du serveur en sf::String pour l'affichage.
inline sf::String fromServerText(const std::string & utf8)
{
	return sf::String::fromUtf8(utf8.begin(), utf8.end());
}
