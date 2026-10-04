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

TEST_CASE("A team may have a single player, who then plays both characters")
{
	TempFile file("teams.json");
	tw::TeamStore store(file.str());
	std::map<std::string, std::string> passwords;
	int id = 0;

	// Second joueur laissé vide : équipe d'un seul joueur, sans mot de passe pour la place vide.
	REQUIRE(store.createTeam(makeInput("Solo", "lea", ""), id, passwords) == "");
	const tw::Team * team = store.findTeam(id);
	REQUIRE(team != nullptr);
	CHECK(tw::playerCount(*team) == 1);
	CHECK(team->players[0].login == "lea");
	CHECK(team->players[1].login.empty());
	CHECK(passwords.size() == 1);
	CHECK(store.authenticate("lea", passwords["lea"]));

	// La place vide n'est pas un compte : aucun login vide ne s'authentifie.
	CHECK(store.findTeamByLogin("") == nullptr);
	CHECK_FALSE(store.authenticate("", ""));

	// Seul le second emplacement rempli : le joueur passe en premier.
	int otherId = 0;
	REQUIRE(store.createTeam(makeInput("Duo", "", "max"), otherId, passwords) == "");
	CHECK(store.findTeam(otherId)->players[0].login == "max");

	// Au moins un joueur, et pas de nom affiché ni de mot de passe sans login.
	CHECK(store.createTeam(makeInput("Vide", "", ""), otherId, passwords) != "");
	tw::TeamInput nameOnly = makeInput("Nom", "tom", "");
	nameOnly.players[1].displayName = "Sam";
	CHECK(store.createTeam(nameOnly, otherId, passwords) != "");

	// Un second joueur ajouté plus tard reçoit un mot de passe ; le premier garde le sien.
	std::string firstHash = store.findTeam(id)->players[0].passwordHash;
	std::map<std::string, std::string> updated;
	REQUIRE(store.updateTeam(id, makeInput("Solo", "lea", "sam"), updated) == "");
	CHECK(tw::playerCount(*store.findTeam(id)) == 2);
	CHECK(store.findTeam(id)->players[0].passwordHash == firstHash);
	CHECK(updated.count("sam") == 1);
	CHECK(updated.count("lea") == 0);

	// Le premier retiré : le second prend sa place, avec son mot de passe.
	std::string samHash = store.findTeam(id)->players[1].passwordHash;
	updated.clear();
	REQUIRE(store.updateTeam(id, makeInput("Solo", "", "sam"), updated) == "");
	CHECK(store.findTeam(id)->players[0].login == "sam");
	CHECK(store.findTeam(id)->players[0].passwordHash == samHash);
	CHECK(store.findTeam(id)->players[1].login.empty());
	CHECK(updated.empty());
	CHECK(store.findTeamByLogin("lea") == nullptr);
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
	// Ancien format, avec un nom en Windows-1252 et une équipe d'un seul joueur.
	std::string content = "/Mattei,Fresi,1,/Gregoire,Colbert,1,/Edouard,Flaquet,2,/Apol\xEEne,Vast,2,/Seul,pwd1,3,";

	std::vector<tw::LegacyPlayer> players = tw::parseLegacyTeamFile(content);
	REQUIRE(players.size() == 5);
	CHECK(players[3].login == u8"Apolîne");
	CHECK(players[3].teamNumber == 2);

	TempFile file("teams.json");
	tw::TeamStore store(file.str());
	std::map<std::string, std::string> passwords;
	std::string report = tw::importLegacyTeams(content, store, passwords);

	CHECK(store.getTeams().size() == 3);
	CHECK(report.find(u8"ignorée") == std::string::npos);
	CHECK(store.authenticate("Mattei", "Fresi"));
	CHECK(passwords["Gregoire"] == "Colbert");
	CHECK(store.authenticate("Seul", "pwd1"));
	CHECK(tw::playerCount(store.getTeams()[2]) == 1);

	// Un second import ne crée pas de doublons.
	tw::importLegacyTeams(content, store, passwords);
	CHECK(store.getTeams().size() == 3);

	// Plus de deux joueurs : équipe ignorée.
	TempFile other("teams.json");
	tw::TeamStore crowded(other.str());
	report = tw::importLegacyTeams("/a1,p1,1,/a2,p2,1,/a3,p3,1,", crowded, passwords);
	CHECK(crowded.getTeams().empty());
	CHECK(report.find(u8"Équipe 1 ignorée") != std::string::npos);
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

	// Équipe d'un seul joueur : une ligne, pas de ligne vide « à réinitialiser ».
	tw::Team solo;
	solo.id = 3;
	solo.name = "Solo";
	solo.players[0].login = "lea";
	solo.players[0].displayName = "Lea";
	std::string soloHtml = tw::CredentialSheet::renderHtml({ solo }, { { "lea", "xyz789" } });
	CHECK(soloHtml.find("xyz789") != std::string::npos);
	CHECK(soloHtml.find(u8"réinitialiser") == std::string::npos);
}
