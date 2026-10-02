// Combats : sessions (choix des classes puis BattleEngine), actions des joueurs,
// diffusion des événements et fin de combat.
#include "TWParser.h"

#include <chrono>
#include <iostream>

#include <Message.h>
#include <PlayerManager.h>

namespace
{
	// Après le délai de choix des classes, s'il manque une équipe entière, on attend encore.
	const std::int64_t CLASS_SELECTION_RETRY_MS = 15 * 1000;

	std::string encode(const std::string & op, const nlohmann::json & body)
	{
		return tw::protocol::Message::encode(op, body);
	}
}

std::int64_t TWParser::nowMs() const
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void TWParser::createSession(tw::Match * match)
{
	std::int64_t deadline = nowMs() + (std::int64_t)config.classSelectionSeconds * 1000;
	BattleSession * session = new BattleSession(nextSessionId++, match, gameData, match->getEnvironment(), deadline);
	sessions[session->getId()] = session;
	match->setBattlePayload(session);
}

BattleSession * TWParser::sessionOfMatch(tw::Match * match)
{
	return match == NULL ? NULL : (BattleSession*)match->getBattlePayload();
}

BattleSession * TWParser::sessionOfPlayer(tw::Player * player)
{
	return sessionOfMatch(tw::PlayerManager::getCurrentOrNextMatchForPlayer(player));
}

void TWParser::sendGameData(ClientState * client)
{
	// Les clients reçoivent exactement les mêmes données (JSON compact, sur une seule ligne).
	if (gameDataMessage.empty())
		gameDataMessage = encode("GD", nlohmann::json::parse(gameData.getSourceText(), nullptr, false, true));
	send(client, gameDataMessage);
}

void TWParser::handlePickClass(ClientState * client, tw::Player * player, int classId)
{
	BattleSession * session = sessionOfPlayer(player);
	if (session == NULL || !player->getHasJoinBattle())
		return;

	if (session->chooseClass(player, classId))
	{
		send(client, "PO" + std::to_string(classId) + "\n");
		if (session->allClassesChosen())
			startBattle(session);
	}
}

void TWParser::startBattle(BattleSession * session)
{
	std::map<tw::Player*, bool> connected;
	for (tw::Player * player : session->getParticipants())
		connected[player] = getClientStateFromPlayer(player) != NULL && player->getHasJoinBattle();

	session->startBattle(nowMs(), connected);
	session->getMatch()->setMatchStatus(tw::MatchStatus::STARTED);

	// Les clients reçoivent l'état complet : les événements produits jusqu'ici y sont déjà.
	session->getEngine()->flushEvents();

	for (tw::Player * player : session->getParticipants())
	{
		ClientState * client = getClientStateFromPlayer(player);
		if (client != NULL && player->getHasJoinBattle())
			sendBattleState(session, client, player, true);
	}

	std::cout << "Combat " << session->getId() << " : début du placement." << std::endl;
}

void TWParser::sendBattleState(BattleSession * session, ClientState * client, tw::Player * player, bool enterScreen)
{
	if (enterScreen)
		send(client, "HG" + std::to_string(session->getMapId()) + "\n");

	int fighterId = player != NULL ? session->fighterIdOf(player) : -1;
	send(client, encode("BI", session->getEngine()->snapshot(fighterId, nowMs())));
}

