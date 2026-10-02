#include "Tournament.h"

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
	case ResultReason::FORFEIT: return "FORFEIT";
	case ResultReason::ADMIN: return "ADMIN";
	case ResultReason::BYE: return "BYE";
	}
	return "";
}
