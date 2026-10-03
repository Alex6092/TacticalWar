#include <doctest.h>

#include <filesystem>
#include <random>

#include <BattleEngine.h>
#include <BattleMirror.h>
#include <BattlePreview.h>

using namespace tw::battle;

namespace
{
	const int MAGE = 1;
	const int ARCHER = 2;
	const int PROTECTEUR = 3;
	const int GUERRIER = 4;

	// Les tests sont lancés depuis la racine du dépôt ou depuis x64/Debug.
	const GameData & gameData()
	{
		static GameData data;
		static bool loaded = false;
		if (!loaded)
		{
			std::string error;
			for (const char * path : { "assets/data/gamedata.json", "../../assets/data/gamedata.json", "../assets/data/gamedata.json" })
			{
				if (std::filesystem::exists(path) && data.loadFromFile(path, error))
				{
					loaded = true;
					break;
				}
			}
			REQUIRE_MESSAGE(loaded, "gamedata.json introuvable ou invalide : " << error);
		}
		return data;
	}

	BattleMap openMap(int size = 15)
	{
		return BattleMap(size, size);
	}

	int spellIndex(int classId, const std::string & spellId)
	{
		const ClassDef * classDef = gameData().findClass(classId);
		for (int i = 0; i < (int)classDef->spells.size(); i++)
		{
			if (classDef->spells[i].id == spellId)
				return i;
		}
		FAIL("sort inconnu " << spellId);
		return -1;
	}

	// Combat prêt à jouer : équipe 1 et 2 placées sur les cellules données.
	struct Arena
	{
		BattleMap map;
		std::unique_ptr<BattleEngine> engine;
		std::int64_t now = 0;

		Arena(const std::vector<std::pair<int, Cell>> & team1, const std::vector<std::pair<int, Cell>> & team2, BattleMap baseMap = openMap(), std::uint32_t seed = 1)
			: map(baseMap)
		{
			for (const auto & entry : team1)
				map.startCells[1].push_back(entry.second);
			for (const auto & entry : team2)
				map.startCells[2].push_back(entry.second);

			engine.reset(new BattleEngine(gameData(), map, seed));
			for (const auto & entry : team1)
				engine->addFighter(1, entry.first, "A" + std::to_string(entry.first));
			for (const auto & entry : team2)
				engine->addFighter(2, entry.first, "B" + std::to_string(entry.first));

			engine->startPlacement(now);
			for (const Fighter & fighter : engine->getState().fighters)
				REQUIRE(engine->setReady(fighter.id, true, now).ok);
			REQUIRE((engine->getState().phase == BattlePhase::FIGHT));
			engine->flushEvents();
		}

		const BattleState & state() const { return engine->getState(); }
		const Fighter & fighter(int id) const { return *state().findFighter(id); }
		int active() const { return state().activeFighterId(); }

		// Passe les tours jusqu'à celui du combattant demandé.
		void playUntilTurnOf(int id)
		{
			for (int guard = 0; active() != id; guard++)
			{
				REQUIRE(guard < 20);
				REQUIRE(engine->endTurn(active(), now).ok);
			}
		}

		std::vector<nlohmann::json> eventsOfType(const std::string & type)
		{
			std::vector<nlohmann::json> found;
			nlohmann::json batch = engine->flushEvents();
			for (const nlohmann::json & event : batch["ev"])
			{
				if (event["t"] == type)
					found.push_back(event);
			}
			return found;
		}
	};
}

TEST_CASE("Game data defines the four classes with four spells each")
{
	const GameData & data = gameData();
	REQUIRE(data.classes.size() == 4);
	for (int classId : { MAGE, ARCHER, PROTECTEUR, GUERRIER })
	{
		const ClassDef * classDef = data.findClass(classId);
		REQUIRE(classDef != nullptr);
		CHECK(classDef->spells.size() == 4);
		CHECK(classDef->baseStats.get(Stat::AP) == 6);
		CHECK((classDef->passive.type != PassiveType::NONE));
	}
	CHECK(data.findClass(GUERRIER)->baseStats.get(Stat::MAX_HP) == 135);
	CHECK(data.findClass(ARCHER)->baseStats.get(Stat::MP) == 4);

	GameData invalid;
	std::string error;
	CHECK_FALSE(invalid.loadFromJsonText("{\"classes\":[{\"id\":1,\"stats\":{\"NOPE\":1},\"spells\":[]}]}", error));
	CHECK_FALSE(error.empty());
}

