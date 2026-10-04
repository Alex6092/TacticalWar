#include "Achievements.h"

#include "BattleState.h"

using namespace tw::battle;

namespace
{
	const int DEMOLISHER_DAMAGE = 150;
	const int GUARDIAN_SUPPORT = 60;
	const int ZONE_KEEPER_POINTS = 3;
	const int LIGHTNING_ROUNDS = 5;
}

const AchievementDef * tw::battle::findAchievement(const std::string & id)
{
	for (const AchievementDef & achievement : ACHIEVEMENTS)
	{
		if (id == achievement.id)
			return &achievement;
	}
	return nullptr;
}

std::vector<std::string> tw::battle::earnedAchievements(const BattleState & state, const Fighter & fighter)
{
	const FighterRecord & record = fighter.record;
	bool winner = fighter.team == state.winnerTeam;
	// Combat joué jusqu'au bout : un forfait ou une décision de l'organisateur ne donne ni
	// « Intouchable » ni « Victoire éclair ».
	bool played = state.endReason == EndReason::KO || state.endReason == EndReason::OBJECTIVE || state.endReason == EndReason::ROUND_LIMIT;

	int teammates = 0;
	int teammatesAlive = 0;
	for (const Fighter & other : state.fighters)
	{
		if (other.team != fighter.team || other.id == fighter.id)
			continue;
		teammates++;
		teammatesAlive += other.alive ? 1 : 0;
	}

	std::vector<std::string> earned;
	auto award = [&earned](bool condition, const char * id) {
		if (condition)
			earned.push_back(id);
	};
	award(fighter.id == state.firstBloodFighterId, "first_blood");
	award(record.kills >= 2, "double_ko");
	award(record.combos >= 2, "combo_master");
	award(record.dealt >= DEMOLISHER_DAMAGE, "demolisher");
	award(record.healed + record.shielded >= GUARDIAN_SUPPORT, "guardian_angel");
	award(played && fighter.alive && record.taken == 0 && record.casts > 0, "untouchable");
	award(winner && fighter.alive && teammates > 0 && teammatesAlive == 0, "last_standing");
	award(record.zonePoints >= ZONE_KEEPER_POINTS, "zone_keeper");
	award(winner && played && state.endReason != EndReason::ROUND_LIMIT && state.round <= LIGHTNING_ROUNDS, "lightning");
	return earned;
}
