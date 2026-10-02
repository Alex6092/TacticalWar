// Bot de test : TacticalWarBot.exe --login <login> --password <mdp> [--server hote:port] [--class <id>] [--delay <ms>] [--verbose]
// Simulation d'équilibrage (sans serveur) : TacticalWarBot.exe --simulate <combats> [--map <id>] [--seed <n>] [--data <gamedata.json>]
// À lancer depuis le dossier du jeu (il lit les cartes dans ./assets/map).
#include <cstdlib>
#include <iostream>
#include <string>

#include "Bot.h"
#include "Simulation.h"

int main(int argc, char ** argv)
{
	Bot::Options options;
	int simulate = 0;
	int simulationMap = 0;
	std::uint32_t seed = 1;
	std::string dataPath = "./assets/data/gamedata.json";
	for (int i = 1; i < argc; i++)
	{
		std::string arg = argv[i];
		bool hasValue = i + 1 < argc;

		if (arg == "--login" && hasValue)
			options.login = argv[++i];
		else if (arg == "--password" && hasValue)
			options.password = argv[++i];
		else if (arg == "--class" && hasValue)
			options.classId = std::atoi(argv[++i]);
		else if (arg == "--delay" && hasValue)
			options.actionDelayMs = std::atoi(argv[++i]);
		else if (arg == "--verbose")
			options.verbose = true;
		else if (arg == "--simulate" && hasValue)
			simulate = std::atoi(argv[++i]);
		else if (arg == "--map" && hasValue)
			simulationMap = std::atoi(argv[++i]);
		else if (arg == "--data" && hasValue)
			dataPath = argv[++i];
		else if (arg == "--seed" && hasValue)
			seed = (std::uint32_t)std::strtoul(argv[++i], nullptr, 10);
		else if (arg == "--server" && hasValue)
		{
			std::string address = argv[++i];
			std::size_t colon = address.rfind(':');
			options.host = colon == std::string::npos ? address : address.substr(0, colon);
			if (colon != std::string::npos)
				options.port = std::atoi(address.substr(colon + 1).c_str());
		}
	}

	if (simulate > 0)
		return runSimulation(simulate, simulationMap, seed, dataPath);

	if (options.login.empty())
	{
		std::cerr << "Usage : TacticalWarBot.exe --login <login> --password <mdp> [--server hote:port] [--class <id>] [--delay <ms>] [--verbose]" << std::endl;
		std::cerr << "        TacticalWarBot.exe --simulate <combats> [--map <id>] [--seed <n>] [--data <gamedata.json>]" << std::endl;
		return 1;
	}

	Bot bot(options);
	return bot.run();
}