TEST_CASE("Line of sight is blocked by obstacles and fighters, not by a single corner")
{
	BattleMap map = openMap(10);
	BattleState state;

	CHECK(hasLineOfSight(state, map, { 0, 0 }, { 5, 0 }));
	map.setCell({ 3, 0 }, false, true);
	CHECK_FALSE(hasLineOfSight(state, map, { 0, 0 }, { 5, 0 }));

	// Les trous (non praticables) ne bloquent pas la vue.
	map.setCell({ 3, 0 }, false, false);
	CHECK(hasLineOfSight(state, map, { 0, 0 }, { 5, 0 }));

	// Diagonale exacte : un seul obstacle sur le coin ne bloque pas, deux le font.
	map.setCell({ 1, 0 }, false, true);
	CHECK(hasLineOfSight(state, map, { 0, 0 }, { 2, 2 }));
	map.setCell({ 0, 1 }, false, true);
	CHECK_FALSE(hasLineOfSight(state, map, { 0, 0 }, { 2, 2 }));

	// Un combattant bloque la vue, sauf s'il est la cible.
	BattleMap empty = openMap(10);
	Fighter blocker;
	blocker.id = 7;
	blocker.position = { 2, 5 };
	state.fighters.push_back(blocker);
	CHECK_FALSE(hasLineOfSight(state, empty, { 0, 5 }, { 4, 5 }));
	CHECK(hasLineOfSight(state, empty, { 0, 5 }, { 2, 5 }));
}

TEST_CASE("Impact zones have the expected shapes")
{
	BattleMap map = openMap(15);
	CHECK(impactCells(map, { 0, 7 }, { 7, 7 }, { ZoneShape::SINGLE, 0 }).size() == 1);
	CHECK(impactCells(map, { 0, 7 }, { 7, 7 }, { ZoneShape::CROSS, 1 }).size() == 5);
	CHECK(impactCells(map, { 0, 7 }, { 7, 7 }, { ZoneShape::CIRCLE, 2 }).size() == 13);
	CHECK(impactCells(map, { 0, 7 }, { 7, 7 }, { ZoneShape::SQUARE, 1 }).size() == 9);
	CHECK(impactCells(map, { 0, 7 }, { 7, 7 }, { ZoneShape::RING, 1 }).size() == 4);

	std::vector<Cell> line = impactCells(map, { 0, 7 }, { 7, 7 }, { ZoneShape::LINE, 2 });
	REQUIRE(line.size() == 3);
	CHECK(line[2] == Cell{ 9, 7 });

	std::vector<Cell> perpendicular = impactCells(map, { 0, 7 }, { 7, 7 }, { ZoneShape::PERPENDICULAR, 1 });
	REQUIRE(perpendicular.size() == 3);
	for (const Cell & cell : perpendicular)
		CHECK(cell.x == 7);

	// Recadrée sur la carte.
	CHECK(impactCells(map, { 5, 5 }, { 0, 0 }, { ZoneShape::CIRCLE, 1 }).size() == 3);
}

TEST_CASE("Turn order follows initiative and alternates teams")
{
	// Archer (50) > Mage (40) > Guerrier (30) > Protecteur (20)
	Arena arena({ { GUERRIER, { 2, 2 } }, { MAGE, { 2, 4 } } }, { { ARCHER, { 10, 2 } }, { PROTECTEUR, { 10, 4 } } });
	const std::vector<int> & order = arena.state().turnOrder;
	REQUIRE(order.size() == 4);
	CHECK(arena.fighter(order[0]).classId == ARCHER);
	CHECK(arena.fighter(order[1]).classId == MAGE);
	CHECK(arena.fighter(order[2]).classId == PROTECTEUR);
	CHECK(arena.fighter(order[3]).classId == GUERRIER);
	CHECK(arena.state().round == 1);
	CHECK(arena.active() == order[0]);

	// Fin de tour au minuteur.
	arena.now += 41000;
	arena.engine->tick(arena.now);
	CHECK(arena.active() == order[1]);
	REQUIRE(arena.engine->endTurn(order[1], arena.now).ok);
	CHECK_FALSE(arena.engine->endTurn(order[1], arena.now).ok);	// plus son tour
}

