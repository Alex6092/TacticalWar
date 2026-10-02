// Génération des poules, des tableaux à élimination et des rondes suisses.
#include "TournamentEngine.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <set>

using namespace tw::tournament;

namespace
{
	// Ordre standard des têtes de série dans un tableau de taille "size" (puissance de 2) :
	// les matchs du premier tour sont (order[0], order[1]), (order[2], order[3])...
	// Les têtes de série 1 et 2 ne peuvent se rencontrer qu'en finale.
	std::vector<int> seedOrder(int size)
	{
		std::vector<int> order = { 1 };
		while ((int)order.size() < size)
		{
			int doubled = (int)order.size() * 2;
			std::vector<int> next;
			for (int seed : order)
			{
				next.push_back(seed);
				next.push_back(doubled + 1 - seed);
			}
			order = next;
		}
		return order;
	}

	int nextPowerOfTwo(int value)
	{
		int size = 1;
		while (size < value)
			size *= 2;
		return size;
	}

	SlotRef slotForTeam(int teamId)
	{
		return teamId == BYE_TEAM ? SlotRef::bye() : SlotRef::team(teamId);
	}

	std::pair<int, int> orderedPair(int a, int b)
	{
		return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
	}

	struct SwissPairing
	{
		const std::map<int, int> & points;
		const std::set<std::pair<int, int>> & played;
		bool avoidRematches;
		int budget;
		std::vector<std::pair<int, int>> pairs;

		// Apparie les équipes restantes (dans l'ordre du classement) avec retour arrière.
		bool pair(const std::vector<int> & remaining)
		{
			if (remaining.empty())
				return true;
			if (--budget < 0)
				return false;

			int team = remaining[0];
			int teamPoints = points.at(team);

			// Préférence : même groupe de points en jouant la moitié basse du groupe
			// (système hollandais), puis les groupes les plus proches.
			std::vector<int> sameGroup;
			std::vector<int> otherGroups;
			for (std::size_t i = 1; i < remaining.size(); i++)
			{
				if (points.at(remaining[i]) == teamPoints)
					sameGroup.push_back(remaining[i]);
				else
					otherGroups.push_back(remaining[i]);
			}

			std::vector<int> candidates;
			std::size_t start = sameGroup.empty() ? 0 : (sameGroup.size() + 1) / 2 - 1;
			for (std::size_t i = 0; i < sameGroup.size(); i++)
				candidates.push_back(sameGroup[(start + i) % sameGroup.size()]);

			std::stable_sort(otherGroups.begin(), otherGroups.end(), [&](int a, int b) {
				return std::abs(points.at(a) - teamPoints) < std::abs(points.at(b) - teamPoints);
			});
			candidates.insert(candidates.end(), otherGroups.begin(), otherGroups.end());

			for (int candidate : candidates)
			{
				if (avoidRematches && played.count(orderedPair(team, candidate)) > 0)
					continue;

				std::vector<int> rest;
				for (std::size_t i = 1; i < remaining.size(); i++)
				{
					if (remaining[i] != candidate)
						rest.push_back(remaining[i]);
				}

				pairs.push_back(std::make_pair(team, candidate));
				if (pair(rest))
					return true;
				pairs.pop_back();

				if (budget < 0)
					return false;
			}

			return false;
		}
	};
}

void TournamentEngine::buildPools(Stage & stage, int stageIndex)
{
	int poolCount = tournament.settings.poolCount;
	stage.pools.assign(poolCount, std::vector<int>());

	// Répartition en serpentin : 1 2 3 4 / 8 7 6 5 / 9 10 11 12...
	for (std::size_t i = 0; i < tournament.teamIds.size(); i++)
	{
		int line = (int)i / poolCount;
		int position = (int)i % poolCount;
		int pool = line % 2 == 0 ? position : poolCount - 1 - position;
		stage.pools[pool].push_back(tournament.teamIds[i]);
	}

	// Round-robin par la méthode du cercle (une équipe fixe, les autres tournent).
	for (int pool = 0; pool < poolCount; pool++)
	{
		std::vector<int> teams = stage.pools[pool];
		if (teams.size() % 2 == 1)
			teams.push_back(BYE_TEAM);

		int count = (int)teams.size();
		std::string bracket = "P" + std::to_string(pool);

		for (int round = 0; round < count - 1; round++)
		{
			int order = 0;
			for (int i = 0; i < count / 2; i++)
			{
				int a = teams[i];
				int b = teams[count - 1 - i];
				// Pas de match contre l'exempt en poule : l'équipe est simplement au repos.
				if (a != BYE_TEAM && b != BYE_TEAM)
					addMatch(stageIndex, bracket, round + 1, order++, SlotRef::team(a), SlotRef::team(b));
			}

			teams.insert(teams.begin() + 1, teams.back());
			teams.pop_back();
		}
	}
}

