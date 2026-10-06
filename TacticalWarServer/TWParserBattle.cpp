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
	// Vote d'abandon : les autres joueurs présents de l'équipe ont ce délai pour le confirmer.
	const std::int64_t SURRENDER_VOTE_MS = 30 * 1000;

	std::string encode(const std::string & op, const nlohmann::json & body)
	{
		return tw::protocol::Message::encode(op, body);
	}

	// Sorts (indices dans les sorts de la classe) et talents d'un message PC ou PV.
	std::vector<int> spellList(const nlohmann::json & body)
	{
		std::vector<int> spells;
		if (body.contains("spells") && body["spells"].is_array())
		{
			for (const nlohmann::json & index : body["spells"])
			{
				if (index.is_number_integer())
					spells.push_back(index.get<int>());
			}
		}
		return spells;
	}

	std::vector<std::string> talentList(const nlohmann::json & body)
	{
		std::vector<std::string> talents;
		if (body.contains("talents") && body["talents"].is_array())
		{
			for (const nlohmann::json & talent : body["talents"])
			{
				if (talent.is_string())
					talents.push_back(talent.get<std::string>());
			}
		}
		return talents;
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
	session->setMapBonuses(config.mapBonuses);
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
	nlohmann::json body = { { "talents", talents }, { "team", session != NULL ? session->teamOf(player) : 0 } };
	// Phase de bannissement en cours : secondes restantes ; sinon, secondes restantes pour choisir.
	if (session != NULL && session->getPhase() == BattleSession::Phase::BAN)
		body["ban"] = std::max<std::int64_t>(1, (session->getBanDeadline() - nowMs() + 999) / 1000);
	else if (session != NULL && session->getPhase() == BattleSession::Phase::CLASS_SELECTION)
		body["seconds"] = std::max<std::int64_t>(0, (session->getClassSelectionDeadline() - nowMs() + 999) / 1000);
	return encode("HC", body);
}

std::string TWParser::classChoiceMessage(BattleSession * session, tw::Player * player)
{
	nlohmann::json body = {
		{ "class", session->chosenClass(player) },
		{ "spells", session->chosenSpells(player) },
		{ "talents", session->chosenTalents(player) },
		{ "appearance", session->appearanceOf(player) }
	};
	// Choix fait par le coéquipier pendant une absence : son nom est montré.
	tw::Player * chooser = session->chooserOf(player);
	if (chooser != NULL && chooser != player)
		body["by"] = displayNameOf(chooser);
	return encode("PO", body);
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

	// PV{"class", "spells", "talents", "appearance"} : brouillon du joueur. La classe regardée est
	// montrée à son coéquipier ; le tout est retenu si le délai expire sans verrouillage.
	// Avec "teammate": true, brouillon pour le coéquipier absent (ou le second personnage).
	nlohmann::json view = nlohmann::json::parse(body, nullptr, false);
	if (!view.is_object())
		return;
	BattleSession::Draft draft;
	draft.classId = view.value("class", 0);
	if (gameData.findClass(draft.classId) == nullptr)
		return;
	draft.spells = spellList(view);
	draft.talents = talentList(view);
	if (view.contains("appearance") && view["appearance"].is_string())
		draft.appearance = tw::battle::allowedAppearance(gameData, progressOf(player->getPseudo()), view["appearance"].get<std::string>());
	bool forTeammate = view.contains("teammate") && view["teammate"].is_boolean() && view["teammate"].get<bool>();
	tw::Player * target = forTeammate ? absentTeammate(session, player) : player;
	if (target != NULL && session->setDraft(target, draft))
		sendTeammateStates(session, target);
}