TEST_CASE("Damage applies power, resistance, shields and erosion")
{
	// Archer (puissance 0) contre Guerrier (résistance 10) à 4 cases.
	Arena arena({ { ARCHER, { 2, 7 } } }, { { GUERRIER, { 6, 7 } } });
	int guerrierHp = gameData().findClass(GUERRIER)->baseStats.get(Stat::MAX_HP);
	int archer = 0;
	int guerrier = 1;
	arena.playUntilTurnOf(archer);

	REQUIRE(arena.engine->cast(archer, spellIndex(ARCHER, "tir_precis"), { 6, 7 }, arena.now).ok);
	std::vector<nlohmann::json> damages = arena.eventsOfType("damage");
	REQUIRE(damages.size() == 1);
	int amount = damages[0]["amount"];
	// 11-13 x 1.00 x 0.90, arrondi
	CHECK(amount >= 10);
	CHECK(amount <= 12);
	CHECK(arena.fighter(guerrier).hp == guerrierHp - amount);
	// Érosion de 10 % des PV perdus.
	CHECK(arena.fighter(guerrier).maxHp == guerrierHp - amount * 10 / 100);
	CHECK(arena.fighter(archer).ap == 3);

	// Tir précis : second lancer possible (3 PA restants), puis plus de PA ni de lancer disponible.
	REQUIRE(arena.engine->cast(archer, spellIndex(ARCHER, "tir_precis"), { 6, 7 }, arena.now).ok);
	CHECK(arena.fighter(archer).ap == 0);
	CHECK_FALSE(arena.engine->cast(archer, spellIndex(ARCHER, "tir_precis"), { 6, 7 }, arena.now).ok);
}

TEST_CASE("Spell resources: casts per turn, cooldowns and AP")
{
	Arena arena({ { MAGE, { 2, 7 } } }, { { GUERRIER, { 5, 7 } } });
	int mage = 0;
	arena.playUntilTurnOf(mage);

	int eclair = spellIndex(MAGE, "eclair");
	REQUIRE(arena.engine->cast(mage, eclair, { 5, 7 }, arena.now).ok);
	REQUIRE(arena.engine->cast(mage, eclair, { 5, 7 }, arena.now).ok);
	REQUIRE(arena.engine->cast(mage, eclair, { 5, 7 }, arena.now).ok);
	// 6 PA dépensés : le tour passe automatiquement s'il ne reste ni PA ni PM ; ici il reste des PM.
	CHECK(arena.fighter(mage).ap == 0);
	ActionResult result = arena.engine->cast(mage, eclair, { 5, 7 }, arena.now);
	CHECK_FALSE(result.ok);

	// Surcharge : 3 cumuls de +10 % de puissance jusqu'à la fin du tour.
	int stacks = 0;
	for (const ActiveEffect & effect : arena.fighter(mage).effects)
		if (effect.stat == Stat::POWER)
			stacks++;
	CHECK(stacks == 3);
	REQUIRE(arena.engine->endTurn(mage, arena.now).ok);
	CHECK(arena.fighter(mage).effects.empty());

	// Relance : Transposition (3 tours).
	arena.playUntilTurnOf(mage);
	int transposition = spellIndex(MAGE, "transposition");
	REQUIRE(arena.engine->cast(mage, transposition, { 2, 4 }, arena.now).ok);
	CHECK(arena.fighter(mage).position == Cell{ 2, 4 });
	CHECK(arena.fighter(mage).cooldowns.at("transposition") == 3);
	CHECK_FALSE(arena.engine->cast(mage, transposition, { 2, 7 }, arena.now).ok);
}

