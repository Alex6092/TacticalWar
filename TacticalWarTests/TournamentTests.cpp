#include <doctest.h>

#include <map>
#include <random>
#include <set>

#include <TournamentEngine.h>
#include <TournamentJson.h>

using namespace tw::tournament;

namespace
{
	std::vector<int> teamIds(int count)
	{
		std::vector<int> ids;
		for (int i = 1; i <= count; i++)
			ids.push_back(i * 10);
		return ids;
	}

	MatchResult resultFor(const TMatch & match, int winner, double hpWinner = 60)
	{
		MatchResult result;
		result.winnerTeamId = winner;
		result.reason = ResultReason::KO;
		result.hpPercentA = winner == match.teamA ? hpWinner : 0;
		result.hpPercentB = winner == match.teamB ? hpWinner : 0;
		result.rounds = 6;
		return result;
	}

	struct SimulationReport
	{
		std::map<int, int> losses;
		std::map<int, int> played;
		std::multiset<std::pair<int, int>> pairings;
		int matchesPlayed = 0;
	};

	// Joue tout le tournoi avec des résultats aléatoires et vérifie les invariants à chaque étape.
	SimulationReport simulate(TournamentEngine & engine, std::mt19937 & rng)
	{
		SimulationReport report;
		int safety = 0;

		while (!engine.isFinished())
		{
			REQUIRE(++safety < 2000);
			std::vector<int> ready = engine.readyMatches();
			REQUIRE_FALSE(ready.empty());

			const TMatch * match = engine.findMatch(ready[0]);
			REQUIRE(match != nullptr);
			REQUIRE(match->teamA > 0);
			REQUIRE(match->teamB > 0);
			REQUIRE(match->teamA != match->teamB);

			REQUIRE(engine.markInProgress(match->id, 1) == "");
			CHECK(engine.markInProgress(match->id, 1) != "");	// déjà en cours

			int winner = std::uniform_int_distribution<int>(0, 1)(rng) == 0 ? match->teamA : match->teamB;
			int loser = winner == match->teamA ? match->teamB : match->teamA;
			std::pair<int, int> pair = std::minmax(match->teamA, match->teamB);
			MatchResult result = resultFor(*match, winner, std::uniform_real_distribution<double>(5, 100)(rng));

			REQUIRE(engine.reportResult(match->id, result) == "");
			report.losses[loser]++;
			report.played[winner]++;
			report.played[loser]++;
			report.pairings.insert(pair);
			report.matchesPlayed++;
		}

		return report;
	}

	void checkCompleteRanking(const TournamentEngine & engine, int teamCount)
	{
		std::vector<RankingEntry> ranking = engine.finalRanking();
		REQUIRE((int)ranking.size() == teamCount);

		std::set<int> teams;
		int previousRank = 0;
		for (const RankingEntry & entry : ranking)
		{
			teams.insert(entry.teamId);
			CHECK(entry.rank >= previousRank);
			CHECK(entry.rank >= 1);
			previousRank = entry.rank;
		}
		CHECK((int)teams.size() == teamCount);
		CHECK(ranking[0].rank == 1);
		CHECK((ranking.size() < 2 || ranking[1].rank == 2));	// Un seul champion
	}
}

TEST_CASE("Settings are validated before starting")
{
	TournamentEngine engine;
	CHECK(engine.start() != "");	// aucune équipe

	REQUIRE(engine.setTeams({ 1, 2, 3 }) == "");
	Settings settings;
	settings.format = Format::POOLS_THEN_BRACKET;
	settings.poolCount = 2;
	REQUIRE(engine.setSettings(settings) == "");
	CHECK(engine.validate() != "");	// une poule n'aurait qu'une équipe

	CHECK(engine.setTeams({ 1, 1, 2 }) != "");
	CHECK(engine.setTeams({ 1, -1 }) != "");

	settings.format = Format::SWISS;
	settings.swissTopCut = 1;
	REQUIRE(engine.setSettings(settings) == "");
	CHECK(engine.validate() != "");
}

