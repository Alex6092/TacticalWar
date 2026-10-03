#include "Tournament.h"

#include <algorithm>
#include <cstdlib>

using namespace tw::tournament;

const char * tw::tournament::toString(Format format)
{
	switch (format)
	{
	case Format::POOLS_THEN_BRACKET: return "POOLS_THEN_BRACKET";
	case Format::DOUBLE_ELIMINATION: return "DOUBLE_ELIMINATION";
	case Format::SWISS: return "SWISS";
	}
	return "";
}

const char * tw::tournament::toString(StageType type)
{
	switch (type)
	{
	case StageType::ROUND_ROBIN_POOLS: return "ROUND_ROBIN_POOLS";
	case StageType::SINGLE_ELIMINATION: return "SINGLE_ELIMINATION";
	case StageType::DOUBLE_ELIMINATION: return "DOUBLE_ELIMINATION";
	case StageType::SWISS: return "SWISS";
	}
	return "";
}

const char * tw::tournament::toString(TournamentStatus status)
{
	switch (status)
	{
	case TournamentStatus::DRAFT: return "DRAFT";
	case TournamentStatus::RUNNING: return "RUNNING";
	case TournamentStatus::FINISHED: return "FINISHED";
	}
	return "";
}

const char * tw::tournament::toString(MatchStatus status)
{
	switch (status)
	{
	case MatchStatus::PENDING: return "PENDING";
	case MatchStatus::READY: return "READY";
	case MatchStatus::IN_PROGRESS: return "IN_PROGRESS";
	case MatchStatus::DONE: return "DONE";
	}
	return "";
}

const char * tw::tournament::toString(ResultReason reason)
{
	switch (reason)
	{
	case ResultReason::KO: return "KO";
	case ResultReason::ROUND_LIMIT: return "ROUND_LIMIT";
	case ResultReason::OBJECTIVE: return "OBJECTIVE";
	case ResultReason::FORFEIT: return "FORFEIT";
	case ResultReason::ADMIN: return "ADMIN";
	case ResultReason::BYE: return "BYE";
	}
	return "";
}

int tw::tournament::matchesPlayed(const Tournament & tournament, int teamId)
{
	int played = 0;
	for (const auto & entry : tournament.matches)
	{
		const TMatch & match = entry.second;
		if (match.status == MatchStatus::DONE && (match.teamA == teamId || match.teamB == teamId))
			played++;
	}
	return played;
}

int tw::tournament::talentSlots(const Tournament & tournament, int teamId)
{
	return std::max(0, std::min(tournament.settings.maxTalents, matchesPlayed(tournament, teamId)));
}

std::string tw::tournament::matchLabel(const Tournament & tournament, const TMatch & match)
{
	const std::string & bracket = match.bracket;
	std::string round = std::to_string(match.round);

	if (bracket.size() > 1 && bracket[0] == 'P')
	{
		int pool = std::atoi(bracket.substr(1).c_str());
		return std::string("Poule ") + (char)('A' + pool) + " - journée " + round;
	}
	if (bracket == "S")
		return "Ronde " + round;
	if (bracket == "GF")
		return "Grande finale";
	if (bracket == "GF2")
		return "Grande finale (revanche)";
	if (bracket == "3P")
		return "Petite finale";
	if (bracket == "L")
		return "Tableau des perdants - tour " + round;

	if (bracket == "W")
	{
		// Nombre de tours du tableau des gagnants (ou du tableau final).
		int maxRound = 0;
		for (const auto & entry : tournament.matches)
		{
			if (entry.second.stageIndex == match.stageIndex && entry.second.bracket == "W")
				maxRound = std::max(maxRound, entry.second.round);
		}

		bool doubleElimination = tournament.stages[match.stageIndex].type == StageType::DOUBLE_ELIMINATION;
		std::string prefix = doubleElimination ? "Gagnants - " : "";
		int fromEnd = maxRound - match.round;
		if (fromEnd == 0)
			return doubleElimination ? "Finale des gagnants" : "Finale";
		if (fromEnd == 1)
			return prefix + "Demi-finale";
		if (fromEnd == 2)
			return prefix + "Quart de finale";
		if (fromEnd == 3)
			return prefix + "Huitième de finale";
		return prefix + "Tour " + round;
	}

	return bracket + " " + round;
}
