// Tournois : administration (création, démarrage, résultats forcés...), lancement automatique
// des matchs prêts et report des résultats des combats.
#include "TWParser.h"

#include <iostream>

#include <Message.h>
#include <PlayerManager.h>
#include <TournamentJson.h>

using namespace tw::tournament;

namespace
{
	std::string encode(const std::string & op, const nlohmann::json & body)
	{
		return tw::protocol::Message::encode(op, body);
	}

	ResultReason reasonFromEngine(tw::battle::EndReason reason)
	{
		switch (reason)
		{
		case tw::battle::EndReason::ROUND_LIMIT: return ResultReason::ROUND_LIMIT;
		case tw::battle::EndReason::FORFEIT: return ResultReason::FORFEIT;
		case tw::battle::EndReason::ADMIN: return ResultReason::ADMIN;
		default: return ResultReason::KO;
		}
	}
}

void TWParser::loadTournaments()
{
	std::string log;
	tournaments.load(log);
	std::cout << log << std::endl;
}

std::string TWParser::teamName(int teamId)
{
	const tw::Team * team = teamStore.findTeam(teamId);
	return team != NULL ? team->name : "Équipe " + std::to_string(teamId);
}

nlohmann::json TWParser::tournamentListJson()
{
	nlohmann::json list = nlohmann::json::array();
	for (int id : tournaments.ids())
	{
		const Tournament & tournament = tournaments.find(id)->get();
		list.push_back({
			{ "id", id },
			{ "name", tournament.name },
			{ "format", toString(tournament.settings.format) },
			{ "status", toString(tournament.status) },
			{ "teams", tournament.teamIds.size() },
			{ "paused", tournaments.isPaused(id) }
		});
	}
	return list;
}

nlohmann::json TWParser::tournamentStateJson(int id)
{
	const TournamentEngine * engine = tournaments.find(id);
	if (engine == NULL)
		return nlohmann::json();

	const Tournament & tournament = engine->get();
	nlohmann::json state = toJson(tournament);
	state["paused"] = tournaments.isPaused(id);

	nlohmann::json labels = nlohmann::json::object();
	for (const auto & entry : tournament.matches)
		labels[std::to_string(entry.first)] = matchLabel(tournament, entry.second);
	state["labels"] = labels;

	nlohmann::json names = nlohmann::json::object();
	for (int teamId : tournament.teamIds)
		names[std::to_string(teamId)] = teamName(teamId);
	state["teamNames"] = names;

	// Classements des phases de poules et de rondes suisses.
	nlohmann::json standings = nlohmann::json::array();
	for (int stageIndex = 0; stageIndex < (int)tournament.stages.size(); stageIndex++)
	{
		const Stage & stage = tournament.stages[stageIndex];
		if (!stage.built || (stage.type != StageType::ROUND_ROBIN_POOLS && stage.type != StageType::SWISS))
			continue;

		int groups = stage.type == StageType::ROUND_ROBIN_POOLS ? (int)stage.pools.size() : 1;
		for (int group = 0; group < groups; group++)
		{
			nlohmann::json rows = nlohmann::json::array();
			for (const StandingRow & row : engine->standings(stageIndex, group))
			{
				rows.push_back({
					{ "team", row.teamId }, { "played", row.played }, { "wins", row.wins }, { "losses", row.losses },
					{ "points", row.points }, { "hp", row.hpDifference }, { "buchholz", row.buchholz }
				});
			}

			std::string title = stage.type == StageType::SWISS ? stage.name : std::string("Poule ") + (char)('A' + group);
			standings.push_back({ { "stage", stageIndex }, { "title", title }, { "rows", rows } });
		}
	}
	state["standings"] = standings;

	nlohmann::json ranking = nlohmann::json::array();
	for (const RankingEntry & entry : engine->finalRanking())
		ranking.push_back({ { "rank", entry.rank }, { "team", entry.teamId } });
	state["ranking"] = ranking;

	return state;
}

void TWParser::sendTournamentAck(ClientState * client, const std::string & error, const std::string & success, int id)
{
	send(client, encode("UA", { { "ok", error.empty() }, { "message", error.empty() ? success : error }, { "id", id } }));
}

void TWParser::notifyTournamentsChanged()
{
	if (admin == NULL)
		return;

	send(admin, encode("UL", { { "tournaments", tournamentListJson() } }));
	if (adminWatchedTournament != 0 && tournaments.find(adminWatchedTournament) != NULL)
		send(admin, encode("UT", tournamentStateJson(adminWatchedTournament)));
}

BattleSession * TWParser::sessionOfTournamentMatch(int tournamentId, int matchId)
{
	for (auto & entry : sessions)
	{
		BattleSession * session = entry.second;
		if (session->getPhase() != BattleSession::Phase::ENDED && session->getTournamentId() == tournamentId && session->getTournamentMatchId() == matchId)
			return session;
	}
	return NULL;
}

