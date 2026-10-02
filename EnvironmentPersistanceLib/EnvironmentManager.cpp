#include "pch.h"
#include "EnvironmentManager.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <nlohmann/json.hpp>
#include <TileRegistry.h>

// fs::u8path est déprécié en C++20 mais reste le moyen simple de passer un chemin UTF-8.
#pragma warning(disable : 4996)

namespace fs = std::filesystem;
using nlohmann::json;

tw::EnvironmentManager * tw::EnvironmentManager::instance = NULL;

namespace
{
	bool readFile(const std::string & path, std::string & content)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
			return false;
		std::stringstream buffer;
		buffer << file.rdbuf();
		content = buffer.str();
		// BOM UTF-8 éventuel.
		if (content.size() >= 3 && content.compare(0, 3, "\xEF\xBB\xBF") == 0)
			content.erase(0, 3);
		return true;
	}

	std::string withSlash(const std::string & directory)
	{
		if (directory.empty())
			return "./";
		char last = directory.back();
		return (last == '/' || last == '\\') ? directory : directory + "/";
	}
}

tw::EnvironmentManager::EnvironmentManager()
	: mapDirectory("./assets/map/")
{
}

tw::EnvironmentManager * tw::EnvironmentManager::getInstance()
{
	if (instance == NULL)
		instance = new tw::EnvironmentManager();

	return instance;
}

void tw::EnvironmentManager::setMapDirectory(const std::string & directory)
{
	mapDirectory = withSlash(directory);
}

tw::Environment * tw::EnvironmentManager::loadEnvironment(int environmentId)
{
	auto received = receivedMaps.find(environmentId);
	if (received != receivedMaps.end())
	{
		Environment * environment = fromJson(received->second);
		if (environment != NULL)
			return environment;
	}

	return loadEnvironmentFrom(mapDirectory, environmentId);
}

tw::Environment * tw::EnvironmentManager::loadEnvironmentFrom(const std::string & directory, int environmentId)
{
	std::string base = withSlash(directory) + std::to_string(environmentId);
	std::string content;
	std::string error;

	if (readFile(base + ".json", content))
	{
		Environment * environment = fromJson(content, &error);
		if (environment == NULL)
			std::cout << "Carte " << base << ".json invalide : " << error << std::endl;
		else
			environment->setId(environmentId);
		return environment;
	}

	if (readFile(base + ".txt", content))
	{
		Environment * environment = fromV1Text(content, environmentId, &error);
		if (environment == NULL)
			std::cout << "Carte " << base << ".txt invalide : " << error << std::endl;
		return environment;
	}

	return NULL;
}

bool tw::EnvironmentManager::saveEnvironment(Environment * environment, std::string * error)
{
	return saveEnvironmentTo(environment, mapDirectory, error);
}

bool tw::EnvironmentManager::saveEnvironmentTo(Environment * environment, const std::string & directory, std::string * error)
{
	try
	{
		fs::create_directories(fs::u8path(withSlash(directory)));
	}
	catch (const std::exception &)
	{
	}

	std::string path = withSlash(directory) + std::to_string(environment->getId()) + ".json";
	std::string temporary = path + ".tmp";
	{
		std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			if (error != nullptr)
				*error = "Impossible d'écrire " + path;
			return false;
		}
		file << toJson(environment);
	}

	std::error_code code;
	fs::rename(fs::u8path(temporary), fs::u8path(path), code);
	if (code)
	{
		if (error != nullptr)
			*error = "Impossible de remplacer " + path + " : " + code.message();
		return false;
	}
	return true;
}

std::string tw::EnvironmentManager::toJson(Environment * environment, bool withRules, bool compact)
{
	int width = environment->getWidth();
	int height = environment->getHeight();

	// Palette : tuiles utilisées, dans l'ordre d'apparition.
	std::vector<std::string> palette;
	std::map<std::string, int> paletteIndex;
	json rules = json::object();
	json starts = { { "1", json::array() }, { "2", json::array() } };

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			CellData * cell = environment->getMapData(x, y);
			std::string tile = cell->getDisplayTile();
			if (paletteIndex.find(tile) == paletteIndex.end())
			{
				paletteIndex[tile] = (int)palette.size();
				palette.push_back(tile);
				rules[tile] = { { "walkable", cell->getIsWalkable() && !cell->getIsObstacle() }, { "obstacle", cell->getIsObstacle() } };
			}

			int team = cell->getTeamStartPointNumber();
			if (team == 1 || team == 2)
				starts[std::to_string(team)].push_back(json::array({ x, y }));
		}
	}

	json root = json::object();
	root["format"] = "tw-map";
	root["version"] = 2;
	root["id"] = environment->getId();
	root["name"] = environment->getName();
	root["width"] = width;
	root["height"] = height;
	root["tournament"] = environment->isInTournamentPool();
	root["palette"] = palette;
	if (withRules)
		root["rules"] = rules;
	root["start"] = starts;

	json rows = json::array();
	for (int y = 0; y < height; y++)
	{
		json row = json::array();
		for (int x = 0; x < width; x++)
			row.push_back(paletteIndex[environment->getMapData(x, y)->getDisplayTile()]);
		rows.push_back(row);
	}

	if (compact)
	{
		root["tiles"] = rows;
		return root.dump(-1, ' ', false, json::error_handler_t::replace);
	}

	// Fichier lisible et facile à comparer : une ligne de la carte par ligne de texte.
	std::string text = "{\n";
	for (auto it = root.begin(); it != root.end(); ++it)
		text += "  " + json(it.key()).dump() + ": " + it.value().dump(-1, ' ', false, json::error_handler_t::replace) + ",\n";
	text += "  \"tiles\": [\n";
	for (int y = 0; y < height; y++)
		text += "    " + rows[y].dump() + (y + 1 < height ? ",\n" : "\n");
	text += "  ]\n}\n";
	return text;
}

