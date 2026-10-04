#include "BattleSession.h"

#include <algorithm>

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

int BattleSession::actingFighter(tw::Player * player) const
{
	int own = fighterIdOf(player);
	if (!engine || own < 0)
		return own;
	const tw::battle::BattleState & state = engine->getState();
	const tw::battle::Fighter * active = state.findFighter(state.activeFighterId());
	if (active != nullptr && active->id != own && active->piloted && active->team == teamOf(player))
		return active->id;
	return own;
}

int BattleSession::teamOf(tw::Player * player) const
{
	if (fighterIdOf(player) < 0)
		return 0;
	return match->playerIsInTeam1(player) ? 1 : 2;
}

int BattleSession::talentSlots(tw::Player * player) const
{
	int team = teamOf(player);
	return team != 0 ? talentSlotsByTeam[team] : 0;
}

void BattleSession::startBanPhase(std::int64_t deadline)
{
	if (phase != Phase::CLASS_SELECTION || banPhase)
		return;
	phase = Phase::BAN;
	banPhase = true;
	banDeadline = deadline;
}

bool BattleSession::ban(tw::Player * player, int classId)
{
	int team = teamOf(player);
	if (phase != Phase::BAN || team == 0 || bans[team] != 0 || data.findClass(classId) == nullptr)
		return false;
	bans[team] = classId;
	return true;
}

void BattleSession::endBanPhase(std::int64_t deadline)
{
	if (phase != Phase::BAN)
		return;
	phase = Phase::CLASS_SELECTION;
	classSelectionDeadline = deadline;
}

bool BattleSession::chooseClass(tw::Player * player, int classId, const std::vector<int> & spells, const std::vector<std::string> & talents)
{
	const tw::battle::ClassDef * classDef = data.findClass(classId);
	if (phase != Phase::CLASS_SELECTION || fighterIdOf(player) < 0 || classes.count(player) > 0 || classDef == nullptr
		|| classId == forbiddenClass(teamOf(player)))
		return false;

	classes[player] = classId;
	spellChoices[player] = tw::battle::validSpellChoice(*classDef, spells);
	talentChoices[player] = tw::battle::validTalentChoice(data, talents, talentSlots(player));
	return true;
}

bool BattleSession::setViewing(tw::Player * player, int classId)
{
	if ((phase != Phase::BAN && phase != Phase::CLASS_SELECTION) || fighterIdOf(player) < 0 || viewing[player] == classId)
		return false;
	viewing[player] = classId;
	return true;
}

int BattleSession::viewingClass(tw::Player * player) const
{
	auto it = viewing.find(player);
	return it == viewing.end() ? 0 : it->second;
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

void BattleSession::startBattle(std::int64_t nowMs, const std::map<tw::Player*, bool> & connected, const std::map<tw::Player*, std::string> & names)
{
	if (phase != Phase::CLASS_SELECTION)
		return;

	engine.reset(new tw::battle::BattleEngine(data, map, seed));
	std::mt19937 rng(seed);

	for (int i = 0; i < (int)participants.size(); i++)
	{
		tw::Player * player = participants[i];
		// Classe (et sorts) non choisis à temps : classe au hasard, sorts par défaut.
		int team = teamOf(player);
		int classId = chosenClass(player);
		std::vector<int> spells;
		if (classId == 0)
		{
			// Au hasard parmi les classes que l'adversaire n'a pas interdites.
			std::vector<int> allowed;
			for (const tw::battle::ClassDef & classDef : data.classes)
			{
				if (classDef.id != forbiddenClass(team))
					allowed.push_back(classDef.id);
			}
			classId = allowed[rng() % allowed.size()];
		}
		else
		{
			spells = spellChoices[player];
		}

		auto name = names.find(player);
		// Talents : ceux choisis, puis des talents au hasard pour les emplacements restés vides.
		std::vector<std::string> talents = talentChoices[player];
		for (const std::string & id : tw::battle::randomTalentChoice(data, (int)data.talents.size(), rng))
		{
			if ((int)talents.size() >= talentSlots(player))
				break;
			if (std::find(talents.begin(), talents.end(), id) == talents.end())
				talents.push_back(id);
		}
		engine->addFighter(team, classId, name != names.end() ? name->second : player->getPseudo(), spells, talents);
	}

	if (zonePoints > 0)
		engine->enableZone(zonePoints);
	if (mapBonuses)
		engine->enableMapBonuses();
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
