#include "ChallengeBoard.h"

#include <algorithm>

using namespace tw;

bool ChallengeBoard::involved(int team) const
{
	return std::any_of(challenges.begin(), challenges.end(), [team](const Challenge & c) { return c.from == team || c.to == team; });
}

std::string ChallengeBoard::challenge(int from, int to, bool tournamentRunning, bool fromFree, bool toFree, std::int64_t nowMs)
{
	expire(nowMs);
	if (tournamentRunning)
		return u8"Un tournoi est en cours : les matchs amicaux reprendront après.";
	if (from == to)
		return u8"Une équipe ne peut pas se défier elle-même.";
	if (!fromFree)
		return u8"Votre équipe a déjà un match prévu ou en cours.";
	if (!toFree)
		return u8"Cette équipe a déjà un match prévu ou en cours.";
	if (sentBy(from) != nullptr)
		return u8"Votre équipe attend déjà la réponse à un défi.";
	if (receivedBy(from) != nullptr)
		return u8"Votre équipe a reçu un défi : répondez-y d'abord.";
	if (involved(to))
		return u8"Cette équipe a déjà un défi en attente.";

	Challenge entry;
	entry.from = from;
	entry.to = to;
	entry.expiresMs = nowMs + TIMEOUT_MS;
	challenges.push_back(entry);
	return std::string();
}

bool ChallengeBoard::answer(int to, int from, std::int64_t nowMs, Challenge * answered)
{
	expire(nowMs);
	for (auto it = challenges.begin(); it != challenges.end(); ++it)
	{
		if (it->to == to && it->from == from)
		{
			if (answered != nullptr)
				*answered = *it;
			challenges.erase(it);
			return true;
		}
	}
	return false;
}

std::vector<ChallengeBoard::Challenge> ChallengeBoard::expire(std::int64_t nowMs)
{
	std::vector<Challenge> expired;
	for (auto it = challenges.begin(); it != challenges.end();)
	{
		if (it->expiresMs <= nowMs)
		{
			expired.push_back(*it);
			it = challenges.erase(it);
		}
		else
		{
			++it;
		}
	}
	return expired;
}

const ChallengeBoard::Challenge * ChallengeBoard::receivedBy(int team) const
{
	for (const Challenge & entry : challenges)
	{
		if (entry.to == team)
			return &entry;
	}
	return nullptr;
}

const ChallengeBoard::Challenge * ChallengeBoard::sentBy(int team) const
{
	for (const Challenge & entry : challenges)
	{
		if (entry.from == team)
			return &entry;
	}
	return nullptr;
}

std::vector<ChallengeBoard::Challenge> ChallengeBoard::cancelInvolving(int team)
{
	std::vector<Challenge> removed;
	for (auto it = challenges.begin(); it != challenges.end();)
	{
		if (it->from == team || it->to == team)
		{
			removed.push_back(*it);
			it = challenges.erase(it);
		}
		else
		{
			++it;
		}
	}
	return removed;
}

std::vector<ChallengeBoard::Challenge> ChallengeBoard::cancelAll()
{
	std::vector<Challenge> removed;
	removed.swap(challenges);
	return removed;
}
