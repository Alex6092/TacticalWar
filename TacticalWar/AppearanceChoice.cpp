#include "AppearanceChoice.h"

#include <algorithm>
#include <nlohmann/json.hpp>

#include <Appearances.h>
#include <Palette.h>

#include "ClientConfig.h"
#include "ClientGameData.h"

namespace
{
	std::vector<std::string> fresh;
}

std::vector<std::string> tw::availableAppearances()
{
	const battle::GameData & data = ClientGameData::get().data();
	battle::PlayerProgress local;
	local.puzzles = ClientConfig::get().solvedPuzzles;
	std::vector<std::string> available;
	const std::vector<std::string> & known = ClientConfig::get().knownAppearances;
	std::vector<std::string> unlocked = battle::unlockedAppearances(data, local);
	// Dans l'ordre des données de jeu.
	for (const battle::AppearanceDef & appearance : data.appearances)
	{
		if (std::find(known.begin(), known.end(), appearance.id) != known.end()
			|| std::find(unlocked.begin(), unlocked.end(), appearance.id) != unlocked.end())
			available.push_back(appearance.id);
	}
	return available;
}

std::string tw::chosenAppearance()
{
	std::vector<std::string> available = availableAppearances();
	const std::string & wanted = ClientConfig::get().appearance;
	if (std::find(available.begin(), available.end(), wanted) != available.end())
		return wanted;
	return available.empty() ? std::string() : available[0];
}

void tw::appearanceColors(const std::string & appearance, int team, int armor[3], int hair[3])
{
	int teamColor[3];
	palette::teamArmor(team, teamColor);
	const battle::AppearanceDef * look = ClientGameData::get().data().findAppearance(appearance);
	battle::armorColor(look, teamColor, armor);
	if (look != nullptr)
	{
		for (int i = 0; i < 3; i++)
			hair[i] = look->hair[i];
	}
	else
	{
		palette::defaultHair(hair);
	}
}

void tw::applyAppearanceMessage(const std::string & text)
{
	nlohmann::json body = nlohmann::json::parse(text, nullptr, false);
	if (!body.is_object())
		return;
	ClientConfig & config = ClientConfig::get();
	config.knownAppearances = body.value("unlocked", std::vector<std::string>());
	std::string selected = body.value("selected", std::string());
	if (!selected.empty())
		config.appearance = selected;
	config.save();
	for (const std::string & id : body.value("new", std::vector<std::string>()))
		fresh.push_back(id);
}

std::vector<std::string> tw::takeFreshAppearances()
{
	std::vector<std::string> result;
	result.swap(fresh);
	return result;
}

std::string tw::appearanceName(const std::string & id)
{
	const battle::AppearanceDef * look = ClientGameData::get().data().findAppearance(id);
	return look != nullptr ? look->name : id;
}