std::vector<int> TournamentEngine::poolQualifiers() const
{
	// Qualifiés rangés par niveau : tous les premiers de poule, puis tous les deuxièmes...
	std::vector<int> qualifiers;
	const Stage & pools = tournament.stages[0];
	for (int rank = 0; rank < tournament.settings.qualifiersPerPool; rank++)
	{
		for (int pool = 0; pool < (int)pools.pools.size(); pool++)
		{
			std::vector<StandingRow> rows = standings(0, pool);
			if (rank < (int)rows.size())
				qualifiers.push_back(rows[rank].teamId);
		}
	}
	return qualifiers;
}

std::vector<int> TournamentEngine::swissQualifiers(int stageIndex, int count) const
{
	std::vector<int> qualifiers;
	std::vector<StandingRow> rows = standings(stageIndex);
	for (int i = 0; i < count && i < (int)rows.size(); i++)
		qualifiers.push_back(rows[i].teamId);
	return qualifiers;
}

void TournamentEngine::buildSingleElimination(Stage & stage, int stageIndex)
{
	int teamCount = (int)stage.seeds.size();
	int size = nextPowerOfTwo(teamCount);
	stage.bracketSize = size;

	std::vector<int> order = seedOrder(size);
	std::vector<std::pair<int, int>> firstRound;
	for (int i = 0; i < size / 2; i++)
	{
		int seedA = order[2 * i];
		int seedB = order[2 * i + 1];
		firstRound.push_back(std::make_pair(
			seedA <= teamCount ? stage.seeds[seedA - 1] : BYE_TEAM,
			seedB <= teamCount ? stage.seeds[seedB - 1] : BYE_TEAM));
	}

	// Après les poules : évite que deux équipes d'une même poule se retrouvent au premier tour.
	if (stageIndex > 0 && tournament.stages[stageIndex - 1].type == StageType::ROUND_ROBIN_POOLS)
	{
		std::map<int, int> poolOf;
		std::map<int, int> tierOf;
		const std::vector<std::vector<int>> & pools = tournament.stages[stageIndex - 1].pools;
		for (int pool = 0; pool < (int)pools.size(); pool++)
		{
			std::vector<StandingRow> rows = standings(stageIndex - 1, pool);
			for (int rank = 0; rank < (int)rows.size(); rank++)
			{
				poolOf[rows[rank].teamId] = pool;
				tierOf[rows[rank].teamId] = rank;
			}
		}

		auto samePool = [&](int a, int b) {
			return a != BYE_TEAM && b != BYE_TEAM && poolOf[a] == poolOf[b];
		};

		for (std::size_t i = 0; i < firstRound.size(); i++)
		{
			if (!samePool(firstRound[i].first, firstRound[i].second))
				continue;

			// Échange de l'équipe B avec celle d'un autre match, de préférence du même niveau.
			for (int pass = 0; pass < 2 && samePool(firstRound[i].first, firstRound[i].second); pass++)
			{
				for (std::size_t j = 0; j < firstRound.size(); j++)
				{
					if (j == i || firstRound[j].second == BYE_TEAM)
						continue;
					if (pass == 0 && tierOf[firstRound[j].second] != tierOf[firstRound[i].second])
						continue;
					if (samePool(firstRound[i].first, firstRound[j].second) || samePool(firstRound[j].first, firstRound[i].second))
						continue;

					std::swap(firstRound[i].second, firstRound[j].second);
					break;
				}
			}
		}
	}

	std::vector<int> previous;
	for (int i = 0; i < (int)firstRound.size(); i++)
		previous.push_back(addMatch(stageIndex, "W", 1, i, slotForTeam(firstRound[i].first), slotForTeam(firstRound[i].second)));

	std::vector<int> semiFinals;
	int round = 2;
	while (previous.size() > 1)
	{
		if (previous.size() == 2)
			semiFinals = previous;

		std::vector<int> next;
		for (int j = 0; j < (int)previous.size() / 2; j++)
			next.push_back(addMatch(stageIndex, "W", round, j, SlotRef::winnerOf(previous[2 * j]), SlotRef::winnerOf(previous[2 * j + 1])));
		previous = next;
		round++;
	}

	if (tournament.settings.thirdPlaceMatch && semiFinals.size() == 2)
		addMatch(stageIndex, "3P", round - 1, 0, SlotRef::loserOf(semiFinals[0]), SlotRef::loserOf(semiFinals[1]));
}

