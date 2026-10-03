#include <doctest.h>

#include <memory>
#include <string>

#include <EditorController.h>
#include <Environment.h>
#include <EnvironmentManager.h>
#include <EnvironmentMap.h>
#include <TileRegistry.h>

using namespace tw;
using namespace tw::editor;

namespace
{
	const char * TILESET = R"({
		"tiles": [
			{ "id": "grass", "name": "Herbe", "category": "ground", "anchor": [66, 45] },
			{ "id": "sand", "name": "Sable", "category": "ground" },
			{ "id": "stone", "name": "Rocher", "category": "obstacle" },
			{ "id": "water", "name": "Eau", "category": "liquid", "shader": "water" },
			{ "id": "bridge", "name": "Pont", "category": "liquid", "walkable": true },
			{ "id": "hole", "name": "Vide", "category": "empty" },
			{ "id": "embers", "name": "Braises", "category": "ground", "turnStart": { "damage": 8 } },
			{ "id": "spring", "name": "Source", "category": "ground", "turnStart": { "heal": 6 } },
			{ "id": "tall_grass", "name": "Hautes herbes", "category": "ground", "blocksLineOfSight": true }
		]
	})";

	void useTestTiles()
	{
		REQUIRE(TileRegistry::get().loadFromString(TILESET));
	}

	// Carte 6 x 5 : départs de l'équipe 1 à gauche, de l'équipe 2 à droite.
	void placeStarts(EditorController & editor)
	{
		editor.setTool(Tool::START_TEAM1);
		editor.pointerDown(0, 0);
		editor.pointerMove(0, 1);
		editor.pointerUp();
		editor.setTool(Tool::START_TEAM2);
		editor.pointerDown(5, 0);
		editor.pointerMove(5, 1);
		editor.pointerUp();
	}

	bool hasBlocking(const std::vector<ValidationMessage> & messages)
	{
		for (const ValidationMessage & message : messages)
		{
			if (message.blocking)
				return true;
		}
		return false;
	}
}

TEST_CASE("Registre de tuiles : catégories, règles par défaut et surcharges")
{
	useTestTiles();
	const TileRegistry & registry = TileRegistry::get();

	const TileDef * grass = registry.find("grass");
	REQUIRE(grass != nullptr);
	CHECK(grass->rules.walkable);
	CHECK_FALSE(grass->rules.blocksLineOfSight);
	CHECK(grass->anchorX == 66);

	const TileDef * stone = registry.find("stone");
	REQUIRE(stone != nullptr);
	CHECK_FALSE(stone->rules.walkable);
	CHECK(stone->rules.blocksLineOfSight);

	const TileDef * water = registry.find("water");
	REQUIRE(water != nullptr);
	CHECK_FALSE(water->rules.walkable);
	CHECK_FALSE(water->rules.blocksLineOfSight);
	CHECK(water->shader == "water");

	CHECK(registry.find("bridge")->rules.walkable);
	CHECK((registry.find("hole")->category == TileCategory::EMPTY));
	CHECK(registry.find("lava") == nullptr);

	CHECK_FALSE(TileRegistry().loadFromString("{}"));
	CHECK_FALSE(TileRegistry().loadFromString("pas du json"));
}

TEST_CASE("Carte : une tuile inconnue est un obstacle")
{
	useTestTiles();
	Environment environment(3, 3, 1, "grass");
	environment.setTile(1, 1, "inconnue");
	CHECK_FALSE(environment.getMapData(1, 1)->getIsWalkable());
	CHECK(environment.getMapData(1, 1)->getIsObstacle());
	CHECK(environment.getMapData(0, 0)->getIsWalkable());
	CHECK(environment.getMapData(3, 0) == nullptr);
}

TEST_CASE("Carte v2 : aller-retour JSON")
{
	useTestTiles();
	Environment environment(6, 4, 7, "grass");
	environment.setName("Le gué");
	environment.setInTournamentPool(false);
	environment.setTile(2, 1, "water");
	environment.setTile(3, 2, "stone");
	environment.setTile(4, 3, "sand");
	environment.getMapData(0, 0)->setTeamStartPoint(1);
	environment.getMapData(5, 3)->setTeamStartPoint(2);

	std::string text = EnvironmentManager::toJson(&environment);
	std::unique_ptr<Environment> loaded(EnvironmentManager::fromJson(text));
	REQUIRE(loaded != nullptr);
	CHECK(loaded->getId() == 7);
	CHECK(loaded->getName() == "Le gué");
	CHECK_FALSE(loaded->isInTournamentPool());
	CHECK(loaded->getWidth() == 6);
	CHECK(loaded->getHeight() == 4);
	for (int x = 0; x < 6; x++)
	{
		for (int y = 0; y < 4; y++)
		{
			CHECK(loaded->getMapData(x, y)->getTile() == environment.getMapData(x, y)->getTile());
			CHECK(loaded->getMapData(x, y)->getIsWalkable() == environment.getMapData(x, y)->getIsWalkable());
			CHECK(loaded->getMapData(x, y)->getTeamStartPointNumber() == environment.getMapData(x, y)->getTeamStartPointNumber());
		}
	}

	// Version compacte (message MP) : une seule ligne, mêmes données.
	std::string compact = EnvironmentManager::toJson(&environment, true, true);
	CHECK(compact.find('\n') == std::string::npos);
	std::unique_ptr<Environment> fromCompact(EnvironmentManager::fromJson(compact));
	REQUIRE(fromCompact != nullptr);
	CHECK(fromCompact->getMapData(3, 2)->getIsObstacle());
}

