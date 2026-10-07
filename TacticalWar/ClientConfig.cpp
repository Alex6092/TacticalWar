#include "ClientConfig.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>
#include <Palette.h>

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
	colorblind = json.value("colorblind", colorblind);
	textScale = json.value("textScale", textScale);
	if (textScale != 115 && textScale != 130)
		textScale = 100;
	turnAlert = json.value("turnAlert", turnAlert);
	fullscreenInFile = json.value("fullscreen", fullscreenInFile);
	fullscreen = fullscreenInFile;
	seeThrough = json.value("seeThrough", seeThrough);
	tw::palette::setColorblind(colorblind);

	for (const nlohmann::json & talent : json.value("talents", nlohmann::json::array()))
	{
		if (talent.is_string())
			talentChoice.push_back(talent.get<std::string>());
	}
	for (const nlohmann::json & puzzleId : json.value("puzzles", nlohmann::json::array()))
	{
		if (puzzleId.is_string())
			solvedPuzzles.insert(puzzleId.get<std::string>());
	}
	for (const nlohmann::json & id : json.value("appearances", nlohmann::json::array()))
	{
		if (id.is_string())
			knownAppearances.push_back(id.get<std::string>());
	}
	appearance = json.value("appearance", std::string());
	preferredClass = json.value("preferredClass", 0);

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
		{ "sound", soundInFile },
		{ "colorblind", colorblind },
		{ "textScale", textScale },
		{ "turnAlert", turnAlert },
		{ "fullscreen", fullscreenInFile },
		{ "seeThrough", seeThrough }
	};
	if (!talentChoice.empty())
		json["talents"] = talentChoice;
	if (!solvedPuzzles.empty())
		json["puzzles"] = solvedPuzzles;
	if (!knownAppearances.empty())
		json["appearances"] = knownAppearances;
	if (!appearance.empty())
		json["appearance"] = appearance;
	if (preferredClass != 0)
		json["preferredClass"] = preferredClass;
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
		else if (arg == "--windowed")
		{
			fullscreen = false;
		}
		else if (arg == "--frame-stats")
		{
			frameStats = true;
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
		else if (arg == "--training-duo-control")
		{
			training = trainingStart = true;
			trainingDuoControl = true;
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
		else if (arg == "--puzzles")
		{
			puzzleList = true;
		}
		else if (arg == "--puzzle" && hasValue)
		{
			puzzle = std::atoi(argv[++i]);
		}
		else if (arg == "--puzzle-demo")
		{
			puzzleDemo = true;
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
		else if (arg == "--class-screen-solo")
		{
			classScreenSolo = true;
		}
		else if (arg == "--class-screen-solo-done")
		{
			classScreenSolo = classScreenSoloDone = true;
		}
		else if (arg == "--class-screen-seconds" && hasValue)
		{
			classScreenSeconds = std::atoi(argv[++i]);
		}
		else if (arg == "--class-screen-chosen-by")
		{
			classScreenChosenBy = true;
		}
		else if (arg == "--class-screen-alone" && hasValue)
		{
			classScreenAlone = std::atoi(argv[++i]);
		}
		else if (arg == "--admin-tab" && hasValue)
		{
			adminTab = std::atoi(argv[++i]);
		}
		else if (arg == "--class-screen-mate" && hasValue)
		{
			classScreenMate = std::atoi(argv[++i]);
		}
		else if (arg == "--class-screen-team" && hasValue)
		{
			classScreenTeam = std::atoi(argv[++i]);
		}
		else if (arg == "--class-screen-forbidden" && hasValue)
		{
			classScreenForbidden = std::atoi(argv[++i]);
		}
		else if (arg == "--training-difficulty" && hasValue)
		{
			training = true;
			trainingDifficulty = argv[++i];
		}
		else if (arg == "--training-bonuses")
		{
			training = true;
			trainingBonuses = true;
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
		else if (arg == "--options")
		{
			openOptions = true;
		}
		else if (arg == "--help-panel")
		{
			openHelp = true;
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
