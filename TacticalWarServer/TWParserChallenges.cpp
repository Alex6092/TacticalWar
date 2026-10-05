#include "TWParser.h"

#include <Message.h>
#include <Tournament.h>

namespace
{
	std::string encode(const std::string & op, const nlohmann::json & body)
	{
		return tw::protocol::Message::encode(op, body);
	}
}

// Défis entre équipes : une équipe en attente défie une équipe libre ; si elle accepte, un match
// amical est créé (choix des classes habituel). Impossible pendant un tournoi en cours.

bool TWParser::tournamentRunning()
{
	for (int id : tournaments.ids())
	{
		const tw::tournament::TournamentEngine * engine = tournaments.find(id);
		if (engine != NULL && engine->get().status == tw::tournament::TournamentStatus::RUNNING)
			return true;
	}
	return false;
}

bool TWParser::teamFree(int teamId)
{
	return isTeamAvailableForMatchCreation(teamId) == 0;
}

void TWParser::sendToTeam(int teamId, const std::string & message)
{
	auto players = teamIdToPlayerList.find(teamId);
	if (players == teamIdToPlayerList.end())
		return;
	for (tw::Player * player : players->second)
	{
		ClientState * client = getClientStateFromPlayer(player);
		if (client != NULL)
			send(client, message);
	}
}

void TWParser::sendChallengeList(ClientState * client, tw::Player * player)
{
	int mine = player->getTeamNumber();
	bool running = tournamentRunning();
	bool mineFree = teamFree(mine);
	nlohmann::json list = nlohmann::json::array();
	for (const auto & entry : teamIdToPlayerList)
	{
		int teamId = entry.first;
		if (teamId == mine)
			continue;
		nlohmann::json online = nlohmann::json::array();
		for (tw::Player * member : entry.second)
		{
			if (getClientStateFromPlayer(member) != NULL)
				online.push_back(displayNameOf(member));
		}
		std::string reason;
		if (online.empty())
			reason = u8"aucun joueur connecté";
		else if (!teamFree(teamId))
			reason = u8"en match";
		else if (challenges.sentBy(teamId) != NULL || challenges.receivedBy(teamId) != NULL)
			reason = u8"défi en attente";
		list.push_back({ { "id", teamId }, { "name", teamName(teamId) }, { "online", online }, { "allowed", reason.empty() }, { "reason", reason } });
	}
	std::string closed;
	if (running)
		closed = u8"Un tournoi est en cours : les défis reprendront après.";
	else if (!mineFree)
		closed = u8"Votre équipe a déjà un match prévu ou en cours.";
	send(client, encode("DL", { { "teams", list }, { "closed", closed } }));
}

void TWParser::handleChallengeMessage(ClientState * client, const std::string & op, const nlohmann::json & body)
{
	tw::Player * player = getPlayerFromClientState(client);
	if (player == NULL)
		return;
	int mine = player->getTeamNumber();
	std::int64_t now = nowMs();

	if (op == "DL")
	{
		sendChallengeList(client, player);
	}
	else if (op == "DD")
	{
		int target = body.value("team", 0);
		if (teamIdToPlayerList.find(target) == teamIdToPlayerList.end())
		{
			send(client, encode("DR", { { "ok", false }, { "message", u8"Équipe inconnue." }, { "from", mine }, { "to", target } }));
			return;
		}
		std::string refusal = challenges.challenge(mine, target, tournamentRunning(), teamFree(mine), teamFree(target), now);
		if (!refusal.empty())
		{
			send(client, encode("DR", { { "ok", false }, { "message", refusal }, { "from", mine }, { "to", target } }));
			return;
		}
		sendToTeam(mine, encode("DR", { { "ok", true }, { "message", displayNameOf(player) + u8" a défié " + teamName(target) + u8" : réponse sous 30 s." },
			{ "from", mine }, { "to", target } }));
		sendToTeam(target, encode("DI", { { "from", mine }, { "name", teamName(mine) }, { "seconds", tw::ChallengeBoard::TIMEOUT_MS / 1000 } }));
	}
	else if (op == "DA")
	{
		int from = body.value("from", 0);
		bool accept = body.value("accept", false);
		tw::ChallengeBoard::Challenge answered;
		if (!challenges.answer(mine, from, now, &answered))
		{
			send(client, encode("DR", { { "ok", false }, { "message", u8"Ce défi n'est plus valable." }, { "from", from }, { "to", mine } }));
			return;
		}
		nlohmann::json result = { { "from", from }, { "to", mine } };
		if (!accept)
		{
			result["ok"] = false;
			result["message"] = teamName(mine) + u8" a refusé le défi (" + displayNameOf(player) + ").";
		}
		else
		{
			std::string error;
			if (tournamentRunning())
				error = u8"Un tournoi a commencé : défi annulé.";
			else if (createFriendlyMatch(u8"Défi : " + teamName(from) + " - " + teamName(mine), from, mine, 0, error) == NULL && error.empty())
				error = u8"Match impossible.";
			result["ok"] = error.empty();
			result["message"] = error.empty() ? teamName(mine) + u8" relève le défi : place au choix des classes !" : error;
		}
		std::string message = encode("DR", result);
		sendToTeam(from, message);
		sendToTeam(mine, message);
	}
}

void TWParser::tickChallenges(std::int64_t now)
{
	std::vector<tw::ChallengeBoard::Challenge> removed = challenges.expire(now);
	for (const tw::ChallengeBoard::Challenge & challenge : removed)
	{
		std::string message = encode("DR", { { "ok", false }, { "message", u8"Défi expiré : pas de réponse de " + teamName(challenge.to) + "." },
			{ "from", challenge.from }, { "to", challenge.to } });
		sendToTeam(challenge.from, message);
		sendToTeam(challenge.to, message);
	}

	// Tournoi qui commence, ou équipe prise par un autre match : les défis en attente sont retirés.
	std::vector<tw::ChallengeBoard::Challenge> cancelled;
	if (!challenges.pending().empty() && tournamentRunning())
	{
		cancelled = challenges.cancelAll();
	}
	else
	{
		std::vector<tw::ChallengeBoard::Challenge> pending = challenges.pending();
		for (const tw::ChallengeBoard::Challenge & challenge : pending)
		{
			if (!teamFree(challenge.from) || !teamFree(challenge.to))
			{
				std::vector<tw::ChallengeBoard::Challenge> gone = challenges.cancelInvolving(teamFree(challenge.from) ? challenge.to : challenge.from);
				cancelled.insert(cancelled.end(), gone.begin(), gone.end());
			}
		}
	}
	for (const tw::ChallengeBoard::Challenge & challenge : cancelled)
	{
		std::string message = encode("DR", { { "ok", false }, { "message", u8"Défi annulé : un match ou un tournoi commence." },
			{ "from", challenge.from }, { "to", challenge.to } });
		sendToTeam(challenge.from, message);
		sendToTeam(challenge.to, message);
	}
}
