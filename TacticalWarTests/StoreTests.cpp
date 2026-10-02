#include <doctest.h>

#include <filesystem>
#include <fstream>

#include <JsonFile.h>
#include <ServerConfig.h>

namespace fs = std::filesystem;

namespace
{
	// Dossier temporaire supprimé à la fin du test.
	struct TempDir
	{
		fs::path path;

		TempDir()
		{
			path = fs::temp_directory_path() / fs::u8path(u8"tw-tests-é-" + std::to_string(std::rand()));
			fs::create_directories(path);
		}

		~TempDir()
		{
			std::error_code ec;
			fs::remove_all(path, ec);
		}

		std::string file(const std::string & name) const
		{
			return (path / fs::u8path(name)).u8string();
		}
	};
}

TEST_CASE("writeJsonFileAtomic creates directories and replaces existing files")
{
	TempDir dir;
	std::string path = dir.file(u8"sous-dossier/équipes.json");

	REQUIRE(tw::store::writeJsonFileAtomic(path, { { "version", 1 } }));
	REQUIRE(tw::store::writeJsonFileAtomic(path, { { "version", 2 } }));

	nlohmann::json json;
	REQUIRE(tw::store::readJsonFile(path, json));
	CHECK(json["version"] == 2);
	CHECK_FALSE(tw::store::fileExists(path + ".tmp"));
}

TEST_CASE("readJsonFile reports missing and invalid files")
{
	TempDir dir;
	nlohmann::json json;
	std::string error;

	CHECK_FALSE(tw::store::readJsonFile(dir.file("absent.json"), json, &error));
	CHECK_FALSE(error.empty());

	std::string invalid = dir.file("invalid.json");
	std::ofstream(fs::u8path(invalid)) << "{ not json";
	CHECK_FALSE(tw::store::readJsonFile(invalid, json, &error));
}

TEST_CASE("appendLine appends JSON Lines records")
{
	TempDir dir;
	std::string path = dir.file("results.jsonl");

	REQUIRE(tw::store::appendLine(path, "{\"match\":1}"));
	REQUIRE(tw::store::appendLine(path, "{\"match\":2}"));

	std::ifstream file(fs::u8path(path));
	std::string first, second;
	std::getline(file, first);
	std::getline(file, second);
	CHECK(first == "{\"match\":1}");
	CHECK(second == "{\"match\":2}");
}

TEST_CASE("ServerConfig is created with defaults and completed when fields are missing")
{
	TempDir dir;
	std::string path = dir.file("server.json");
	std::string log;

	tw::ServerConfig created = tw::ServerConfig::loadOrCreate(path, log);
	CHECK(created.gamePort == 12345);
	REQUIRE(tw::store::fileExists(path));

	REQUIRE(tw::store::writeJsonFileAtomic(path, { { "gamePort", 4000 } }));
	tw::ServerConfig loaded = tw::ServerConfig::loadOrCreate(path, log);
	CHECK(loaded.gamePort == 4000);
	CHECK(loaded.httpPort == 8080);

	nlohmann::json completed;
	REQUIRE(tw::store::readJsonFile(path, completed));
	CHECK(completed["httpPort"] == 8080);
	CHECK(completed["gamePort"] == 4000);
}

TEST_CASE("An invalid ServerConfig file is never overwritten")
{
	TempDir dir;
	std::string path = dir.file("server.json");
	std::ofstream(fs::u8path(path)) << "{ broken";

	std::string log;
	tw::ServerConfig config = tw::ServerConfig::loadOrCreate(path, log);
	CHECK(config.gamePort == 12345);

	std::ifstream file(fs::u8path(path));
	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	CHECK(content == "{ broken");
}
