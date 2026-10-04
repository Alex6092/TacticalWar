// Gestion des équipes et des comptes joueurs : chargement, administration (création,
// modification, désactivation, mots de passe, import de l'ancien fichier equipe.txt).
#include "TWParser.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <JsonFile.h>
#include <LegacyImport.h>
#include <Message.h>
#include <PlayerManager.h>

namespace
{
	const char * LEGACY_TEAM_FILE = "./assets/equipe.txt";

	bool readWholeFile(const std::string & path, std::string & content)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
			return false;

		std::stringstream buffer;
		buffer << file.rdbuf();
		content = buffer.str();
		return true;
	}
}

void TWParser::loadTeams()
{
	std::string error;
	if (!teamStore.load(error))
	{
		// Ne jamais écraser un fichier existant que l'on n'a pas su lire.
		teamStoreReadOnly = true;
		std::cerr << "ERREUR : " << error << std::endl
			<< "Les équipes ne pourront pas être modifiées tant que le fichier n'est pas corrigé." << std::endl;
	}

	credentials.load();

	if (!teamStoreReadOnly && !teamStore.fileExists())
	{
		// Premier lancement : reprise des équipes de l'ancien fichier s'il existe.
		std::string content;
		if (readWholeFile(LEGACY_TEAM_FILE, content))
		{
			std::map<std::string, std::string> passwords;
			std::string report = tw::importLegacyTeams(content, teamStore, passwords);
			for (const auto & entry : passwords)
				credentials.set(entry.first, entry.second);

			std::cout << "Import de " << LEGACY_TEAM_FILE << " : " << report;
		}

		saveTeams();
	}

	rebuildPlayers();
	std::cout << teamStore.getTeams().size() << " équipe(s) chargée(s)." << std::endl;
}

bool TWParser::saveTeams(std::string * error)
{
	std::string localError;
	if (teamStoreReadOnly)
	{
		localError = "Le fichier des équipes est en lecture seule (erreur au chargement).";
	}
	else if (!teamStore.save(&localError) || !credentials.save(teamStore.getTeams(), &localError))
	{
		std::cerr << "ERREUR de sauvegarde : " << localError << std::endl;
	}
	else
	{
		return true;
	}

	if (error != nullptr)
		*error = localError;
	return false;
}

void TWParser::rebuildPlayers()
{
	playersMap.clear();
	teamIdToPlayerList.clear();

	for (const tw::Team & team : teamStore.getTeams())
	{
		for (const tw::PlayerAccount & account : team.players)
		{
			if (account.login.empty())
				continue;
			tw::Player * player = NULL;
			std::map<std::string, tw::Player*>::iterator it = allPlayers.find(account.login);
			if (it == allPlayers.end())
			{
				player = new tw::Player(account.login, "", team.id);
				allPlayers[account.login] = player;
			}
			else
			{
				player = it->second;
				player->setTeamNumber(team.id);
			}

			if (team.active)
			{
				playersMap[account.login] = player;
				teamIdToPlayerList[team.id].push_back(player);
			}
		}

		// Joueur seul : son second personnage complète l'équipe (le même d'une reconstruction à l'autre,
		// car les matchs déjà créés le gardent).
		if (team.active && tw::playerCount(team) == 1)
		{
			tw::Player *& standIn = standIns[team.id];
			if (standIn == NULL)
				standIn = new tw::Player("second " + std::to_string(team.id), "", team.id);
			teamIdToPlayerList[team.id].push_back(standIn);
		}
	}

	// Les clients dont le compte n'existe plus (ou dont l'équipe est désactivée) sont déconnectés.
	std::vector<ClientState*> toKick;
	for (auto & entry : clients)
	{
		ClientState * client = entry.second;
		if (!client->isAdmin() && client->getPseudo().size() > 0 && playersMap.find(client->getPseudo()) == playersMap.end())
			toKick.push_back(client);
	}

	for (ClientState * client : toKick)
	{
		std::map<std::string, tw::Player*>::iterator it = allPlayers.find(client->getPseudo());
		if (it != allPlayers.end() && getClientStateFromPlayer(it->second) == client)
			connectedPlayerMap.erase(it->second);
		kick(client);
	}
}

