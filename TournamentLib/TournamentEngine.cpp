#include "TournamentEngine.h"

#include <algorithm>
#include <set>

using namespace tw::tournament;

int TMatch::loserTeamId() const
{
	if (!result.has_value())
		return UNKNOWN_TEAM;
	return result->winnerTeamId == teamA ? teamB : teamA;
}

TournamentEngine::TournamentEngine(const Tournament & tournament)
	: tournament(tournament)
{
}

std::string TournamentEngine::setTeams(const std::vector<int> & teamIdsInSeedOrder)
{
	if (tournament.status != TournamentStatus::DRAFT)
		return "Le tournoi a déjà commencé : les équipes ne peuvent plus changer.";

	std::set<int> unique(teamIdsInSeedOrder.begin(), teamIdsInSeedOrder.end());
	if (unique.size() != teamIdsInSeedOrder.size())
		return "Une équipe est inscrite deux fois.";
	if (unique.count(UNKNOWN_TEAM) > 0 || unique.count(BYE_TEAM) > 0)
		return "Identifiant d'équipe invalide.";

	tournament.teamIds = teamIdsInSeedOrder;
	return "";
}

std::string TournamentEngine::setSettings(const Settings & settings)
{
	if (tournament.status != TournamentStatus::DRAFT)
		return "Le tournoi a déjà commencé : les paramètres ne peuvent plus changer.";

	tournament.settings = settings;
	return "";
}

std::string TournamentEngine::validate() const
{
	const Settings & s = tournament.settings;
	int teamCount = (int)tournament.teamIds.size();

	if (teamCount < 2)
		return "Il faut au moins 2 équipes.";

	switch (s.format)
	{
	case Format::POOLS_THEN_BRACKET:
	{
		if (s.poolCount < 1)
			return "Il faut au moins une poule.";
		if (teamCount < 2 * s.poolCount)
			return "Pas assez d'équipes : chaque poule doit contenir au moins 2 équipes.";
		int smallestPool = teamCount / s.poolCount;
		if (s.qualifiersPerPool < 1 || s.qualifiersPerPool > smallestPool)
			return "Le nombre de qualifiés par poule doit être entre 1 et " + std::to_string(smallestPool) + ".";
		if (s.qualifiersPerPool * s.poolCount < 2)
			return "Il faut au moins 2 qualifiés au total pour la phase finale.";
		break;
	}
	case Format::DOUBLE_ELIMINATION:
		break;
	case Format::SWISS:
		if (s.swissRounds < 0)
			return "Nombre de rondes invalide.";
		if (s.swissTopCut == 1 || s.swissTopCut < 0 || s.swissTopCut > teamCount)
			return "La phase finale doit qualifier entre 2 et " + std::to_string(teamCount) + " équipes (ou 0 pour aucune).";
		break;
	}

	return "";
}

std::string TournamentEngine::start()
{
	if (tournament.status != TournamentStatus::DRAFT)
		return "Le tournoi a déjà commencé.";

	std::string error = validate();
	if (!error.empty())
		return error;

	const Settings & s = tournament.settings;
	tournament.stages.clear();
	tournament.matches.clear();

	switch (s.format)
	{
	case Format::POOLS_THEN_BRACKET:
	{
		Stage pools;
		pools.type = StageType::ROUND_ROBIN_POOLS;
		pools.name = "Poules";
		Stage finals;
		finals.type = StageType::SINGLE_ELIMINATION;
		finals.name = "Phase finale";
		tournament.stages = { pools, finals };
		break;
	}
	case Format::DOUBLE_ELIMINATION:
	{
		Stage bracket;
		bracket.type = StageType::DOUBLE_ELIMINATION;
		bracket.name = "Double élimination";
		tournament.stages = { bracket };
		break;
	}
	case Format::SWISS:
	{
		Stage swiss;
		swiss.type = StageType::SWISS;
		swiss.name = "Rondes suisses";
		tournament.stages = { swiss };
		if (s.swissTopCut >= 2)
		{
			Stage finals;
			finals.type = StageType::SINGLE_ELIMINATION;
			finals.name = "Phase finale";
			tournament.stages.push_back(finals);
		}
		break;
	}
	}

	tournament.status = TournamentStatus::RUNNING;
	buildStage(0);
	propagate();
	return "";
}

int TournamentEngine::addMatch(int stageIndex, const std::string & bracket, int round, int order, const SlotRef & a, const SlotRef & b)
{
	TMatch match;
	match.id = tournament.nextMatchId++;
	match.stageIndex = stageIndex;
	match.bracket = bracket;
	match.round = round;
	match.order = order;
	match.slotA = a;
	match.slotB = b;
	tournament.matches[match.id] = match;
	return match.id;
}