TEST_CASE("Pools then bracket: every format size plays to a complete ranking")
{
	for (int teamCount = 4; teamCount <= 17; teamCount++)
	{
		for (int poolCount = 1; poolCount <= 4 && teamCount >= 2 * poolCount; poolCount++)
		{
			CAPTURE(teamCount);
			CAPTURE(poolCount);

			TournamentEngine engine;
			Settings settings;
			settings.format = Format::POOLS_THEN_BRACKET;
			settings.poolCount = poolCount;
			settings.qualifiersPerPool = poolCount == 1 ? 4 : 2;
			REQUIRE(engine.setSettings(settings) == "");
			REQUIRE(engine.setTeams(teamIds(teamCount)) == "");
			REQUIRE(engine.start() == "");

			// Chaque équipe joue une fois contre chaque équipe de sa poule.
			const Stage & pools = engine.get().stages[0];
			std::map<int, int> expectedPoolMatches;
			for (const std::vector<int> & pool : pools.pools)
			{
				CHECK(pool.size() >= 2);
				for (int team : pool)
					expectedPoolMatches[team] = (int)pool.size() - 1;
			}

			std::mt19937 rng(teamCount * 31 + poolCount);
			SimulationReport report = simulate(engine, rng);

			std::map<int, int> poolMatches;
			std::set<std::pair<int, int>> poolPairs;
			for (const auto & entry : engine.get().matches)
			{
				const TMatch & match = entry.second;
				if (match.stageIndex != 0)
					continue;
				poolMatches[match.teamA]++;
				poolMatches[match.teamB]++;
				CHECK(poolPairs.insert(std::minmax(match.teamA, match.teamB)).second);	// pas de doublon
			}
			CHECK(poolMatches == expectedPoolMatches);

			checkCompleteRanking(engine, teamCount);
		}
	}
}

TEST_CASE("Pool qualifiers from the same pool do not meet in the first bracket round")
{
	TournamentEngine engine;
	Settings settings;
	settings.format = Format::POOLS_THEN_BRACKET;
	settings.poolCount = 3;
	settings.qualifiersPerPool = 2;
	REQUIRE(engine.setSettings(settings) == "");
	REQUIRE(engine.setTeams(teamIds(12)) == "");
	REQUIRE(engine.start() == "");

	std::mt19937 rng(7);
	while (!engine.get().stages[1].built)
	{
		const TMatch * match = engine.findMatch(engine.readyMatches()[0]);
		REQUIRE(engine.reportResult(match->id, resultFor(*match, match->teamA)) == "");
	}

	std::map<int, int> poolOf;
	const Stage & pools = engine.get().stages[0];
	for (int pool = 0; pool < (int)pools.pools.size(); pool++)
		for (int team : pools.pools[pool])
			poolOf[team] = pool;

	int firstRoundMatches = 0;
	for (const auto & entry : engine.get().matches)
	{
		const TMatch & match = entry.second;
		if (match.stageIndex == 1 && match.bracket == "W" && match.round == 1 && !match.isBye())
		{
			firstRoundMatches++;
			CHECK(poolOf[match.teamA] != poolOf[match.teamB]);
		}
	}
	CHECK(firstRoundMatches == 2);	// 6 qualifiés dans un tableau de 8 : 2 exempts
	CHECK(engine.get().stages[1].bracketSize == 8);
}

TEST_CASE("Double elimination: everybody but the champion loses exactly twice")
{
	for (int teamCount = 2; teamCount <= 17; teamCount++)
	{
		for (int reset = 0; reset < 2; reset++)
		{
			CAPTURE(teamCount);
			CAPTURE(reset);

			TournamentEngine engine;
			Settings settings;
			settings.format = Format::DOUBLE_ELIMINATION;
			settings.grandFinalReset = reset == 1;
			REQUIRE(engine.setSettings(settings) == "");
			REQUIRE(engine.setTeams(teamIds(teamCount)) == "");
			REQUIRE(engine.start() == "");

			std::mt19937 rng(teamCount * 97 + reset);
			SimulationReport report = simulate(engine, rng);

			std::vector<RankingEntry> ranking = engine.finalRanking();
			checkCompleteRanking(engine, teamCount);
			int champion = ranking[0].teamId;

			for (int team : teamIds(teamCount))
			{
				CAPTURE(team);
				if (team == champion)
					CHECK(report.losses[team] <= (reset == 1 ? 1 : 1));
				else if (reset == 1)
					CHECK(report.losses[team] == 2);
				else
					CHECK((report.losses[team] == 2 || (report.losses[team] == 1 && ranking[1].teamId == team)));
			}
		}
	}
}

