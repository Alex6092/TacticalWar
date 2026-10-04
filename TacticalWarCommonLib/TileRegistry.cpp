#include "pch.h"
#include "TileRegistry.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using namespace tw;

const char * TileRegistry::DEFAULT_PATH = "./assets/tiles/tileset.json";
const char * TileRegistry::LEGACY_GROUND = "grass";
const char * TileRegistry::LEGACY_OBSTACLE = "stone";
const char * TileRegistry::LEGACY_HOLE = "water";

const char * tw::toString(TileCategory category)
{
	switch (category)
	{
	case TileCategory::GROUND: return "ground";
	case TileCategory::OBSTACLE: return "obstacle";
	case TileCategory::LIQUID: return "liquid";
	case TileCategory::EMPTY: return "empty";
	}
	return "ground";
}

TileCategory tw::tileCategoryFromString(const std::string & text)
{
	if (text == "obstacle") return TileCategory::OBSTACLE;
	if (text == "liquid") return TileCategory::LIQUID;
	if (text == "empty") return TileCategory::EMPTY;
	return TileCategory::GROUND;
}

TileRules TileRules::forCategory(TileCategory category)
{
	TileRules rules;
	rules.walkable = category == TileCategory::GROUND;
	rules.blocksLineOfSight = category == TileCategory::OBSTACLE;
	return rules;
}

TileRules TileRules::unknown()
{
	TileRules rules;
	rules.walkable = false;
	rules.blocksLineOfSight = true;
	return rules;
}

TileRegistry & TileRegistry::get()
{
	static TileRegistry registry;
	static bool loaded = false;
	if (!loaded)
	{
		loaded = true;
		if (!registry.load(DEFAULT_PATH))
			registry.loadBuiltins();
	}
	return registry;
}

const char * TileRegistry::legacyTile(bool walkable, bool obstacle)
{
	if (obstacle)
		return LEGACY_OBSTACLE;
	return walkable ? LEGACY_GROUND : LEGACY_HOLE;
}

void TileRegistry::add(const TileDef & tile)
{
	auto it = index.find(tile.id);
	if (it != index.end())
	{
		tiles[it->second] = tile;
		return;
	}
	index[tile.id] = tiles.size();
	tiles.push_back(tile);
}

void TileRegistry::loadBuiltins()
{
	// Tuiles d'origine du jeu (utilisées si tileset.json est absent).
	tiles.clear();
	index.clear();

	TileDef grass;
	grass.id = LEGACY_GROUND;
	grass.name = "Herbe";
	grass.texture = "assets/tiles/resized/Grass_01.png";
	grass.anchorX = 66;
	grass.anchorY = 45;
	grass.group = "Sols";
	add(grass);

	TileDef stone;
	stone.id = LEGACY_OBSTACLE;
	stone.name = "Rocher";
	stone.category = TileCategory::OBSTACLE;
	stone.texture = "assets/tiles/resized/Stone_02.png";
	stone.anchorX = 70;
	stone.anchorY = 59;
	stone.rules = TileRules::forCategory(TileCategory::OBSTACLE);
	stone.group = "Obstacles";
	add(stone);

	TileDef water;
	water.id = LEGACY_HOLE;
	water.name = "Eau";
	water.category = TileCategory::LIQUID;
	water.texture = "assets/tiles/resized/Water_01.png";
	water.anchorX = 69;
	water.anchorY = 33;
	water.rules = TileRules::forCategory(TileCategory::LIQUID);
	water.group = "Liquides";
	water.shader = "water";
	add(water);
}

bool TileRegistry::load(const std::string & path, std::string * error)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		if (error != nullptr)
			*error = "Fichier introuvable : " + path;
		return false;
	}

	std::stringstream content;
	content << file.rdbuf();
	return loadFromString(content.str(), error);
}

bool TileRegistry::loadFromString(const std::string & text, std::string * error)
{
	nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
	if (root.is_discarded() || !root.is_object() || !root.contains("tiles") || !root["tiles"].is_array())
	{
		if (error != nullptr)
			*error = "Jeu de tuiles invalide (objet avec un tableau \"tiles\" attendu).";
		return false;
	}

	std::vector<TileDef> loaded;
	for (const nlohmann::json & item : root["tiles"])
	{
		if (!item.is_object() || !item.contains("id") || !item["id"].is_string())
			continue;

		TileDef tile;
		tile.id = item["id"].get<std::string>();
		tile.name = item.value("name", tile.id);
		tile.category = tileCategoryFromString(item.value("category", std::string("ground")));
		tile.texture = item.value("texture", std::string());
		if (item.contains("anchor") && item["anchor"].is_array() && item["anchor"].size() == 2)
		{
			tile.anchorX = item["anchor"][0].get<float>();
			tile.anchorY = item["anchor"][1].get<float>();
		}

		// Règles par défaut selon la catégorie, modifiables tuile par tuile.
		tile.rules = TileRules::forCategory(tile.category);
		tile.rules.walkable = item.value("walkable", tile.rules.walkable);
		tile.rules.blocksLineOfSight = item.value("blocksLineOfSight", tile.rules.blocksLineOfSight);
		if (item.contains("turnStart") && item["turnStart"].is_object())
		{
			tile.rules.turnDamage = std::max(0, item["turnStart"].value("damage", 0));
			tile.rules.turnHeal = std::max(0, item["turnStart"].value("heal", 0));
		}

		tile.group = item.value("group", std::string());
		tile.shader = item.value("shader", std::string());
		loaded.push_back(tile);
	}

	if (loaded.empty())
	{
		if (error != nullptr)
			*error = "Le jeu de tuiles ne contient aucune tuile.";
		return false;
	}

	// Les tuiles d'origine restent toujours disponibles (cartes v1), éventuellement redéfinies.
	loadBuiltins();
	for (const TileDef & tile : loaded)
		add(tile);
	return true;
}

const TileDef * TileRegistry::find(const std::string & id) const
{
	auto it = index.find(id);
	return it == index.end() ? nullptr : &tiles[it->second];
}
