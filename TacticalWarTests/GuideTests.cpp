#include <doctest.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
	// Dossier qui contient assets/ : les tests sont lancés depuis la racine du dépôt ou depuis x64/<configuration>.
	std::string guideRoot()
	{
		for (const char * root : { "", "../../", "../" })
		{
			if (std::filesystem::exists(std::string(root) + "assets/web/guide.html"))
				return root;
		}
		FAIL("assets/web/guide.html introuvable");
		return std::string();
	}

	std::string readAll(const std::string & path)
	{
		std::ifstream file(path, std::ios::binary);
		std::stringstream content;
		content << file.rdbuf();
		return content.str();
	}

	// Empreinte FNV-1a de gamedata.json, sans BOM ni retours chariot : même calcul que
	// tools/docs/make_player_guide.py.
	std::string gamedataHash(std::string data)
	{
		if (data.compare(0, 3, "\xEF\xBB\xBF") == 0)
			data = data.substr(3);
		std::uint32_t value = 0x811C9DC5u;
		for (char c : data)
		{
			if (c == '\r')
				continue;
			value ^= (unsigned char)c;
			value *= 0x01000193u;
		}
		char text[9];
		std::snprintf(text, sizeof(text), "%08x", value);
		return text;
	}
}

TEST_CASE("le guide du joueur est à jour avec les données du jeu")
{
	std::string root = guideRoot();
	std::string guide = readAll(root + "assets/web/guide.html");
	std::string marker = "<meta name=\"gamedata-hash\" content=\"";
	size_t start = guide.find(marker);
	REQUIRE_MESSAGE(start != std::string::npos, "empreinte absente du guide");
	start += marker.size();
	std::string stored = guide.substr(start, guide.find('"', start) - start);

	CHECK_MESSAGE(stored == gamedataHash(readAll(root + "assets/data/gamedata.json")),
		"gamedata.json a changé : régénérer le guide avec py tools/docs/make_player_guide.py");
}
