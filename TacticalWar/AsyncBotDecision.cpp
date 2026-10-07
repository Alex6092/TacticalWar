#include "AsyncBotDecision.h"

#include <chrono>
#include <random>

using namespace tw;

namespace
{
	bool ready(const std::future<battle::BotAction> & future)
	{
		return future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	}
}

AsyncBotDecision::~AsyncBotDecision()
{
	cancelled = true;
	if (future.valid())
		future.wait();
}

bool AsyncBotDecision::requested(const Key & key) const
{
	return future.valid() && running == key;
}

void AsyncBotDecision::request(const Key & key, const battle::BattleState & state, const battle::BattleMap & map, const battle::GameData & gameData,
	std::uint32_t seed, const battle::BotOptions & options)
{
	if (requested(key))
		return;
	if (future.valid())
	{
		cancelled = true;
		if (!ready(future))
			return;
		future.get();
	}

	if (!data)
		data = std::make_shared<const battle::GameData>(gameData);
	cancelled = false;
	running = key;
	battle::BotOptions cancellable = options;
	cancellable.cancel = &cancelled;
	std::shared_ptr<const battle::GameData> sharedData = data;
	int fighterId = key.fighter;
	future = std::async(std::launch::async, [state, map, sharedData, fighterId, seed, cancellable]() {
		std::mt19937 rng(seed);
		return battle::chooseBotAction(state, map, *sharedData, fighterId, rng, cancellable);
	});
}

bool AsyncBotDecision::take(const Key & key, battle::BotAction & action)
{
	if (!requested(key) || !ready(future))
		return false;
	action = future.get();
	running = Key();
	return true;
}

void AsyncBotDecision::cancel()
{
	cancelled = true;
	running = Key();
}
