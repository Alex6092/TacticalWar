#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <nlohmann/json.hpp>

#include <GameData.h>

using namespace tw::battle;
using nlohmann::json;

namespace
{
	// Dossier qui contient assets/ : les tests sont lancés depuis la racine du dépôt ou depuis x64/<configuration>.
	std::string assetRoot()
	{
		for (const char * root : { "", "../../", "../" })
		{
			if (std::filesystem::exists(std::string(root) + "assets/spellsprites/effects.json"))
				return root;
		}
		FAIL("assets/spellsprites/effects.json introuvable");
		return std::string();
	}

	json readJson(const std::string & path)
	{
		std::ifstream file(path, std::ios::binary);
		std::stringstream content;
		content << file.rdbuf();
		return json::parse(content.str(), nullptr, false);
	}

	// Fichier des données ("./assets/...") depuis le dossier des ressources.
	std::string onDisk(const std::string & root, std::string path)
	{
		if (path.compare(0, 2, "./") == 0)
			path = path.substr(2);
		return root + path;
	}
}

TEST_CASE("Visuels des sorts : lecture de l'objet visual")
{
	const char * text = R"({ "classes": [ { "id": 1, "name": "Test", "stats": { "MAX_HP": 10 }, "spells": [
		{ "id": "complet", "visual": { "cast": "c", "projectile": { "effect": "p", "speed": 20, "arc": 0.5 }, "impact": "i",
			"impactOn": "cells", "status": "s", "tick": "t", "glyph": "g", "glyphTrigger": "gt", "impactSound": "son.ogg" } },
		{ "id": "ancien", "fx": "./assets/spellsprites/claw1_red" } ] } ] })";

	GameData data;
	std::string error;
	REQUIRE_MESSAGE(data.loadFromJsonText(text, error), error);

	const SpellVisual & visual = data.classes[0].spells[0].visual;
	CHECK(visual.cast == "c");
	CHECK(visual.projectile == "p");
	CHECK(visual.projectileSpeed == doctest::Approx(20));
	CHECK(visual.projectileArc == doctest::Approx(0.5));
	CHECK(visual.impact == "i");
	CHECK(visual.impactOn == "cells");
	CHECK(visual.status == "s");
	CHECK(visual.tick == "t");
	CHECK(visual.glyph == "g");
	CHECK(visual.glyphTrigger == "gt");
	CHECK(visual.impactSound == "son.ogg");

	// Sans objet "visual" : valeurs par défaut, et l'ancien champ "fx" reste lu.
	const SpellDef & legacy = data.classes[0].spells[1];
	CHECK(legacy.visual.impact.empty());
	CHECK(legacy.visual.impactOn == "target");
	CHECK(legacy.visual.projectileSpeed == doctest::Approx(12));
	CHECK(legacy.fxSprite == "./assets/spellsprites/claw1_red");
}

TEST_CASE("Visuels des sorts : catalogue d'effets, planches et sons présents")
{
	std::string root = assetRoot();
	GameData data;
	std::string error;
	REQUIRE_MESSAGE(data.loadFromFile(root + "assets/data/gamedata.json", error), error);

	json catalog = readJson(root + "assets/spellsprites/effects.json");
	REQUIRE(catalog.is_object());
	REQUIRE(catalog.contains("effects"));
	const json & effects = catalog["effects"];

	// Chaque effet du catalogue : planche (image et atlas) présente, couche connue.
	for (auto it = effects.begin(); it != effects.end(); ++it)
	{
		INFO("effet " << it.key());
		std::string sheet = it.value().value("sheet", std::string());
		REQUIRE_FALSE(sheet.empty());
		CHECK(std::filesystem::exists(onDisk(root, sheet) + ".png"));
		CHECK(std::filesystem::exists(onDisk(root, sheet) + ".txt"));
		std::string layer = it.value().value("layer", std::string("top"));
		CHECK((layer == "top" || layer == "ground"));
	}

	// Effets joués directement par le client : signal d'équipe, et ses sons.
	for (const char * name : { "ping", "ping_arrow" })
		CHECK_MESSAGE(effects.contains(name), "effet manquant : " << name);
	for (const char * sound : { "assets/sound/ui/ping.ogg", "assets/sound/ui/emote.ogg" })
		CHECK_MESSAGE(std::filesystem::exists(root + sound), "son manquant : " << sound);

	// Effets génériques (mort, collision, poussée…).
	json events = catalog.value("events", json::object());
	for (auto it = events.begin(); it != events.end(); ++it)
	{
		INFO("événement " << it.key());
		CHECK(effects.contains(it.value().get<std::string>()));
	}

	// Chaque sort : effets connus du catalogue, sons présents, et une animation qui lui est propre.
	std::map<std::string, std::string> signatures;
	for (const ClassDef & classDef : data.classes)
	{
		for (const SpellDef & spell : classDef.spells)
		{
			INFO("sort " << spell.id);
			const SpellVisual & visual = spell.visual;
			for (const std::string & name : { visual.cast, visual.projectile, visual.impact, visual.status, visual.tick, visual.glyph, visual.glyphTrigger })
			{
				if (!name.empty())
					CHECK_MESSAGE(effects.contains(name), "effet inconnu : " << name);
			}
			CHECK((visual.impactOn == "target" || visual.impactOn == "cells" || visual.impactOn == "caster"));
			CHECK_FALSE((visual.cast.empty() && visual.projectile.empty() && visual.impact.empty()));

			CHECK_FALSE(spell.sound.empty());
			CHECK(std::filesystem::exists(onDisk(root, spell.sound)));
			if (!visual.impactSound.empty())
				CHECK(std::filesystem::exists(onDisk(root, visual.impactSound)));

			std::string signature = visual.cast + "|" + visual.projectile + "|" + visual.impact + "|" + visual.impactOn
				+ "|" + visual.status + "|" + visual.glyph;
			auto same = signatures.find(signature);
			CHECK_MESSAGE(same == signatures.end(), "même animation que " << (same != signatures.end() ? same->second : std::string()));
			signatures[signature] = spell.id;
		}
	}
}
