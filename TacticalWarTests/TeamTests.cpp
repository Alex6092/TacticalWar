#include <doctest.h>

#include <filesystem>

#include <CredentialSheet.h>
#include <LegacyImport.h>
#include <PasswordHasher.h>
#include <TeamStore.h>

namespace fs = std::filesystem;

namespace
{
	struct TempFile
	{
		fs::path path;

		explicit TempFile(const std::string & name)
		{
			path = fs::temp_directory_path() / ("tw-teams-" + std::to_string(std::rand())) / name;
		}

		~TempFile()
		{
			std::error_code ec;
			fs::remove_all(path.parent_path(), ec);
		}

		std::string str() const { return path.u8string(); }
	};

	tw::TeamInput makeInput(const std::string & name, const std::string & login1, const std::string & login2)
	{
		tw::TeamInput input;
		input.name = name;
		input.players[0].login = login1;
		input.players[1].login = login2;
		return input;
	}
}

TEST_CASE("PasswordHasher verifies only the right password")
{
	std::string hash = tw::PasswordHasher::hash(u8"mot de passe é", 1000);
	CHECK(hash.rfind("pbkdf2-sha256$1000$", 0) == 0);
	CHECK(tw::PasswordHasher::verify(u8"mot de passe é", hash));
	CHECK_FALSE(tw::PasswordHasher::verify("mot de passe", hash));
	CHECK_FALSE(tw::PasswordHasher::verify("", ""));
	CHECK_FALSE(tw::PasswordHasher::verify("x", "pbkdf2-sha256$abc$zz$zz"));

	// Le sel est aléatoire : deux empreintes du même mot de passe diffèrent.
	CHECK(tw::PasswordHasher::hash("abc", 1000) != tw::PasswordHasher::hash("abc", 1000));
}

TEST_CASE("Generated passwords avoid ambiguous characters")
{
	for (int i = 0; i < 200; i++)
	{
		std::string password = tw::PasswordHasher::generatePassword();
		REQUIRE(password.size() == 6);
		CHECK(password.find_first_of("0o1liIO") == std::string::npos);
	}
}

TEST_CASE("TeamStore creates teams, generates passwords and persists them")
{
	TempFile file("teams.json");
	std::map<std::string, std::string> passwords;
	int id = 0;

	{
		tw::TeamStore store(file.str());
		std::string error;
		REQUIRE(store.load(error));
		CHECK(store.getTeams().empty());

		tw::TeamInput input = makeInput(u8"Les Éclairs", "alice", "bob");
		input.players[1].password = "secret";
		REQUIRE(store.createTeam(input, id, passwords) == "");
		REQUIRE(store.save());

		CHECK(passwords.size() == 2);
		CHECK(passwords["bob"] == "secret");
		CHECK(passwords["alice"].size() == 6);
	}

	tw::TeamStore reloaded(file.str());
	std::string error;
	REQUIRE(reloaded.load(error));
	REQUIRE(reloaded.getTeams().size() == 1);
	CHECK(reloaded.getTeams()[0].name == u8"Les Éclairs");
	CHECK(reloaded.getTeams()[0].players[0].displayName == "alice");

	std::string canonical;
	int teamId = 0;
	CHECK(reloaded.authenticate("ALICE", passwords["alice"], &canonical, &teamId));
	CHECK(canonical == "alice");
	CHECK(teamId == id);
	CHECK_FALSE(reloaded.authenticate("alice", "wrong"));

	// Les identifiants ne sont jamais réutilisés.
	int secondId = 0;
	REQUIRE(reloaded.createTeam(makeInput("B", "c", "d"), secondId, passwords) == "");
	CHECK(secondId == id + 1);
}

