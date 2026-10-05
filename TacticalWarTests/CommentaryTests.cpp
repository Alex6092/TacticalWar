#include <doctest.h>

#include <Commentary.h>

using namespace tw::battle;
using nlohmann::json;

namespace
{
	BattleState fourFighters()
	{
		BattleState state;
		state.phase = BattlePhase::FIGHT;
		const char * names[4] = { u8"Léa", "Tom", "Zoé", "Max" };
		for (int i = 0; i < 4; i++)
		{
			Fighter fighter;
			fighter.id = i;
			fighter.team = i < 2 ? 1 : 2;
			fighter.name = names[i];
			fighter.hp = 100;
			fighter.maxHp = 100;
			fighter.alive = true;
			state.fighters.push_back(fighter);
		}
		return state;
	}

	bool contains(const std::string & text, const std::string & part)
	{
		return text.find(part) != std::string::npos;
	}
}

TEST_CASE("The commentator names a combo with its author and target")
{
	Commentary commentary(7);
	commentary.setTeamNames("Les Bleus", "Les Rouges");
	BattleState state = fourFighters();
	json events = json::array({ { { "t", "combo" }, { "f", 2 }, { "src", 0 }, { "name", "Brise-glace" }, { "percent", 40 } } });
	std::string line = commentary.onEvents(events, state, 10000);
	CHECK(contains(line, "Brise-glace"));
	CHECK(contains(line, "Zoé"));
}

TEST_CASE("Two knockouts in the same turn make a double KO, the end of the fight always passes")
{
	Commentary commentary(3);
	commentary.setTeamNames("Les Bleus", "Les Rouges");
	BattleState state = fourFighters();
	state.fighters[2].alive = false;
	state.fighters[2].hp = 0;
	state.fighters[3].alive = false;
	state.fighters[3].hp = 0;
	json events = json::array({
		{ { "t", "turn" }, { "f", 0 }, { "round", 4 } },
		{ { "t", "damage" }, { "f", 2 }, { "src", 0 }, { "amount", 18 }, { "absorbed", 0 } },
		{ { "t", "death" }, { "f", 2 } },
		{ { "t", "damage" }, { "f", 3 }, { "src", 0 }, { "amount", 12 }, { "absorbed", 0 } },
		{ { "t", "death" }, { "f", 3 } } });
	std::string line = commentary.onEvents(events, state, 20000);
	CHECK((contains(line, "Double KO") || contains(line, u8"deuxième KO") || contains(line, u8"Deux combattants")));

	// Fin du combat juste après : dite tout de suite, malgré la phrase précédente.
	state.phase = BattlePhase::ENDED;
	std::string end = commentary.onEvents(json::array({ { { "t", "end" }, { "winner", 1 }, { "round", 4 } } }), state, 20500);
	CHECK(contains(end, "Les Bleus"));
}

TEST_CASE("The commentator speaks at most every 3 seconds and keeps the most important line")
{
	Commentary commentary(11);
	BattleState state = fourFighters();
	json orb = json::array({ { { "t", "orb-" }, { "f", 1 }, { "kind", "soin" } } });
	CHECK_FALSE(commentary.onEvents(orb, state, 1000).empty());

	// Trop tôt : un gros coup puis un orbe ; c'est le gros coup qui sera dit au bon moment.
	json hit = json::array({ { { "t", "damage" }, { "f", 3 }, { "src", 2 }, { "amount", 30 }, { "absorbed", 0 } } });
	CHECK(commentary.onEvents(hit, state, 1500).empty());
	CHECK(commentary.onEvents(orb, state, 2000).empty());
	CHECK(commentary.poll(3000).empty());
	std::string later = commentary.poll(4000);
	CHECK(contains(later, "30"));
	CHECK(commentary.poll(8000).empty());

	// Une phrase en attente trop longtemps est oubliée.
	CHECK_FALSE(commentary.onEvents(orb, state, 20000).empty());
	CHECK(commentary.onEvents(hit, state, 20500).empty());
	CHECK(commentary.poll(20500 + Commentary::STALE_MS).empty());
}

TEST_CASE("The commentator is reproducible with the same seed")
{
	BattleState state = fourFighters();
	json combo = json::array({ { { "t", "combo" }, { "f", 3 }, { "src", 1 }, { "name", "Jugement ardent" } } });
	for (std::uint32_t seed : { 1u, 2u, 3u })
	{
		Commentary first(seed);
		Commentary second(seed);
		CHECK(first.onEvents(combo, state, 5000) == second.onEvents(combo, state, 5000));
	}
}
