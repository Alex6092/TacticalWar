#include <doctest.h>

#include <filesystem>
#include <JsonFile.h>
#include <ReplayStore.h>

using namespace tw::store;
using nlohmann::json;

namespace
{
	std::string freshDirectory(const char * name)
	{
		std::filesystem::path dir = std::filesystem::temp_directory_path() / name;
		std::filesystem::remove_all(dir);
		std::filesystem::create_directories(dir);
		return dir.string();
	}
}

TEST_CASE("Rediffusions : enregistrement, liste et relecture")
{
	ReplayLibrary library(freshDirectory("tw_replay_tests"));

	{
		ReplayWriter writer;
		REQUIRE(writer.open(library.pathOf("20261002-100000-1"), {
			{ "title", "Poule A" }, { "teams", json::array({ "Les Éclairs", "Équipe 2" }) }, { "date", "02/10/2026 10:00" },
			{ "mapId", 3 }, { "snapshot", { { "seq", 4 }, { "you", -1 } } } }));
		writer.append(0, { { "seq", 5 }, { "ev", json::array() } });
		writer.append(1200, { { "seq", 6 }, { "ev", json::array() } });
		writer.finish({ { "winner", 2 }, { "reason", "KO" }, { "rounds", 7 } });
	}
	{
		// Combat interrompu (serveur arrêté) : pas de ligne de fin, dernière ligne tronquée.
		ReplayWriter writer;
		REQUIRE(writer.open(library.pathOf("20261002-110000-2"), {
			{ "title", "Poule B" }, { "snapshot", { { "seq", 1 } } } }));
		writer.append(10, { { "seq", 2 } });
		REQUIRE(appendLine(library.pathOf("20261002-110000-2"), "{\"t\": 20, \"bat"));
	}

	json list = library.list();
	REQUIRE(list.size() == 2);
	CHECK(list[0]["id"] == "20261002-110000-2");	// Le plus récent d'abord
	CHECK(list[0]["complete"] == false);
	CHECK(list[1]["title"] == "Poule A");
	CHECK(list[1]["teams"][0] == "Les Éclairs");
	CHECK(list[1]["winner"] == 2);
	CHECK(list[1]["rounds"] == 7);

	Replay replay;
	REQUIRE(library.load("20261002-100000-1", replay));
	CHECK(replay.header["mapId"] == 3);
	REQUIRE(replay.batches.size() == 2);
	CHECK(replay.batches[1].first == 1200);
	CHECK(replay.batches[1].second["seq"] == 6);
	CHECK(replay.end["winner"] == 2);

	Replay interrupted;
	REQUIRE(library.load("20261002-110000-2", interrupted));
	CHECK(interrupted.batches.size() == 1);
	CHECK(interrupted.end.is_null());

	// Les identifiants ne peuvent pas désigner un autre fichier.
	CHECK_FALSE(library.load("../teams", replay));
	CHECK_FALSE(isValidReplayId("a/b"));
	CHECK_FALSE(isValidReplayId(""));
	CHECK(isValidReplayId("20261002-100000-1"));

	CHECK(library.remove("20261002-110000-2"));
	CHECK(library.list().size() == 1);
}
