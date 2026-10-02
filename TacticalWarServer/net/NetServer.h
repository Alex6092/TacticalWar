#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace tw
{
	namespace net
	{
		// Identifiant d'une connexion. Il n'est jamais réutilisé pendant la vie du serveur,
		// contrairement aux handles de socket.
		typedef std::uint32_t ConnId;

		typedef std::chrono::steady_clock Clock;

		// Reçoit les événements réseau. Tous les appels sont faits depuis le thread
		// qui exécute NetServer::run (le thread de jeu) : aucun verrou n'est nécessaire.
		class NetHandler
		{
		public:
			virtual ~NetHandler() {}
			virtual void onConnected(ConnId id, const std::string & remoteAddress) = 0;
			// Une ligne complète reçue, sans le '\n' final. Les keepalive ne sont pas transmis.
			virtual void onMessage(ConnId id, const std::string & line) = 0;
			virtual void onDisconnected(ConnId id) = 0;
			// Appelé régulièrement (environ toutes les tickIntervalMs) pour les minuteurs.
			virtual void onTick(Clock::time_point now) = 0;
		};

		struct NetServerOptions
		{
			std::uint16_t port = 12345;
			int tickIntervalMs = 50;
			int keepaliveIntervalMs = 10000;
			int keepaliveTimeoutMs = 30000;
			std::size_t maxPendingOutputBytes = 1024 * 1024;
			int maxConnections = 400;
		};

		// Serveur TCP mono-thread à base de select() et de sockets non bloquantes.
		// Chaque connexion a un tampon d'envoi : un client lent ne bloque jamais les autres.
		class NetServer
		{
		public:
			NetServer(NetHandler & handler, const NetServerOptions & options);
			~NetServer();

			bool start(std::string & error);

			// Boucle principale ; rend la main quand stopRequested passe à true.
			void run(const std::atomic<bool> & stopRequested);

			// Met des données en file d'envoi (le message doit déjà contenir son '\n').
			void send(ConnId id, const std::string & data);

			// Ferme la connexion après avoir tenté d'envoyer les données en attente.
			// onDisconnected est appelé plus tard, depuis la boucle (jamais pendant l'appel).
			void close(ConnId id);

			std::string remoteAddress(ConnId id) const;
			std::size_t connectionCount() const { return connections.size(); }

		private:
			struct Connection;

			void acceptConnections(Clock::time_point now);
			void receive(Connection & connection, Clock::time_point now);
			void flush(Connection & connection, Clock::time_point now);
			void handleKeepalive(Connection & connection, Clock::time_point now);
			void destroyClosedConnections();

			NetHandler & handler;
			NetServerOptions options;
			std::uintptr_t listeningSocket;
			ConnId nextId;
			std::map<ConnId, std::unique_ptr<Connection>> connections;
			bool winsockStarted;
		};
	}
}
