#include "LegacyImport.h"
#include "TeamStore.h"

#include <Windows.h>

using namespace tw;

namespace
{
	std::vector<std::string> split(const std::string & text, char separator)
	{
		std::vector<std::string> parts;
		std::size_t start = 0;
		while (true)
		{
			std::size_t end = text.find(separator, start);
			parts.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
			if (end == std::string::npos)
				break;
			start = end + 1;
		}
		return parts;
	}

	std::string trim(const std::string & text)
	{
		std::size_t start = text.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			return "";
		std::size_t end = text.find_last_not_of(" \t\r\n");
		return text.substr(start, end - start + 1);
	}
}

bool tw::isValidUtf8(const std::string & text)
{
	if (text.empty())
		return true;
	return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), (int)text.size(), NULL, 0) > 0;
}

std::string tw::legacyTextToUtf8(const std::string & text)
{
	if (isValidUtf8(text))
		return text;

	int wideLength = MultiByteToWideChar(1252, 0, text.data(), (int)text.size(), NULL, 0);
	std::wstring wide(wideLength, L'\0');
	MultiByteToWideChar(1252, 0, text.data(), (int)text.size(), &wide[0], wideLength);

	int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), NULL, 0, NULL, NULL);
	std::string utf8(utf8Length, '\0');
	WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), &utf8[0], utf8Length, NULL, NULL);
	return utf8;
}

std::vector<LegacyPlayer> tw::parseLegacyTeamFile(const std::string & content)
{
	std::vector<LegacyPlayer> players;
	std::string text = legacyTextToUtf8(content);
	if (text.compare(0, 3, "\xEF\xBB\xBF") == 0)
		text = text.substr(3);

	for (const std::string & record : split(text, '/'))
	{
		std::vector<std::string> fields = split(record, ',');
		if (fields.size() < 3)
			continue;

		LegacyPlayer player;
		player.login = trim(fields[0]);
		player.password = trim(fields[1]);
		player.teamNumber = std::atoi(trim(fields[2]).c_str());

		if (!player.login.empty() && player.teamNumber > 0)
			players.push_back(player);
	}

	return players;
}

std::string tw::importLegacyTeams(const std::string & content, TeamStore & store, std::map<std::string, std::string> & clearPasswords)
{
	std::map<int, std::vector<LegacyPlayer>> byTeam;
	for (const LegacyPlayer & player : parseLegacyTeamFile(content))
		byTeam[player.teamNumber].push_back(player);

	int imported = 0;
	std::string report;

	for (const auto & entry : byTeam)
	{
		// Une équipe d'un seul joueur est acceptée : il jouera les deux personnages.
		const std::vector<LegacyPlayer> & members = entry.second;
		if (members.size() > PLAYERS_PER_TEAM)
		{
			report += "Équipe " + std::to_string(entry.first) + " ignorée : " + std::to_string(members.size()) + " joueurs (2 au plus).\n";
			continue;
		}

		TeamInput input;
		input.name = "Équipe " + std::to_string(entry.first);
		input.seed = 0;
		for (std::size_t i = 0; i < members.size(); i++)
		{
			input.players[i].login = members[i].login;
			input.players[i].displayName = members[i].login;
			input.players[i].password = members[i].password;
		}

		int teamId = 0;
		std::map<std::string, std::string> passwords;
		std::string error = store.createTeam(input, teamId, passwords);
		if (!error.empty())
		{
			report += input.name + " ignorée : " + error + "\n";
			continue;
		}

		clearPasswords.insert(passwords.begin(), passwords.end());
		imported++;
	}

	return std::to_string(imported) + " équipe(s) importée(s).\n" + report;
}
