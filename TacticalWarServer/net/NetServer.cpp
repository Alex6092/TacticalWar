// select() est limité à FD_SETSIZE sockets par ensemble (64 par défaut sous Windows).
#define FD_SETSIZE 512

#include <WinSock2.h>
#include <WS2tcpip.h>

#include "NetServer.h"

#include <iostream>
#include <vector>

#include <LineFramer.h>
#include <Opcodes.h>

#pragma comment(lib, "ws2_32.lib")

using namespace tw::net;

namespace
{
	const std::string KEEPALIVE_PING_LINE = std::string(tw::protocol::KEEPALIVE_PING) + "\n";

	// Délai maximal pour envoyer les données en attente d'une connexion en cours de fermeture.
	const auto CLOSE_FLUSH_DELAY = std::chrono::milliseconds(1000);
}

struct NetServer::Connection
{
	SOCKET socket = INVALID_SOCKET;
	ConnId id = 0;
	std::string remoteAddress;
	tw::protocol::LineFramer framer;
	std::string outgoing;
	std::size_t outgoingOffset = 0;
	Clock::time_point lastReceive;
	Clock::time_point lastSend;

	// Fermeture demandée : on essaie encore d'envoyer les données en attente.
	bool closing = false;
	Clock::time_point closeDeadline;
	// Connexion morte (erreur, déconnexion du client) : à détruire immédiatement.
	bool dead = false;

	std::size_t pendingOutput() const { return outgoing.size() - outgoingOffset; }
};

NetServer::NetServer(NetHandler & handler, const NetServerOptions & options)
	: handler(handler), options(options), listeningSocket(INVALID_SOCKET), nextId(1), winsockStarted(false)
{
	if (this->options.maxConnections > FD_SETSIZE - 1)
		this->options.maxConnections = FD_SETSIZE - 1;
}

NetServer::~NetServer()
{
	for (auto & entry : connections)
		closesocket(entry.second->socket);
	connections.clear();

	if (listeningSocket != INVALID_SOCKET)
		closesocket((SOCKET)listeningSocket);

	if (winsockStarted)
		WSACleanup();
}

