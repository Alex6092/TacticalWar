#include <atomic>
#include <cstdint>
#include <iostream>
#include <Windows.h>

#include <PasswordHasher.h>
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

	const char * configPath = "server.json";
	std::string log;
	tw::ServerConfig config = tw::ServerConfig::loadOrCreate(configPath, log);
	std::cout << log << std::endl;

	// TacticalWarServer.exe --set-admin-password <mot de passe>
	if (argc >= 2 && std::string(argv[1]) == "--set-admin-password")
	{
		if (argc < 3 || std::string(argv[2]).size() < 4)
		{
			std::cerr << "Usage : TacticalWarServer.exe --set-admin-password <mot de passe (4 caractères minimum)>" << std::endl;
			return 1;
		}

		config.admin.passwordHash = tw::PasswordHasher::hash(argv[2]);
		if (!config.save(configPath))
		{
			std::cerr << "Impossible d'enregistrer " << configPath << std::endl;
			return 1;
		}
		std::cout << "Mot de passe admin enregistré dans " << configPath << "." << std::endl;
		return 0;
	}

	// Premier lancement : un mot de passe admin est généré et affiché une seule fois.
	if (config.admin.passwordHash.empty())
	{
		std::string password = tw::PasswordHasher::generatePassword(8);
		config.admin.passwordHash = tw::PasswordHasher::hash(password);
		config.save(configPath);

		std::cout << std::endl
			<< "=====================================================" << std::endl
			<< " Compte admin : " << config.admin.login << " / " << password << std::endl
			<< " (notez-le : il ne sera plus affiché. Pour le changer :" << std::endl
			<< "  TacticalWarServer.exe --set-admin-password <mdp>)" << std::endl
			<< "=====================================================" << std::endl << std::endl;
	}

	TWParser parser(config);

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