TEST_CASE("Tackle costs MP and AP when leaving an adjacent enemy")
{
	// Mage (fuite 4) collé à un Guerrier (tacle 8) : ratio (4+2)/(2*10) = 0.3.
	Arena arena({ { MAGE, { 5, 7 } } }, { { GUERRIER, { 6, 7 } } });
	int mage = 0;
	arena.playUntilTurnOf(mage);
	REQUIRE(arena.fighter(mage).mp == 3);

	MovePreview preview = previewMove(arena.state(), arena.engine->getMap(), gameData(), arena.fighter(mage), { { 4, 7 }, { 3, 7 }, { 2, 7 } });
	REQUIRE(preview.error.empty());
	REQUIRE(preview.tackles.size() == 1);
	CHECK(preview.tackles[0].lostMp == 2);	// round(3 x 0.7)
	CHECK(preview.tackles[0].lostAp == 2);	// round(6 x 0.7 x 0.5)
	CHECK(preview.path.size() == 1);		// Il ne reste qu'un PM

	REQUIRE(arena.engine->move(mage, { { 4, 7 }, { 3, 7 }, { 2, 7 } }, arena.now).ok);
	CHECK(arena.fighter(mage).position == Cell{ 4, 7 });
	CHECK(arena.fighter(mage).ap == 4);

	// L'Archer (fuite 8) contre un Protecteur (tacle 5) : (8+2)/(2*7) = 0.71.
	Arena second({ { ARCHER, { 5, 7 } } }, { { PROTECTEUR, { 6, 7 } } });
	second.playUntilTurnOf(0);
	MovePreview archerPreview = previewMove(second.state(), second.engine->getMap(), gameData(), second.fighter(0), { { 4, 7 }, { 3, 7 } });
	REQUIRE(archerPreview.tackles.size() == 1);
	CHECK(archerPreview.tackles[0].lostMp == 1);
	CHECK(archerPreview.path.size() == 2);

	// Chemins invalides.
	CHECK_FALSE(arena.engine->move(mage, { { 2, 2 } }, arena.now).ok);
	CHECK_FALSE(second.engine->move(0, { { 6, 7 } }, second.now).ok);	// occupé
}

TEST_CASE("Poison ticks at the start of each of the target's next three turns")
{
	Arena arena({ { ARCHER, { 2, 7 } } }, { { GUERRIER, { 6, 7 } } });
	int archer = 0;
	int guerrier = 1;
	arena.playUntilTurnOf(archer);

	REQUIRE(arena.engine->cast(archer, spellIndex(ARCHER, "fleche_empoisonnee"), { 6, 7 }, arena.now).ok);
	arena.engine->flushEvents();

	int dotTicks = 0;
	for (int turn = 0; turn < 4; turn++)
	{
		arena.playUntilTurnOf(guerrier);
		for (const nlohmann::json & damage : arena.eventsOfType("damage"))
			if (damage["kind"] == "dot")
				dotTicks++;
		REQUIRE(arena.engine->endTurn(guerrier, arena.now).ok);
	}
	CHECK(dotTicks == 3);
	CHECK(arena.fighter(guerrier).effects.empty());
}

TEST_CASE("A movement debuff applies during the target's next turn only")
{
	Arena arena({ { ARCHER, { 2, 7 } } }, { { GUERRIER, { 6, 7 } } });
	int archer = 0;
	int guerrier = 1;
	arena.playUntilTurnOf(archer);
	REQUIRE(arena.engine->cast(archer, spellIndex(ARCHER, "fleche_entravante"), { 6, 7 }, arena.now).ok);
	int baseMp = gameData().findClass(GUERRIER)->baseStats.get(Stat::MP);

	arena.playUntilTurnOf(guerrier);
	CHECK(arena.fighter(guerrier).mp == baseMp - 2);
	REQUIRE(arena.engine->endTurn(guerrier, arena.now).ok);
	arena.playUntilTurnOf(guerrier);
	CHECK(arena.fighter(guerrier).mp == baseMp);
}

