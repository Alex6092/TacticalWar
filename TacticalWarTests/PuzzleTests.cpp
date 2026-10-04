#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

#include <Environment.h>
#include <EnvironmentManager.h>
#include <EnvironmentMap.h>
#include <Puzzle.h>

using namespace tw::battle;

namespace
{
	// Les tests sont lancés depuis la racine du dépôt ou depuis x64/Debug.
	std::string assetsDir()
	{
		for (const char * path : { "assets", "../../assets", "../assets" })
		{
			if (std::filesystem::exists(std::string(path) + "/puzzles"))
				return path;
		}
		return std::string();
	}

	std::string readFile(const std::filesystem::path & path)
	{
		std::ifstream file(path, std::ios::binary);
		std::stringstream content;
		content << file.rdbuf();
		return content.str();
	}

	const GameData & puzzleData()
	{
		static GameData data;
		static bool loaded = false;
		if (!loaded)
		{
			std::string error;
			REQUIRE(data.loadFromFile(assetsDir() + "/data/gamedata.json", error));
			loaded = true;
		}
		return data;
	}

	// Joue une suite d'actions (dégâts minimum), puis termine les tours restants de l'équipe 1.
	BattleState play(const Puzzle & puzzle, const BattleMap & map, const std::vector<PuzzleAction> & actions)
	{
		BattleEngine engine(puzzleData(), map, puzzleState(puzzleData(), map, puzzle), 1);
		engine.setRollMode(BattleEngine::RollMode::MIN);
		std::int64_t now = 1000;
		for (const PuzzleAction & action : actions)
			playPuzzleAction(engine, action, now);
		for (int guard = 0; guard < 8 && !puzzleSolved(engine.getState()) && !puzzleFailed(engine.getState()); guard++)
			engine.endTurn(engine.getState().activeFighterId(), now);
		return engine.getState();
	}
}

TEST_CASE("Each puzzle loads, its solution solves it, and doing nothing or its trap does not")
{
	std::string assets = assetsDir();
	REQUIRE_FALSE(assets.empty());
	int count = 0;
	std::vector<int> orders;
	for (const auto & entry : std::filesystem::directory_iterator(assets + "/puzzles"))
	{
		if (entry.path().extension() != ".json")
			continue;
		INFO(entry.path().filename().string());
		Puzzle puzzle;
		std::string error;
		REQUIRE_MESSAGE(parsePuzzle(readFile(entry.path()), puzzle, error), error);
		count++;
		orders.push_back(puzzle.order);
		CHECK_FALSE(puzzle.title.empty());
		CHECK_FALSE(puzzle.goal.empty());
		CHECK_FALSE(puzzle.hint.empty());
		REQUIRE_FALSE(puzzle.solution.empty());

		std::unique_ptr<tw::Environment> environment(tw::EnvironmentManager::fromJson(readFile(assets + "/map/" + std::to_string(puzzle.mapId) + ".json")));
		REQUIRE(environment != nullptr);
		BattleMap map = battleMapFromEnvironment(environment.get());

		// Position de départ : chacun sur sa propre case praticable ; tour du premier combattant de l'équipe 1.
		BattleState start = puzzleState(puzzleData(), map, puzzle);
		for (const Fighter & fighter : start.fighters)
		{
			CHECK(map.isWalkable(fighter.position));
			for (const Fighter & other : start.fighters)
				CHECK((other.id == fighter.id || other.position != fighter.position));
		}
		CHECK(start.findFighter(start.activeFighterId())->team == 1);
		CHECK_FALSE(puzzleSolved(start));
		CHECK_FALSE(puzzleFailed(start));

		CHECK(puzzleSolved(play(puzzle, map, puzzle.solution)));
		BattleState idle = play(puzzle, map, {});
		CHECK_FALSE(puzzleSolved(idle));
		CHECK(puzzleFailed(idle));
		if (!puzzle.trap.empty())
			CHECK_FALSE(puzzleSolved(play(puzzle, map, puzzle.trap)));
	}
	CHECK(count == 6);
	std::sort(orders.begin(), orders.end());
	CHECK(orders == std::vector<int>{ 1, 2, 3, 4, 5, 6 });

	std::string error;
	Puzzle invalid;
	CHECK_FALSE(parsePuzzle("{}", invalid, error));
	CHECK_FALSE(parsePuzzle(R"({"id": "x", "map": 4, "fighters": [{"team": 1, "class": 4, "x": 1, "y": 1}]})", invalid, error));
}
