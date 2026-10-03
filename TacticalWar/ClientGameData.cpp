#include "ClientGameData.h"

#include <iostream>

ClientGameData & ClientGameData::get()
{
	static ClientGameData instance;
	return instance;
}

const tw::battle::GameData & ClientGameData::data()
{
	if (!loaded)
	{
		std::string error;
		loaded = gameData.loadFromFile("./assets/data/gamedata.json", error);
		if (!loaded)
			std::cout << error << std::endl;
	}
	return gameData;
}

bool ClientGameData::loadFromFile(const std::string & path, std::string & error)
{
	tw::battle::GameData loadedData;
	if (!loadedData.loadFromFile(path, error))
		return false;

	gameData = loadedData;
	loaded = true;
	return true;
}

bool ClientGameData::loadFromServer(const std::string & json)
{
	std::string error;
	tw::battle::GameData received;
	if (!received.loadFromJsonText(json, error))
	{
		std::cout << error << std::endl;
		return false;
	}

	gameData = received;
	loaded = true;
	return true;
}