TEST_CASE("Shields absorb damage before HP and expire")
{
	Arena arena({ { PROTECTEUR, { 4, 7 } }, { GUERRIER, { 5, 7 } } }, { { ARCHER, { 10, 7 } } });
	int protecteur = 0;
	int guerrier = 1;
	int archer = 2;
	arena.playUntilTurnOf(protecteur);
	REQUIRE(arena.engine->cast(protecteur, spellIndex(PROTECTEUR, "bouclier_sacre"), { 5, 7 }, arena.now).ok);
	CHECK(arena.fighter(guerrier).shield == 20);

	arena.playUntilTurnOf(archer);
	REQUIRE(arena.engine->move(archer, { { 9, 7 } }, arena.now).ok);
	REQUIRE(arena.engine->cast(archer, spellIndex(ARCHER, "tir_precis"), { 5, 7 }, arena.now).ok);
	const Fighter & warrior = arena.fighter(guerrier);
	CHECK(warrior.hp == gameData().findClass(GUERRIER)->baseStats.get(Stat::MAX_HP));
	CHECK(warrior.shield < 20);
}

TEST_CASE("Push collides with obstacles and fighters; pull brings the target closer")
{
	BattleMap map = openMap(15);
	map.setCell({ 8, 7 }, false, true);
	Arena arena({ { ARCHER, { 4, 7 } } }, { { GUERRIER, { 6, 7 } } }, map);
	int archer = 0;
	int guerrier = 1;
	arena.playUntilTurnOf(archer);

	REQUIRE(arena.engine->cast(archer, spellIndex(ARCHER, "fleche_recul"), { 6, 7 }, arena.now).ok);
	CHECK(arena.fighter(guerrier).position == Cell{ 7, 7 });
	bool collision = false;
	for (const nlohmann::json & damage : arena.eventsOfType("damage"))
		if (damage["kind"] == "collision")
			collision = damage["amount"] == 12;	// 2 cases restantes x 6
	CHECK(collision);

	Arena pull({ { GUERRIER, { 2, 7 } } }, { { MAGE, { 6, 7 } } });
	pull.playUntilTurnOf(0);
	REQUIRE(pull.engine->cast(0, spellIndex(GUERRIER, "provocation"), { 6, 7 }, pull.now).ok);
	CHECK(pull.fighter(1).position == Cell{ 3, 7 });
	CHECK(effectiveStat(pull.state(), gameData(), pull.fighter(0), Stat::LOCK) == 12);
}

TEST_CASE("Charge dashes next to the target")
{
	Arena arena({ { GUERRIER, { 2, 7 } } }, { { MAGE, { 6, 7 } } });
	arena.playUntilTurnOf(0);
	REQUIRE(arena.engine->cast(0, spellIndex(GUERRIER, "charge"), { 6, 7 }, arena.now).ok);
	CHECK(arena.fighter(0).position == Cell{ 5, 7 });
	CHECK(arena.fighter(1).hp < 80);
}

TEST_CASE("Frost glyph triggers on enemies starting their turn inside it")
{
	Arena arena({ { MAGE, { 2, 7 } } }, { { GUERRIER, { 6, 7 } } });
	int mage = 0;
	int guerrier = 1;
	arena.playUntilTurnOf(mage);
	REQUIRE(arena.engine->cast(mage, spellIndex(MAGE, "glyphe_givre"), { 6, 7 }, arena.now).ok);
	REQUIRE(arena.state().glyphs.size() == 1);
	CHECK(arena.state().glyphs[0].cells.size() == 5);

	arena.playUntilTurnOf(guerrier);
	CHECK(arena.fighter(guerrier).hp < gameData().findClass(GUERRIER)->baseStats.get(Stat::MAX_HP));
	CHECK(arena.fighter(guerrier).mp == gameData().findClass(GUERRIER)->baseStats.get(Stat::MP) - 2);

	// Le glyphe disparaît au début du 2e tour suivant du Mage.
	REQUIRE(arena.engine->endTurn(guerrier, arena.now).ok);
	arena.playUntilTurnOf(mage);
	REQUIRE(arena.engine->endTurn(mage, arena.now).ok);
	arena.playUntilTurnOf(mage);
	CHECK(arena.state().glyphs.empty());
}

