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

	// Les combattants portent le nom affiché des joueurs (jamais leur login).
	std::map<tw::Player*, std::string> names;
	for (tw::Player * player : session->getParticipants())
		names[player] = displayNameOf(player);

	session->startBattle(nowMs(), connected, names);
	publicDirty = true;
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
	nlohmann::json snapshot = session->getEngine()->snapshot(fighterId, nowMs());

	// Noms des équipes et du match, pour l'affichage (bandeau spectateur, écran de fin).
	tw::Match * match = session->getMatch();
	snapshot["teams"] = nlohmann::json::array({ teamName(match->getTeam1()[0]->getTeamNumber()), teamName(match->getTeam2()[0]->getTeamNumber()) });
	snapshot["title"] = match->getMatchName();
	send(client, encode("BI", snapshot));
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

	for (tw::net::ConnId spectator : session->spectators)
	{
		auto it = clients.find(spectator);
		if (it != clients.end())
			send(it->second, message);
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

	tw::tournament::ResultReason reason = tw::tournament::ResultReason::KO;
	if (state.endReason == tw::battle::EndReason::ROUND_LIMIT)
		reason = tw::tournament::ResultReason::ROUND_LIMIT;
	else if (state.endReason == tw::battle::EndReason::FORFEIT)
		reason = tw::tournament::ResultReason::FORFEIT;
	else if (state.endReason == tw::battle::EndReason::ADMIN)
		reason = tw::tournament::ResultReason::ADMIN;
	reportTournamentResult(session, state.winnerTeam, reason, session->getEngine()->teamHpPercent(1), session->getEngine()->teamHpPercent(2), state.round);

	session->markEnded();
	match->setBattlePayload(NULL);
	publicDirty = true;

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

void TWParser::trackAbsences(BattleSession * session, std::int64_t now)
{
	// Une équipe entièrement déconnectée pendant le délai de forfait perd le combat.
	const tw::battle::BattleState & state = session->getEngine()->getState();
	for (int team = 1; team <= 2; team++)
	{
		bool someoneConnected = false;
		for (const tw::battle::Fighter & fighter : state.fighters)
		{
			if (fighter.team == team && fighter.connected)
				someoneConnected = true;
		}

		if (someoneConnected)
		{
			session->absentSince[team] = 0;
		}
		else if (session->absentSince[team] == 0)
		{
			session->absentSince[team] = now;
		}
		else if (now - session->absentSince[team] >= (std::int64_t)config.forfeitSeconds * 1000)
		{
			std::cout << "Combat " << session->getId() << " : forfait de l'équipe " << team << " (déconnectée)." << std::endl;
			session->getEngine()->forfeit(team, now);
			return;
		}
	}
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
			{
				startBattle(session);
			}
			else if (present[0] || present[1])
			{
				// Une seule équipe présente : l'autre perd par forfait après le délai.
				int absentTeam = present[0] ? 2 : 1;
				if (session->absentSince[absentTeam] == 0)
					session->absentSince[absentTeam] = now;

				if (now - session->absentSince[absentTeam] >= (std::int64_t)config.forfeitSeconds * 1000)
					finishWithoutBattle(session, absentTeam == 1 ? 2 : 1, tw::tournament::ResultReason::FORFEIT);
				else
					session->postponeClassSelection(now + 1000);
			}
			else
			{
				session->postponeClassSelection(now + CLASS_SELECTION_RETRY_MS);
			}
		}
		else if (session->getPhase() == BattleSession::Phase::BATTLE)
		{
			trackAbsences(session, now);
			session->getEngine()->tick(now);
			broadcastBattleEvents(session);
		}
	}

	dispatchTournamentMatches();
	if (tournaments.takeChanged())
	{
		notifyTournamentsChanged();
		publicDirty = true;
	}

	publishPublicState();
	refreshSessionList();

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
