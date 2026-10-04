#include "TeamStore.h"
#include "JsonFile.h"
#include "PasswordHasher.h"

#include <algorithm>
#include <cctype>
#include <set>

using namespace tw;

namespace
{
	const int FILE_VERSION = 1;
	const std::size_t MAX_NAME_LENGTH = 40;
	const std::size_t MAX_LOGIN_LENGTH = 32;

	std::string toLower(const std::string & text)
	{
		std::string lower = text;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
		return lower;
	}

	std::string trim(const std::string & text)
	{
		std::size_t start = text.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			return "";
		std::size_t end = text.find_last_not_of(" \t\r\n");
		return text.substr(start, end - start + 1);
	}

	// Le login historique (HG login;password) sépare les champs par ';'.
	bool containsForbiddenSeparator(const std::string & text)
	{
		return text.find_first_of(";\n\r") != std::string::npos;
	}

	// Joueurs saisis dans l'ordre, emplacements vides à la fin : un joueur seul est toujours le premier.
	TeamInput withPlayersFirst(const TeamInput & input)
	{
		TeamInput ordered = input;
		std::stable_partition(ordered.players.begin(), ordered.players.end(),
			[](const PlayerInput & player) { return !trim(player.login).empty(); });
		return ordered;
	}
}

nlohmann::json tw::teamToJson(const Team & team, bool includePasswordHashes)
{
	nlohmann::json players = nlohmann::json::array();
	for (const PlayerAccount & player : team.players)
	{
		nlohmann::json p = { { "login", player.login }, { "displayName", player.displayName } };
		if (includePasswordHashes)
			p["passwordHash"] = player.passwordHash;
		players.push_back(p);
	}

	return {
		{ "id", team.id },
		{ "name", team.name },
		{ "tag", team.tag },
		{ "seed", team.seed },
		{ "active", team.active },
		{ "players", players }
	};
}

Team tw::teamFromJson(const nlohmann::json & json)
{
	Team team;
	team.id = json.value("id", 0);
	team.name = json.value("name", "");
	team.tag = json.value("tag", "");
	team.seed = json.value("seed", 0);
	team.active = json.value("active", true);

	if (json.contains("players") && json["players"].is_array())
	{
		const nlohmann::json & players = json["players"];
		for (std::size_t i = 0; i < players.size() && i < team.players.size(); i++)
		{
			team.players[i].login = players[i].value("login", "");
			team.players[i].displayName = players[i].value("displayName", "");
			team.players[i].passwordHash = players[i].value("passwordHash", "");
		}
	}

	return team;
}

TeamInput tw::teamInputFromJson(const nlohmann::json & json)
{
	TeamInput input;
	input.name = json.value("name", "");
	input.tag = json.value("tag", "");
	input.seed = json.value("seed", 0);

	if (json.contains("players") && json["players"].is_array())
	{
		const nlohmann::json & players = json["players"];
		for (std::size_t i = 0; i < players.size() && i < input.players.size(); i++)
		{
			input.players[i].login = players[i].value("login", "");
			input.players[i].displayName = players[i].value("displayName", "");
			input.players[i].password = players[i].value("password", "");
		}
	}

	return input;
}

TeamStore::TeamStore(const std::string & filePath)
	: filePath(filePath), nextId(1)
{
}

bool TeamStore::fileExists() const
{
	return store::fileExists(filePath);
}

bool TeamStore::load(std::string & error)
{
	teams.clear();
	nextId = 1;

	if (!fileExists())
		return true;

	nlohmann::json json;
	if (!store::readJsonFile(filePath, json, &error))
		return false;

	if (json.contains("teams") && json["teams"].is_array())
	{
		for (const nlohmann::json & teamJson : json["teams"])
		{
			Team team = teamFromJson(teamJson);
			if (team.id > 0)
			{
				teams.push_back(team);
				nextId = std::max(nextId, team.id + 1);
			}
		}
	}

	nextId = std::max(nextId, json.value("nextId", 1));
	return true;
}