int TournamentEngine::resolveSlot(const SlotRef & slot) const
{
	switch (slot.kind)
	{
	case SlotRef::Kind::TEAM:
		return slot.value;
	case SlotRef::Kind::BYE:
		return BYE_TEAM;
	case SlotRef::Kind::WINNER_OF:
	case SlotRef::Kind::LOSER_OF:
	{
		const TMatch * source = findMatch(slot.value);
		if (source == nullptr || source->status != MatchStatus::DONE || !source->result.has_value())
			return UNKNOWN_TEAM;
		return slot.kind == SlotRef::Kind::WINNER_OF ? source->result->winnerTeamId : source->loserTeamId();
	}
	default:
		return UNKNOWN_TEAM;
	}
}

void TournamentEngine::refreshMatch(TMatch & match)
{
	if (match.status == MatchStatus::DONE || match.status == MatchStatus::IN_PROGRESS)
		return;

	match.teamA = resolveSlot(match.slotA);
	match.teamB = resolveSlot(match.slotB);

	if (match.teamA == UNKNOWN_TEAM || match.teamB == UNKNOWN_TEAM)
	{
		match.status = MatchStatus::PENDING;
		return;
	}

	if (match.isBye())
	{
		// Exempt : victoire automatique (ou exempt contre exempt : la place reste vide).
		MatchResult result;
		result.reason = ResultReason::BYE;
		result.winnerTeamId = match.teamA == BYE_TEAM ? match.teamB : match.teamA;
		result.hpPercentA = match.teamA == BYE_TEAM ? 0 : 100;
		result.hpPercentB = match.teamB == BYE_TEAM ? 0 : 100;
		match.result = result;
		match.status = MatchStatus::DONE;
		return;
	}

	match.status = MatchStatus::READY;
}

void TournamentEngine::propagate()
{
	bool changed = true;
	while (changed)
	{
		changed = false;

		for (auto & entry : tournament.matches)
		{
			TMatch & match = entry.second;
			MatchStatus before = match.status;
			int teamA = match.teamA;
			int teamB = match.teamB;
			refreshMatch(match);
			if (match.status != before || match.teamA != teamA || match.teamB != teamB)
				changed = true;
		}

		for (int i = 0; i < (int)tournament.stages.size(); i++)
		{
			Stage & stage = tournament.stages[i];
			if (stage.built && !stage.finished && isStageComplete(i))
			{
				onStageComplete(i);
				changed = true;
			}
		}
	}

	bool allFinished = !tournament.stages.empty();
	for (const Stage & stage : tournament.stages)
		allFinished = allFinished && stage.finished;

	if (tournament.status != TournamentStatus::DRAFT)
		tournament.status = allFinished ? TournamentStatus::FINISHED : TournamentStatus::RUNNING;
}

bool TournamentEngine::isStageComplete(int stageIndex) const
{
	const Stage & stage = tournament.stages[stageIndex];
	if (!stage.built)
		return false;

	// Tous les matchs générés sont terminés (pour le suisse : ceux de la ronde en cours ;
	// onStageComplete génère alors la ronde suivante ou termine la phase).
	for (const auto & entry : tournament.matches)
	{
		if (entry.second.stageIndex == stageIndex && entry.second.status != MatchStatus::DONE)
			return false;
	}

	return true;
}

void TournamentEngine::onStageComplete(int stageIndex)
{
	Stage & stage = tournament.stages[stageIndex];

	// Double élimination : si le vainqueur du tableau des perdants gagne la grande finale,
	// une revanche est jouée (chaque équipe a alors une défaite).
	if (stage.type == StageType::DOUBLE_ELIMINATION && tournament.settings.grandFinalReset)
	{
		const TMatch * grandFinal = nullptr;
		bool hasReset = false;
		for (const auto & entry : tournament.matches)
		{
			if (entry.second.stageIndex != stageIndex)
				continue;
			if (entry.second.bracket == "GF")
				grandFinal = &entry.second;
			if (entry.second.bracket == "GF2")
				hasReset = true;
		}

		if (grandFinal != nullptr && !hasReset && grandFinal->result.has_value()
			&& grandFinal->result->reason != ResultReason::BYE
			&& grandFinal->result->winnerTeamId == grandFinal->teamB)
		{
			int gfId = grandFinal->id;
			int gfRound = grandFinal->round;
			addMatch(stageIndex, "GF2", gfRound + 1, 0, SlotRef::loserOf(gfId), SlotRef::winnerOf(gfId));
			return;
		}
	}

	// Suisse : ronde suivante.
	if (stage.type == StageType::SWISS && stage.currentRound < stage.totalRounds)
	{
		buildNextSwissRound(stage, stageIndex);
		return;
	}

	stage.finished = true;

	if (stageIndex + 1 < (int)tournament.stages.size() && !tournament.stages[stageIndex + 1].built)
		buildStage(stageIndex + 1);
}