void TournamentEngine::buildDoubleElimination(Stage & stage, int stageIndex)
{
	int teamCount = (int)stage.seeds.size();
	int size = nextPowerOfTwo(teamCount);
	stage.bracketSize = size;

	// Tableau des gagnants.
	std::vector<std::vector<int>> winners;
	std::vector<int> order = seedOrder(size);
	std::vector<int> firstRound;
	for (int i = 0; i < size / 2; i++)
	{
		int seedA = order[2 * i];
		int seedB = order[2 * i + 1];
		firstRound.push_back(addMatch(stageIndex, "W", 1, i,
			slotForTeam(seedA <= teamCount ? stage.seeds[seedA - 1] : BYE_TEAM),
			slotForTeam(seedB <= teamCount ? stage.seeds[seedB - 1] : BYE_TEAM)));
	}
	winners.push_back(firstRound);

	while (winners.back().size() > 1)
	{
		const std::vector<int> & previous = winners.back();
		std::vector<int> next;
		for (int j = 0; j < (int)previous.size() / 2; j++)
			next.push_back(addMatch(stageIndex, "W", (int)winners.size() + 1, j, SlotRef::winnerOf(previous[2 * j]), SlotRef::winnerOf(previous[2 * j + 1])));
		winners.push_back(next);
	}

	int winnersFinal = winners.back()[0];
	SlotRef losersChampion = SlotRef::loserOf(winnersFinal);

	// Tableau des perdants : alternance de tours "internes" (les survivants s'affrontent) et de
	// tours "d'arrivée" (les survivants affrontent les perdants du tour suivant des gagnants).
	if (winners.size() >= 2)
	{
		int losersRound = 1;
		std::vector<int> current;
		for (int j = 0; j < (int)winners[0].size() / 2; j++)
			current.push_back(addMatch(stageIndex, "L", losersRound, j, SlotRef::loserOf(winners[0][2 * j]), SlotRef::loserOf(winners[0][2 * j + 1])));

		for (std::size_t r = 1; r < winners.size(); r++)
		{
			// Tour d'arrivée : l'ordre des perdants est inversé un tour sur deux pour éviter les revanches.
			losersRound++;
			const std::vector<int> & dropping = winners[r];
			std::vector<int> arrival;
			for (int j = 0; j < (int)current.size(); j++)
			{
				int index = r % 2 == 1 ? (int)dropping.size() - 1 - j : j;
				arrival.push_back(addMatch(stageIndex, "L", losersRound, j, SlotRef::winnerOf(current[j]), SlotRef::loserOf(dropping[index])));
			}
			current = arrival;

			// Tour interne (sauf après l'arrivée du perdant de la finale des gagnants).
			if (r + 1 < winners.size())
			{
				losersRound++;
				std::vector<int> internal;
				for (int j = 0; j < (int)current.size() / 2; j++)
					internal.push_back(addMatch(stageIndex, "L", losersRound, j, SlotRef::winnerOf(current[2 * j]), SlotRef::winnerOf(current[2 * j + 1])));
				current = internal;
			}
		}

		losersChampion = SlotRef::winnerOf(current[0]);
	}

	addMatch(stageIndex, "GF", 1, 0, SlotRef::winnerOf(winnersFinal), losersChampion);
}

void TournamentEngine::buildNextSwissRound(Stage & stage, int stageIndex)
{
	int round = stage.currentRound + 1;

	std::vector<int> ranking;
	std::map<int, int> points;
	std::map<int, bool> hadBye;

	if (round == 1)
	{
		ranking = tournament.teamIds;
		for (int team : ranking)
			points[team] = 0;
	}
	else
	{
		std::vector<StandingRow> rows = standings(stageIndex);
		for (const StandingRow & row : rows)
		{
			ranking.push_back(row.teamId);
			points[row.teamId] = row.points;
			hadBye[row.teamId] = row.hadBye;
		}
	}

	std::set<std::pair<int, int>> played;
	for (const auto & entry : tournament.matches)
	{
		const TMatch & match = entry.second;
		if (match.stageIndex == stageIndex && !match.isBye())
			played.insert(orderedPair(match.teamA, match.teamB));
	}

	// Nombre impair : l'exempt va à l'équipe la moins bien classée qui n'en a pas encore eu.
	int byeTeam = UNKNOWN_TEAM;
	if (ranking.size() % 2 == 1)
	{
		for (int i = (int)ranking.size() - 1; i >= 0 && byeTeam == UNKNOWN_TEAM; i--)
		{
			if (!hadBye[ranking[i]])
				byeTeam = ranking[i];
		}
		if (byeTeam == UNKNOWN_TEAM)
			byeTeam = ranking.back();
		ranking.erase(std::find(ranking.begin(), ranking.end(), byeTeam));
	}

	std::vector<std::pair<int, int>> pairs;
	if (round == 1)
	{
		// Première ronde : moitié haute contre moitié basse des têtes de série.
		int half = (int)ranking.size() / 2;
		for (int i = 0; i < half; i++)
			pairs.push_back(std::make_pair(ranking[i], ranking[i + half]));
	}
	else
	{
		SwissPairing pairing{ points, played, true, 200000, {} };
		if (!pairing.pair(ranking))
		{
			// Aucun appariement sans revanche : on accepte les revanches.
			pairing.avoidRematches = false;
			pairing.budget = 200000;
			pairing.pairs.clear();
			pairing.pair(ranking);
		}
		pairs = pairing.pairs;
	}

	int order = 0;
	for (const std::pair<int, int> & pair : pairs)
		addMatch(stageIndex, "S", round, order++, SlotRef::team(pair.first), SlotRef::team(pair.second));
	if (byeTeam != UNKNOWN_TEAM)
		addMatch(stageIndex, "S", round, order++, SlotRef::team(byeTeam), SlotRef::bye());

	stage.currentRound = round;
}
