#include "BattleState.h"

#include <algorithm>

using namespace tw::battle;

namespace
{
	const std::uint8_t WALKABLE = 1;
	const std::uint8_t BLOCKS_SIGHT = 2;
}

BattleMap::BattleMap(int width, int height)
	: width(width), height(height), flags(width * height, WALKABLE)
{
}

bool BattleMap::isWalkable(const Cell & cell) const
{
	return contains(cell) && (flags[cell.y * width + cell.x] & WALKABLE) != 0;
}

bool BattleMap::blocksSight(const Cell & cell) const
{
	return contains(cell) && (flags[cell.y * width + cell.x] & BLOCKS_SIGHT) != 0;
}

void BattleMap::setCell(const Cell & cell, bool walkable, bool blocksSight)
{
	if (!contains(cell))
		return;
	flags[cell.y * width + cell.x] = (walkable ? WALKABLE : 0) | (blocksSight ? BLOCKS_SIGHT : 0);
}

bool ZoneState::contains(const Cell & cell) const
{
	return std::find(cells.begin(), cells.end(), cell) != cells.end();
}

bool Fighter::hasState(const std::string & state) const
{
	for (const ActiveEffect & effect : effects)
	{
		if (effect.type == EffectType::STATE && effect.state == state)
			return true;
	}
	return false;
}

int BattleState::activeFighterId() const
{
	if (phase != BattlePhase::FIGHT || turnOrder.empty())
		return -1;
	return turnOrder[turnIndex];
}

Fighter * BattleState::findFighter(int id)
{
	for (Fighter & fighter : fighters)
	{
		if (fighter.id == id)
			return &fighter;
	}
	return nullptr;
}

const Fighter * BattleState::findFighter(int id) const
{
	for (const Fighter & fighter : fighters)
	{
		if (fighter.id == id)
			return &fighter;
	}
	return nullptr;
}

const Fighter * BattleState::fighterAt(const Cell & cell) const
{
	for (const Fighter & fighter : fighters)
	{
		if (fighter.alive && fighter.position == cell)
			return &fighter;
	}
	return nullptr;
}

const char * tw::battle::toString(BattlePhase phase)
{
	switch (phase)
	{
	case BattlePhase::PLACEMENT: return "PLACEMENT";
	case BattlePhase::FIGHT: return "FIGHT";
	case BattlePhase::ENDED: return "ENDED";
	}
	return "";
}

const char * tw::battle::toString(EndReason reason)
{
	switch (reason)
	{
	case EndReason::NONE: return "NONE";
	case EndReason::KO: return "KO";
	case EndReason::ROUND_LIMIT: return "ROUND_LIMIT";
	case EndReason::FORFEIT: return "FORFEIT";
	case EndReason::ADMIN: return "ADMIN";
	case EndReason::OBJECTIVE: return "OBJECTIVE";
	}
	return "";
}
