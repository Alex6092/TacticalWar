#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <set>

#include <TournamentService.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using namespace tw;
using namespace tw::tournament;

namespace
{
	struct DataDir
	{
		fs::path path;
		DataDir() { path = fs::temp_directory_path() / ("tw-service-" + std::to_string(std::rand())); }
		~DataDir() { std::error_code ec; fs::remove_all(path, ec); }
		std::string str() const { return path.u8string(); }
	};

	MatchResult winBy(int team)
	{
		MatchResult result;
		result.winnerTeamId = team;
		result.reason = ResultReason::KO;
		result.hpPercentA = 50;
		result.rounds = 5;
		return result;
	}

	Settings doubleElimination()
	{
		Settings settings;
		settings.format = Format::DOUBLE_ELIMINATION;
		return settings;
	}
}

TEST_CASE("TournamentService launches ready matches within the constraints")
{
	DataDir dir;
	TournamentService service(dir.str());
	std::string log;
	REQUIRE(service.load(log));

	int id = 0;
	REQUIRE(service.create("Coupe", doubleElimination(), { 1, 2, 3, 4, 5, 6, 7, 8 }, id) == "");
	CHECK(service.nextLaunches([](int) { return false; }, 0, 0, {}).empty());	// pas encore démarré
	REQUIRE(service.start(id) == "");

	TournamentService::DispatchOptions options;
	options.maxConcurrentMatches = 3;
	std::vector<TournamentService::LaunchRequest> launches = service.nextLaunches([](int) { return false; }, 0, 0, options);
	CHECK(launches.size() == 3);

	std::set<int> teams;
	for (const auto & launch : launches)
	{
		CHECK(teams.insert(launch.teamA).second);
		CHECK(teams.insert(launch.teamB).second);
	}

	// Une équipe occupée n'est pas relancée.
	int busyTeam = launches[0].teamA;
	launches = service.nextLaunches([busyTeam](int team) { return team == busyTeam; }, 0, 0, {});
	for (const auto & launch : launches)
		CHECK((launch.teamA != busyTeam && launch.teamB != busyTeam));
	CHECK(launches.size() == 3);

	// Pause : plus aucun lancement.
	REQUIRE(service.setPaused(id, true) == "");
	CHECK(service.nextLaunches([](int) { return false; }, 0, 0, {}).empty());
	REQUIRE(service.setPaused(id, false) == "");

	// Repos entre deux matchs.
	launches = service.nextLaunches([](int) { return false; }, 0, 0, {});
	service.markTeamFinished(launches[0].teamA, 1000);
	auto rested = service.nextLaunches([](int) { return false; }, 0, 5000, {});
	for (const auto & launch : rested)
		CHECK((launch.teamA != launches[0].teamA && launch.teamB != launches[0].teamA));
	CHECK(service.nextLaunches([](int) { return false; }, 0, 30000, {}).size() == 4);
}

TEST_CASE("TournamentService persists results and recovers in-progress matches")
{
	DataDir dir;
	int id = 0;
	TournamentService::LaunchRequest played;
	TournamentService::LaunchRequest running;

	{
		TournamentService service(dir.str());
		std::string log;
		REQUIRE(service.load(log));
		REQUIRE(service.create(u8"Coupe d'été", doubleElimination(), { 1, 2, 3, 4 }, id) == "");
		REQUIRE(service.start(id) == "");

		auto launches = service.nextLaunches([](int) { return false; }, 0, 0, {});
		REQUIRE(launches.size() == 2);
		played = launches[0];
		running = launches[1];
		service.markLaunched(played, 10);
		service.markLaunched(running, 11);
		REQUIRE(service.reportResult(id, played.matchId, winBy(played.teamA), 1234) == "");
		CHECK(service.reportResult(id, played.matchId, winBy(played.teamA), 1234) != "");	// déjà terminé
	}

	// Redémarrage du serveur.
	TournamentService reloaded(dir.str());
	std::string log;
	REQUIRE(reloaded.load(log));
	const TournamentEngine * engine = reloaded.find(id);
	REQUIRE(engine != nullptr);
	CHECK(engine->get().name == u8"Coupe d'été");
	CHECK((engine->findMatch(played.matchId)->status == MatchStatus::DONE));
	CHECK((engine->findMatch(running.matchId)->status == MatchStatus::READY));

	// Journal des résultats.
	std::ifstream journal(fs::u8path(dir.str()) / "results.jsonl");
	std::string line;
	REQUIRE(std::getline(journal, line));
	nlohmann::json entry = nlohmann::json::parse(line);
	CHECK(entry["seed"] == 1234);
	CHECK(entry["winner"] == played.teamA);

	// Correction par l'admin d'un match terminé (sans cascade : rien n'a encore été joué après).
	REQUIRE(reloaded.forceResult(id, played.matchId, winBy(played.teamB), false) == "");
	CHECK(reloaded.find(id)->findMatch(played.matchId)->result->winnerTeamId == played.teamB);

	// Un tournoi en cours avec des matchs lancés ne peut pas être supprimé.
	reloaded.markLaunched({ id, running.matchId, running.teamA, running.teamB }, 12);
	CHECK(reloaded.remove(id) != "");
	REQUIRE(reloaded.resetMatch(id, running.matchId) == "");
	CHECK(reloaded.remove(id) == "");
	CHECK(reloaded.find(id) == nullptr);
}

TEST_CASE("TournamentService only edits tournaments before they start")
{
	DataDir dir;
	TournamentService service(dir.str());
	std::string log;
	REQUIRE(service.load(log));

	int id = 0;
	CHECK(service.create("", doubleElimination(), { 1, 2 }, id) != "");
	REQUIRE(service.create("A", doubleElimination(), { 1, 2 }, id) == "");
	REQUIRE(service.update(id, "B", doubleElimination(), { 1, 2, 3 }) == "");
	CHECK(service.find(id)->get().teamIds.size() == 3);
	REQUIRE(service.start(id) == "");
	CHECK(service.update(id, "C", doubleElimination(), { 1, 2 }) != "");
}
