#include "ClientConfig.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace
{
	const char * CONFIG_PATH = "client.json";
}

ClientConfig & ClientConfig::get()
{
	static ClientConfig * instance = NULL;
	if (instance == NULL)
	{
		instance = new ClientConfig();
		instance->load();
	}
	return *instance;
}

void ClientConfig::load()
{
	std::ifstream file(CONFIG_PATH, std::ios::binary);
	if (!file)
	{
		save();
		return;
	}

	std::stringstream content;
	content << file.rdbuf();
	nlohmann::json json = nlohmann::json::parse(content.str(), nullptr, false);
	if (json.is_discarded() || !json.is_object())
	{
		std::cout << CONFIG_PATH << " invalide : valeurs par défaut utilisées." << std::endl;
		return;
	}

	serverHost = json.value("serverHost", serverHost);
	serverPort = json.value("serverPort", serverPort);
	soundInFile = json.value("sound", soundInFile);
	soundEnabled = soundInFile;

	for (const nlohmann::json & talent : json.value("talents", nlohmann::json::array()))
	{
		if (talent.is_string())
			talentChoice.push_back(talent.get<std::string>());
	}

	const nlohmann::json & spells = json.contains("spells") ? json["spells"] : nlohmann::json();
	if (spells.is_object())
	{
		for (auto it = spells.begin(); it != spells.end(); ++it)
		{
			if (!it.value().is_array())
				continue;
			std::vector<int> & choice = spellChoices[std::atoi(it.key().c_str())];
			for (const nlohmann::json & index : it.value())
			{
				if (index.is_number_integer())
					choice.push_back(index.get<int>());
			}
		}
	}
}

void ClientConfig::save() const
{
	nlohmann::json json = {
		{ "serverHost", serverHost },
		{ "serverPort", serverPort },
		{ "sound", soundInFile }
	};
	if (!talentChoice.empty())
		json["talents"] = talentChoice;
	if (!spellChoices.empty())
	{
		nlohmann::json spells = nlohmann::json::object();
		for (const auto & entry : spellChoices)
			spells[std::to_string(entry.first)] = entry.second;
		json["spells"] = spells;
	}

	std::ofstream file(CONFIG_PATH, std::ios::binary | std::ios::trunc);
	file << json.dump(2) << "\n";
}

void ClientConfig::applyCommandLine(int argc, char ** argv)
{
	for (int i = 1; i < argc; i++)
	{
		std::string arg = argv[i];
		bool hasValue = i + 1 < argc;

		if (arg == "--server" && hasValue)
		{
			if (!setServerAddress(argv[++i]))
				std::cout << "Adresse de serveur invalide : " << argv[i] << std::endl;
		}
		else if (arg == "--login" && hasValue)
		{
			autoLogin = argv[++i];
			autoConnect = true;
		}
		else if (arg == "--password" && hasValue)
		{
			autoPassword = argv[++i];
		}
		else if (arg == "--window" && hasValue)
		{
			std::string size = argv[++i];
			std::size_t x = size.find('x');
			if (x != std::string::npos)
			{
				windowWidth = (unsigned int)std::atoi(size.substr(0, x).c_str());
				windowHeight = (unsigned int)std::atoi(size.substr(x + 1).c_str());
			}
		}
		else if (arg == "--screenshot" && hasValue)
		{
			screenshotPath = argv[++i];
		}
		else if (arg == "--screenshot-after" && hasValue)
		{
			std::string value = argv[++i];
			screenshotAtEnd = value == "end";
			screenshotDelaySeconds = screenshotAtEnd ? 1e9f : (float)std::atof(value.c_str());
		}
		else if (arg == "--no-sound")
		{
			soundEnabled = false;
		}
		else if (arg == "--fx-gallery")
		{
			fxGallery = true;
		}
		else if (arg == "--fx-spell" && hasValue)
		{
			fxGallery = true;
			fxSpell = argv[++i];
		}
		else if (arg == "--fx-map" && hasValue)
		{
			fxGallery = true;
			fxMap = std::atoi(argv[++i]);
		}
		else if (arg == "--training")
		{
			training = true;
		}
		else if (arg == "--training-start")
		{
			training = trainingStart = true;
		}
		else if (arg == "--training-class" && hasValue)
		{
			training = trainingStart = true;
			trainingClass = std::atoi(argv[++i]);
		}
		else if (arg == "--training-map" && hasValue)
		{
			training = trainingStart = true;
			trainingMap = std::atoi(argv[++i]);
		}
		else if (arg == "--training-1v1")
		{
			training = trainingStart = true;
			trainingDuel = true;
		}
		else if (arg == "--training-talents" && hasValue)
		{
			training = trainingStart = true;
			trainingTalents = std::atoi(argv[++i]);
		}
		else if (arg == "--class-screen" && hasValue)
		{
			classScreenTalents = std::atoi(argv[++i]);
		}
		else if (arg == "--tutorial")
		{
			tutorial = true;
		}
		else if (arg == "--tutorial-step" && hasValue)
		{
			tutorial = true;
			tutorialStep = std::atoi(argv[++i]);
		}
		else if (arg == "--class-screen-ban" && hasValue)
		{
			classScreenBan = std::atoi(argv[++i]);
		}
		else if (arg == "--class-screen-forbidden" && hasValue)
		{
			classScreenForbidden = std::atoi(argv[++i]);
		}
		else if (arg == "--training-zone")
		{
			training = trainingStart = true;
			trainingZone = true;
		}
		else if (arg == "--training-autoplay")
		{
			training = trainingStart = true;
			trainingAutoplay = true;
		}
		else if (arg == "--spectator")
		{
			autoLogin.clear();
			autoPassword.clear();
			autoConnect = true;
		}
		else if (arg == "--director")
		{
			autoLogin.clear();
			autoPassword.clear();
			autoConnect = true;
			directorMode = true;
		}
	}
}

std::string ClientConfig::getServerAddress() const
{
	if (serverPort == 12345)
		return serverHost;
	return serverHost + ":" + std::to_string(serverPort);
}

bool ClientConfig::setServerAddress(const std::string & address)
{
	std::string host = address;
	unsigned short port = 12345;

	std::size_t colon = address.rfind(':');
	if (colon != std::string::npos)
	{
		host = address.substr(0, colon);
		int parsedPort = std::atoi(address.substr(colon + 1).c_str());
		if (parsedPort <= 0 || parsedPort > 65535)
			return false;
		port = (unsigned short)parsedPort;
	}

	if (host.empty())
		return false;

	serverHost = host;
	serverPort = port;
	return true;
}
