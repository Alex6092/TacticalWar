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

std::string BattleSession::choiceRefusal(tw::Player * player, int classId) const
{
	if (phase == Phase::BAN)
		return "Le bannissement n'est pas terminé.";
	if (phase != Phase::CLASS_SELECTION)
		return "Délai écoulé : la classe affichée a été retenue.";
	if (fighterIdOf(player) < 0)
		return "Ce combat n'est pas le vôtre.";
	if (classes.count(player) > 0)
		return "Classe déjà verrouillée.";
	if (data.findClass(classId) == nullptr)
		return "Classe inconnue.";
	if (classId == forbiddenClass(teamOf(player)))
		return "Classe interdite par l'adversaire.";
	return std::string();
}

bool BattleSession::chooseClass(tw::Player * player, int classId, const std::vector<int> & spells, const std::vector<std::string> & talents,
	tw::Player * chooser)
{
	if (!choiceRefusal(player, classId).empty())
		return false;

	const tw::battle::ClassDef * classDef = data.findClass(classId);
	classes[player] = classId;
	spellChoices[player] = tw::battle::validSpellChoice(*classDef, spells);
	talentChoices[player] = tw::battle::validTalentChoice(data, talents, talentSlots(player));
	choosers[player] = chooser != NULL ? chooser : player;
	return true;
}

bool BattleSession::setDraft(tw::Player * player, const Draft & draft)
{
	if ((phase != Phase::BAN && phase != Phase::CLASS_SELECTION) || fighterIdOf(player) < 0 || classes.count(player) > 0)
		return false;
	bool changed = viewingClass(player) != draft.classId;
	drafts[player] = draft;
	return changed;
}

int BattleSession::viewingClass(tw::Player * player) const
{
	auto it = drafts.find(player);
	return it == drafts.end() ? 0 : it->second.classId;
}

int BattleSession::lockViewedClasses()
{
	int locked = 0;
	for (tw::Player * player : participants)
	{
		auto draft = drafts.find(player);
		if (draft == drafts.end() || classes.count(player) > 0)
			continue;
		if (chooseClass(player, draft->second.classId, draft->second.spells, draft->second.talents))
		{
			if (!draft->second.appearance.empty())
				appearances[player] = draft->second.appearance;
			locked++;
		}
	}
	return locked;
}

std::vector<int> BattleSession::chosenSpells(tw::Player * player) const
{
	auto it = spellChoices.find(player);
	return it == spellChoices.end() ? std::vector<int>() : it->second;
}

std::vector<std::string> BattleSession::chosenTalents(tw::Player * player) const
{
	auto it = talentChoices.find(player);
	return it == talentChoices.end() ? std::vector<std::string>() : it->second;
}

tw::Player * BattleSession::chooserOf(tw::Player * player) const
{
	auto it = choosers.find(player);
	return it == choosers.end() ? NULL : it->second;
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
		// Ni classe choisie ni classe regardée (joueur jamais connecté) : classe au hasard, sorts par défaut.
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
		auto appearance = appearances.find(player);
		engine->addFighter(team, classId, name != names.end() ? name->second : player->getPseudo(), spells, talents,
			appearance != appearances.end() ? appearance->second : std::string());
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