void TWParser::handleTournamentAdminMessage(ClientState * client, const std::string & op, const nlohmann::json & body)
{
	int id = body.value("id", 0);
	std::string error;

	try
	{
		if (op == "UL")
		{
			send(client, encode("UL", { { "tournaments", tournamentListJson() } }));
			return;
		}
		else if (op == "UG")
		{
			adminWatchedTournament = id;
			if (tournaments.find(id) != NULL)
				send(client, encode("UT", tournamentStateJson(id)));
			return;
		}
		else if (op == "UC" || op == "UE")
		{
			Settings settings = settingsFromJson(body.value("settings", nlohmann::json::object()));
			std::vector<int> teams = body.value("teams", std::vector<int>());
			std::string name = body.value("name", std::string());
			if (op == "UC")
			{
				error = tournaments.create(name, settings, teams, id);
				if (error.empty())
					adminWatchedTournament = id;
			}
			else
			{
				error = tournaments.update(id, name, settings, teams);
			}
			sendTournamentAck(client, error, op == "UC" ? "Tournoi créé." : "Tournoi enregistré.", id);
		}
		else if (op == "UB")
		{
			error = tournaments.start(id);
			sendTournamentAck(client, error, "Tournoi démarré : les matchs vont être lancés automatiquement.", id);
		}
		else if (op == "UP")
		{
			bool paused = body.value("paused", true);
			error = tournaments.setPaused(id, paused);
			sendTournamentAck(client, error, paused ? "Lancement des matchs suspendu." : "Lancement des matchs repris.", id);
		}
		else if (op == "UD")
		{
			error = tournaments.remove(id);
			if (error.empty() && adminWatchedTournament == id)
				adminWatchedTournament = 0;
			sendTournamentAck(client, error, "Tournoi supprimé.", id);
		}
		else if (op == "UF")
		{
			// Résultat imposé par l'admin. Un combat en cours est arrêté avec ce vainqueur.
			int matchId = body.at("match").get<int>();
			int winner = body.at("winner").get<int>();
			BattleSession * session = sessionOfTournamentMatch(id, matchId);
			const TournamentEngine * engine = tournaments.find(id);
			const TMatch * match = engine != NULL ? engine->findMatch(matchId) : NULL;

			if (match == NULL)
			{
				error = "Match introuvable.";
			}
			else if (session != NULL)
			{
				int side = winner == match->teamA ? 1 : 2;
				if (session->getPhase() == BattleSession::Phase::BATTLE)
				{
					session->getEngine()->declareWinner(side, nowMs());
					broadcastBattleEvents(session);
				}
				else
				{
					finishWithoutBattle(session, side, ResultReason::ADMIN);
				}
			}
			else
			{
				MatchResult result;
				result.winnerTeamId = winner;
				result.reason = ResultReason::ADMIN;
				result.hpPercentA = winner == match->teamA ? 100 : 0;
				result.hpPercentB = winner == match->teamB ? 100 : 0;
				error = tournaments.forceResult(id, matchId, result, body.value("cascade", false));
			}
			sendTournamentAck(client, error, "Résultat enregistré.", id);
		}
		else if (op == "US")
		{
			// Arrêt d'un combat en cours : décision aux points de vie restants.
			BattleSession * session = sessionOfTournamentMatch(id, body.at("match").get<int>());
			if (session == NULL || session->getPhase() != BattleSession::Phase::BATTLE)
			{
				error = "Ce match n'est pas en cours de combat.";
			}
			else
			{
				session->getEngine()->stopByDecision(nowMs());
				broadcastBattleEvents(session);
			}
			sendTournamentAck(client, error, "Combat arrêté : décision aux points de vie.", id);
		}
		else if (op == "UX")
		{
			// Match à rejouer : le combat en cours est annulé sans résultat.
			int matchId = body.at("match").get<int>();
			BattleSession * session = sessionOfTournamentMatch(id, matchId);
			if (session != NULL)
				cancelSession(session);
			error = tournaments.resetMatch(id, matchId);
			sendTournamentAck(client, error, "Le match sera relancé.", id);
		}
	}
	catch (const nlohmann::json::exception &)
	{
		sendTournamentAck(client, "Requête invalide.", "", id);
	}

	notifyTournamentsChanged();
}

