#include <atomic>
#include <cstdint>
#include <iostream>
#include <Windows.h>

#include <ServerConfig.h>
#include "TWParser.h"
#include "net/NetServer.h"

namespace
{
	std::atomic<bool> stopRequested(false);

	// Ctrl+C, fermeture de la console, déconnexion de session : arrêt propre de la boucle.
	BOOL WINAPI onConsoleEvent(DWORD eventType)
	{
		stopRequested = true;
		return TRUE;
	}
}

int main(int argc, char** argv)
{
	SetConsoleOutputCP(CP_UTF8);

	std::string log;
	tw::ServerConfig config = tw::ServerConfig::loadOrCreate("server.json", log);
	std::cout << log << std::endl;

	TWParser parser;

	tw::net::NetServerOptions options;
	options.port = (std::uint16_t)config.gamePort;
	options.keepaliveIntervalMs = config.keepaliveIntervalSeconds * 1000;
	options.keepaliveTimeoutMs = config.keepaliveTimeoutSeconds * 1000;

	tw::net::NetServer server(parser, options);
	parser.setNetServer(&server);

	std::string error;
	if (!server.start(error))
	{
		std::cerr << "Démarrage impossible : " << error << std::endl;
		return 1;
	}

	SetConsoleCtrlHandler(onConsoleEvent, TRUE);
	std::cout << "Serveur en écoute sur le port " << config.gamePort << " (Ctrl+C pour arrêter)." << std::endl;

	server.run(stopRequested);

	std::cout << "Arrêt du serveur." << std::endl;
	return 0;
}