TEST_CASE("TeamStore rejects invalid or conflicting teams")
{
	TempFile file("teams.json");
	tw::TeamStore store(file.str());
	std::map<std::string, std::string> passwords;
	int id = 0;

	REQUIRE(store.createTeam(makeInput("A", "alice", "bob"), id, passwords) == "");

	CHECK(store.createTeam(makeInput("", "x", "y"), id, passwords) != "");
	CHECK(store.createTeam(makeInput("a", "x", "y"), id, passwords) != "");			// même nom (casse ignorée)
	CHECK(store.createTeam(makeInput("B", "Alice", "y"), id, passwords) != "");		// login déjà pris
	CHECK(store.createTeam(makeInput("B", "x", "X"), id, passwords) != "");			// deux fois le même login
	CHECK(store.createTeam(makeInput("B", "x;y", "z"), id, passwords) != "");		// séparateur interdit
	CHECK(store.createTeam(makeInput("B", "admin", "z"), id, passwords) != "");		// login réservé
	CHECK(store.getTeams().size() == 1);
}

TEST_CASE("TeamStore updates teams, resets passwords and deactivates teams")
{
	TempFile file("teams.json");
	tw::TeamStore store(file.str());
	std::map<std::string, std::string> passwords;
	int id = 0;
	REQUIRE(store.createTeam(makeInput("A", "alice", "bob"), id, passwords) == "");
	std::string bobPassword = passwords["bob"];

	// Renommer un joueur lui génère un nouveau mot de passe, l'autre garde le sien.
	std::map<std::string, std::string> updated;
	tw::TeamInput input = makeInput("A2", "alice", "carol");
	REQUIRE(store.updateTeam(id, input, updated) == "");
	CHECK(updated.count("carol") == 1);
	CHECK(updated.count("alice") == 0);
	CHECK(store.authenticate("alice", passwords["alice"]));
	CHECK_FALSE(store.authenticate("bob", bobPassword));

	std::string newPassword;
	REQUIRE(store.resetPassword("carol", newPassword) == "");
	CHECK(store.authenticate("carol", newPassword));

	REQUIRE(store.setActive(id, false) == "");
	CHECK_FALSE(store.authenticate("carol", newPassword));

	REQUIRE(store.deleteTeam(id) == "");
	CHECK(store.getTeams().empty());
	CHECK(store.deleteTeam(id) != "");
}

TEST_CASE("The legacy equipe.txt file is fully imported")
{
	// Ancien format, avec un nom en Windows-1252 et une équipe incomplète.
	std::string content = "/Mattei,Fresi,1,/Gregoire,Colbert,1,/Edouard,Flaquet,2,/Apol\xEEne,Vast,2,/Seul,pwd1,3,";

	std::vector<tw::LegacyPlayer> players = tw::parseLegacyTeamFile(content);
	REQUIRE(players.size() == 5);
	CHECK(players[3].login == u8"Apolîne");
	CHECK(players[3].teamNumber == 2);

	TempFile file("teams.json");
	tw::TeamStore store(file.str());
	std::map<std::string, std::string> passwords;
	std::string report = tw::importLegacyTeams(content, store, passwords);

	CHECK(store.getTeams().size() == 2);
	CHECK(report.find(u8"Équipe 3 ignorée") != std::string::npos);
	CHECK(store.authenticate("Mattei", "Fresi"));
	CHECK(passwords["Gregoire"] == "Colbert");

	// Un second import ne crée pas de doublons.
	tw::importLegacyTeams(content, store, passwords);
	CHECK(store.getTeams().size() == 2);
}

TEST_CASE("The credential sheet lists active teams with escaped names")
{
	tw::Team team;
	team.id = 1;
	team.name = "<Les Bleus>";
	team.players[0].login = "alice";
	team.players[0].displayName = "Alice";
	team.players[1].login = "bob";
	team.players[1].displayName = "Bob";

	tw::Team inactive = team;
	inactive.id = 2;
	inactive.name = "Inactive";
	inactive.active = false;

	std::map<std::string, std::string> passwords = { { "alice", "abc234" } };
	std::string html = tw::CredentialSheet::renderHtml({ team, inactive }, passwords);

	CHECK(html.find("&lt;Les Bleus&gt;") != std::string::npos);
	CHECK(html.find("abc234") != std::string::npos);
	CHECK(html.find(u8"réinitialiser") != std::string::npos);	// mot de passe de bob inconnu
	CHECK(html.find("Inactive") == std::string::npos);
}