TEST_CASE("Swiss rounds avoid rematches and rank every team")
{
	for (int teamCount = 3; teamCount <= 17; teamCount++)
	{
		for (int topCut : { 0, 4 })
		{
			if (topCut > teamCount)
				continue;

			CAPTURE(teamCount);
			CAPTURE(topCut);

			TournamentEngine engine;
			Settings settings;
			settings.format = Format::SWISS;
			settings.swissTopCut = topCut;
			REQUIRE(engine.setSettings(settings) == "");
			REQUIRE(engine.setTeams(teamIds(teamCount)) == "");
			REQUIRE(engine.start() == "");

			int rounds = engine.get().stages[0].totalRounds;
			int expectedRounds = 0;
			while ((1 << expectedRounds) < teamCount)
				expectedRounds++;
			CHECK(rounds == expectedRounds);

			std::mt19937 rng(teamCount * 13 + topCut);
			SimulationReport report = simulate(engine, rng);

			// Pas de revanche pendant les rondes suisses.
			std::set<std::pair<int, int>> swissPairs;
			std::map<int, int> byes;
			for (const auto & entry : engine.get().matches)
			{
				const TMatch & match = entry.second;
				if (match.stageIndex != 0)
					continue;
				if (match.isBye())
				{
					byes[match.result->winnerTeamId]++;
					continue;
				}
				CHECK(swissPairs.insert(std::minmax(match.teamA, match.teamB)).second);
			}

			// Chaque équipe joue (ou est exempte) à chaque ronde, et au plus un exempt par équipe.
			for (int team : teamIds(teamCount))
			{
				std::vector<StandingRow> rows = engine.standings(0);
				for (const StandingRow & row : rows)
					if (row.teamId == team)
						CHECK(row.played == rounds);
				CHECK(byes[team] <= 1);
			}

			checkCompleteRanking(engine, teamCount);
		}
	}
}

TEST_CASE("Pool standings use points, then head-to-head, then HP difference")
{
	TournamentEngine engine;
	Settings settings;
	settings.format = Format::POOLS_THEN_BRACKET;
	settings.poolCount = 1;
	settings.qualifiersPerPool = 2;
	REQUIRE(engine.setSettings(settings) == "");
	REQUIRE(engine.setTeams({ 1, 2, 3 }) == "");
	REQUIRE(engine.start() == "");

	// 3 bat 1, 1 bat 2, 3 bat 2 : 3 premier (2 victoires), 1 deuxième.
	for (int i = 0; i < 3; i++)
	{
		const TMatch * match = engine.findMatch(engine.readyMatches()[0]);
		std::set<int> teams = { match->teamA, match->teamB };
		int winner = teams.count(3) ? 3 : 1;
		REQUIRE(engine.reportResult(match->id, resultFor(*match, winner)) == "");
	}

	std::vector<StandingRow> rows = engine.standings(0, 0);
	REQUIRE(rows.size() == 3);
	CHECK(rows[0].teamId == 3);
	CHECK(rows[0].points == 6);
	CHECK(rows[1].teamId == 1);
	CHECK(rows[2].teamId == 2);
	CHECK(rows[2].losses == 2);

	// La phase finale (1 match entre les 2 qualifiés) est générée.
	REQUIRE(engine.get().stages[1].built);
	std::vector<int> ready = engine.readyMatches();
	REQUIRE(ready.size() == 1);
	const TMatch * finalMatch = engine.findMatch(ready[0]);
	CHECK(std::set<int>({ finalMatch->teamA, finalMatch->teamB }) == std::set<int>({ 1, 3 }));
}

TEST_CASE("Correcting a result propagates to the next matches")
{
	TournamentEngine engine;
	Settings settings;
	settings.format = Format::DOUBLE_ELIMINATION;
	REQUIRE(engine.setSettings(settings) == "");
	REQUIRE(engine.setTeams(teamIds(4)) == "");
	REQUIRE(engine.start() == "");

	std::vector<int> ready = engine.readyMatches();
	REQUIRE(ready.size() == 2);
	const TMatch * first = engine.findMatch(ready[0]);
	int firstId = first->id;
	int originalWinner = first->teamA;
	int originalLoser = first->teamB;
	REQUIRE(engine.reportResult(firstId, resultFor(*first, originalWinner)) == "");

	const TMatch * second = engine.findMatch(ready[1]);
	REQUIRE(engine.reportResult(second->id, resultFor(*second, second->teamA)) == "");

	// Les matchs suivants (finale des gagnants, tableau des perdants) sont prêts mais non joués :
	// la correction est acceptée et les équipes sont inversées.
	REQUIRE(engine.amendResult(firstId, resultFor(*engine.findMatch(firstId), originalLoser), false) == "");

	bool foundInWinners = false;
	bool foundInLosers = false;
	for (const auto & entry : engine.get().matches)
	{
		const TMatch & match = entry.second;
		if (match.bracket == "W" && match.round == 2)
			foundInWinners = match.teamA == originalLoser || match.teamB == originalLoser;
		if (match.bracket == "L" && match.round == 1)
			foundInLosers = match.teamA == originalWinner || match.teamB == originalWinner;
	}
	CHECK(foundInWinners);
	CHECK(foundInLosers);

	// Une fois la finale des gagnants jouée, la correction du premier tour exige la cascade.
	int winnersFinal = 0;
	for (int id : engine.readyMatches())
		if (engine.findMatch(id)->bracket == "W")
			winnersFinal = id;
	REQUIRE(winnersFinal != 0);
	const TMatch * wf = engine.findMatch(winnersFinal);
	REQUIRE(engine.reportResult(winnersFinal, resultFor(*wf, wf->teamA)) == "");

	CHECK(engine.amendResult(firstId, resultFor(*engine.findMatch(firstId), originalWinner), false) != "");
	REQUIRE(engine.amendResult(firstId, resultFor(*engine.findMatch(firstId), originalWinner), true) == "");
	CHECK((engine.findMatch(winnersFinal)->status != MatchStatus::DONE));
}