nlohmann::json TWParser::teammateState(BattleSession * session, tw::Player * player)
{
	int chosen = session->chosenClass(player);
	return {
		{ "name", displayNameOf(player) },
		{ "class", chosen },
		{ "viewing", session->viewingClass(player) },
		{ "locked", chosen != 0 },
		{ "appearance", session->appearanceOf(player) },
		{ "present", getClientStateFromPlayer(player) != NULL && player->getHasJoinBattle() },
		// Second personnage d'un joueur seul dans son équipe (jamais présent : il le joue aussi).
		{ "standIn", isStandIn(player) }
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
	// La classe interdite par l'adversaire n'est connue qu'à la fin de la phase, avec le délai du choix.
	if (done)
	{
		body["forbidden"] = session->forbiddenClass(team);
		body["seconds"] = std::max<std::int64_t>(0, (session->getClassSelectionDeadline() - nowMs() + 999) / 1000);
	}
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
	std::string appearance;
	bool forTeammate = false;
	if (!body.empty() && body[0] == '{')
	{
		nlohmann::json pick = nlohmann::json::parse(body, nullptr, false);
		if (pick.is_object())
		{
			classId = pick.value("class", 0);
			forTeammate = pick.value("teammate", false);
			appearance = pick.value("appearance", std::string());
			spells = spellList(pick);
			talents = talentList(pick);
		}
	}
	else
	{
		classId = std::atoi(body.c_str());
	}

	// PC{..., "teammate": true} : choix pour le coéquipier absent (refusé s'il est là).
	tw::Player * target = forTeammate ? absentTeammate(session, player) : player;
	// Refus (délai écoulé, classe interdite ou déjà verrouillée...) : le joueur en est averti.
	std::string refusal = target == NULL ? std::string("Votre coéquipier est là : il choisit lui-même.") : session->choiceRefusal(target, classId);
	if (!refusal.empty())
	{
		send(client, encode("ER", { { "op", "PC" }, { "message", refusal } }));
		return;
	}
	// Apparence : une de celles que le joueur qui choisit a débloquées (sinon classique).
	appearance = tw::battle::allowedAppearance(gameData, progressOf(player->getPseudo()), appearance);
	if (session->chooseClass(target, classId, spells, talents, player))
	{
		session->setAppearance(target, appearance);
		if (target == player && !appearance.empty())
			profiles.setAppearance(player->getPseudo(), appearance);
		if (target == player)
			send(client, classChoiceMessage(session, player));
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

	// Commentateur du combat (phrases tirées avec le numéro du combat).
	tw::battle::Commentary * commentary = new tw::battle::Commentary((std::uint32_t)session->getId() * 2654435761u);
	commentary->setTeamNames(teamName(session->getMatch()->getTeam1()[0]->getTeamNumber()), teamName(session->getMatch()->getTeam2()[0]->getTeamNumber()));
	commentaries[session->getId()].reset(commentary);

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

	// Type de signal : 0 ici, 1 attaquez, 2 repli, 3 danger (absent : ici, pour les anciens clients).
	int kind = std::max(0, std::min(3, body.value("kind", 0)));
	std::string message = encode("BG", { { "f", fighter->id }, { "x", cell.x }, { "y", cell.y }, { "kind", kind } });
	for (tw::Player * mate : session->getParticipants())
	{
		const tw::battle::Fighter * other = state.findFighter(session->fighterIdOf(mate));
		ClientState * mateClient = getClientStateFromPlayer(mate);
		if (other != nullptr && other->team == fighter->team && mateClient != NULL && mate->getHasJoinBattle())
			send(mateClient, message);
	}
}

std::vector<tw::Player*> TWParser::surrenderVoters(BattleSession * session, int team)
{
	// Les joueurs présents de l'équipe (le second personnage d'un joueur seul n'est jamais présent).
	std::vector<tw::Player*> voters;
	for (tw::Player * player : session->getParticipants())
	{
		if (session->teamOf(player) == team && isPresent(player))
			voters.push_back(player);
	}
	return voters;
}

void TWParser::handleSurrender(ClientState * client, const nlohmann::json & body)
{
	// CQ{"vote": true} : le joueur propose (ou confirme) l'abandon de son équipe ; false : il retire son vote.
	tw::Player * player = getPlayerFromClientState(client);
	BattleSession * session = player != NULL ? sessionOfPlayer(player) : NULL;
	if (session == NULL || session->getPhase() != BattleSession::Phase::BATTLE || session->getEngine()->isOver())
	{
		send(client, encode("ER", { { "op", "CQ" }, { "message", "Aucun combat en cours." } }));
		return;
	}
	int team = session->teamOf(player);
	if (team == 0)
		return;

	std::int64_t now = nowMs();
	std::set<tw::Player*> & votes = session->surrenderVotes[team];
	if (body.value("vote", true))
	{
		if (votes.empty())
			session->surrenderSince[team] = now;
		votes.insert(player);
	}
	else
	{
		votes.erase(player);
	}

	// Seul joueur présent, ou tous les joueurs présents d'accord : l'équipe abandonne tout de suite.
	std::vector<tw::Player*> voters = surrenderVoters(session, team);
	bool unanimous = !voters.empty();
	for (tw::Player * voter : voters)
		unanimous = unanimous && votes.count(voter) > 0;
	if (unanimous)
	{
		std::cout << "Combat " << session->getId() << " : abandon de l'equipe " << team << "." << std::endl;
		votes.clear();
		session->surrenderSince[team] = 0;
		session->getEngine()->surrender(team, now);
		broadcastBattleEvents(session);
		return;
	}
	if (votes.empty())
		session->surrenderSince[team] = 0;
	sendSurrenderVote(session, team, player, false);
}

void TWParser::sendSurrenderVote(BattleSession * session, int team, tw::Player * from, bool expired)
{
	const std::set<tw::Player*> & votes = session->surrenderVotes[team];
	std::int64_t remaining = votes.empty() ? 0
		: std::max<std::int64_t>(0, (session->surrenderSince[team] + SURRENDER_VOTE_MS - nowMs() + 999) / 1000);
	int needed = (int)surrenderVoters(session, team).size();
	for (tw::Player * mate : session->getParticipants())
	{
		ClientState * mateClient = getClientStateFromPlayer(mate);
		if (session->teamOf(mate) != team || mateClient == NULL || !mate->getHasJoinBattle())
			continue;
		nlohmann::json body = { { "from", from != NULL ? displayNameOf(from) : std::string() }, { "votes", (int)votes.size() },
			{ "needed", needed }, { "expiresIn", remaining }, { "voted", votes.count(mate) > 0 } };
		if (expired)
			body["expired"] = true;
		send(mateClient, encode("BQ", body));
	}
}

void TWParser::clearSurrenderVotes(BattleSession * session, int team, bool expired)
{
	if (team != 1 && team != 2)
		return;
	if (session->surrenderVotes[team].empty() && session->surrenderSince[team] == 0)
		return;
	session->surrenderVotes[team].clear();
	session->surrenderSince[team] = 0;
	sendSurrenderVote(session, team, NULL, expired);
}

void TWParser::broadcastBattleEvents(BattleSession * session)
{
	tw::battle::BattleEngine * engine = session->getEngine();
	if (engine == NULL || !engine->hasPendingEvents())
		return;

	nlohmann::json batch = engine->flushEvents();
	recordBatch(session, batch);
	auto commentary = commentaries.find(session->getId());
	if (commentary != commentaries.end())
		addComment(session, commentary->second->onEvents(batch.value("ev", nlohmann::json::array()), engine->getState(), nowMs()));
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

void TWParser::addComment(BattleSession * session, const std::string & text)
{
	if (text.empty())
		return;
	comments.push_front({ { "text", text }, { "match", session->getMatch()->getMatchName() }, { "session", session->getId() },
		{ "tournament", session->getTournamentId() }, { "at", nowMs() } });
	while (comments.size() > 30)
		comments.pop_back();
	publicDirty = true;
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
	else if (state.endReason == tw::battle::EndReason::SURRENDER)
		reason = tw::tournament::ResultReason::SURRENDER;
	// Bilan des joueurs : enregistré avec le résultat du tournoi, et affiché sur la page projetée.
	std::vector<tw::tournament::PlayerRecord> players;
	nlohmann::json mvp;
	for (const tw::battle::Fighter & fighter : state.fighters)
	{
		const tw::battle::ClassDef * classDef = gameData.findClass(fighter.classId);
		tw::tournament::PlayerRecord player;
		player.name = fighter.name;
		// Second personnage d'un joueur seul : son bilan revient à ce joueur (sur un nom à lui).
		tw::Player * account = session->playerOfFighter(fighter.id);
		if (isStandIn(account))
		{
			const tw::Team * team = teamStore.findTeam(account->getTeamNumber());
			player.name = team != NULL ? team->players[0].displayName : fighter.name;
			player.standIn = true;
		}
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
	recordProfiles(session);

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

tw::battle::PlayerProgress TWParser::progressOf(const std::string & login) const
{
	tw::PlayerProfile profile = profiles.get(login);
	tw::battle::PlayerProgress progress;
	progress.achievements = profile.achievements;
	progress.wins = profile.wins;
	progress.mvp = profile.mvp;
	progress.puzzles = profile.puzzles;
	return progress;
}

void TWParser::sendAppearances(ClientState * client, const std::string & login, const std::vector<std::string> & fresh)
{
	if (client == NULL)
		return;
	tw::PlayerProfile profile = profiles.get(login);
	nlohmann::json body = {
		{ "unlocked", tw::battle::unlockedAppearances(gameData, progressOf(login)) },
		{ "selected", profile.appearance },
		{ "new", fresh },
		{ "progress", {
			{ "wins", profile.wins },
			{ "mvp", profile.mvp },
			{ "puzzles", (int)profile.puzzles.size() },
			{ "achievements", std::vector<std::string>(profile.achievements.begin(), profile.achievements.end()) }
		} }
	};
	send(client, encode("PA", body));
}

void TWParser::handlePuzzles(ClientState * client, tw::Player * player, const std::string & body)
{
	nlohmann::json request = nlohmann::json::parse(body, nullptr, false);
	if (!request.is_object() || !request.contains("solved") || !request["solved"].is_array())
		return;
	std::vector<std::string> solved;
	for (const nlohmann::json & id : request["solved"])
	{
		if (id.is_string())
			solved.push_back(id.get<std::string>());
	}
	std::vector<std::string> before = tw::battle::unlockedAppearances(gameData, progressOf(player->getPseudo()));
	if (!profiles.addPuzzles(player->getPseudo(), solved))
		return;
	std::vector<std::string> fresh;
	for (const std::string & id : tw::battle::unlockedAppearances(gameData, progressOf(player->getPseudo())))
	{
		if (std::find(before.begin(), before.end(), id) == before.end())
			fresh.push_back(id);
	}
	sendAppearances(client, player->getPseudo(), fresh);
}

void TWParser::recordProfiles(BattleSession * session)
{
	const tw::battle::BattleState & state = session->getEngine()->getState();
	for (const tw::battle::Fighter & fighter : state.fighters)
	{
		tw::Player * player = session->playerOfFighter(fighter.id);
		if (player == NULL || isStandIn(player))
			continue;
		std::string login = player->getPseudo();
		std::vector<std::string> before = tw::battle::unlockedAppearances(gameData, progressOf(login));
		profiles.recordBattle(login, fighter.record.badges, fighter.team == state.winnerTeam, fighter.id == state.mvpFighterId);
		std::vector<std::string> fresh;
		for (const std::string & id : tw::battle::unlockedAppearances(gameData, progressOf(login)))
		{
			if (std::find(before.begin(), before.end(), id) == before.end())
				fresh.push_back(id);
		}
		if (!fresh.empty())
			sendAppearances(getClientStateFromPlayer(player), login, fresh);
	}
}

void TWParser::onPlayerConnectionChanged(tw::Player * player, bool connected)
{
	BattleSession * session = sessionOfPlayer(player);
	if (session == NULL || session->getPhase() != BattleSession::Phase::BATTLE)
		return;

	session->getEngine()->setConnected(session->fighterIdOf(player), connected, nowMs());
	refreshPilots(session);
	// Les votants changent : un vote d'abandon en cours est annulé.
	clearSurrenderVotes(session, session->teamOf(player), false);
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

			// Délai écoulé : le combat commence (classes affichées retenues, sinon au hasard), à
			// condition qu'au moins un joueur de chaque équipe soit présent.
			bool present[2] = { false, false };
			for (tw::Player * player : session->getParticipants())
			{
				if (getClientStateFromPlayer(player) != NULL && player->getHasJoinBattle())
					present[session->getMatch()->playerIsInTeam1(player) ? 0 : 1] = true;
			}

			if (present[0] && present[1])
			{
				// Joueurs non verrouillés : la classe affichée sur leur écran (et leurs sorts) est retenue.
				int drafted = session->lockViewedClasses();
				if (drafted > 0)
					std::cout << "Combat " << session->getId() << " : " << drafted << " classe(s) affichee(s) retenue(s) a l'expiration du delai." << std::endl;
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
			// Vote d'abandon non confirmé à temps : annulé.
			for (int team = 1; team <= 2; team++)
			{
				if (session->surrenderSince[team] != 0 && now - session->surrenderSince[team] >= SURRENDER_VOTE_MS)
					clearSurrenderVotes(session, team, true);
			}
			trackAbsences(session, now);
			session->getEngine()->tick(now);
			broadcastBattleEvents(session);
			// Phrase gardée pour plus tard (trop rapprochée de la précédente).
			auto commentary = commentaries.find(session->getId());
			if (commentary != commentaries.end())
				addComment(session, commentary->second->poll(now));
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
			commentaries.erase(it->first);
			delete it->second;
			it = sessions.erase(it);
		}
		else
		{
			it++;
		}
	}
}