void TWParser::handleBattleAction(ClientState * client, const std::string & op, const nlohmann::json & body)
{
	tw::Player * player = getPlayerFromClientState(client);
	BattleSession * session = player != NULL ? sessionOfPlayer(player) : NULL;
	if (session == NULL || session->getPhase() != BattleSession::Phase::BATTLE)
	{
		send(client, encode("ER", { { "op", op }, { "message", "Aucun combat en cours." } }));
		return;
	}

	tw::battle::BattleEngine * engine = session->getEngine();
	int fighterId = session->fighterIdOf(player);
	std::int64_t now = nowMs();
	tw::battle::ActionResult result;

	try
	{
		if (op == "BR")
		{
			sendBattleState(session, client, player, false);
			return;
		}
		else if (op == "CP")
		{
			result = engine->place(fighterId, { body.at("x").get<int>(), body.at("y").get<int>() }, now);
		}
		else if (op == "Cs")
		{
			result = engine->setReady(fighterId, body.value("ready", true), now);
		}
		else if (op == "Cm")
		{
			std::vector<tw::battle::Cell> path;
			for (const nlohmann::json & cell : body.at("path"))
				path.push_back({ cell.at(0).get<int>(), cell.at(1).get<int>() });
			result = engine->move(fighterId, path, now);
		}
		else if (op == "CL")
		{
			result = engine->cast(fighterId, body.at("slot").get<int>(), { body.at("x").get<int>(), body.at("y").get<int>() }, now);
		}
		else if (op == "Ct")
		{
			result = engine->endTurn(fighterId, now);
		}
	}
	catch (const nlohmann::json::exception &)
	{
		result = tw::battle::ActionResult::failure("Requête invalide.");
	}

	if (!result.ok)
		send(client, encode("ER", { { "op", op }, { "message", result.error } }));

	broadcastBattleEvents(session);
}

void TWParser::broadcastBattleEvents(BattleSession * session)
{
	tw::battle::BattleEngine * engine = session->getEngine();
	if (engine == NULL || !engine->hasPendingEvents())
		return;

	std::string message = encode("BV", engine->flushEvents());
	for (tw::Player * player : session->getParticipants())
	{
		ClientState * client = getClientStateFromPlayer(player);
		if (client != NULL && player->getHasJoinBattle())
			send(client, message);
	}

	if (engine->isOver() && session->getPhase() == BattleSession::Phase::BATTLE)
		finishBattle(session);
}

void TWParser::finishBattle(BattleSession * session)
{
	const tw::battle::BattleState & state = session->getEngine()->getState();
	tw::Match * match = session->getMatch();

	std::cout << "Combat " << session->getId() << " terminé : équipe " << state.winnerTeam << " gagnante ("
		<< tw::battle::toString(state.endReason) << ", tour " << state.round << ")." << std::endl;

	session->markEnded();
	match->setBattlePayload(NULL);

	for (tw::Player * player : session->getParticipants())
		player->setHasJoinBattle(false);

	// Déclenche la mise à jour des listes (admin, spectateurs).
	match->setWinnerTeam(state.winnerTeam);
}

void TWParser::onPlayerConnectionChanged(tw::Player * player, bool connected)
{
	BattleSession * session = sessionOfPlayer(player);
	if (session == NULL || session->getPhase() != BattleSession::Phase::BATTLE)
		return;

	session->getEngine()->setConnected(session->fighterIdOf(player), connected, nowMs());
	broadcastBattleEvents(session);
}

void TWParser::tickBattles()
{
	std::int64_t now = nowMs();

	std::vector<BattleSession*> active;
	for (auto & entry : sessions)
		active.push_back(entry.second);

	for (BattleSession * session : active)
	{
		if (session->getPhase() == BattleSession::Phase::CLASS_SELECTION)
		{
			if (now < session->getClassSelectionDeadline())
				continue;

			// Délai écoulé : les classes manquantes sont tirées au hasard, à condition
			// qu'au moins un joueur de chaque équipe soit présent.
			bool present[2] = { false, false };
			for (tw::Player * player : session->getParticipants())
			{
				if (getClientStateFromPlayer(player) != NULL && player->getHasJoinBattle())
					present[session->getMatch()->playerIsInTeam1(player) ? 0 : 1] = true;
			}

			if (present[0] && present[1])
				startBattle(session);
			else
				session->postponeClassSelection(now + CLASS_SELECTION_RETRY_MS);
		}
		else if (session->getPhase() == BattleSession::Phase::BATTLE)
		{
			session->getEngine()->tick(now);
			broadcastBattleEvents(session);
		}
	}

	for (auto it = sessions.begin(); it != sessions.end();)
	{
		if (it->second->getPhase() == BattleSession::Phase::ENDED)
		{
			delete it->second;
			it = sessions.erase(it);
		}
		else
		{
			it++;
		}
	}
}