TEST_CASE("Purification removes enemy buffs and ally debuffs")
{
	Arena arena({ { PROTECTEUR, { 6, 7 } }, { MAGE, { 6, 9 } } }, { { GUERRIER, { 7, 7 } } });
	int protecteur = 0;
	int mage = 1;
	int guerrier = 2;

	arena.playUntilTurnOf(guerrier);
	REQUIRE(arena.engine->cast(guerrier, spellIndex(GUERRIER, "rempart"), { 7, 7 }, arena.now).ok);
	CHECK(effectiveStat(arena.state(), gameData(), arena.fighter(guerrier), Stat::RESISTANCE) == 40);

	arena.playUntilTurnOf(protecteur);
	REQUIRE(arena.engine->cast(protecteur, spellIndex(PROTECTEUR, "purification"), { 6, 7 }, arena.now).ok);
	CHECK(effectiveStat(arena.state(), gameData(), arena.fighter(guerrier), Stat::RESISTANCE) == 10);
	// L'aura du Protecteur donne +10 % de résistance au Mage (à 2 cases).
	CHECK(effectiveStat(arena.state(), gameData(), arena.fighter(mage), Stat::RESISTANCE) == 10);
}

TEST_CASE("The battle ends when a team is dead, and a forfeit ends it immediately")
{
	Arena arena({ { ARCHER, { 2, 7 } } }, { { MAGE, { 6, 7 } } });
	arena.engine->forfeit(2, arena.now);
	CHECK(arena.engine->isOver());
	CHECK(arena.state().winnerTeam == 1);
	CHECK((arena.state().endReason == EndReason::FORFEIT));
	CHECK_FALSE(arena.engine->endTurn(arena.active(), arena.now).ok);
}

TEST_CASE("Random battles always end and every event serializes")
{
	std::mt19937 rng(2024);
	int classIds[] = { MAGE, ARCHER, PROTECTEUR, GUERRIER };

	for (int battle = 0; battle < 300; battle++)
	{
		CAPTURE(battle);
		BattleMap map = openMap(13);
		for (int i = 0; i < 12; i++)
		{
			Cell obstacle = { (int)(rng() % 9) + 2, (int)(rng() % 13) };
			map.setCell(obstacle, false, rng() % 2 == 0);
		}

		Arena arena(
			{ { classIds[rng() % 4], { 0, 4 } }, { classIds[rng() % 4], { 0, 8 } } },
			{ { classIds[rng() % 4], { 12, 4 } }, { classIds[rng() % 4], { 12, 8 } } },
			map, (std::uint32_t)battle);

		// Copie tenue par un client : snapshot initial puis événements.
		BattleState mirror;
		BattleMap mirrorMap = map;
		BattleMirror::applySnapshot(mirror, mirrorMap, arena.engine->snapshot(0, arena.now));

		auto syncMirror = [&]() {
			nlohmann::json batch = nlohmann::json::parse(arena.engine->flushEvents().dump());
			for (const nlohmann::json & event : batch["ev"])
				BattleMirror::applyEvent(mirror, event);

			const BattleState & truth = arena.state();
			INFO(batch.dump());
			REQUIRE((mirror.phase == truth.phase));
			REQUIRE(mirror.activeFighterId() == truth.activeFighterId());
			REQUIRE(mirror.glyphs.size() == truth.glyphs.size());
			for (const Fighter & real : truth.fighters)
			{
				const Fighter & copy = *mirror.findFighter(real.id);
				REQUIRE(copy.alive == real.alive);
				REQUIRE(copy.hp == real.hp);
				if (!real.alive)
					continue;
				REQUIRE(copy.position == real.position);
				REQUIRE(copy.maxHp == real.maxHp);
				REQUIRE(copy.shield == real.shield);
				REQUIRE(copy.ap == real.ap);
				REQUIRE(copy.mp == real.mp);
				REQUIRE(copy.cooldowns == real.cooldowns);
				REQUIRE(copy.effects.size() == real.effects.size());
				for (std::size_t i = 0; i < real.effects.size(); i++)
					REQUIRE(copy.effects[i].remainingTurns == real.effects[i].remainingTurns);
			}
		};

		for (int action = 0; action < 5000 && !arena.engine->isOver(); action++)
		{
			syncMirror();

			int id = arena.active();
			const Fighter & fighter = arena.fighter(id);
			int choice = rng() % 3;

			if (choice == 0 && fighter.mp > 0)
			{
				std::vector<Cell> reachable = reachableCells(arena.state(), arena.engine->getMap(), fighter);
				if (!reachable.empty())
				{
					Cell target = reachable[rng() % reachable.size()];
					arena.engine->move(id, findPath(arena.state(), arena.engine->getMap(), fighter, target), arena.now);
					continue;
				}
			}

			if (choice == 1)
			{
				int index = rng() % 4;
				const SpellDef * spell = spellOf(gameData(), fighter, index);
				if (checkSpellResources(fighter, *spell).empty())
				{
					std::vector<Cell> cells = castableCells(arena.state(), arena.engine->getMap(), gameData(), fighter, *spell);
					if (!cells.empty())
					{
						REQUIRE(arena.engine->cast(id, index, cells[rng() % cells.size()], arena.now).ok);
						continue;
					}
				}
			}

			arena.now += 1000;
			REQUIRE(arena.engine->endTurn(id, arena.now).ok);
		}
		syncMirror();

		REQUIRE(arena.engine->isOver());
		CHECK((arena.state().winnerTeam == 1 || arena.state().winnerTeam == 2));
		CHECK(arena.state().round <= gameData().rules.maxRounds + 1);
		CHECK(arena.engine->snapshot(-1, arena.now)["fighters"].size() == 4);
	}
}