void TWParser::dispatchTournamentMatches()
{
	int running = 0;
	for (auto & entry : sessions)
	{
		if (entry.second->getPhase() != BattleSession::Phase::ENDED)
			running++;
	}

	tw::TournamentService::DispatchOptions options;
	options.maxConcurrentMatches = config.maxConcurrentMatches;
	options.restMs = (std::int64_t)config.restSeconds * 1000;

	auto isTeamBusy = [this](int teamId) {
		return isTeamAvailableForMatchCreation(teamId) == -1;
	};

	for (const tw::TournamentService::LaunchRequest & request : tournaments.nextLaunches(isTeamBusy, running, nowMs(), options))
	{
		const Tournament & tournament = tournaments.find(request.tournamentId)->get();
		const TMatch * match = tournaments.find(request.tournamentId)->findMatch(request.matchId);
		std::vector<tw::Player*> teamA = teamIdToPlayerList[request.teamA];
		std::vector<tw::Player*> teamB = teamIdToPlayerList[request.teamB];

		// Équipe supprimée ou désactivée en cours de tournoi : forfait.
		if (teamA.size() < 2 || teamB.size() < 2)
		{
			MatchResult result;
			result.reason = ResultReason::FORFEIT;
			result.winnerTeamId = teamA.size() < 2 ? request.teamB : request.teamA;
			tournaments.forceResult(request.tournamentId, request.matchId, result, false);
			continue;
		}

		std::string name = tournament.name + " - " + matchLabel(tournament, *match);
		tw::Match * m = new tw::Match(name);
		m->setTeam1Players(teamA[0], teamA[1]);
		m->setTeam2Players(teamB[0], teamB[1]);

		// Carte tirée de façon reproductible à partir de la graine du tournoi.
		std::uint32_t pick = tournament.rngSeed + (std::uint32_t)request.matchId * 7919u;
		m->setEnvironment(tournamentEnvironments[pick % tournamentEnvironments.size()]);

		m->addEventListener(this);
		tw::PlayerManager::addMatch(m);
		createSession(m);
		BattleSession * session = sessionOfMatch(m);
		session->setTournamentMatch(request.tournamentId, request.matchId);
		tournaments.markLaunched(request, session->getId());

		std::cout << "Lancement : " << name << " (" << teamName(request.teamA) << " contre " << teamName(request.teamB) << ")" << std::endl;
		notifyMatchCreated(m);
	}
}

void TWParser::reportTournamentResult(BattleSession * session, int winnerSide, ResultReason reason, double hpPercent1, double hpPercent2, int rounds)
{
	if (session->getTournamentId() == 0)
		return;

	const TournamentEngine * engine = tournaments.find(session->getTournamentId());
	const TMatch * match = engine != NULL ? engine->findMatch(session->getTournamentMatchId()) : NULL;
	if (match == NULL)
		return;

	// Côté 1 du combat = équipe A du match de tournoi.
	MatchResult result;
	result.winnerTeamId = winnerSide == 1 ? match->teamA : match->teamB;
	result.reason = reason;
	result.hpPercentA = hpPercent1;
	result.hpPercentB = hpPercent2;
	result.rounds = rounds;

	int teamA = match->teamA;
	int teamB = match->teamB;
	std::string error = tournaments.reportResult(session->getTournamentId(), session->getTournamentMatchId(), result, session->getSeed());
	if (!error.empty())
		std::cerr << "Résultat non enregistré : " << error << std::endl;

	tournaments.markTeamFinished(teamA, nowMs());
	tournaments.markTeamFinished(teamB, nowMs());
}

void TWParser::finishWithoutBattle(BattleSession * session, int winnerSide, ResultReason reason)
{
	tw::Match * match = session->getMatch();
	std::cout << "Match " << session->getId() << " : victoire de l'équipe " << winnerSide << " sans combat (" << toString(reason) << ")." << std::endl;

	reportTournamentResult(session, winnerSide, reason, winnerSide == 1 ? 100 : 0, winnerSide == 2 ? 100 : 0, 0);

	session->markEnded();
	match->setBattlePayload(NULL);
	for (tw::Player * player : session->getParticipants())
	{
		player->setHasJoinBattle(false);
		ClientState * client = getClientStateFromPlayer(player);
		if (client != NULL)
			send(client, "HW\n");
	}
	match->setWinnerTeam(winnerSide);
}

void TWParser::cancelSession(BattleSession * session)
{
	tw::Match * match = session->getMatch();
	// Combat annulé : sa rediffusion n'a pas d'intérêt.
	stopRecording(session, nlohmann::json::object(), false);
	session->markEnded();
	match->setBattlePayload(NULL);

	for (tw::Player * player : session->getParticipants())
	{
		player->setHasJoinBattle(false);
		ClientState * client = getClientStateFromPlayer(player);
		if (client != NULL)
			send(client, "HW\n");
	}

	// Les spectateurs reviennent à la liste des combats.
	for (tw::net::ConnId spectator : session->spectators)
	{
		auto it = clients.find(spectator);
		if (it != clients.end())
			send(it->second, "HW\n");
	}
	session->spectators.clear();

	// Le match annulé n'a pas de vainqueur : il disparaît des listes.
	match->setWinnerTeam(0);
}