bool NetServer::start(std::string & error)
{
	WSADATA wsData;
	if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0)
	{
		error = "WSAStartup a échoué";
		return false;
	}
	winsockStarted = true;

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		error = "Impossible de créer la socket d'écoute";
		return false;
	}

	sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_port = htons(options.port);
	address.sin_addr.S_un.S_addr = INADDR_ANY;

	if (bind(sock, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
	{
		error = "Le port " + std::to_string(options.port) + " est déjà utilisé (un autre serveur est-il lancé ?)";
		closesocket(sock);
		return false;
	}

	if (listen(sock, SOMAXCONN) == SOCKET_ERROR)
	{
		error = "listen() a échoué";
		closesocket(sock);
		return false;
	}

	u_long nonBlocking = 1;
	ioctlsocket(sock, FIONBIO, &nonBlocking);

	listeningSocket = (std::uintptr_t)sock;
	return true;
}

void NetServer::run(const std::atomic<bool> & stopRequested)
{
	Clock::time_point nextTick = Clock::now();

	while (!stopRequested)
	{
		fd_set readSet;
		fd_set writeSet;
		FD_ZERO(&readSet);
		FD_ZERO(&writeSet);
		FD_SET((SOCKET)listeningSocket, &readSet);

		for (auto & entry : connections)
		{
			Connection & connection = *entry.second;
			if (connection.dead)
				continue;

			if (!connection.closing)
				FD_SET(connection.socket, &readSet);
			if (connection.pendingOutput() > 0)
				FD_SET(connection.socket, &writeSet);
		}

		Clock::time_point now = Clock::now();
		long long waitMs = std::chrono::duration_cast<std::chrono::milliseconds>(nextTick - now).count();
		if (waitMs < 0)
			waitMs = 0;
		if (waitMs > options.tickIntervalMs)
			waitMs = options.tickIntervalMs;

		timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = (long)(waitMs * 1000);

		int ready = select(0, &readSet, &writeSet, nullptr, &timeout);
		now = Clock::now();

		if (ready == SOCKET_ERROR)
		{
			std::cerr << "select() a échoué : " << WSAGetLastError() << std::endl;
			Sleep(10);
		}
		else if (ready > 0)
		{
			if (FD_ISSET((SOCKET)listeningSocket, &readSet))
				acceptConnections(now);

			// Les handlers peuvent fermer des connexions ou envoyer des données pendant le
			// traitement : on travaille sur une copie des identifiants.
			std::vector<ConnId> ids;
			for (auto & entry : connections)
				ids.push_back(entry.first);

			for (ConnId id : ids)
			{
				auto it = connections.find(id);
				if (it == connections.end())
					continue;

				Connection & connection = *it->second;
				if (!connection.dead && FD_ISSET(connection.socket, &readSet))
					receive(connection, now);
				if (!connection.dead && FD_ISSET(connection.socket, &writeSet))
					flush(connection, now);
			}
		}

		for (auto & entry : connections)
			handleKeepalive(*entry.second, now);

		if (now >= nextTick)
		{
			handler.onTick(now);
			nextTick = now + std::chrono::milliseconds(options.tickIntervalMs);
		}

		destroyClosedConnections();
	}
}

void NetServer::acceptConnections(Clock::time_point now)
{
	while (true)
	{
		sockaddr_in address = {};
		int addressLength = sizeof(address);
		SOCKET client = accept((SOCKET)listeningSocket, (sockaddr*)&address, &addressLength);
		if (client == INVALID_SOCKET)
			return;

		if ((int)connections.size() >= options.maxConnections)
		{
			std::cerr << "Connexion refusée : nombre maximal de connexions atteint." << std::endl;
			closesocket(client);
			continue;
		}

		u_long nonBlocking = 1;
		ioctlsocket(client, FIONBIO, &nonBlocking);

		BOOL noDelay = TRUE;
		setsockopt(client, IPPROTO_TCP, TCP_NODELAY, (const char*)&noDelay, sizeof(noDelay));

		char ip[INET_ADDRSTRLEN] = {};
		inet_ntop(AF_INET, &address.sin_addr, ip, sizeof(ip));

		std::unique_ptr<Connection> connection(new Connection());
		connection->socket = client;
		connection->id = nextId++;
		connection->remoteAddress = ip;
		connection->lastReceive = now;
		connection->lastSend = now;

		ConnId id = connection->id;
		std::string remote = connection->remoteAddress;
		connections[id] = std::move(connection);

		handler.onConnected(id, remote);
	}
}

void NetServer::receive(Connection & connection, Clock::time_point now)
{
	char buffer[16 * 1024];

	while (!connection.dead)
	{
		int received = recv(connection.socket, buffer, sizeof(buffer), 0);
		if (received > 0)
		{
			connection.lastReceive = now;
			if (!connection.framer.feed(buffer, received))
			{
				std::cerr << "Connexion " << connection.id << " fermée : message trop long." << std::endl;
				connection.dead = true;
				break;
			}
		}
		else if (received == 0)
		{
			connection.dead = true;
		}
		else
		{
			if (WSAGetLastError() != WSAEWOULDBLOCK)
				connection.dead = true;
			break;
		}
	}

	// Transmet les messages complets, même si la connexion vient de se fermer :
	// le client a pu envoyer un dernier message avant de partir.
	ConnId id = connection.id;
	std::string line;
	while (connection.framer.nextLine(line))
	{
		if (line == tw::protocol::KEEPALIVE_PONG)
			continue;

		handler.onMessage(id, line);

		// Le handler peut avoir demandé la fermeture de cette connexion.
		auto it = connections.find(id);
		if (it == connections.end() || it->second->closing)
			break;
	}
}

void NetServer::flush(Connection & connection, Clock::time_point now)
{
	while (connection.pendingOutput() > 0)
	{
		std::size_t remaining = connection.pendingOutput();
		int toSend = remaining > 64 * 1024 ? 64 * 1024 : (int)remaining;
		int sent = ::send(connection.socket, connection.outgoing.data() + connection.outgoingOffset, toSend, 0);

		if (sent > 0)
		{
			connection.outgoingOffset += sent;
			connection.lastSend = now;
		}
		else
		{
			if (sent == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
				connection.dead = true;
			break;
		}
	}

	if (connection.pendingOutput() == 0)
	{
		connection.outgoing.clear();
		connection.outgoingOffset = 0;
	}
	else if (connection.outgoingOffset > 256 * 1024)
	{
		connection.outgoing.erase(0, connection.outgoingOffset);
		connection.outgoingOffset = 0;
	}
}

void NetServer::handleKeepalive(Connection & connection, Clock::time_point now)
{
	if (connection.dead || connection.closing)
		return;

	auto silence = std::chrono::duration_cast<std::chrono::milliseconds>(now - connection.lastReceive).count();
	if (silence > options.keepaliveTimeoutMs)
	{
		std::cerr << "Connexion " << connection.id << " (" << connection.remoteAddress << ") fermée : plus de réponse." << std::endl;
		connection.dead = true;
		return;
	}

	auto idle = std::chrono::duration_cast<std::chrono::milliseconds>(now - connection.lastSend).count();
	if (idle > options.keepaliveIntervalMs)
		send(connection.id, KEEPALIVE_PING_LINE);
}

void NetServer::send(ConnId id, const std::string & data)
{
	auto it = connections.find(id);
	if (it == connections.end())
		return;

	Connection & connection = *it->second;
	if (connection.dead)
		return;

	connection.outgoing += data;

	if (connection.pendingOutput() > options.maxPendingOutputBytes)
	{
		std::cerr << "Connexion " << id << " fermée : le client ne lit plus ses messages." << std::endl;
		connection.dead = true;
		return;
	}

	flush(connection, Clock::now());
}

void NetServer::close(ConnId id)
{
	auto it = connections.find(id);
	if (it == connections.end())
		return;

	Connection & connection = *it->second;
	if (!connection.closing)
	{
		connection.closing = true;
		connection.closeDeadline = Clock::now() + CLOSE_FLUSH_DELAY;
	}
}

std::string NetServer::remoteAddress(ConnId id) const
{
	auto it = connections.find(id);
	return it == connections.end() ? std::string() : it->second->remoteAddress;
}

void NetServer::destroyClosedConnections()
{
	Clock::time_point now = Clock::now();
	std::vector<ConnId> toDestroy;

	for (auto & entry : connections)
	{
		Connection & connection = *entry.second;
		bool flushedOrExpired = connection.pendingOutput() == 0 || now >= connection.closeDeadline;
		if (connection.dead || (connection.closing && flushedOrExpired))
			toDestroy.push_back(entry.first);
	}

	for (ConnId id : toDestroy)
	{
		auto it = connections.find(id);
		if (it == connections.end())
			continue;

		closesocket(it->second->socket);
		connections.erase(it);

		// Appelé après la suppression : un send() vers cette connexion est ignoré.
		handler.onDisconnected(id);
	}
}
