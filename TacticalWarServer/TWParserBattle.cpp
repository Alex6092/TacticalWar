// Combats : sessions (choix des classes puis BattleEngine), actions des joueurs,
// diffusion des événements et fin de combat.
#include "TWParser.h"
#include <Achievements.h>

#include <algorithm>
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
	// Matchs amicaux : mode de server.json (un match de tournoi prend ensuite le réglage du tournoi).
	session->setZonePoints(config.battleMode == "ZONE" ? config.zonePoints : 0);
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

std::string TWParser::classSelectionMessage(tw::Player * player)
{
	BattleSession * session = sessionOfPlayer(player);
	int talents = session != NULL ? session->talentSlots(player) : 0;
	nlohmann::json body = { { "talents", talents } };
	// Phase de bannissement en cours : secondes restantes.
	if (session != NULL && session->getPhase() == BattleSession::Phase::BAN)
		body["ban"] = std::max<std::int64_t>(1, (session->getBanDeadline() - nowMs() + 999) / 1000);
	return encode("HC", body);
}

void TWParser::handleBan(ClientState * client, tw::Player * player, const std::string & body)
{
	BattleSession * session = sessionOfPlayer(player);
	if (session == NULL || !player->getHasJoinBattle())
		return;

	// PB{"class": id} : le premier bannissement d'un joueur de l'équipe compte.
	nlohmann::json pick = nlohmann::json::parse(body, nullptr, false);
	int classId = pick.is_object() ? pick.value("class", 0) : 0;
	if (!session->ban(player, classId))
		return;

	int team = session->teamOf(player);
	std::cout << "Combat " << session->getId() << " : l'equipe " << team << " bannit la classe " << classId << std::endl;
	for (tw::Player * mate : session->getParticipants())
	{
		ClientState * mateClient = getClientStateFromPlayer(mate);
		if (session->teamOf(mate) == team && mateClient != NULL && mate->getHasJoinBattle())
			sendBanState(session, mateClient, mate);
	}
	publicDirty = true;
	if (session->allTeamsBanned())
		finishBanPhase(session);
}

void TWParser::handleViewClass(ClientState * client, tw::Player * player, const std::string & body)
{
	BattleSession * session = sessionOfPlayer(player);
	if (session == NULL || !player->getHasJoinBattle())
		return;

	// PV{"class": id} : la classe que le joueur regarde, montrée à son coéquipier.
	nlohmann::json view = nlohmann::json::parse(body, nullptr, false);
	int classId = view.is_object() ? view.value("class", 0) : 0;
	if (gameData.findClass(classId) != nullptr && session->setViewing(player, classId))
		sendTeammateStates(session, player);
}

nlohmann::json TWParser::teammateState(BattleSession * session, tw::Player * player)
{
	int chosen = session->chosenClass(player);
	return {
		{ "name", displayNameOf(player) },
		{ "class", chosen },
		{ "viewing", session->viewingClass(player) },
		{ "locked", chosen != 0 },
		{ "present", getClientStateFromPlayer(player) != NULL && player->getHasJoinBattle() }
	};
}

void TWParser::sendTeammateStates(BattleSession * session, tw::Player * about)
{
	// Seulement aux coéquipiers, pendant le bannissement et le choix des classes.
	if (session->getPhase() != BattleSession::Phase::BAN && session->getPhase() != BattleSession::Phase::CLASS_SELECTION)
		return;
	std::string message = encode("PT", teammateState(session, about));
	int team = session->teamOf(about);
	for (tw::Player * mate : session->getParticipants())
	{
		ClientState * client = getClientStateFromPlayer(mate);
		if (mate != about && session->teamOf(mate) == team && client != NULL && mate->getHasJoinBattle())
			send(client, message);
	}
}

void TWParser::sendBanState(BattleSession * session, ClientState * client, tw::Player * player)
{
	if (!session->hasBanPhase())
		return;
	int team = session->teamOf(player);
	bool done = session->getPhase() != BattleSession::Phase::BAN;
	nlohmann::json body = { { "banned", session->bannedBy(team) }, { "done", done } };
	// La classe interdite par l'adversaire n'est connue qu'à la fin de la phase.
	if (done)
		body["forbidden"] = session->forbiddenClass(team);
	send(client, encode("BB", body));
}

void TWParser::finishBanPhase(BattleSession * session)
{
	// Le délai du choix des classes part de la fin du bannissement.
	session->endBanPhase(nowMs() + (std::int64_t)config.classSelectionSeconds * 1000);
	std::cout << "Combat " << session->getId() << " : classes interdites " << session->forbiddenClass(1) << " (equipe 1), "
		<< session->forbiddenClass(2) << " (equipe 2)." << std::endl;
	for (tw::Player * player : session->getParticipants())
	{
		ClientState * client = getClientStateFromPlayer(player);
		if (client != NULL && player->getHasJoinBattle())
			sendBanState(session, client, player);
	}
	publicDirty = true;
}

