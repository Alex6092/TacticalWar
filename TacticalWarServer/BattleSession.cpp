#include "BattleSession.h"

#include <EnvironmentMap.h>

#include <random>

BattleSession::BattleSession(int id, tw::Match * match, const tw::battle::GameData & data, tw::Environment * environment, std::int64_t classSelectionDeadline)
	: id(id), match(match), data(data), map(toBattleMap(environment)), mapId(environment->getId()),
	phase(Phase::CLASS_SELECTION), classSelectionDeadline(classSelectionDeadline)
{
	for (tw::Player * player : match->getTeam1())
		participants.push_back(player);
	for (tw::Player * player : match->getTeam2())
		participants.push_back(player);

	seed = std::random_device()();
}

tw::battle::BattleMap BattleSession::toBattleMap(tw::Environment * environment)
{
	return tw::battle::battleMapFromEnvironment(environment);
}

int BattleSession::fighterIdOf(tw::Player * player) const
{
	for (int i = 0; i < (int)participants.size(); i++)
	{
		if (participants[i] == player)
			return i;
	}
	return -1;
}

tw::Player * BattleSession::playerOfFighter(int fighterId) const
{
	return fighterId >= 0 && fighterId < (int)participants.size() ? participants[fighterId] : NULL;
}

bool BattleSession::chooseClass(tw::Player * player, int classId)
{
	if (phase != Phase::CLASS_SELECTION || fighterIdOf(player) < 0 || classes.count(player) > 0 || data.findClass(classId) == nullptr)
		return false;

	classes[player] = classId;
	return true;
}

int BattleSession::chosenClass(tw::Player * player) const
{
	auto it = classes.find(player);
	return it == classes.end() ? 0 : it->second;
}

bool BattleSession::allClassesChosen() const
{
	return classes.size() == participants.size();
}

void BattleSession::startBattle(std::int64_t nowMs, const std::map<tw::Player*, bool> & connected)
{
	if (phase != Phase::CLASS_SELECTION)
		return;

	engine.reset(new tw::battle::BattleEngine(data, map, seed));
	std::mt19937 rng(seed);

	for (int i = 0; i < (int)participants.size(); i++)
	{
		tw::Player * player = participants[i];
		int classId = chosenClass(player);
		if (classId == 0)
			classId = data.classes[rng() % data.classes.size()].id;

		int team = match->playerIsInTeam1(player) ? 1 : 2;
		engine->addFighter(team, classId, player->getPseudo());
	}

	engine->startPlacement(nowMs);

	// Les joueurs absents ne bloquent pas le placement (ils peuvent revenir en cours de combat).
	for (int i = 0; i < (int)participants.size(); i++)
	{
		auto it = connected.find(participants[i]);
		if (it == connected.end() || !it->second)
			engine->setConnected(i, false, nowMs);
	}

	phase = Phase::BATTLE;
}