void TournamentEngine::buildStage(int stageIndex)
{
	Stage & stage = tournament.stages[stageIndex];
	stage.built = true;
	stage.finished = false;

	switch (stage.type)
	{
	case StageType::ROUND_ROBIN_POOLS:
		buildPools(stage, stageIndex);
		break;
	case StageType::SINGLE_ELIMINATION:
		if (stageIndex == 0)
			stage.seeds = tournament.teamIds;
		else if (tournament.stages[stageIndex - 1].type == StageType::ROUND_ROBIN_POOLS)
			stage.seeds = poolQualifiers();
		else
			stage.seeds = swissQualifiers(stageIndex - 1, tournament.settings.swissTopCut);
		buildSingleElimination(stage, stageIndex);
		break;
	case StageType::DOUBLE_ELIMINATION:
		stage.seeds = tournament.teamIds;
		buildDoubleElimination(stage, stageIndex);
		break;
	case StageType::SWISS:
	{
		int teamCount = (int)tournament.teamIds.size();
		int rounds = tournament.settings.swissRounds;
		if (rounds <= 0)
		{
			rounds = 0;
			while ((1 << rounds) < teamCount)
				rounds++;
		}
		// Au-delà, des revanches deviendraient inévitables.
		int maxRounds = teamCount % 2 == 0 ? teamCount - 1 : teamCount;
		stage.totalRounds = std::max(1, std::min(rounds, maxRounds));
		stage.currentRound = 0;
		buildNextSwissRound(stage, stageIndex);
		break;
	}
	}
}

std::vector<int> TournamentEngine::readyMatches() const
{
	std::vector<const TMatch*> ready;
	for (const auto & entry : tournament.matches)
	{
		if (entry.second.status == MatchStatus::READY)
			ready.push_back(&entry.second);
	}

	std::sort(ready.begin(), ready.end(), [this](const TMatch * a, const TMatch * b) {
		double keyA = scheduleKey(*a);
		double keyB = scheduleKey(*b);
		if (keyA != keyB)
			return keyA < keyB;
		if (a->order != b->order)
			return a->order < b->order;
		return a->id < b->id;
	});

	std::vector<int> ids;
	for (const TMatch * match : ready)
		ids.push_back(match->id);
	return ids;
}

double TournamentEngine::scheduleKey(const TMatch & match) const
{
	double stageOffset = match.stageIndex * 1000.0;
	const std::string & bracket = match.bracket;

	// Tableau des perdants : il a deux fois plus de tours que celui des gagnants.
	if (bracket == "W")
		return stageOffset + 2.0 * match.round - 1.0;
	if (bracket == "L")
		return stageOffset + match.round + 1.0;
	if (bracket == "3P")
		return stageOffset + 2.0 * match.round - 1.5;	// Petite finale avant la finale
	if (bracket == "GF" || bracket == "GF2")
		return stageOffset + 500.0 + match.round;
	return stageOffset + match.round;
}

const TMatch * TournamentEngine::findMatch(int matchId) const
{
	auto it = tournament.matches.find(matchId);
	return it == tournament.matches.end() ? nullptr : &it->second;
}

std::string TournamentEngine::markInProgress(int matchId, int sessionId)
{
	auto it = tournament.matches.find(matchId);
	if (it == tournament.matches.end())
		return "Match introuvable.";
	if (it->second.status != MatchStatus::READY)
		return "Ce match n'est pas prêt à être joué.";

	it->second.status = MatchStatus::IN_PROGRESS;
	it->second.sessionId = sessionId;
	return "";
}

std::string TournamentEngine::reportResult(int matchId, const MatchResult & result)
{
	auto it = tournament.matches.find(matchId);
	if (it == tournament.matches.end())
		return "Match introuvable.";

	TMatch & match = it->second;
	if (match.status == MatchStatus::DONE)
		return "Ce match est déjà terminé (utiliser la correction de résultat).";
	if (match.status == MatchStatus::PENDING)
		return "Les équipes de ce match ne sont pas encore connues.";
	if (result.winnerTeamId != match.teamA && result.winnerTeamId != match.teamB)
		return "Le vainqueur ne joue pas ce match.";

	match.result = result;
	match.status = MatchStatus::DONE;
	match.sessionId = 0;
	propagate();
	return "";
}