bool TeamStore::save(std::string * error) const
{
	nlohmann::json teamsJson = nlohmann::json::array();
	for (const Team & team : teams)
		teamsJson.push_back(teamToJson(team, true));

	nlohmann::json json = {
		{ "version", FILE_VERSION },
		{ "nextId", nextId },
		{ "teams", teamsJson }
	};

	return store::writeJsonFileAtomic(filePath, json, error);
}

const Team * TeamStore::findTeam(int id) const
{
	for (const Team & team : teams)
	{
		if (team.id == id)
			return &team;
	}
	return nullptr;
}

Team * TeamStore::findTeamMutable(int id)
{
	return const_cast<Team*>(findTeam(id));
}

const Team * TeamStore::findTeamByLogin(const std::string & login, int * playerIndex) const
{
	std::string wanted = toLower(trim(login));
	if (wanted.empty())
		return nullptr;	// Emplacement vide d'une équipe d'un seul joueur : aucun compte.
	for (const Team & team : teams)
	{
		for (int i = 0; i < (int)team.players.size(); i++)
		{
			if (toLower(team.players[i].login) == wanted)
			{
				if (playerIndex != nullptr)
					*playerIndex = i;
				return &team;
			}
		}
	}
	return nullptr;
}

std::string TeamStore::validateLogin(const std::string & login)
{
	if (login.empty())
		return "Le login d'un joueur est vide.";
	if (login.size() > MAX_LOGIN_LENGTH)
		return "Le login \"" + login + "\" est trop long (" + std::to_string(MAX_LOGIN_LENGTH) + " caractères maximum).";
	if (containsForbiddenSeparator(login) || login.find(' ') != std::string::npos)
		return "Le login \"" + login + "\" contient un caractère interdit (espace ou ';').";
	if (toLower(login) == "admin")
		return "Le login \"admin\" est réservé.";
	return "";
}

std::string TeamStore::validatePassword(const std::string & password)
{
	if (password.size() < 4)
		return "Un mot de passe doit contenir au moins 4 caractères.";
	if (containsForbiddenSeparator(password))
		return "Un mot de passe ne peut pas contenir ';'.";
	return "";
}

std::string TeamStore::validate(const TeamInput & input, int ignoredTeamId) const
{
	std::string name = trim(input.name);
	if (name.empty())
		return "Le nom de l'équipe est obligatoire.";
	if (name.size() > MAX_NAME_LENGTH)
		return "Le nom de l'équipe est trop long (" + std::to_string(MAX_NAME_LENGTH) + " caractères maximum).";

	for (const Team & team : teams)
	{
		if (team.id != ignoredTeamId && toLower(team.name) == toLower(name))
			return "Une équipe s'appelle déjà \"" + name + "\".";
	}

	std::set<std::string> logins;
	for (std::size_t i = 0; i < input.players.size(); i++)
	{
		const PlayerInput & player = input.players[i];
		std::string login = trim(player.login);
		// Emplacement laissé vide : équipe d'un seul joueur (le login est obligatoire si le reste est saisi).
		if (login.empty())
		{
			if (!trim(player.displayName).empty() || !player.password.empty())
				return "Le joueur " + std::to_string(i + 1) + " n'a pas de login : saisissez-le, ou videz tous ses champs.";
			continue;
		}
		std::string error = validateLogin(login);
		if (!error.empty())
			return error;

		if (!logins.insert(toLower(login)).second)
			return "Les deux joueurs ont le même login.";

		int index = 0;
		const Team * owner = findTeamByLogin(login, &index);
		if (owner != nullptr && owner->id != ignoredTeamId)
			return "Le login \"" + login + "\" est déjà utilisé par l'équipe \"" + owner->name + "\".";

		if (!player.password.empty())
		{
			error = validatePassword(player.password);
			if (!error.empty())
				return error;
		}
	}

	if (logins.empty())
		return "Une équipe a au moins un joueur : saisissez un login.";
	return "";
}