namespace
{
	// Aperçu d'un combattant (le lanceur peut aussi en avoir un : passif à chaque sort, vol de vie…).
	const TargetPreview * previewOf(const std::vector<TargetPreview> & previews, int fighterId)
	{
		for (const TargetPreview & preview : previews)
		{
			if (preview.fighterId == fighterId)
				return &preview;
		}
		return nullptr;
	}
}

TEST_CASE("Spell preview brackets the damage of real casts")
{
	// Éclair du Mage sur un Guerrier : les dégâts réels restent dans la fourchette annoncée.
	int slot = spellIndex(MAGE, "eclair");
	for (std::uint32_t seed = 1; seed <= 30; seed++)
	{
		CAPTURE(seed);
		Arena arena({ { MAGE, { 2, 2 } } }, { { GUERRIER, { 2, 6 } } }, openMap(), seed);
		arena.playUntilTurnOf(0);
		arena.engine->flushEvents();

		std::vector<TargetPreview> previews = previewSpell(arena.state(), arena.map, gameData(), 0, slot, { 2, 6 });
		const TargetPreview * preview = previewOf(previews, 1);
		REQUIRE(preview != nullptr);
		CHECK(preview->minDamage > 0);
		CHECK(preview->minDamage <= preview->maxDamage);
		CHECK_FALSE(preview->koPossible);
		int minDamage = preview->minDamage;
		int maxDamage = preview->maxDamage;

		REQUIRE(arena.engine->cast(0, slot, { 2, 6 }, arena.now).ok);
		std::vector<nlohmann::json> damage = arena.eventsOfType("damage");
		REQUIRE(damage.size() == 1);
		int amount = damage[0]["amount"].get<int>();
		CHECK(amount >= minDamage);
		CHECK(amount <= maxDamage);
	}
}