bool TWParser::teamHasPendingMatch(int teamId)
{
	return isTeamAvailableForMatchCreation(teamId) == -1;
}

bool TWParser::teamHasAnyMatch(int teamId)
{
	const tw::Team * team = teamStore.findTeam(teamId);
	if (team == NULL)
		return false;

	for (const tw::PlayerAccount & account : team->players)
	{
		std::map<std::string, tw::Player*>::iterator it = allPlayers.find(account.login);
		if (it != allPlayers.end() && !tw::PlayerManager::getAllMatchsForPlayer(it->second).empty())
			return true;
	}
	return false;
}

void TWParser::sendTeamResult(ClientState * client, bool ok, const std::string & message, const std::map<std::string, std::string> & passwords)
{
	nlohmann::json body = {
		{ "ok", ok },
		{ "message", message },
		{ "passwords", passwords }
	};
	send(client, tw::protocol::Message::encode("TR", body));
}

void TWParser::handleTeamAdminMessage(ClientState * client, const std::string & op, const nlohmann::json & body)
{
	if (teamStoreReadOnly)
	{
		sendTeamResult(client, false, "Le fichier des équipes n'a pas pu être lu : corrigez data/teams.json puis redémarrez le serveur.");
		return;
	}

	std::string error;
	std::string message;
	std::map<std::string, std::string> passwords;
	int teamId = body.value("id", 0);

	// Une équipe en match (planifié ou en cours) ne peut pas être modifiée : les joueurs
	// connectés perdraient leur compte en plein combat.
	if ((op == "TU" || op == "TD" || op == "TA") && teamHasPendingMatch(teamId))
	{
		sendTeamResult(client, false, "Cette équipe a un match planifié ou en cours : modification impossible.");
		return;
	}

	if (op == "TC")
	{
		int newId = 0;
		error = teamStore.createTeam(tw::teamInputFromJson(body), newId, passwords);
		message = "Équipe créée.";
	}
	else if (op == "TU")
	{
		error = teamStore.updateTeam(teamId, tw::teamInputFromJson(body), passwords);
		message = "Équipe modifiée.";
	}
	else if (op == "TD")
	{
		if (teamHasAnyMatch(teamId))
		{
			// Les matchs passés référencent l'équipe : on la désactive au lieu de la supprimer.
			error = teamStore.setActive(teamId, false);
			message = "L'équipe a déjà joué : elle a été désactivée au lieu d'être supprimée.";
		}
		else
		{
			error = teamStore.deleteTeam(teamId);
			message = "Équipe supprimée.";
		}
	}
	else if (op == "TA")
	{
		bool active = body.value("active", true);
		error = teamStore.setActive(teamId, active);
		message = active ? "Équipe réactivée." : "Équipe désactivée.";
	}
	else if (op == "TK")
	{
		std::string login = body.value("login", "");
		std::string password;
		error = teamStore.resetPassword(login, password);
		if (error.empty())
		{
			int index = 0;
			const tw::Team * team = teamStore.findTeamByLogin(login, &index);
			passwords[team->players[index].login] = password;
		}
		message = "Nouveau mot de passe généré.";
	}
	else if (op == "TI")
	{
		std::string content;
		if (!readWholeFile(LEGACY_TEAM_FILE, content))
			error = std::string("Fichier introuvable : ") + LEGACY_TEAM_FILE;
		else
			message = tw::importLegacyTeams(content, teamStore, passwords);
	}

	if (!error.empty())
	{
		sendTeamResult(client, false, error);
		return;
	}

	for (const auto & entry : passwords)
		credentials.set(entry.first, entry.second);

	if (!saveTeams(&error))
	{
		sendTeamResult(client, false, "Modification appliquée mais NON sauvegardée : " + error);
	}
	else
	{
		sendTeamResult(client, true, message, passwords);
	}

	rebuildPlayers();
	notifyTeamList(admin);
}