TEST_CASE("Carte v2 : les règles envoyées par le serveur priment sur le registre local")
{
	useTestTiles();
	Environment environment(5, 5, 3, "grass");
	environment.setTile(2, 2, "bridge");
	std::string withRules = EnvironmentManager::toJson(&environment, true, true);

	// Un client dont le registre ne connaît pas "bridge" applique quand même la règle du serveur.
	REQUIRE(TileRegistry::get().loadFromString(R"({"tiles": [{ "id": "grass", "category": "ground" }]})"));
	std::unique_ptr<Environment> client(EnvironmentManager::fromJson(withRules));
	REQUIRE(client != nullptr);
	CHECK(client->getMapData(2, 2)->getIsWalkable());
	CHECK_FALSE(client->getMapData(2, 2)->getIsObstacle());

	std::unique_ptr<Environment> withoutRules(EnvironmentManager::fromJson(EnvironmentManager::toJson(&environment)));
	CHECK(withoutRules->getMapData(2, 2)->getIsObstacle());
	useTestTiles();
}

TEST_CASE("Cases spéciales : règles de la tuile, carte envoyée aux clients et carte du combat")
{
	useTestTiles();
	const TileRegistry & registry = TileRegistry::get();
	CHECK(registry.find("embers")->rules.turnDamage == 8);
	CHECK(registry.find("spring")->rules.turnHeal == 6);
	CHECK(registry.find("tall_grass")->rules.walkable);
	CHECK(registry.find("tall_grass")->rules.blocksLineOfSight);
	CHECK_FALSE(registry.find("grass")->rules.hasTurnEffect());

	Environment environment(5, 5, 4, "grass");
	environment.setTile(1, 1, "embers");
	environment.setTile(2, 2, "spring");
	environment.setTile(3, 3, "tall_grass");
	environment.setTile(4, 4, "stone");
	std::string withRules = EnvironmentManager::toJson(&environment, true, true);

	// Un client qui ne connaît pas ces tuiles applique les règles reçues avec la carte.
	REQUIRE(TileRegistry::get().loadFromString(R"({"tiles": [{ "id": "grass", "category": "ground" }]})"));
	std::unique_ptr<Environment> client(EnvironmentManager::fromJson(withRules));
	useTestTiles();
	REQUIRE(client != nullptr);
	CHECK(client->getMapData(1, 1)->getRules().turnDamage == 8);
	CHECK(client->getMapData(2, 2)->getRules().turnHeal == 6);
	CHECK(client->getMapData(3, 3)->getIsWalkable());
	CHECK(client->getMapData(3, 3)->getIsObstacle());

	// Carte du combat : les hautes herbes sont praticables et cachent, le rocher ne laisse rien passer.
	battle::BattleMap map = battle::battleMapFromEnvironment(client.get());
	CHECK(map.turnDamage({ 1, 1 }) == 8);
	CHECK(map.turnHeal({ 2, 2 }) == 6);
	CHECK(map.turnDamage({ 0, 0 }) == 0);
	CHECK(map.isWalkable({ 3, 3 }));
	CHECK(map.blocksSight({ 3, 3 }));
	CHECK_FALSE(map.isWalkable({ 4, 4 }));
	CHECK(map.blocksSight({ 4, 4 }));
}

