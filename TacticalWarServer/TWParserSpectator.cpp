// Mode spectateur : liste des combats en cours, arrivée et départ des spectateurs.
// Les spectateurs reçoivent le même flux d'événements que les joueurs (avec "you" = -1).
#include "TWParser.h"

#include <Message.h>

namespace
{
	std::string encode(const std::string & op, const nlohmann::json & body)
	{
		return tw::protocol::Message::encode(op, body);
	}
}

nlohmann::json TWParser::sessionListJson()
{
	nlohmann::json list = nlohmann::json::array();
	for (auto & entry : sessions)
	{
		BattleSession * session = entry.second;
		if (session->getPhase() == BattleSession::Phase::ENDED)
			continue;

		tw::Match * match = session->getMatch();
		nlohmann::json item = {
			{ "session", session->getId() },
			{ "name", match->getMatchName() },
			{ "teams", nlohmann::json::array({ teamName(match->getTeam1()[0]->getTeamNumber()), teamName(match->getTeam2()[0]->getTeamNumber()) }) },
			{ "phase", session->getPhase() == BattleSession::Phase::BAN ? "BAN" : "CLASS_SELECTION" },
			{ "round", 0 },
			{ "spectators", session->spectators.size() },
			{ "tournament", session->getTournamentId() },
			{ "match", session->getTournamentMatchId() }
		};

		if (session->getPhase() == BattleSession::Phase::BATTLE)
		{
			const tw::battle::BattleEngine * engine = session->getEngine();
			item["phase"] = tw::battle::toString(engine->getState().phase);
			item["round"] = engine->getState().round;
			item["hp1"] = engine->teamHpPercent(1);
			item["hp2"] = engine->teamHpPercent(2);
		}
		list.push_back(item);
	}
	return list;
}

void TWParser::notifySessionList(ClientState * only)
{
	std::string message = encode("SL", { { "sessions", sessionListJson() } });
	if (only != NULL)
	{
		send(only, message);
		return;
	}

	for (ClientState * spectator : spectatorModeClientDiffusionList)
		send(spectator, message);
	if (admin != NULL)
		send(admin, message);
}

void TWParser::refreshSessionList()
{
	// Signature des combats (identifiant, phase, tour, nombre de spectateurs) : la liste n'est
	// renvoyée que si elle a changé.
	std::string signature;
	for (auto & entry : sessions)
	{
		BattleSession * session = entry.second;
		signature += std::to_string(session->getId()) + ":" + std::to_string((int)session->getPhase()) + ":" + std::to_string(session->spectators.size());
		if (session->getPhase() == BattleSession::Phase::BATTLE)
			signature += ":" + std::to_string(session->getEngine()->getState().round);
		signature += ";";
	}

	if (signature != lastSessionSignature)
	{
		lastSessionSignature = signature;
		notifySessionList();
	}
}

BattleSession * TWParser::spectatedSession(ClientState * client)
{
	for (auto & entry : sessions)
	{
		if (entry.second->getPhase() != BattleSession::Phase::ENDED && entry.second->spectators.count(client->getConnId()) > 0)
			return entry.second;
	}
	return NULL;
}

void TWParser::removeSpectator(ClientState * client)
{
	for (auto & entry : sessions)
		entry.second->spectators.erase(client->getConnId());
}

void TWParser::handleSpectatorMessage(ClientState * client, const std::string & op, const nlohmann::json & body)
{
	if (op == "RL" || op == "RP")
	{
		handleReplayMessage(client, op, body);
	}
	else if (op == "SL")
	{
		notifySessionList(client);
	}
	else if (op == "SW")
	{
		auto it = sessions.find(body.value("session", 0));
		if (it == sessions.end() || it->second->getPhase() != BattleSession::Phase::BATTLE)
		{
			send(client, encode("ER", { { "op", op }, { "message", "Ce combat n'est pas (ou plus) en cours." } }));
			notifySessionList(client);
			return;
		}

		removeSpectator(client);
		stopPlayback(client);
		it->second->spectators.insert(client->getConnId());
		sendGameData(client);
		sendBattleState(it->second, client, NULL, true);
	}
	else if (op == "SU")
	{
		removeSpectator(client);
		stopPlayback(client);
		notifySessionList(client);
	}
}