void TWParser::handlePickClass(ClientState * client, tw::Player * player, const std::string & body)
{
	BattleSession * session = sessionOfPlayer(player);
	if (session == NULL || !player->getHasJoinBattle())
		return;

	// PC{"class": id, "spells": [indices]} ; ancien format PC<classId> : sorts par défaut.
	int classId = 0;
	std::vector<int> spells;
	std::vector<std::string> talents;
	bool forTeammate = false;
	if (!body.empty() && body[0] == '{')
	{
		nlohmann::json pick = nlohmann::json::parse(body, nullptr, false);
		if (pick.is_object())
		{
			classId = pick.value("class", 0);
			forTeammate = pick.value("teammate", false);
			if (pick.contains("spells") && pick["spells"].is_array())
			{
				for (const nlohmann::json & index : pick["spells"])
				{
					if (index.is_number_integer())
						spells.push_back(index.get<int>());
				}
			}
			if (pick.contains("talents") && pick["talents"].is_array())
			{
				for (const nlohmann::json & talent : pick["talents"])
				{
					if (talent.is_string())
						talents.push_back(talent.get<std::string>());
				}
			}
		}
	}
	else
	{
		classId = std::atoi(body.c_str());
	}

	// PC{..., "teammate": true} : choix pour le coéquipier absent (refusé s'il est là).
	tw::Player * target = forTeammate ? absentTeammate(session, player) : player;
	if (target != NULL && session->chooseClass(target, classId, spells, talents))
	{
		if (target == player)
			send(client, "PO" + std::to_string(classId) + "\n");
		sendTeammateStates(session, target);
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
	refreshPilots(session);
	publicDirty = true;
	session->getMatch()->setMatchStatus(tw::MatchStatus::STARTED);

	// Les clients reçoivent l'état complet : les événements produits jusqu'ici y sont déjà.
	session->getEngine()->flushEvents();
	startRecording(session);

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
	{
		// La carte est envoyée avec ses règles : le client n'a pas besoin d'en avoir une copie à jour.
		auto map = mapMessages.find(session->getMapId());
		if (map != mapMessages.end())
			send(client, map->second);
		send(client, "HG" + std::to_string(session->getMapId()) + "\n");
	}

	int fighterId = player != NULL ? session->fighterIdOf(player) : -1;
	send(client, encode("BI", battleSnapshot(session, fighterId)));
}

nlohmann::json TWParser::battleSnapshot(BattleSession * session, int fighterId)
{
	nlohmann::json snapshot = session->getEngine()->snapshot(fighterId, nowMs());

	// Noms des équipes et du match, pour l'affichage (bandeau spectateur, écran de fin).
	tw::Match * match = session->getMatch();
	snapshot["teams"] = nlohmann::json::array({ teamName(match->getTeam1()[0]->getTeamNumber()), teamName(match->getTeam2()[0]->getTeamNumber()) });
	if (session->hasBanPhase())
		snapshot["forbidden"] = nlohmann::json::array({ session->forbiddenClass(1), session->forbiddenClass(2) });
	snapshot["title"] = match->getMatchName();
	return snapshot;
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
	// Déplacement, sort et fin de tour : pour le combattant actif s'il est piloté par ce joueur.
	bool turnAction = op == "Cm" || op == "CL" || op == "Ct";
	int fighterId = turnAction ? session->actingFighter(player) : session->fighterIdOf(player);
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
		else if (op == "CE")
		{
			result = config.emotesEnabled ? engine->emote(fighterId, body.at("id").get<int>(), now)
				: tw::battle::ActionResult::failure("Les émotes sont désactivées par l'organisateur.");
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

void TWParser::handlePing(ClientState * client, const nlohmann::json & body)
{
	// Signal d'un joueur à son équipe : relayé à ses seuls coéquipiers. Ni les adversaires ni les
	// spectateurs ne le reçoivent (l'écran projeté est visible des joueurs), et il n'est pas enregistré.
	tw::Player * player = getPlayerFromClientState(client);
	BattleSession * session = player != NULL ? sessionOfPlayer(player) : NULL;
	if (session == NULL || session->getPhase() != BattleSession::Phase::BATTLE)
		return;

	const tw::battle::BattleState & state = session->getEngine()->getState();
	const tw::battle::Fighter * fighter = state.findFighter(session->fighterIdOf(player));
	tw::battle::Cell cell = { body.value("x", -1), body.value("y", -1) };
	if (fighter == nullptr || !session->getEngine()->getMap().contains(cell))
		return;

	std::int64_t now = nowMs();
	std::deque<std::int64_t> & recent = recentPings[player];
	while (!recent.empty() && now - recent.front() > 5000)
		recent.pop_front();
	if (recent.size() >= 3)
		return;
	recent.push_back(now);

	std::string message = encode("BG", { { "f", fighter->id }, { "x", cell.x }, { "y", cell.y } });
	for (tw::Player * mate : session->getParticipants())
	{
		const tw::battle::Fighter * other = state.findFighter(session->fighterIdOf(mate));
		ClientState * mateClient = getClientStateFromPlayer(mate);
		if (other != nullptr && other->team == fighter->team && mateClient != NULL && mate->getHasJoinBattle())
			send(mateClient, message);
	}
}

void TWParser::broadcastBattleEvents(BattleSession * session)
{
	tw::battle::BattleEngine * engine = session->getEngine();
	if (engine == NULL || !engine->hasPendingEvents())
		return;

	nlohmann::json batch = engine->flushEvents();
	recordBatch(session, batch);
	std::string message = encode("BV", batch);
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
	else if (state.endReason == tw::battle::EndReason::OBJECTIVE)
		reason = tw::tournament::ResultReason::OBJECTIVE;
	// Bilan des joueurs : enregistré avec le résultat du tournoi, et affiché sur la page projetée.
	std::vector<tw::tournament::PlayerRecord> players;
	nlohmann::json mvp;
	for (const tw::battle::Fighter & fighter : state.fighters)
	{
		const tw::battle::ClassDef * classDef = gameData.findClass(fighter.classId);
		tw::tournament::PlayerRecord player;
		player.name = fighter.name;
		player.className = classDef != nullptr ? classDef->name : std::string();
		player.side = fighter.team;
		player.dealt = fighter.record.dealt;
		player.healed = fighter.record.healed;
		player.shielded = fighter.record.shielded;
		player.kills = fighter.record.kills;
		player.mvp = fighter.id == state.mvpFighterId;
		player.badges = fighter.record.badges;
		players.push_back(player);
		if (player.mvp)
		{
			// Hauts faits du MVP par leur nom, pour la page projetée.
			nlohmann::json badges = nlohmann::json::array();
			for (const std::string & id : player.badges)
			{
				const tw::battle::AchievementDef * achievement = tw::battle::findAchievement(id);
				badges.push_back(achievement != nullptr ? std::string(achievement->name) : id);
			}
			mvp = { { "name", player.name }, { "class", player.className }, { "side", player.side }, { "dealt", player.dealt },
				{ "healed", player.healed }, { "shielded", player.shielded }, { "kills", player.kills }, { "badges", badges } };
		}
	}
	reportTournamentResult(session, state.winnerTeam, reason, session->getEngine()->teamHpPercent(1), session->getEngine()->teamHpPercent(2), state.round, players);

	recentBattles.push_front({
		{ "name", match->getMatchName() },
		{ "tournament", session->getTournamentId() },
		{ "match", session->getTournamentMatchId() },
		{ "teams", nlohmann::json::array({ teamName(match->getTeam1()[0]->getTeamNumber()), teamName(match->getTeam2()[0]->getTeamNumber()) }) },
		{ "winner", state.winnerTeam },
		{ "reason", tw::battle::toString(state.endReason) },
		{ "rounds", state.round },
		{ "mvp", mvp }
	});
	while (recentBattles.size() > 6)
		recentBattles.pop_back();
	stopRecording(session, { { "winner", state.winnerTeam }, { "reason", tw::battle::toString(state.endReason) }, { "rounds", state.round } }, true);

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
	refreshPilots(session);
	broadcastBattleEvents(session);
}

bool TWParser::isPresent(tw::Player * player)
{
	return getClientStateFromPlayer(player) != NULL && player->getHasJoinBattle();
}

tw::Player * TWParser::absentTeammate(BattleSession * session, tw::Player * player)
{
	for (tw::Player * mate : session->getParticipants())
	{
		if (mate != player && session->teamOf(mate) == session->teamOf(player) && !isPresent(mate))
			return mate;
	}
	return NULL;
}

void TWParser::refreshPilots(BattleSession * session)
{
	// Combattant d'un joueur absent dont un coéquipier est là : piloté par ce coéquipier.
	tw::battle::BattleEngine * engine = session->getEngine();
	if (engine == NULL)
		return;
	for (tw::Player * player : session->getParticipants())
	{
		bool mateHere = false;
		for (tw::Player * mate : session->getParticipants())
			mateHere = mateHere || (mate != player && session->teamOf(mate) == session->teamOf(player) && isPresent(mate));
		engine->setPiloted(session->fighterIdOf(player), !isPresent(player) && mateHere, nowMs());
	}
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
		if (session->getPhase() == BattleSession::Phase::BAN)
		{
			// Une équipe qui n'a pas banni à temps n'interdit rien.
			if (now >= session->getBanDeadline())
				finishBanPhase(session);
		}
		else if (session->getPhase() == BattleSession::Phase::CLASS_SELECTION)
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