TEST_CASE("Carte v1 : lecture de l'ancien format")
{
	useTestTiles();
	std::string v1 = "3\n2\n9\n0,0,0,1,1\n0,1,1,0,0\n0,2,0,0,0\n1,0,0,1,2\n1,1,0,1,0\n1,2,0,1,0\n";
	std::unique_ptr<Environment> environment(EnvironmentManager::fromV1Text(v1, 9));
	REQUIRE(environment != nullptr);
	CHECK(environment->getWidth() == 2);
	CHECK(environment->getHeight() == 3);
	CHECK(environment->getMapData(0, 0)->getTile() == "grass");
	CHECK(environment->getMapData(0, 0)->getTeamStartPointNumber() == 1);
	CHECK(environment->getMapData(0, 1)->getTile() == "stone");
	CHECK(environment->getMapData(0, 1)->getIsObstacle());
	CHECK(environment->getMapData(0, 2)->getTile() == "water");
	CHECK_FALSE(environment->getMapData(0, 2)->getIsWalkable());
	CHECK(environment->getMapData(1, 0)->getTeamStartPointNumber() == 2);

	CHECK(EnvironmentManager::fromV1Text("n'importe quoi", 1) == nullptr);
	CHECK(EnvironmentManager::fromJson("{\"format\": \"autre\"}") == nullptr);
}

TEST_CASE("Éditeur : un trait est annulé et rétabli d'un bloc")
{
	useTestTiles();
	EditorController editor;
	editor.newMap(6, 5, 1, "grass");
	CHECK_FALSE(editor.isModified());

	editor.setTool(Tool::PAINT);
	editor.setTile("water");
	editor.pointerDown(1, 1);
	editor.pointerMove(2, 1);
	editor.pointerMove(3, 1);
	editor.pointerMove(2, 1);
	editor.pointerUp();

	Environment * map = editor.getEnvironment();
	CHECK(map->getMapData(1, 1)->getTile() == "water");
	CHECK(map->getMapData(3, 1)->getTile() == "water");
	CHECK(editor.isModified());

	REQUIRE(editor.undo());
	CHECK(editor.getEnvironment()->getMapData(1, 1)->getTile() == "grass");
	CHECK(editor.getEnvironment()->getMapData(2, 1)->getTile() == "grass");
	CHECK(editor.getEnvironment()->getMapData(3, 1)->getTile() == "grass");
	CHECK_FALSE(editor.isModified());
	CHECK_FALSE(editor.undo());

	REQUIRE(editor.redo());
	CHECK(editor.getEnvironment()->getMapData(2, 1)->getTile() == "water");
	CHECK_FALSE(editor.redo());

	editor.markSaved();
	CHECK_FALSE(editor.isModified());

	// Une nouvelle action vide la pile de rétablissement.
	editor.undo();
	editor.pointerDown(0, 0);
	editor.pointerUp();
	CHECK_FALSE(editor.canRedo());
}

TEST_CASE("Éditeur : remplissage et rectangle")
{
	useTestTiles();
	EditorController editor;
	editor.newMap(6, 5, 1, "grass");

	// Une ligne d'eau coupe la carte en deux zones d'herbe.
	editor.setTool(Tool::RECTANGLE);
	editor.setTile("water");
	editor.pointerDown(2, 0);
	editor.pointerMove(2, 4);
	int x0, y0, x1, y1;
	REQUIRE(editor.getRectangle(x0, y0, x1, y1));
	CHECK(x0 == 2);
	CHECK(y1 == 4);
	editor.pointerUp();
	CHECK_FALSE(editor.getRectangle(x0, y0, x1, y1));
	for (int y = 0; y < 5; y++)
		CHECK(editor.getEnvironment()->getMapData(2, y)->getTile() == "water");

	editor.setTool(Tool::FILL);
	editor.setTile("sand");
	editor.pointerDown(0, 0);
	editor.pointerUp();
	CHECK(editor.getEnvironment()->getMapData(1, 3)->getTile() == "sand");
	CHECK(editor.getEnvironment()->getMapData(2, 2)->getTile() == "water");
	CHECK(editor.getEnvironment()->getMapData(3, 0)->getTile() == "grass");

	editor.undo();
	CHECK(editor.getEnvironment()->getMapData(1, 3)->getTile() == "grass");
	editor.undo();
	CHECK(editor.getEnvironment()->getMapData(2, 2)->getTile() == "grass");
}

TEST_CASE("Éditeur : redimensionnement annulable")
{
	useTestTiles();
	EditorController editor;
	editor.newMap(6, 5, 3, "grass");
	editor.getEnvironment()->setName("Petite");
	editor.setTile("stone");
	editor.pointerDown(5, 3);
	editor.pointerUp();

	editor.resize(8, 6, "sand");
	CHECK(editor.getEnvironment()->getWidth() == 8);
	CHECK(editor.getEnvironment()->getHeight() == 6);
	CHECK(editor.getEnvironment()->getMapData(0, 5)->getTile() == "sand");
	CHECK(editor.getEnvironment()->getMapData(7, 0)->getTile() == "sand");
	CHECK(editor.getEnvironment()->getName() == "Petite");
	CHECK(editor.getEnvironment()->getId() == 3);

	REQUIRE(editor.undo());
	CHECK(editor.getEnvironment()->getWidth() == 6);
	CHECK(editor.getEnvironment()->getMapData(5, 3)->getTile() == "stone");
	REQUIRE(editor.redo());
	CHECK(editor.getEnvironment()->getWidth() == 8);

	editor.resize(1000, 1, "grass");
	CHECK(editor.getEnvironment()->getWidth() == EditorController::MAX_SIZE);
	CHECK(editor.getEnvironment()->getHeight() == EditorController::MIN_SIZE);
}

