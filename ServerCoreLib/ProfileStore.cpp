#include "ProfileStore.h"
#include "JsonFile.h"

#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>

using namespace tw;

namespace
{
	const int FILE_VERSION = 1;
	const std::size_t MAX_PUZZLES = 64;
	const std::size_t MAX_ID_LENGTH = 40;

	std::set<std::string> stringSet(const nlohmann::json & value)
	{
		std::set<std::string> result;
		if (value.is_array())
		{
			for (const nlohmann::json & item : value)
			{
				if (item.is_string())
					result.insert(item.get<std::string>());
			}
		}
		return result;
	}
}

ProfileStore::ProfileStore(const std::string & path)
	: path(path)
{
}

std::string ProfileStore::key(const std::string & login)
{
	std::string lower = login;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return lower;
}

bool ProfileStore::load(std::string * error)
{
	profiles.clear();
	if (!store::fileExists(path))
		return true;
	nlohmann::json json;
	if (!store::readJsonFile(path, json, error))
		return false;
	const nlohmann::json & players = json.value("players", nlohmann::json::object());
	for (auto it = players.begin(); it != players.end(); ++it)
	{
		PlayerProfile profile;
		profile.achievements = stringSet(it.value().value("achievements", nlohmann::json::array()));
		profile.wins = it.value().value("wins", 0);
		profile.mvp = it.value().value("mvp", 0);
		profile.puzzles = stringSet(it.value().value("puzzles", nlohmann::json::array()));
		profile.appearance = it.value().value("appearance", std::string());
		profiles[key(it.key())] = profile;
	}
	return true;
}

bool ProfileStore::save()
{
	nlohmann::json players = nlohmann::json::object();
	for (const auto & entry : profiles)
	{
		const PlayerProfile & profile = entry.second;
		players[entry.first] = {
			{ "achievements", std::vector<std::string>(profile.achievements.begin(), profile.achievements.end()) },
			{ "wins", profile.wins },
			{ "mvp", profile.mvp },
			{ "puzzles", std::vector<std::string>(profile.puzzles.begin(), profile.puzzles.end()) },
			{ "appearance", profile.appearance }
		};
	}
	return store::writeJsonFileAtomic(path, { { "version", FILE_VERSION }, { "players", players } });
}

PlayerProfile ProfileStore::get(const std::string & login) const
{
	auto it = profiles.find(key(login));
	return it != profiles.end() ? it->second : PlayerProfile();
}

void ProfileStore::recordBattle(const std::string & login, const std::vector<std::string> & badges, bool won, bool mvp)
{
	PlayerProfile & profile = profiles[key(login)];
	profile.achievements.insert(badges.begin(), badges.end());
	profile.wins += won ? 1 : 0;
	profile.mvp += mvp ? 1 : 0;
	save();
}

bool ProfileStore::addPuzzles(const std::string & login, const std::vector<std::string> & puzzles)
{
	PlayerProfile & profile = profiles[key(login)];
	bool added = false;
	for (const std::string & id : puzzles)
	{
		if (id.empty() || id.size() > MAX_ID_LENGTH || profile.puzzles.size() >= MAX_PUZZLES)
			continue;
		added = profile.puzzles.insert(id).second || added;
	}
	if (added)
		save();
	return added;
}

void ProfileStore::setAppearance(const std::string & login, const std::string & appearance)
{
	PlayerProfile & profile = profiles[key(login)];
	if (profile.appearance == appearance)
		return;
	profile.appearance = appearance;
	save();
}