std::string TournamentEngine::resetMatch(int matchId)
{
	auto it = tournament.matches.find(matchId);
	if (it == tournament.matches.end())
		return "Match introuvable.";
	if (it->second.status != MatchStatus::IN_PROGRESS)
		return "Ce match n'est pas en cours.";

	it->second.status = MatchStatus::READY;
	it->second.sessionId = 0;
	return "";
}

void TournamentEngine::recoverAfterRestart()
{
	for (auto & entry : tournament.matches)
	{
		if (entry.second.status == MatchStatus::IN_PROGRESS)
		{
			entry.second.status = MatchStatus::READY;
			entry.second.sessionId = 0;
		}
	}
}

void TournamentEngine::collectDependents(int matchId, std::set<int> & dependents) const
{
	for (const auto & entry : tournament.matches)
	{
		const TMatch & match = entry.second;
		bool depends = false;
		for (const SlotRef * slot : { &match.slotA, &match.slotB })
		{
			if ((slot->kind == SlotRef::Kind::WINNER_OF || slot->kind == SlotRef::Kind::LOSER_OF) && slot->value == matchId)
				depends = true;
		}

		if (depends && dependents.insert(match.id).second)
			collectDependents(match.id, dependents);
	}
}

std::string TournamentEngine::amendResult(int matchId, const MatchResult & result, bool cascade)
{
	auto it = tournament.matches.find(matchId);
	if (it == tournament.matches.end())
		return "Match introuvable.";

	TMatch & match = it->second;
	if (match.status != MatchStatus::DONE || !match.result.has_value())
		return "Seul le résultat d'un match terminé peut être corrigé.";
	if (match.result->reason == ResultReason::BYE)
		return "Un exempt ne peut pas être corrigé.";
	if (result.winnerTeamId != match.teamA && result.winnerTeamId != match.teamB)
		return "Le vainqueur ne joue pas ce match.";

	const int stageIndex = match.stageIndex;
	Stage & stage = tournament.stages[stageIndex];

	// 1. Matchs à annuler : ceux qui dépendent du résultat dans le tableau...
	std::set<int> dependents;
	collectDependents(matchId, dependents);

	// ...les rondes suisses suivantes (leurs appariements dépendent du classement)...
	std::set<int> toDelete;
	if (stage.type == StageType::SWISS)
	{
		for (const auto & entry : tournament.matches)
		{
			if (entry.second.stageIndex == stageIndex && entry.second.round > match.round)
				toDelete.insert(entry.first);
		}
	}

	// ...et les phases suivantes si elles ont déjà été générées.
	bool resetLaterStages = false;
	for (int i = stageIndex + 1; i < (int)tournament.stages.size(); i++)
	{
		if (tournament.stages[i].built && (stage.type == StageType::ROUND_ROBIN_POOLS || stage.type == StageType::SWISS))
		{
			resetLaterStages = true;
			for (const auto & entry : tournament.matches)
			{
				if (entry.second.stageIndex == i)
					toDelete.insert(entry.first);
			}
		}
	}

	// 2. Refus si des matchs concernés ont déjà été joués ou sont en cours.
	std::set<int> affected = dependents;
	affected.insert(toDelete.begin(), toDelete.end());
	if (!cascade)
	{
		for (int id : affected)
		{
			const TMatch & other = tournament.matches.at(id);
			bool played = other.status == MatchStatus::IN_PROGRESS
				|| (other.status == MatchStatus::DONE && other.result.has_value() && other.result->reason != ResultReason::BYE);
			if (played)
				return "Des matchs qui dépendent de ce résultat ont déjà été joués ou sont en cours : correction refusée (utiliser la cascade pour les annuler).";
		}
	}

	// 3. Application.
	match.result = result;

	for (int id : dependents)
	{
		if (toDelete.count(id) > 0)
			continue;

		TMatch & other = tournament.matches.at(id);
		// La revanche de grande finale n'existe que si le vainqueur du tableau des perdants a gagné.
		if (other.bracket == "GF2")
		{
			toDelete.insert(id);
			continue;
		}

		other.status = MatchStatus::PENDING;
		other.result.reset();
		other.sessionId = 0;
		other.teamA = UNKNOWN_TEAM;
		other.teamB = UNKNOWN_TEAM;
	}

	for (int id : toDelete)
		tournament.matches.erase(id);

	if (stage.type == StageType::SWISS)
		stage.currentRound = match.round;

	if (resetLaterStages)
	{
		for (int i = stageIndex + 1; i < (int)tournament.stages.size(); i++)
		{
			tournament.stages[i].built = false;
			tournament.stages[i].finished = false;
			tournament.stages[i].seeds.clear();
		}
	}

	for (int i = stageIndex; i < (int)tournament.stages.size(); i++)
		tournament.stages[i].finished = false;

	tournament.status = TournamentStatus::RUNNING;
	propagate();
	return "";
}