TEST_CASE("Spell preview reports shields, periodic effects, pushes, collisions and knockouts")
{
	const GameData & data = gameData();
	Arena arena({ { ARCHER, { 2, 2 } }, { PROTECTEUR, { 3, 2 } } }, { { GUERRIER, { 2, 5 } } });

	// Le calcul ne touche pas au combat.
	std::uint64_t seq = arena.engine->getSeq();

	// Bouclier sacré du Protecteur sur l'Archer : la valeur du bouclier est annoncée.
	arena.playUntilTurnOf(1);
	const SpellDef * shieldSpell = spellOf(data, arena.fighter(1), spellIndex(PROTECTEUR, "bouclier_sacre"));
	int shieldValue = 0;
	for (const EffectDef & effect : shieldSpell->effects)
	{
		if (effect.type == EffectType::SHIELD)
			shieldValue = effect.min;
	}
	std::vector<TargetPreview> shield = previewSpell(arena.state(), arena.map, data, 1, spellIndex(PROTECTEUR, "bouclier_sacre"), { 2, 2 });
	REQUIRE(shield.size() == 1);
	CHECK(shield[0].fighterId == 0);
	CHECK(shield[0].minShield == shieldValue);
	CHECK(shield[0].maxDamage == 0);

	// Flèche empoisonnée de l'Archer : dégâts et poison par tour.
	arena.playUntilTurnOf(0);
	std::vector<TargetPreview> poison = previewSpell(arena.state(), arena.map, data, 0, spellIndex(ARCHER, "fleche_empoisonnee"), { 2, 5 });
	REQUIRE(poison.size() == 1);
	CHECK(poison[0].minDamage > 0);
	bool perTurn = false;
	for (const std::string & note : poison[0].notes)
		perTurn = perTurn || note.find("/tour") != std::string::npos;
	CHECK(perTurn);

	// Flèche de recul : la cible est repoussée.
	std::vector<TargetPreview> push = previewSpell(arena.state(), arena.map, data, 0, spellIndex(ARCHER, "fleche_recul"), { 2, 5 });
	REQUIRE(push.size() == 1);
	CHECK(std::find(push[0].notes.begin(), push[0].notes.end(), std::string("Repoussé")) != push[0].notes.end());

	// Cible presque morte : hors combat certain.
	BattleState weakened = arena.state();
	weakened.findFighter(2)->hp = 1;
	std::vector<TargetPreview> knockout = previewSpell(weakened, arena.map, data, 0, spellIndex(ARCHER, "fleche_empoisonnee"), { 2, 5 });
	REQUIRE(knockout.size() == 1);
	CHECK(knockout[0].koCertain);
	CHECK(knockout[0].koPossible);

	// Cible impossible : aucun aperçu.
	CHECK(previewSpell(arena.state(), arena.map, data, 0, spellIndex(ARCHER, "tir_precis"), { 2, 5 }).empty());

	CHECK(arena.engine->getSeq() == seq);
	CHECK(arena.fighter(2).hp == arena.fighter(2).maxHp);
}

TEST_CASE("Spell preview includes collision damage when a push is blocked")
{
	BattleMap map = openMap();
	map.setCell({ 2, 7 }, false, true);
	Arena arena({ { ARCHER, { 2, 2 } } }, { { GUERRIER, { 2, 5 } } }, map);
	arena.playUntilTurnOf(0);

	int slot = spellIndex(ARCHER, "fleche_recul");
	std::vector<TargetPreview> previews = previewSpell(arena.state(), arena.map, gameData(), 0, slot, { 2, 5 });
	REQUIRE(previews.size() == 1);
	CHECK(std::find(previews[0].notes.begin(), previews[0].notes.end(), std::string("Collision")) != previews[0].notes.end());

	arena.engine->flushEvents();
	REQUIRE(arena.engine->cast(0, slot, { 2, 5 }, arena.now).ok);
	int total = 0;
	for (const nlohmann::json & event : arena.eventsOfType("damage"))
		total += event["amount"].get<int>();
	CHECK(total >= previews[0].minDamage);
	CHECK(total <= previews[0].maxDamage);
}

TEST_CASE("Next-turn reach uses a full turn of movement points")
{
	Arena arena({ { ARCHER, { 7, 7 } } }, { { GUERRIER, { 0, 0 } } }, openMap(15));
	int mp = arena.fighter(0).baseStats.get(Stat::MP);
	std::size_t expected = (std::size_t)(2 * mp * (mp + 1));
	CHECK(nextTurnReach(arena.state(), arena.map, gameData(), arena.fighter(0)).size() == expected);

	// Les PM dépensés pendant le tour ne réduisent pas la portée du tour suivant.
	arena.playUntilTurnOf(0);
	REQUIRE(arena.engine->move(0, { { 7, 8 }, { 7, 9 } }, arena.now).ok);
	CHECK(arena.fighter(0).mp == mp - 2);
	CHECK(nextTurnReach(arena.state(), arena.map, gameData(), arena.fighter(0)).size() == expected);
}
