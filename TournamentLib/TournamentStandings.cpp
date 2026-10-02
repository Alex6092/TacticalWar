// Classements des poules et des rondes suisses, et classement final du tournoi.
#include "TournamentEngine.h"

#include <algorithm>
#include <map>

using namespace tw::tournament;

int TournamentEngine::seedIndex(int teamId) const
{
	auto it = std::find(tournament.teamIds.begin(), tournament.teamIds.end(), teamId);
	return (int)(it - tournament.teamIds.begin());
}

std::vector<StandingRow> TournamentEngine::computeRows(const std::vector<int> & teams, int stageIndex, const std::string & bracket) const
{
	std::map<int, StandingRow> rows;
	for (int team : teams)
	{
		rows[team].teamId = team;
	}

	std::map<int, std::vector<int>> opponents;
	const Settings & settings = tournament.settings;

	for (const auto & entry : tournament.matches)
	{
		const TMatch & match = entry.second;
		if (match.stageIndex != stageIndex || match.status != MatchStatus::DONE || !match.result.has_value())
			continue;
		if (!bracket.empty() && match.bracket != bracket)
			continue;

		const MatchResult & result = *match.result;

		if (match.isBye())
		{
			int team = result.winnerTeamId;
			if (rows.count(team) > 0)
			{
				StandingRow & row = rows[team];
				row.played++;
				row.wins++;
				row.points += settings.pointsForWin;
				row.hadBye = true;
			}
			continue;
		}

		const int sides[2] = { match.teamA, match.teamB };
		const double hp[2] = { result.hpPercentA, result.hpPercentB };
		for (int side = 0; side < 2; side++)
		{
			int team = sides[side];
			if (rows.count(team) == 0)
				continue;

			StandingRow & row = rows[team];
			row.played++;
			if (result.winnerTeamId == team)
			{
				row.wins++;
				row.points += settings.pointsForWin;
			}
			else
			{
				row.losses++;
				row.points += settings.pointsForLoss;
			}
			row.hpDifference += hp[side] - hp[1 - side];
			opponents[team].push_back(sides[1 - side]);
		}
	}

	// Buchholz : somme des points des adversaires rencontrés.
	for (auto & entry : rows)
	{
		for (int opponent : opponents[entry.first])
		{
			auto it = rows.find(opponent);
			if (it != rows.end())
				entry.second.buchholz += it->second.points;
		}
	}

	std::vector<StandingRow> result;
	for (int team : teams)
		result.push_back(rows[team]);
	return result;
}

void TournamentEngine::sortPoolRows(std::vector<StandingRow> & rows, int stageIndex, const std::string & bracket) const
{
	std::sort(rows.begin(), rows.end(), [this](const StandingRow & a, const StandingRow & b) {
		if (a.points != b.points)
			return a.points > b.points;
		if (a.hpDifference != b.hpDifference)
			return a.hpDifference > b.hpDifference;
		if (a.wins != b.wins)
			return a.wins > b.wins;
		return seedIndex(a.teamId) < seedIndex(b.teamId);
	});

	// Égalité de points entre exactement deux équipes : la confrontation directe prime.
	for (std::size_t i = 0; i + 1 < rows.size(); i++)
	{
		int samePoints = 0;
		for (const StandingRow & row : rows)
		{
			if (row.points == rows[i].points)
				samePoints++;
		}
		if (samePoints != 2 || rows[i + 1].points != rows[i].points)
			continue;

		for (const auto & entry : tournament.matches)
		{
			const TMatch & match = entry.second;
			if (match.stageIndex != stageIndex || match.bracket != bracket || !match.result.has_value())
				continue;

			bool between = (match.teamA == rows[i].teamId && match.teamB == rows[i + 1].teamId)
				|| (match.teamB == rows[i].teamId && match.teamA == rows[i + 1].teamId);
			if (between && match.result->winnerTeamId == rows[i + 1].teamId)
				std::swap(rows[i], rows[i + 1]);
		}
		i++;
	}
}

void TournamentEngine::sortSwissRows(std::vector<StandingRow> & rows) const
{
	std::sort(rows.begin(), rows.end(), [this](const StandingRow & a, const StandingRow & b) {
		if (a.points != b.points)
			return a.points > b.points;
		if (a.buchholz != b.buchholz)
			return a.buchholz > b.buchholz;
		if (a.hpDifference != b.hpDifference)
			return a.hpDifference > b.hpDifference;
		return seedIndex(a.teamId) < seedIndex(b.teamId);
	});
}

std::vector<StandingRow> TournamentEngine::standings(int stageIndex, int poolIndex) const
{
	if (stageIndex < 0 || stageIndex >= (int)tournament.stages.size())
		return std::vector<StandingRow>();

	const Stage & stage = tournament.stages[stageIndex];
	std::vector<StandingRow> rows;

	switch (stage.type)
	{
	case StageType::ROUND_ROBIN_POOLS:
	{
		if (poolIndex < 0 || poolIndex >= (int)stage.pools.size())
			return rows;
		std::string bracket = "P" + std::to_string(poolIndex);
		rows = computeRows(stage.pools[poolIndex], stageIndex, bracket);
		sortPoolRows(rows, stageIndex, bracket);
		break;
	}
	case StageType::SWISS:
		rows = computeRows(tournament.teamIds, stageIndex, "S");
		sortSwissRows(rows);
		break;
	default:
		rows = computeRows(stage.seeds, stageIndex, "");
		std::stable_sort(rows.begin(), rows.end(), [](const StandingRow & a, const StandingRow & b) { return a.wins > b.wins; });
		break;
	}

	return rows;
}