tw::Environment * tw::EnvironmentManager::fromJson(const std::string & text, std::string * error)
{
	json root = json::parse(text, nullptr, false);
	if (root.is_discarded() || !root.is_object())
	{
		if (error != nullptr)
			*error = "JSON invalide";
		return NULL;
	}

	try
	{
		if (root.value("format", std::string()) != "tw-map" || root.value("version", 0) != 2)
		{
			if (error != nullptr)
				*error = "format \"tw-map\" version 2 attendu";
			return NULL;
		}

		int width = root.at("width").get<int>();
		int height = root.at("height").get<int>();
		if (width <= 0 || height <= 0 || width > 200 || height > 200)
		{
			if (error != nullptr)
				*error = "dimensions invalides";
			return NULL;
		}

		std::vector<std::string> palette = root.at("palette").get<std::vector<std::string>>();
		const json & rows = root.at("tiles");
		const json & rules = root.contains("rules") ? root["rules"] : json::object();

		Environment * environment = new Environment(width, height, root.value("id", 0));
		environment->setName(root.value("name", std::string()));
		environment->setInTournamentPool(root.value("tournament", true));

		for (int y = 0; y < height && y < (int)rows.size(); y++)
		{
			const json & row = rows[y];
			for (int x = 0; x < width && x < (int)row.size(); x++)
			{
				int index = row[x].get<int>();
				std::string tile = index >= 0 && index < (int)palette.size() ? palette[index] : TileRegistry::LEGACY_GROUND;
				if (rules.contains(tile))
					environment->setTile(x, y, tile, rules[tile].value("walkable", false), rules[tile].value("obstacle", true));
				else
					environment->setTile(x, y, tile);
			}
		}

		const json & starts = root.value("start", json::object());
		for (int team = 1; team <= 2; team++)
		{
			for (const json & cell : starts.value(std::to_string(team), json::array()))
			{
				CellData * data = environment->getMapData(cell.at(0).get<int>(), cell.at(1).get<int>());
				if (data != NULL)
					data->setTeamStartPoint(team);
			}
		}
		return environment;
	}
	catch (const json::exception & e)
	{
		if (error != nullptr)
			*error = e.what();
		return NULL;
	}
}

tw::Environment * tw::EnvironmentManager::fromV1Text(const std::string & text, int environmentId, std::string * error)
{
	std::istringstream input(text);
	int height = 0;
	int width = 0;
	int id = 0;
	if (!(input >> height >> width >> id) || width <= 0 || height <= 0 || width > 200 || height > 200)
	{
		if (error != nullptr)
			*error = "en-tête invalide";
		return NULL;
	}

	Environment * environment = new Environment(width, height, environmentId);
	std::string line;
	while (std::getline(input, line))
	{
		int x, y, obstacle, walkable, team;
		if (std::sscanf(line.c_str(), "%d,%d,%d,%d,%d", &x, &y, &obstacle, &walkable, &team) != 5)
			continue;

		CellData * cell = environment->getMapData(x, y);
		if (cell == NULL)
			continue;
		environment->setTile(x, y, TileRegistry::legacyTile(walkable != 0, obstacle != 0));
		cell->setTeamStartPoint(team);
	}
	return environment;
}

bool tw::EnvironmentManager::registerReceivedMap(const std::string & text)
{
	Environment * environment = fromJson(text);
	if (environment == NULL)
		return false;

	receivedMaps[environment->getId()] = text;
	delete environment;
	return true;
}

std::vector<int> tw::EnvironmentManager::getAlreadyExistingIds()
{
	return getAlreadyExistingIds(mapDirectory);
}

std::vector<int> tw::EnvironmentManager::getAlreadyExistingIds(const std::string & directory)
{
	std::set<int> ids;
	try
	{
		for (auto & entry : fs::directory_iterator(fs::u8path(directory)))
		{
			std::string extension = entry.path().extension().string();
			if (extension != ".json" && extension != ".txt")
				continue;

			try
			{
				std::string stem = entry.path().stem().string();
				std::size_t used = 0;
				int id = std::stoi(stem, &used);
				if (id > 0 && used == stem.size())
					ids.insert(id);
			}
			catch (const std::exception &)
			{
			}
		}
	}
	catch (const std::exception &)
	{
		// Dossier absent : aucune carte.
	}

	return std::vector<int>(ids.begin(), ids.end());
}

int tw::EnvironmentManager::getAvailableId()
{
	std::vector<int> used = getAlreadyExistingIds();
	return used.empty() ? 1 : used.back() + 1;
}
