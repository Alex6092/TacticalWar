// Bot de test : TacticalWarBot.exe --login <login> --password <mdp> [--server hote:port] [--class <id>] [--delay <ms>] [--verbose]
// À lancer depuis le dossier du jeu (il lit les cartes dans ./assets/map).
#include <iostream>
#include <string>

#include "Bot.h"

int main(int argc, char ** argv)
{
	Bot::Options options;
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
		else if (arg == "--server" && hasValue)
		{
			std::string address = argv[++i];
			std::size_t colon = address.rfind(':');
			options.host = colon == std::string::npos ? address : address.substr(0, colon);
			if (colon != std::string::npos)
				options.port = std::atoi(address.substr(colon + 1).c_str());
		}
	}

	if (options.login.empty())
	{
		std::cerr << "Usage : TacticalWarBot.exe --login <login> --password <mdp> [--server hote:port] [--class <id>] [--delay <ms>] [--verbose]" << std::endl;
		return 1;
	}

	Bot bot(options);
	return bot.run();
}