std::vector<std::vector<int>> TournamentEngine::eliminationPlacement(int stageIndex) const
{
	std::vector<std::vector<int>> groups;
	const Stage & stage = tournament.stages[stageIndex];

	auto losersOf = [&](const std::string & bracket, int round) {
		std::vector<int> losers;
		for (const auto & entry : tournament.matches)
		{
			const TMatch & match = entry.second;
			if (match.stageIndex == stageIndex && match.bracket == bracket && match.round == round && match.result.has_value())
			{
				int loser = match.loserTeamId();
				if (loser != BYE_TEAM && loser != UNKNOWN_TEAM)
					losers.push_back(loser);
			}
		}
		return losers;
	};

	auto findMatchIn = [&](const std::string & bracket) -> const TMatch * {
		const TMatch * found = nullptr;
		for (const auto & entry : tournament.matches)
		{
			const TMatch & match = entry.second;
			if (match.stageIndex == stageIndex && match.bracket == bracket && (found == nullptr || match.round > found->round))
				found = &match;
		}
		return found;
	};

	if (stage.type == StageType::SINGLE_ELIMINATION)
	{
		const TMatch * finalMatch = findMatchIn("W");
		if (finalMatch == nullptr || !finalMatch->result.has_value())
			return groups;

		groups.push_back({ finalMatch->result->winnerTeamId });
		if (finalMatch->loserTeamId() != BYE_TEAM)
			groups.push_back({ finalMatch->loserTeamId() });

		int firstUnranked = finalMatch->round - 1;
		const TMatch * thirdPlace = findMatchIn("3P");
		if (thirdPlace != nullptr && thirdPlace->result.has_value())
		{
			groups.push_back({ thirdPlace->result->winnerTeamId });
			if (thirdPlace->loserTeamId() != BYE_TEAM)
				groups.push_back({ thirdPlace->loserTeamId() });
			firstUnranked = finalMatch->round - 2;
		}

		for (int round = firstUnranked; round >= 1; round--)
		{
			std::vector<int> losers = losersOf("W", round);
			if (!losers.empty())
				groups.push_back(losers);
		}
	}
	else if (stage.type == StageType::DOUBLE_ELIMINATION)
	{
		const TMatch * grandFinal = findMatchIn("GF");
		const TMatch * reset = findMatchIn("GF2");
		const TMatch * decisive = reset != nullptr ? reset : grandFinal;
		if (decisive == nullptr || !decisive->result.has_value())
			return groups;

		groups.push_back({ decisive->result->winnerTeamId });
		if (decisive->loserTeamId() != BYE_TEAM)
			groups.push_back({ decisive->loserTeamId() });

		int losersRounds = 0;
		for (const auto & entry : tournament.matches)
		{
			if (entry.second.stageIndex == stageIndex && entry.second.bracket == "L")
				losersRounds = std::max(losersRounds, entry.second.round);
		}

		for (int round = losersRounds; round >= 1; round--)
		{
			std::vector<int> losers = losersOf("L", round);
			if (!losers.empty())
				groups.push_back(losers);
		}
	}

	return groups;
}

std::vector<RankingEntry> TournamentEngine::finalRanking() const
{
	std::vector<RankingEntry> ranking;
	if (tournament.status != TournamentStatus::FINISHED)
		return ranking;

	std::vector<std::vector<int>> groups;
	std::vector<int> ranked;

	auto addGroups = [&](const std::vector<std::vector<int>> & newGroups) {
		for (const std::vector<int> & group : newGroups)
		{
			groups.push_back(group);
			ranked.insert(ranked.end(), group.begin(), group.end());
		}
	};
	auto isRanked = [&](int team) {
		return std::find(ranked.begin(), ranked.end(), team) != ranked.end();
	};

	const Settings & settings = tournament.settings;
	switch (settings.format)
	{
	case Format::POOLS_THEN_BRACKET:
	{
		addGroups(eliminationPlacement(1));

		// Équipes non qualifiées : par place dans leur poule, puis points et différence de PV.
		const Stage & pools = tournament.stages[0];
		std::size_t largestPool = 0;
		for (const std::vector<int> & pool : pools.pools)
			largestPool = std::max(largestPool, pool.size());

		std::vector<std::vector<StandingRow>> poolRows;
		for (int pool = 0; pool < (int)pools.pools.size(); pool++)
			poolRows.push_back(standings(0, pool));

		for (std::size_t position = 0; position < largestPool; position++)
		{
			std::vector<StandingRow> tier;
			for (const std::vector<StandingRow> & rows : poolRows)
			{
				if (position < rows.size() && !isRanked(rows[position].teamId))
					tier.push_back(rows[position]);
			}
			sortSwissRows(tier);
			for (const StandingRow & row : tier)
				addGroups({ { row.teamId } });
		}
		break;
	}
	case Format::DOUBLE_ELIMINATION:
		addGroups(eliminationPlacement(0));
		break;
	case Format::SWISS:
		if (tournament.stages.size() > 1)
			addGroups(eliminationPlacement(1));
		for (const StandingRow & row : standings(0))
		{
			if (!isRanked(row.teamId))
				addGroups({ { row.teamId } });
		}
		break;
	}

	int position = 1;
	for (const std::vector<int> & group : groups)
	{
		for (int team : group)
			ranking.push_back({ position, team });
		position += (int)group.size();
	}

	return ranking;
}