TEST_CASE("Correcting a pool result after the bracket was generated regenerates it")
{
	TournamentEngine engine;
	Settings settings;
	settings.format = Format::POOLS_THEN_BRACKET;
	settings.poolCount = 1;
	settings.qualifiersPerPool = 2;
	REQUIRE(engine.setSettings(settings) == "");
	REQUIRE(engine.setTeams({ 1, 2, 3 }) == "");
	REQUIRE(engine.start() == "");

	int lastPoolMatch = 0;
	while (!engine.get().stages[1].built)
	{
		const TMatch * match = engine.findMatch(engine.readyMatches()[0]);
		lastPoolMatch = match->id;
		REQUIRE(engine.reportResult(match->id, resultFor(*match, match->teamA)) == "");
	}

	const TMatch * pool = engine.findMatch(lastPoolMatch);
	REQUIRE(engine.amendResult(lastPoolMatch, resultFor(*pool, pool->teamB), false) == "");
	CHECK(engine.get().stages[1].built);	// régénérée avec le nouveau classement
	CHECK(engine.readyMatches().size() == 1);
}

TEST_CASE("Restart recovery and JSON persistence keep the whole state")
{
	TournamentEngine engine;
	Settings settings;
	settings.format = Format::SWISS;
	settings.swissTopCut = 4;
	Tournament base;
	base.id = 3;
	base.name = u8"Coupe d'hiver";
	engine = TournamentEngine(base);
	REQUIRE(engine.setSettings(settings) == "");
	REQUIRE(engine.setTeams(teamIds(7)) == "");
	REQUIRE(engine.start() == "");

	std::vector<int> ready = engine.readyMatches();
	const TMatch * match = engine.findMatch(ready[0]);
	REQUIRE(engine.reportResult(match->id, resultFor(*match, match->teamB, 42)) == "");
	REQUIRE(engine.markInProgress(ready[1], 77) == "");

	nlohmann::json json = toJson(engine.get());
	Tournament restored;
	REQUIRE(fromJson(nlohmann::json::parse(json.dump()), restored));
	CHECK(toJson(restored) == json);
	CHECK(restored.name == u8"Coupe d'hiver");

	TournamentEngine reloaded(restored);
	CHECK((reloaded.findMatch(ready[1])->status == MatchStatus::IN_PROGRESS));
	reloaded.recoverAfterRestart();
	CHECK((reloaded.findMatch(ready[1])->status == MatchStatus::READY));
	CHECK(reloaded.findMatch(ready[1])->sessionId == 0);

	// Le tournoi rechargé peut être terminé normalement.
	std::mt19937 rng(5);
	simulate(reloaded, rng);
	checkCompleteRanking(reloaded, 7);

	Tournament invalid;
	CHECK_FALSE(fromJson(nlohmann::json::parse("{\"name\":\"x\"}"), invalid));
}

TEST_CASE("Ready matches are ordered by stage, round and bracket")
{
	TournamentEngine engine;
	Settings settings;
	settings.format = Format::POOLS_THEN_BRACKET;
	settings.poolCount = 2;
	settings.qualifiersPerPool = 2;
	REQUIRE(engine.setSettings(settings) == "");
	REQUIRE(engine.setTeams(teamIds(8)) == "");
	REQUIRE(engine.start() == "");

	std::vector<int> ready = engine.readyMatches();
	REQUIRE(ready.size() == 12);	// 2 poules de 4 : 6 matchs chacune
	int previousRound = 0;
	for (int id : ready)
	{
		const TMatch * match = engine.findMatch(id);
		CHECK(match->round >= previousRound);
		previousRound = match->round;
	}
}