TEST_CASE("Éditeur : validation de la carte")
{
	useTestTiles();
	EditorController editor;
	editor.newMap(6, 5, 1, "grass");
	editor.getEnvironment()->setName("Test");

	// Sans départs : bloquant.
	CHECK(hasBlocking(editor.validate()));

	placeStarts(editor);
	CHECK(editor.validate().empty());

	// Une rivière infranchissable sépare les équipes.
	editor.setTool(Tool::RECTANGLE);
	editor.setTile("water");
	editor.pointerDown(3, 0);
	editor.pointerMove(3, 4);
	editor.pointerUp();
	CHECK(hasBlocking(editor.validate()));

	// Un pont rétablit le passage.
	editor.setTool(Tool::PAINT);
	editor.setTile("bridge");
	editor.pointerDown(3, 2);
	editor.pointerUp();
	CHECK_FALSE(hasBlocking(editor.validate()));

	// Un départ dans les hautes herbes est accepté : elles sont praticables.
	editor.setTile("tall_grass");
	editor.pointerDown(0, 1);
	editor.pointerUp();
	CHECK_FALSE(hasBlocking(editor.validate()));
	editor.undo();

	// Un départ sur un rocher est refusé.
	editor.setTile("stone");
	editor.pointerDown(0, 1);
	editor.pointerUp();
	std::vector<ValidationMessage> messages = editor.validate();
	CHECK(hasBlocking(messages));

	// Îlot : simple avertissement.
	editor.undo();
	editor.setTile("stone");
	editor.pointerDown(4, 4);
	editor.pointerMove(5, 3);
	editor.pointerUp();
	messages = editor.validate();
	CHECK_FALSE(hasBlocking(messages));
	REQUIRE(messages.size() == 1);
	CHECK(messages[0].x == 5);
	CHECK(messages[0].y == 4);
}

TEST_CASE("Zone à tenir : peinte dans l'éditeur, enregistrée et vérifiée")
{
	useTestTiles();
	EditorController editor;
	editor.newMap(6, 5, 1, "grass");
	editor.getEnvironment()->setName("Test");
	placeStarts(editor);

	// Zone au milieu : aussi proche des deux équipes, pas de message.
	editor.setTool(Tool::ZONE);
	editor.pointerDown(2, 0);
	editor.pointerMove(3, 0);
	editor.pointerUp();
	CHECK(editor.getEnvironment()->getMapData(2, 0)->getIsZone());
	CHECK(editor.validate().empty());

	// Annulable comme les autres outils.
	REQUIRE(editor.undo());
	CHECK_FALSE(editor.getEnvironment()->getMapData(2, 0)->getIsZone());
	REQUIRE(editor.redo());

	// Enregistrée dans le fichier de la carte (absente quand rien n'est peint).
	std::string text = EnvironmentManager::toJson(editor.getEnvironment());
	CHECK(text.find("\"zone\"") != std::string::npos);
	std::unique_ptr<Environment> loaded(EnvironmentManager::fromJson(text));
	REQUIRE(loaded != nullptr);
	CHECK(loaded->getMapData(2, 0)->getIsZone());
	CHECK(loaded->getMapData(3, 0)->getIsZone());
	CHECK_FALSE(loaded->getMapData(4, 0)->getIsZone());

	// Zone collée aux départs de l'équipe 2 : avertissement, non bloquant.
	editor.setTool(Tool::ERASE_ZONE);
	editor.pointerDown(2, 0);
	editor.pointerMove(3, 0);
	editor.pointerUp();
	CHECK(EnvironmentManager::toJson(editor.getEnvironment()).find("\"zone\"") == std::string::npos);
	editor.setTool(Tool::ZONE);
	editor.pointerDown(4, 0);
	editor.pointerUp();
	std::vector<ValidationMessage> messages = editor.validate();
	REQUIRE(messages.size() == 1);
	CHECK_FALSE(messages[0].blocking);
	CHECK(messages[0].text.find("2") != std::string::npos);
}