std::string TeamStore::createTeam(const TeamInput & input, int & newTeamId, std::map<std::string, std::string> & clearPasswords)
{
	std::string error = validate(input, 0);
	if (!error.empty())
		return error;

	Team team;
	team.id = nextId++;
	team.name = trim(input.name);
	team.tag = trim(input.tag);
	team.seed = input.seed;

	TeamInput ordered = withPlayersFirst(input);
	for (std::size_t i = 0; i < team.players.size(); i++)
	{
		const PlayerInput & playerInput = ordered.players[i];
		PlayerAccount & account = team.players[i];
		account.login = trim(playerInput.login);
		if (account.login.empty())
			continue;	// Équipe d'un seul joueur
		account.displayName = trim(playerInput.displayName).empty() ? account.login : trim(playerInput.displayName);

		std::string password = playerInput.password.empty() ? PasswordHasher::generatePassword() : playerInput.password;
		account.passwordHash = PasswordHasher::hash(password);
		clearPasswords[account.login] = password;
	}

	teams.push_back(team);
	newTeamId = team.id;
	return "";
}

std::string TeamStore::updateTeam(int teamId, const TeamInput & input, std::map<std::string, std::string> & clearPasswords)
{
	Team * team = findTeamMutable(teamId);
	if (team == nullptr)
		return "Équipe introuvable.";

	std::string error = validate(input, teamId);
	if (!error.empty())
		return error;

	team->name = trim(input.name);
	team->tag = trim(input.tag);
	team->seed = input.seed;

	std::array<PlayerAccount, PLAYERS_PER_TEAM> previous = team->players;
	TeamInput ordered = withPlayersFirst(input);
	for (std::size_t i = 0; i < team->players.size(); i++)
	{
		const PlayerInput & playerInput = ordered.players[i];
		PlayerAccount account;
		account.login = trim(playerInput.login);
		if (!account.login.empty())
		{
			account.displayName = trim(playerInput.displayName).empty() ? account.login : trim(playerInput.displayName);
			// Un joueur déjà dans l'équipe (même login) garde son mot de passe, même s'il change de place.
			for (const PlayerAccount & old : previous)
			{
				if (!old.login.empty() && toLower(old.login) == toLower(account.login))
					account.passwordHash = old.passwordHash;
			}

			// Un nouveau joueur sans mot de passe fourni reçoit un mot de passe généré.
			std::string password = playerInput.password;
			if (password.empty() && account.passwordHash.empty())
				password = PasswordHasher::generatePassword();

			if (!password.empty())
			{
				account.passwordHash = PasswordHasher::hash(password);
				clearPasswords[account.login] = password;
			}
		}
		team->players[i] = account;
	}

	return "";
}

std::string TeamStore::resetPassword(const std::string & login, std::string & newClearPassword)
{
	int index = 0;
	Team * team = const_cast<Team*>(findTeamByLogin(login, &index));
	if (team == nullptr)
		return "Joueur introuvable.";

	newClearPassword = PasswordHasher::generatePassword();
	team->players[index].passwordHash = PasswordHasher::hash(newClearPassword);
	return "";
}

std::string TeamStore::setActive(int teamId, bool active)
{
	Team * team = findTeamMutable(teamId);
	if (team == nullptr)
		return "Équipe introuvable.";

	team->active = active;
	return "";
}

std::string TeamStore::deleteTeam(int teamId)
{
	for (std::vector<Team>::iterator it = teams.begin(); it != teams.end(); it++)
	{
		if (it->id == teamId)
		{
			teams.erase(it);
			return "";
		}
	}
	return "Équipe introuvable.";
}

bool TeamStore::authenticate(const std::string & login, const std::string & password, std::string * canonicalLogin, int * teamId) const
{
	int index = 0;
	const Team * team = findTeamByLogin(login, &index);
	if (team == nullptr || !team->active)
		return false;

	if (!PasswordHasher::verify(password, team->players[index].passwordHash))
		return false;

	if (canonicalLogin != nullptr)
		*canonicalLogin = team->players[index].login;
	if (teamId != nullptr)
		*teamId = team->id;
	return true;
}
