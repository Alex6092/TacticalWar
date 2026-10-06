#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <random>
#include <set>

#include <Achievements.h>
#include <Appearances.h>
#include <BattleEngine.h>
#include <BattleMirror.h>
#include <BattlePreview.h>
#include <BotBrain.h>
#include <StateJson.h>
#include <Highlights.h>
#include <Emotes.h>

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

		// spells : sorts emportés par chaque combattant, dans l'ordre de création (par défaut, ceux de la classe).
		Arena(const std::vector<std::pair<int, Cell>> & team1, const std::vector<std::pair<int, Cell>> & team2, BattleMap baseMap = openMap(), std::uint32_t seed = 1,
			const std::vector<std::vector<int>> & spells = std::vector<std::vector<int>>(), bool bonuses = false)
			: map(baseMap)
		{
			for (const auto & entry : team1)
				map.startCells[1].push_back(entry.second);
			for (const auto & entry : team2)
				map.startCells[2].push_back(entry.second);

			engine.reset(new BattleEngine(gameData(), map, seed));
			auto spellsOf = [&spells](std::size_t index) { return index < spells.size() ? spells[index] : std::vector<int>(); };
			for (const auto & entry : team1)
				engine->addFighter(1, entry.first, "A" + std::to_string(entry.first), spellsOf(engine->getState().fighters.size()));
			for (const auto & entry : team2)
				engine->addFighter(2, entry.first, "B" + std::to_string(entry.first), spellsOf(engine->getState().fighters.size()));
			if (bonuses)
				engine->enableMapBonuses();

			engine->startPlacement(now);
			for (const Fighter & fighter : engine->getState().fighters)
				REQUIRE(engine->setReady(fighter.id, true, now).ok);
			REQUIRE((engine->getState().phase == BattlePhase::FIGHT));
			engine->flushEvents();
		}

		const BattleState & state() const { return engine->getState(); }
		const Fighter & fighter(int id) const { return *state().findFighter(id); }
		// Emplacement de la barre où le combattant a rangé ce sort.
		int slotOf(int id, const std::string & spellId) const
		{
			for (int slot = 0; slot < SPELL_SLOTS; slot++)
			{
				const SpellDef * spell = spellOf(gameData(), fighter(id), slot);
				if (spell != nullptr && spell->id == spellId)
					return slot;
			}
			FAIL("sort non emporté " << spellId);
			return -1;
		}
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

TEST_CASE("Game data defines the four classes with seven spells each")
{
	const GameData & data = gameData();
	REQUIRE(data.classes.size() == 4);
	for (int classId : { MAGE, ARCHER, PROTECTEUR, GUERRIER })
	{
		const ClassDef * classDef = data.findClass(classId);
		REQUIRE(classDef != nullptr);
		CHECK(classDef->spells.size() == 7);
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

	// Fin de tour au minuteur (durée du tour, puis réserve de temps).
	arena.now += (gameData().rules.turnSeconds + gameData().rules.timeBankSeconds) * 1000 + 1000;
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

		int picked[4];
		std::vector<std::vector<int>> spells;
		for (int i = 0; i < 4; i++)
		{
			picked[i] = classIds[rng() % 4];
			spells.push_back(randomSpellChoice(*gameData().findClass(picked[i]), rng));
		}
		Arena arena(
			{ { picked[0], { 0, 4 } }, { picked[1], { 0, 8 } } },
			{ { picked[2], { 12, 4 } }, { picked[3], { 12, 8 } } },
			map, (std::uint32_t)battle, spells, battle % 2 == 0);

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
			REQUIRE(mirror.blocks.size() == truth.blocks.size());
			REQUIRE(mirror.orbs.size() == truth.orbs.size());
			for (const Orb & orb : truth.orbs)
				REQUIRE(mirror.orbAt(orb.cell) != nullptr);
			for (const Block & block : truth.blocks)
			{
				const Block * copy = mirror.findBlock(block.uid);
				REQUIRE(copy != nullptr);
				REQUIRE(copy->hp == block.hp);
				REQUIRE(copy->cell == block.cell);
				REQUIRE(copy->remainingTurns == block.remainingTurns);
			}
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

TEST_CASE("Spell preview ignores shields already in place, even in a client mirror")
{
	Arena arena({ { ARCHER, { 2, 2 } } }, { { GUERRIER, { 2, 8 } } });
	arena.playUntilTurnOf(0);

	// Le Guerrier a déjà un bouclier ; le miroir d'un client ne connaît pas le compteur d'identifiants.
	BattleState mirror = arena.state();
	ActiveEffect shield;
	shield.uid = 40;
	shield.type = EffectType::SHIELD;
	shield.value = 30;
	shield.remainingTurns = 2;
	shield.positive = true;
	mirror.findFighter(1)->effects.push_back(shield);
	mirror.findFighter(1)->shield = 30;
	mirror.nextUid = 1;

	std::vector<TargetPreview> previews = previewSpell(mirror, arena.map, gameData(), 0, arena.slotOf(0, "tir_precis"), { 2, 8 });
	REQUIRE(previews.size() == 1);
	CHECK(previews[0].maxAbsorbed > 0);
	CHECK(previews[0].minShield == 0);
	CHECK(previews[0].maxShield == 0);
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

TEST_CASE("Emotes are broadcast as events and rate limited per fighter")
{
	Arena arena({ { MAGE, { 2, 2 } } }, { { GUERRIER, { 2, 6 } } });
	BattleState before = arena.state();

	REQUIRE(arena.engine->emote(0, 2, 1000).ok);
	std::vector<nlohmann::json> events = arena.eventsOfType("emote");
	REQUIRE(events.size() == 1);
	CHECK(events[0]["f"].get<int>() == 0);
	CHECK(events[0]["id"].get<int>() == 2);

	// Une émote toutes les EMOTE_COOLDOWN_MS par combattant, à n'importe quel moment.
	CHECK_FALSE(arena.engine->emote(0, 1, 2000).ok);
	CHECK(arena.engine->emote(1, 1, 2000).ok);
	CHECK(arena.engine->emote(0, 1, 1000 + EMOTE_COOLDOWN_MS).ok);

	CHECK_FALSE(arena.engine->emote(0, EMOTE_COUNT, 99999).ok);
	CHECK_FALSE(arena.engine->emote(0, -1, 99999).ok);
	CHECK_FALSE(arena.engine->emote(9, 0, 99999).ok);

	// Les émotes ne touchent pas au combat ; le miroir des clients les ignore.
	CHECK(arena.state().activeFighterId() == before.activeFighterId());
	CHECK(arena.fighter(0).ap == before.findFighter(0)->ap);
	BattleState mirror = before;
	BattleMirror::applyEvent(mirror, { { "t", "emote" }, { "f", 0 }, { "id", 1 } });
	CHECK(mirror.fighters.size() == before.fighters.size());
}

TEST_CASE("A long turn eats into the fighter's time bank, and an empty bank ends the turn on time")
{
	Arena arena({ { ARCHER, { 2, 2 } } }, { { GUERRIER, { 2, 8 } } });
	const std::int64_t turnMs = gameData().rules.turnSeconds * 1000;
	const std::int64_t bankMs = gameData().rules.timeBankSeconds * 1000;
	REQUIRE(bankMs > 10000);
	arena.playUntilTurnOf(0);
	CHECK(arena.fighter(0).timeBankMs == bankMs);
	CHECK(arena.state().deadlineMs - arena.now == turnMs + bankMs);

	// Tour fini 10 s après la durée normale : 10 s de moins dans la réserve.
	arena.now += turnMs + 10000;
	REQUIRE(arena.engine->endTurn(0, arena.now).ok);
	CHECK(arena.fighter(0).timeBankMs == bankMs - 10000);

	// Un tour plus court que la durée normale ne touche pas à la réserve.
	arena.playUntilTurnOf(0);
	arena.now += 5000;
	REQUIRE(arena.engine->endTurn(0, arena.now).ok);
	CHECK(arena.fighter(0).timeBankMs == bankMs - 10000);

	// Réserve épuisée : le tour s'arrête à son échéance, et la réserve reste vide.
	arena.playUntilTurnOf(0);
	std::int64_t deadline = arena.state().deadlineMs;
	CHECK(deadline - arena.now == turnMs + bankMs - 10000);
	arena.now = deadline;
	arena.engine->tick(arena.now);
	CHECK(arena.active() != 0);
	CHECK(arena.fighter(0).timeBankMs == 0);
	arena.playUntilTurnOf(0);
	CHECK(arena.state().deadlineMs - arena.now == turnMs);

	// Les clients lisent la réserve dans l'état complet.
	BattleState mirror;
	BattleMap mirrorMap;
	BattleMirror::applySnapshot(mirror, mirrorMap, arena.engine->snapshot(0, arena.now));
	CHECK(mirror.findFighter(0)->timeBankMs == 0);
	CHECK(mirror.findFighter(1)->timeBankMs == arena.fighter(1).timeBankMs);
}

TEST_CASE("A fighter piloted by its teammate keeps a full turn while its player is away")
{
	Arena arena({ { ARCHER, { 2, 2 } }, { GUERRIER, { 3, 2 } } }, { { MAGE, { 2, 8 } } });
	const int turnMs = gameData().rules.turnSeconds * 1000;
	const int shortMs = gameData().rules.disconnectedTurnSeconds * 1000;

	// Joueur du Guerrier absent : tour raccourci...
	arena.engine->setConnected(1, false, arena.now);
	arena.playUntilTurnOf(1);
	CHECK(arena.state().deadlineMs - arena.now == shortMs);

	// ... sauf si son coéquipier le pilote : tour complet (avec sa réserve), rendu aussi pendant le tour en cours.
	const int bankMs = gameData().rules.timeBankSeconds * 1000;
	arena.engine->setPiloted(1, true, arena.now);
	CHECK(arena.state().deadlineMs - arena.now == turnMs + bankMs);
	CHECK(arena.fighter(1).piloted);
	REQUIRE(arena.engine->endTurn(1, arena.now).ok);
	arena.playUntilTurnOf(1);
	CHECK(arena.state().deadlineMs - arena.now == turnMs + bankMs);

	// Le miroir des clients suit l'état piloté (instantané et événement).
	BattleState mirror;
	BattleMap mirrorMap;
	BattleMirror::applySnapshot(mirror, mirrorMap, arena.engine->snapshot(0, arena.now));
	CHECK(mirror.findFighter(1)->piloted);
	arena.engine->flushEvents();
	arena.engine->setPiloted(1, false, arena.now);
	nlohmann::json batch = arena.engine->flushEvents();
	for (const nlohmann::json & event : batch["ev"])
		BattleMirror::applyEvent(mirror, event);
	CHECK_FALSE(mirror.findFighter(1)->piloted);
}

TEST_CASE("Battle records credit damage, shields, casts and knockouts to the right fighter")
{
	Arena arena({ { ARCHER, { 2, 2 } }, { PROTECTEUR, { 3, 2 } } }, { { MAGE, { 2, 7 } } });
	int shot = spellIndex(ARCHER, "tir_precis");

	// Tir précis de l'Archer : dégâts infligés et subis, sort compté.
	arena.playUntilTurnOf(0);
	arena.engine->flushEvents();
	REQUIRE(arena.engine->cast(0, shot, { 2, 7 }, arena.now).ok);
	std::vector<nlohmann::json> damage = arena.eventsOfType("damage");
	REQUIRE(damage.size() == 1);
	int amount = damage[0]["amount"].get<int>();
	CHECK(arena.fighter(0).record.dealt == amount);
	CHECK(arena.fighter(0).record.casts == 1);
	CHECK(arena.fighter(2).record.taken == amount);
	CHECK(arena.fighter(2).record.dealt == 0);

	// Bouclier sacré du Protecteur sur l'Archer.
	arena.playUntilTurnOf(1);
	REQUIRE(arena.engine->cast(1, spellIndex(PROTECTEUR, "bouclier_sacre"), { 2, 2 }, arena.now).ok);
	CHECK(arena.fighter(1).record.shielded > 0);
	CHECK(arena.fighter(1).record.dealt == 0);

	// L'Archer tire jusqu'à la mise hors combat du Mage : un KO, et il est le meilleur du combat.
	for (int guard = 0; guard < 80 && !arena.engine->isOver(); guard++)
	{
		if (arena.active() != 0)
			REQUIRE(arena.engine->endTurn(arena.active(), arena.now).ok);
		else if (!arena.engine->cast(0, shot, { 2, 7 }, arena.now).ok)
			REQUIRE(arena.engine->endTurn(0, arena.now).ok);
	}
	REQUIRE(arena.engine->isOver());
	CHECK(arena.fighter(0).record.kills == 1);
	CHECK(arena.fighter(0).record.dealt == arena.fighter(2).record.taken);
	CHECK(arena.state().mvpFighterId == 0);
	CHECK(arena.state().firstBloodFighterId == 0);
	const std::vector<std::string> & badges = arena.fighter(0).record.badges;
	CHECK(std::find(badges.begin(), badges.end(), "first_blood") != badges.end());

	std::vector<nlohmann::json> ends = arena.eventsOfType("end");
	REQUIRE(ends.size() == 1);
	CHECK(ends[0]["mvp"].get<int>() == 0);
	CHECK(ends[0]["records"].size() == 3);
	CHECK(ends[0]["records"][0]["badges"] == nlohmann::json(badges));

	// Les clients retrouvent le bilan dans l'état complet.
	BattleState mirror;
	BattleMap mirrorMap;
	BattleMirror::applySnapshot(mirror, mirrorMap, arena.engine->snapshot(-1, arena.now));
	CHECK(mirror.mvpFighterId == 0);
	CHECK(mirror.findFighter(0)->record.kills == 1);
	CHECK(mirror.findFighter(1)->record.shielded == arena.fighter(1).record.shielded);
	CHECK(mirror.findFighter(0)->record.badges == badges);
}

TEST_CASE("Special cells hurt or heal the fighter standing on them at the start of its turn")
{
	BattleMap map = openMap();
	map.setTurnEffect({ 2, 2 }, 8, 0);	// Braises sous l'Archer
	map.setTurnEffect({ 2, 8 }, 0, 6);	// Source sous le Guerrier
	Arena arena({ { ARCHER, { 2, 2 } } }, { { GUERRIER, { 2, 8 } } }, map);

	// Braises : 8 dégâts fixes au début du tour (le premier tour commence au lancement du combat).
	int hp = arena.fighter(0).hp;
	arena.playUntilTurnOf(1);
	arena.playUntilTurnOf(0);
	std::vector<nlohmann::json> damage = arena.eventsOfType("damage");
	REQUIRE(damage.size() >= 1);
	CHECK(damage.back()["kind"] == "terrain");
	CHECK(damage.back()["amount"].get<int>() == 8);
	CHECK(damage.back()["src"].get<int>() == -1);
	CHECK(arena.fighter(0).hp < hp);

	// Source : soigne un combattant blessé (6 PV), rien s'il a tous ses PV.
	BattleState wounded = arena.state();
	wounded.findFighter(1)->hp -= 20;
	BattleEngine engine(gameData(), arena.map, wounded, 1);
	REQUIRE(engine.endTurn(0, arena.now).ok);
	CHECK(engine.getState().findFighter(1)->hp == wounded.findFighter(1)->hp + 6);
	nlohmann::json batch = engine.flushEvents();
	bool healed = false;
	for (const nlohmann::json & event : batch["ev"])
		healed = healed || (event["t"] == "heal" && event["kind"] == "terrain" && event["amount"].get<int>() == 6);
	CHECK(healed);
}

TEST_CASE("Tall grass can be walked through but hides what is behind it")
{
	BattleMap map = openMap(9);
	map.setCell({ 4, 2 }, true, true);
	Arena arena({ { ARCHER, { 4, 0 } } }, { { GUERRIER, { 4, 4 } } }, map);
	CHECK_FALSE(hasLineOfSight(arena.state(), arena.map, { 4, 0 }, { 4, 4 }));
	CHECK(hasLineOfSight(arena.state(), arena.map, { 4, 0 }, { 4, 2 }));
	arena.playUntilTurnOf(0);
	std::vector<Cell> path = findPath(arena.state(), arena.map, arena.fighter(0), { 4, 2 });
	REQUIRE(path.size() == 2);
	CHECK(arena.engine->move(0, path, arena.now).ok);
	CHECK(arena.fighter(0).position == Cell{ 4, 2 });
}

TEST_CASE("The AI keeps off embers and heads for a spring when wounded")
{
	// Destination du premier déplacement de l'IA ce tour-ci (après ses éventuels sorts).
	auto destination = [](const BattleMap & map, const BattleState & state) {
		BattleEngine engine(gameData(), map, state, 1);
		std::mt19937 rng(3);
		for (int i = 0; i < 6; i++)
		{
			BotAction action = chooseBotAction(engine.getState(), engine.getMap(), gameData(), 0, rng);
			if (action.kind == BotAction::Kind::MOVE)
				return action.path.back();
			if (action.kind != BotAction::Kind::CAST || !engine.cast(0, action.slot, action.target, 0).ok)
				break;
		}
		return Cell{ -1, -1 };
	};

	// Guerrier loin de l'Archer : sans case à effet, il avance vers lui.
	BattleMap plain = openMap();
	Arena arena({ { GUERRIER, { 2, 2 } } }, { { ARCHER, { 2, 13 } } }, plain);
	arena.playUntilTurnOf(0);
	Cell usual = destination(plain, arena.state());
	REQUIRE(usual != Cell{ -1, -1 });

	BattleMap embers = plain;
	embers.setTurnEffect(usual, 8, 0);
	Cell avoided = destination(embers, arena.state());
	CHECK(avoided != Cell{ -1, -1 });
	CHECK(avoided != usual);

	// Blessé, il fait un détour par une source ; en pleine forme, il l'ignore.
	BattleMap spring = plain;
	spring.setTurnEffect({ 3, 3 }, 0, 6);
	CHECK(destination(spring, arena.state()) != Cell{ 3, 3 });
	BattleState hurt = arena.state();
	hurt.findFighter(0)->hp = hurt.findFighter(0)->maxHp / 2;
	CHECK(destination(spring, hurt) == Cell{ 3, 3 });
}

TEST_CASE("Achievements reward each feat at the end of the battle")
{
	BattleState state;
	state.phase = BattlePhase::ENDED;
	state.winnerTeam = 1;
	state.endReason = EndReason::KO;
	state.round = 9;
	Fighter striker;
	striker.id = 0;
	striker.team = 1;
	Fighter healer;
	healer.id = 1;
	healer.team = 1;
	Fighter enemy;
	enemy.id = 2;
	enemy.team = 2;
	enemy.alive = false;
	state.fighters = { striker, healer, enemy };
	auto earned = [&state](int id) { return earnedAchievements(state, *state.findFighter(id)); };
	using Badges = std::vector<std::string>;

	// Un combattant qui n'a rien fait n'obtient rien.
	CHECK(earned(1).empty());

	Fighter & a = *state.findFighter(0);
	a.record.kills = 2;
	a.record.combos = 2;
	a.record.dealt = 150;
	a.record.taken = 5;
	a.record.casts = 6;
	state.firstBloodFighterId = 0;
	CHECK(earned(0) == Badges{ "first_blood", "double_ko", "combo_master", "demolisher" });
	a.record.kills = 1;
	a.record.combos = 1;
	a.record.dealt = 149;
	state.firstBloodFighterId = -1;
	CHECK(earned(0).empty());

	// Soins et boucliers donnés ; aucun dégât subi en ayant joué.
	Fighter & h = *state.findFighter(1);
	h.record.healed = 35;
	h.record.shielded = 25;
	h.record.casts = 4;
	CHECK(earned(1) == Badges{ "guardian_angel", "untouchable" });
	h.record.taken = 1;
	CHECK(earned(1) == Badges{ "guardian_angel" });

	// Seul survivant de l'équipe gagnante.
	h.alive = false;
	CHECK(earned(0) == Badges{ "last_standing" });
	CHECK(earned(2).empty());

	// Zone tenue pour 3 points ; victoire en 5 tours ou moins, pas sur une décision de l'organisateur.
	a.record.zonePoints = 3;
	state.round = 5;
	CHECK(earned(0) == Badges{ "last_standing", "zone_keeper", "lightning" });
	state.endReason = EndReason::ADMIN;
	CHECK(earned(0) == Badges{ "last_standing", "zone_keeper" });

	// Chaque haut fait a un nom et une description, et un identifiant unique.
	std::set<std::string> ids;
	for (const AchievementDef & achievement : ACHIEVEMENTS)
	{
		CHECK(findAchievement(achievement.id) == &achievement);
		CHECK(std::string(achievement.name).size() > 0);
		CHECK(std::string(achievement.description).size() > 0);
		ids.insert(achievement.id);
	}
	CHECK(ids.size() == ACHIEVEMENT_COUNT);
	CHECK(findAchievement("inconnu") == nullptr);
}

TEST_CASE("The MVP has the best record score, the winning team breaking ties")
{
	BattleState state;
	state.winnerTeam = 2;
	Fighter striker;
	striker.id = 0;
	striker.team = 1;
	striker.record.dealt = 50;
	Fighter healer;
	healer.id = 1;
	healer.team = 2;
	healer.record.healed = 30;
	healer.record.shielded = 40;
	state.fighters = { striker, healer };

	CHECK(recordScore(healer.record) == 50);
	CHECK(chooseMvp(state) == 1);
	state.fighters[0].record.kills = 1;
	CHECK(recordScore(state.fighters[0].record) == 75);
	CHECK(chooseMvp(state) == 0);

	BattleState idle;
	idle.fighters = { Fighter() };
	CHECK(chooseMvp(idle) == -1);
}

TEST_CASE("Bot mistakes for easy training stay legal and vary the choices")
{
	Arena arena({ { ARCHER, { 2, 2 } } }, { { GUERRIER, { 2, 8 } } });
	arena.playUntilTurnOf(0);

	BotOptions clumsy;
	clumsy.mistakePercent = 100;
	std::set<std::string> choices;
	for (std::uint32_t seed = 1; seed <= 40; seed++)
	{
		std::mt19937 rng(seed);
		BotAction action = chooseBotAction(arena.state(), arena.map, gameData(), 0, rng, clumsy);
		if (action.kind == BotAction::Kind::END_TURN)
			continue;

		// Chaque erreur reste une action acceptée par le moteur.
		BattleEngine copy(gameData(), arena.map, arena.state(), seed);
		ActionResult result = action.kind == BotAction::Kind::CAST ? copy.cast(0, action.slot, action.target, arena.now)
			: copy.move(0, action.path, arena.now);
		CHECK_MESSAGE(result.ok, result.error);

		Cell end = action.path.empty() ? action.target : action.path.back();
		choices.insert(std::to_string((int)action.kind) + ":" + std::to_string(action.slot) + ":" + std::to_string(end.x) + "," + std::to_string(end.y));
	}
	CHECK(choices.size() >= 3);
}

TEST_CASE("Combos boost damage on a marked target and consume the mark when required")
{
	Arena arena({ { GUERRIER, { 5, 5 } } }, { { MAGE, { 5, 6 } } });
	arena.playUntilTurnOf(0);
	int taillade = spellIndex(GUERRIER, "taillade");

	BattleState frozen = arena.state();
	ActiveEffect mark;
	mark.uid = frozen.nextUid++;
	mark.type = EffectType::STATE;
	mark.state = "gele";
	mark.name = "Gelé";
	mark.remainingTurns = 1;
	frozen.findFighter(1)->effects.push_back(mark);

	// Deux Taillade de suite (jets minimaux) : dégâts de chacune et combinaisons déclenchées.
	auto strike = [&](const BattleState & start, std::vector<int> & damage, std::vector<nlohmann::json> & combos) {
		BattleEngine engine(gameData(), arena.map, start, 1);
		engine.setRollMode(BattleEngine::RollMode::MIN);
		for (int i = 0; i < 2; i++)
		{
			int before = engine.getState().findFighter(1)->hp;
			REQUIRE(engine.cast(0, taillade, { 5, 6 }, arena.now).ok);
			damage.push_back(before - engine.getState().findFighter(1)->hp);
		}
		CHECK_FALSE(engine.getState().findFighter(1)->hasState("gele"));
		nlohmann::json batch = engine.flushEvents();
		for (const nlohmann::json & event : batch["ev"])
		{
			if (event["t"] == "combo")
				combos.push_back(event);
		}
		CHECK(engine.getState().findFighter(0)->record.combos == (int)combos.size());
	};

	std::vector<int> plain, boosted;
	std::vector<nlohmann::json> none, triggered;
	strike(arena.state(), plain, none);
	strike(frozen, boosted, triggered);

	CHECK(none.empty());
	REQUIRE(triggered.size() == 1);
	CHECK(triggered[0]["name"] == "Brise-glace");
	CHECK(triggered[0]["percent"].get<int>() == 40);
	CHECK(triggered[0]["src"].get<int>() == 0);
	CHECK(triggered[0]["f"].get<int>() == 1);
	// +40 % sur la première, la cible dégèle : pas de bonus sur la seconde.
	CHECK(boosted[0] == (int)std::lround(plain[0] * 1.4));
	CHECK(boosted[1] == plain[1]);

	// L'aperçu annonce la combinaison et compte le bonus.
	std::vector<TargetPreview> previews = previewSpell(frozen, arena.map, gameData(), 0, taillade, { 5, 6 });
	REQUIRE(previews.size() == 1);
	CHECK(previews[0].minDamage == boosted[0]);
	const std::vector<std::string> & notes = previews[0].notes;
	CHECK(std::find(notes.begin(), notes.end(), "Combo Brise-glace +40 %") != notes.end());
}

TEST_CASE("Entangling arrow marks the target for the Mage until the end of its second turn")
{
	Arena arena({ { ARCHER, { 2, 2 } }, { MAGE, { 3, 2 } } }, { { GUERRIER, { 2, 7 } } });
	arena.playUntilTurnOf(0);
	REQUIRE(arena.engine->cast(0, spellIndex(ARCHER, "fleche_entravante"), { 2, 7 }, arena.now).ok);
	REQUIRE(arena.fighter(2).hasState("entrave"));
	// Une marque est négative : la purification d'un allié l'enlève, celle d'un ennemi non.
	for (const ActiveEffect & effect : arena.fighter(2).effects)
	{
		if (effect.type == EffectType::STATE)
			CHECK_FALSE(effect.positive);
	}

	// Le Mage en profite à son tour, sans consommer la marque ; elle s'efface après deux tours de la cible.
	int targetTurnEnds = 0;
	bool comboChecked = false;
	for (int guard = 0; arena.fighter(2).hasState("entrave"); guard++)
	{
		REQUIRE(guard < 20);
		int active = arena.active();
		if (active == 1 && !comboChecked)
		{
			arena.eventsOfType("combo");
			REQUIRE(arena.engine->cast(1, spellIndex(MAGE, "eclair"), { 2, 7 }, arena.now).ok);
			std::vector<nlohmann::json> combos = arena.eventsOfType("combo");
			REQUIRE(combos.size() == 1);
			CHECK(combos[0]["name"] == "Cible immobile");
			CHECK(arena.fighter(2).hasState("entrave"));
			comboChecked = true;
		}
		if (active == 2)
			targetTurnEnds++;
		REQUIRE(arena.engine->endTurn(active, arena.now).ok);
	}
	CHECK(comboChecked);
	CHECK(targetTurnEnds == 2);
}

TEST_CASE("Fireball marks burned enemies only and the frost glyph freezes until the next turn")
{
	Arena arena({ { MAGE, { 2, 7 } }, { GUERRIER, { 6, 6 } } }, { { PROTECTEUR, { 6, 7 } } });
	arena.playUntilTurnOf(0);
	REQUIRE(arena.engine->cast(0, spellIndex(MAGE, "boule_de_feu"), { 6, 7 }, arena.now).ok);
	CHECK(arena.fighter(2).hasState("brule"));
	// L'allié touché brûle aussi, mais n'est pas marqué.
	CHECK(arena.fighter(1).effects.size() > 0);
	CHECK_FALSE(arena.fighter(1).hasState("brule"));

	Arena frost({ { MAGE, { 2, 7 } }, { GUERRIER, { 2, 9 } } }, { { ARCHER, { 6, 7 } } });
	frost.playUntilTurnOf(0);
	REQUIRE(frost.engine->cast(0, spellIndex(MAGE, "glyphe_givre"), { 6, 7 }, frost.now).ok);
	CHECK_FALSE(frost.fighter(2).hasState("gele"));
	frost.playUntilTurnOf(2);
	CHECK(frost.fighter(2).hasState("gele"));
	// Toujours gelé après son tour : le Guerrier peut en profiter.
	REQUIRE(frost.engine->endTurn(2, frost.now).ok);
	CHECK(frost.fighter(2).hasState("gele"));
}

TEST_CASE("Combos between two classes list the marking and finishing spells, both ways")
{
	auto has = [](const std::vector<std::string> & list, const std::string & name) {
		return std::find(list.begin(), list.end(), name) != list.end();
	};

	// Mage et Guerrier : Brise-glace, la marque venant aussi du glyphe.
	std::vector<ComboLink> links = combosBetween(gameData(), MAGE, GUERRIER);
	REQUIRE(links.size() == 1);
	CHECK(links[0].name == "Brise-glace");
	CHECK(links[0].setterClass == MAGE);
	CHECK(links[0].finisherClass == GUERRIER);
	CHECK(links[0].percent == 40);
	CHECK(has(links[0].setters, "Glyphe de givre"));
	CHECK(has(links[0].setters, "Prison de glace"));
	CHECK(has(links[0].finishers, "Taillade"));
	CHECK(has(links[0].finishers, "Charge"));
	// L'ordre des classes ne change pas le résultat.
	CHECK(combosBetween(gameData(), GUERRIER, MAGE).size() == 1);

	// Les 4 combinaisons du jeu, chacune entre deux classes différentes.
	std::set<std::string> names;
	const int classes[4] = { MAGE, ARCHER, PROTECTEUR, GUERRIER };
	for (int i = 0; i < 4; i++)
	{
		CHECK(combosBetween(gameData(), classes[i], classes[i]).empty());
		for (int j = i + 1; j < 4; j++)
		{
			for (const ComboLink & link : combosBetween(gameData(), classes[i], classes[j]))
			{
				CHECK(link.setterClass != link.finisherClass);
				CHECK_FALSE(link.setters.empty());
				CHECK_FALSE(link.finishers.empty());
				names.insert(link.name);
			}
		}
	}
	CHECK(names == std::set<std::string>{ "Brise-glace", "Cible immobile", "Dans le mille", "Jugement ardent" });
	CHECK(combosBetween(gameData(), MAGE, 99).empty());
}

TEST_CASE("Each combo uses a mark set by a spell of another class")
{
	std::map<std::string, std::set<int>> producers;
	std::function<void(int, const EffectDef &)> scan = [&](int classId, const EffectDef & effect) {
		if (effect.type == EffectType::STATE && effect.negative)
			producers[effect.state].insert(classId);
		for (const EffectDef & triggered : effect.glyphEffects)
			scan(classId, triggered);
	};
	for (const ClassDef & classDef : gameData().classes)
	{
		for (const SpellDef & spell : classDef.spells)
		{
			for (const EffectDef & effect : spell.effects)
				scan(classDef.id, effect);
		}
	}

	int combos = 0;
	for (const ClassDef & classDef : gameData().classes)
	{
		for (const SpellDef & spell : classDef.spells)
		{
			for (const EffectDef & effect : spell.effects)
			{
				if (effect.comboState.empty())
					continue;
				combos++;
				INFO(spell.id << " : " << effect.comboState);
				REQUIRE(producers.count(effect.comboState) == 1);
				bool otherClass = false;
				for (int producer : producers[effect.comboState])
					otherClass = otherClass || producer != classDef.id;
				CHECK(otherClass);
				CHECK_FALSE(effect.comboName.empty());
			}
		}
	}
	CHECK(combos >= 4);

	// Une combinaison doit porter sur des dégâts directs, avec un état et un bonus.
	GameData data;
	std::string error;
	CHECK_FALSE(data.loadFromJsonText(R"({ "classes": [ { "id": 1, "name": "T", "stats": { "MAX_HP": 10 }, "spells": [
		{ "id": "s", "effects": [ { "type": "HEAL", "min": 5, "combo": { "state": "x", "percent": 20 } } ] } ] } ] })", error));
	CHECK_FALSE(data.loadFromJsonText(R"({ "classes": [ { "id": 1, "name": "T", "stats": { "MAX_HP": 10 }, "spells": [
		{ "id": "s", "effects": [ { "type": "DAMAGE", "min": 5, "combo": { "state": "x" } } ] } ] } ] })", error));
}

namespace
{
	// Duel de Guerriers sur une carte ouverte 9x9, départs face à face, en mode zone à tenir.
	struct ZoneDuel
	{
		BattleMap map;
		std::unique_ptr<BattleEngine> engine;
		std::int64_t now = 0;

		explicit ZoneDuel(int points)
			: map(9, 9)
		{
			map.startCells[1] = { { 1, 4 } };
			map.startCells[2] = { { 7, 4 } };
			engine.reset(new BattleEngine(gameData(), map, 1));
			engine->addFighter(1, GUERRIER, "A");
			engine->addFighter(2, GUERRIER, "B");
			engine->enableZone(points);
			engine->startPlacement(now);
			REQUIRE(engine->setReady(0, true, now).ok);
			REQUIRE(engine->setReady(1, true, now).ok);
			REQUIRE((engine->getState().phase == BattlePhase::FIGHT));
			engine->flushEvents();
		}

		const BattleState & state() const { return engine->getState(); }

		void playUntilTurnOf(int id)
		{
			for (int guard = 0; state().activeFighterId() != id; guard++)
			{
				REQUIRE(guard < 10);
				REQUIRE(engine->endTurn(state().activeFighterId(), now).ok);
			}
		}

		// Passe les tours jusqu'au décompte de fin de tour complet ; retourne l'événement "score".
		nlohmann::json finishRound()
		{
			int round = state().round;
			for (int guard = 0; state().round == round && !engine->isOver(); guard++)
			{
				REQUIRE(guard < 10);
				REQUIRE(engine->endTurn(state().activeFighterId(), now).ok);
			}
			nlohmann::json batch = engine->flushEvents();
			for (const nlohmann::json & event : batch["ev"])
			{
				if (event["t"] == "score")
					return event;
			}
			return nlohmann::json();
		}
	};
}

TEST_CASE("The zone to hold is central and equidistant from both teams, unless painted")
{
	BattleMap map(9, 9);
	map.startCells[1] = { { 0, 4 } };
	map.startCells[2] = { { 8, 4 } };
	std::vector<Cell> zone = objectiveZone(map);
	REQUIRE(zone.size() == 5);
	CHECK(zone[0] == Cell{ 4, 4 });
	int distances[3];
	zoneDistances(map, zone, distances);
	CHECK(distances[1] == 4);
	CHECK(distances[2] == 4);

	// Centre bloqué : une autre croix de cases praticables, toujours à égale distance.
	map.setCell({ 4, 4 }, false, true);
	zone = objectiveZone(map);
	REQUIRE(zone.size() >= 5);
	for (const Cell & cell : zone)
		CHECK(map.isWalkable(cell));
	zoneDistances(map, zone, distances);
	CHECK(distances[1] == distances[2]);

	// Zone peinte : ses cases praticables seulement.
	map.zoneCells = { { 2, 2 }, { 2, 3 }, { 4, 4 } };
	CHECK(objectiveZone(map) == std::vector<Cell>{ { 2, 2 }, { 2, 3 } });
}

TEST_CASE("Holding the zone alone scores a point each round until the target score")
{
	ZoneDuel duel(2);
	const ZoneState & zone = duel.state().zone;
	REQUIRE(zone.enabled);
	REQUIRE(zone.cells.size() == 5);
	REQUIRE(zone.contains({ 4, 4 }));
	REQUIRE(zone.contains({ 4, 3 }));

	duel.playUntilTurnOf(0);
	REQUIRE(duel.engine->move(0, { { 2, 4 }, { 3, 4 }, { 4, 4 } }, duel.now).ok);
	nlohmann::json score = duel.finishRound();
	REQUIRE(score.is_object());
	CHECK(score["holder"].get<int>() == 1);
	CHECK(score["scores"] == nlohmann::json::array({ 1, 0 }));
	CHECK(zone.scores[1] == 1);
	CHECK_FALSE(duel.engine->isOver());

	// Le miroir des clients reçoit la zone dans l'état complet, puis les points.
	BattleState mirror;
	BattleMap mirrorMap;
	BattleMirror::applySnapshot(mirror, mirrorMap, duel.engine->snapshot(-1, duel.now));
	CHECK(mirror.zone.enabled);
	CHECK(mirror.zone.cells == zone.cells);
	CHECK(mirror.zone.pointsToWin == 2);
	CHECK(mirror.zone.scores[1] == 1);

	score = duel.finishRound();
	BattleMirror::applyEvent(mirror, score);
	CHECK(mirror.zone.scores[1] == 2);
	CHECK(duel.engine->isOver());
	CHECK(duel.state().winnerTeam == 1);
	CHECK((duel.state().endReason == EndReason::OBJECTIVE));

	// Bilan : les 2 points reviennent au Guerrier dans la zone ; victoire en moins de 5 tours.
	CHECK(duel.state().findFighter(0)->record.zonePoints == 2);
	CHECK(duel.state().findFighter(1)->record.zonePoints == 0);
	CHECK(duel.state().findFighter(0)->record.badges == std::vector<std::string>{ "lightning" });
}

TEST_CASE("A contested zone scores nothing and a decision counts zone points first")
{
	ZoneDuel duel(5);
	// Chacun entre dans la zone pendant le premier tour complet.
	for (int i = 0; i < 2; i++)
	{
		int active = duel.state().activeFighterId();
		std::vector<Cell> path = active == 0 ? std::vector<Cell>{ { 2, 4 }, { 3, 4 }, { 4, 4 } }
			: std::vector<Cell>{ { 6, 4 }, { 6, 3 }, { 5, 3 }, { 4, 3 } };
		REQUIRE(duel.engine->move(active, path, duel.now).ok);
		REQUIRE(duel.engine->endTurn(active, duel.now).ok);
	}
	nlohmann::json batch = duel.engine->flushEvents();
	nlohmann::json score;
	for (const nlohmann::json & event : batch["ev"])
	{
		if (event["t"] == "score")
			score = event;
	}
	REQUIRE(score.is_object());
	CHECK(score["holder"].get<int>() == 0);
	CHECK(score["contested"].get<bool>());
	CHECK(duel.state().zone.scores[1] == 0);
	CHECK(duel.state().zone.scores[2] == 0);

	// Arrêt par l'organisateur : les points de zone priment sur les PV (égaux ici).
	BattleState scored = duel.state();
	scored.zone.scores[1] = 1;
	scored.zone.scores[2] = 3;
	BattleEngine judge(gameData(), duel.map, scored, 1);
	judge.stopByDecision(duel.now);
	CHECK(judge.getState().winnerTeam == 2);
	CHECK((judge.getState().endReason == EndReason::ADMIN));
}

TEST_CASE("Each fighter carries four spells chosen among the seven of its class")
{
	const ClassDef & guerrier = *gameData().findClass(GUERRIER);
	CHECK(defaultSpells(guerrier) == std::vector<int>{ 0, 1, 2, 3 });
	CHECK(validSpellChoice(guerrier, { 5, 4, 0, 6 }) == std::vector<int>{ 5, 4, 0, 6 });
	for (const std::vector<int> & invalid : std::vector<std::vector<int>>{ { 0, 0, 1, 2 }, { 0, 1, 2 }, { 0, 1, 2, 7 }, { -1, 1, 2, 3 }, { 0, 1, 2, 3, 4 } })
		CHECK(validSpellChoice(guerrier, invalid) == defaultSpells(guerrier));

	std::mt19937 rng(3);
	for (int i = 0; i < 20; i++)
	{
		std::vector<int> random = randomSpellChoice(guerrier, rng);
		CHECK(validSpellChoice(guerrier, random) == random);
		CHECK(std::is_sorted(random.begin(), random.end()));
	}

	// Le Guerrier emporte Tourbillon (emplacement 0) et Cri de guerre ; le choix de l'Archer n'est pas valable.
	Arena arena({ { GUERRIER, { 5, 5 } } }, { { ARCHER, { 5, 6 } } }, openMap(), 1, { { 4, 5, 0, 1 }, { 9, 9, 9, 9 } });
	CHECK(arena.fighter(0).spells == std::vector<int>{ 4, 5, 0, 1 });
	CHECK(arena.fighter(1).spells == std::vector<int>{ 0, 1, 2, 3 });
	REQUIRE(spellOf(gameData(), arena.fighter(0), 0) != nullptr);
	CHECK(spellOf(gameData(), arena.fighter(0), 0)->id == "tourbillon");
	CHECK(spellOf(gameData(), arena.fighter(0), 4) == nullptr);
	CHECK(fighterSpells(gameData(), arena.fighter(0)).size() == 4);

	// Les clients reçoivent le choix ; un ancien état (sans "spells") garde les 4 premiers sorts de la classe.
	BattleState mirror;
	BattleMap mirrorMap;
	BattleMirror::applySnapshot(mirror, mirrorMap, arena.engine->snapshot(-1, arena.now));
	CHECK(mirror.findFighter(0)->spells == std::vector<int>{ 4, 5, 0, 1 });
	Fighter legacy = arena.fighter(0);
	legacy.spells.clear();
	CHECK(spellOf(gameData(), legacy, 0)->id == "taillade");
	CHECK(spellOf(gameData(), legacy, 4) == nullptr);

	// L'emplacement 0 lance bien Tourbillon.
	arena.playUntilTurnOf(0);
	int hp = arena.fighter(1).hp;
	REQUIRE(arena.engine->cast(0, 0, { 5, 5 }, arena.now).ok);
	CHECK(arena.fighter(1).hp < hp);
}

TEST_CASE("Tourbillon hits adjacent enemies only and Cri de guerre boosts nearby allies")
{
	Arena arena({ { GUERRIER, { 5, 5 } }, { MAGE, { 4, 5 } }, { ARCHER, { 8, 5 } } },
		{ { PROTECTEUR, { 6, 5 } }, { ARCHER, { 5, 4 } }, { MAGE, { 5, 7 } } }, openMap(), 1, { { 4, 5, 0, 1 } });
	arena.playUntilTurnOf(0);
	int ally = arena.fighter(1).hp;
	int adjacent1 = arena.fighter(3).hp;
	int adjacent2 = arena.fighter(4).hp;
	int far = arena.fighter(5).hp;
	REQUIRE(arena.engine->cast(0, arena.slotOf(0, "tourbillon"), { 5, 5 }, arena.now).ok);
	CHECK(arena.fighter(3).hp < adjacent1);
	CHECK(arena.fighter(4).hp < adjacent2);
	CHECK(arena.fighter(5).hp == far);
	CHECK(arena.fighter(1).hp == ally);

	int power = effectiveStat(arena.state(), gameData(), arena.fighter(1), Stat::POWER);
	int archerPower = effectiveStat(arena.state(), gameData(), arena.fighter(2), Stat::POWER);
	REQUIRE(arena.engine->cast(0, arena.slotOf(0, "cri_de_guerre"), { 5, 5 }, arena.now).ok);
	CHECK(effectiveStat(arena.state(), gameData(), arena.fighter(1), Stat::POWER) == power + 20);
	// L'Archer allié est à 3 cases : hors de portée du cri.
	CHECK(effectiveStat(arena.state(), gameData(), arena.fighter(2), Stat::POWER) == archerPower);
}

TEST_CASE("Pluie de fleches hits an area and the trap immobilises and entangles")
{
	Arena arena({ { ARCHER, { 1, 5 } } }, { { GUERRIER, { 7, 5 } }, { MAGE, { 7, 6 } }, { PROTECTEUR, { 9, 9 } } }, openMap(), 1, { { 4, 5, 0, 1 } });
	arena.playUntilTurnOf(0);
	int a = arena.fighter(1).hp;
	int b = arena.fighter(2).hp;
	int c = arena.fighter(3).hp;
	REQUIRE(arena.engine->cast(0, arena.slotOf(0, "pluie_de_fleches"), { 7, 5 }, arena.now).ok);
	CHECK(arena.fighter(1).hp < a);
	CHECK(arena.fighter(2).hp < b);
	CHECK(arena.fighter(3).hp == c);

	Arena trap({ { ARCHER, { 1, 5 } } }, { { GUERRIER, { 5, 5 } } }, openMap(), 1, { { 5, 0, 1, 2 } });
	trap.playUntilTurnOf(0);
	CHECK_FALSE(trap.engine->cast(0, trap.slotOf(0, "piege"), { 5, 5 }, trap.now).ok);	// Case occupée
	REQUIRE(trap.engine->cast(0, trap.slotOf(0, "piege"), { 4, 5 }, trap.now).ok);

	// Le Guerrier finit son tour sur le piège : il se déclenche au début de son tour suivant.
	trap.playUntilTurnOf(1);
	REQUIRE(trap.engine->move(1, { { 4, 5 } }, trap.now).ok);
	REQUIRE(trap.engine->endTurn(1, trap.now).ok);
	int hp = trap.fighter(1).hp;
	trap.playUntilTurnOf(1);
	CHECK(trap.fighter(1).hp < hp);
	CHECK(trap.fighter(1).mp == 0);
	CHECK(trap.fighter(1).hasState("entrave"));
}

TEST_CASE("Terrain spells raise walls of destructible blocks")
{
	// Mur de glace : 3 blocs en travers de la direction du lancer, qui bloquent le passage et la vue.
	Arena arena({ { MAGE, { 2, 5 } } }, { { GUERRIER, { 9, 5 } } }, openMap(), 1, { { 6, 0, 1, 2 } });
	arena.playUntilTurnOf(0);
	REQUIRE(arena.engine->cast(0, arena.slotOf(0, "mur_de_glace"), { 5, 5 }, arena.now).ok);
	const BattleState & state = arena.state();
	REQUIRE(state.blocks.size() == 3);
	for (const Cell & cell : std::vector<Cell>{ { 5, 4 }, { 5, 5 }, { 5, 6 } })
	{
		const Block * block = state.blockAt(cell);
		REQUIRE(block != nullptr);
		CHECK(block->hp == 30);
		CHECK(block->maxHp == 30);
		CHECK(block->team == 1);
		CHECK(block->blocksMove);
		CHECK(block->blocksSight);
	}
	CHECK_FALSE(hasLineOfSight(state, arena.map, { 2, 5 }, { 9, 5 }));
	CHECK_FALSE(cellWalkable(state, arena.map, { 5, 5 }));
	for (const Cell & step : findPath(state, arena.map, arena.fighter(1), { 3, 5 }))
		CHECK(state.blockAt(step) == nullptr);

	// Un sort de dégâts peut viser un bloc, même de son équipe ; l'aperçu le montre ; le bilan ne
	// compte pas ces dégâts.
	int eclair = arena.slotOf(0, "eclair");
	int uid = state.blockAt({ 5, 5 })->uid;
	std::vector<TargetPreview> previews = previewSpell(state, arena.map, gameData(), 0, eclair, { 5, 5 });
	bool previewed = false;
	for (const TargetPreview & preview : previews)
		previewed = previewed || (preview.blockUid == uid && preview.minDamage > 0 && !preview.koPossible);
	CHECK(previewed);
	int dealt = arena.fighter(0).record.dealt;
	REQUIRE(arena.engine->cast(0, eclair, { 5, 5 }, arena.now).ok);
	CHECK(state.blockAt({ 5, 5 })->hp < 30);
	CHECK(arena.fighter(0).record.dealt == dealt);
	CHECK(arena.eventsOfType("blockhit").size() == 1);

	// Le mur dure 2 tours du lanceur.
	REQUIRE(arena.engine->endTurn(0, arena.now).ok);
	arena.playUntilTurnOf(0);
	CHECK(state.blocks.size() == 3);
	REQUIRE(arena.engine->endTurn(0, arena.now).ok);
	arena.playUntilTurnOf(0);
	CHECK(state.blocks.empty());
}

TEST_CASE("A wall block can be broken, and is removed with its caster")
{
	// Éboulis : un rocher de 40 PV ; on ne le pose pas sur une case occupée.
	Arena duel({ { GUERRIER, { 2, 5 } } }, { { ARCHER, { 9, 5 } } }, openMap(), 1, { { 6, 0, 1, 2 } });
	duel.playUntilTurnOf(0);
	CHECK_FALSE(duel.engine->cast(0, duel.slotOf(0, "eboulis"), { 2, 5 }, duel.now).ok);
	REQUIRE(duel.engine->cast(0, duel.slotOf(0, "eboulis"), { 3, 5 }, duel.now).ok);
	REQUIRE(duel.state().blocks.size() == 1);
	CHECK(duel.state().blockAt({ 3, 5 })->hp == 40);

	// Les sorts de dégâts visent un bloc, pas les soins.
	const SpellDef & soin = gameData().findClass(PROTECTEUR)->spells[1];
	CHECK_FALSE(checkTarget(duel.state(), duel.map, gameData(), duel.fighter(0), soin, { 3, 5 }).empty());
	int taillade = duel.slotOf(0, "taillade");
	CHECK(checkTarget(duel.state(), duel.map, gameData(), duel.fighter(0), *spellOf(gameData(), duel.fighter(0), taillade), { 3, 5 }).empty());

	// Trois coups de Taillade (16 à 19) cassent le rocher : le passage s'ouvre.
	REQUIRE(duel.engine->cast(0, taillade, { 3, 5 }, duel.now).ok);
	REQUIRE(duel.engine->endTurn(0, duel.now).ok);
	duel.playUntilTurnOf(0);
	REQUIRE(duel.engine->cast(0, taillade, { 3, 5 }, duel.now).ok);
	duel.engine->flushEvents();
	REQUIRE(duel.engine->cast(0, taillade, { 3, 5 }, duel.now).ok);
	std::vector<nlohmann::json> removed = duel.eventsOfType("block-");
	REQUIRE(removed.size() == 1);
	CHECK(removed[0]["reason"] == "destroyed");
	CHECK(duel.state().blocks.empty());
	CHECK(cellWalkable(duel.state(), duel.map, { 3, 5 }));

	// Les blocs d'un combattant hors combat disparaissent.
	Arena fallen({ { GUERRIER, { 2, 5 } } }, { { ARCHER, { 9, 5 } } }, openMap(), 1, { { 6, 0, 1, 2 } });
	fallen.playUntilTurnOf(0);
	REQUIRE(fallen.engine->cast(0, fallen.slotOf(0, "eboulis"), { 2, 4 }, fallen.now).ok);
	BattleState state = fallen.state();
	state.findFighter(0)->hp = 1;
	BattleEngine engine(gameData(), fallen.map, state, 1);
	REQUIRE(engine.endTurn(0, 0).ok);
	REQUIRE(engine.getState().activeFighterId() == 1);
	REQUIRE(engine.cast(1, 0, { 2, 5 }, 0).ok);	// Tir précis
	CHECK_FALSE(engine.getState().findFighter(0)->alive);
	CHECK(engine.getState().blocks.empty());
}

TEST_CASE("Pushing a fighter into a wall damages the wall")
{
	Arena arena({ { ARCHER, { 2, 5 } } }, { { GUERRIER, { 4, 5 } } }, openMap(), 1, { { 2, 6, 0, 1 } });
	arena.playUntilTurnOf(0);
	BattleState state = arena.state();
	Block rock;
	rock.uid = 900;
	rock.group = 899;
	rock.casterId = 1;
	rock.team = 2;
	rock.spellId = "eboulis";
	rock.name = "Éboulis";
	rock.cell = { 6, 5 };
	rock.hp = 40;
	rock.maxHp = 40;
	rock.remainingTurns = 3;
	state.blocks.push_back(rock);
	BattleEngine engine(gameData(), arena.map, state, 1);

	// Flèche de recul : 3 cases, bloquée au bout d'une case par le rocher (2 cases non parcourues).
	REQUIRE(engine.cast(0, arena.slotOf(0, "fleche_recul"), { 4, 5 }, 0).ok);
	CHECK(engine.getState().findFighter(1)->position == Cell{ 5, 5 });
	CHECK(engine.getState().findBlock(900)->hp == 40 - 2 * gameData().rules.collisionDamageToHit);
}

TEST_CASE("The sacred veil hides from view but lets fighters through")
{
	Arena veil({ { PROTECTEUR, { 2, 5 } } }, { { ARCHER, { 9, 5 } } }, openMap(), 1, { { 6, 0, 1, 2 } });
	veil.playUntilTurnOf(0);
	REQUIRE(veil.engine->cast(0, veil.slotOf(0, "voile_sacre"), { 4, 5 }, veil.now).ok);
	const BattleState & state = veil.state();
	REQUIRE(state.blocks.size() == 3);
	CHECK(cellWalkable(state, veil.map, { 4, 5 }));
	CHECK(cellBlocksSight(state, veil.map, { 4, 5 }));
	CHECK_FALSE(hasLineOfSight(state, veil.map, { 2, 5 }, { 9, 5 }));

	// On s'arrête dans le voile ; un tir sur le Protecteur abîme aussi le voile.
	REQUIRE(veil.engine->move(0, { { 3, 5 }, { 4, 5 } }, veil.now).ok);
	REQUIRE(veil.engine->endTurn(0, veil.now).ok);
	veil.playUntilTurnOf(1);
	int hp = veil.fighter(0).hp;
	REQUIRE(veil.engine->cast(1, veil.slotOf(1, "tir_precis"), { 4, 5 }, veil.now).ok);
	CHECK(veil.fighter(0).hp < hp);
	CHECK(state.blockAt({ 4, 5 })->hp < 25);
}

TEST_CASE("Bonus orbs appear on symmetric central cells and are picked up on the way")
{
	// Sans le réglage : jamais d'orbe.
	Arena plain({ { GUERRIER, { 0, 4 } } }, { { ARCHER, { 12, 8 } } }, openMap(13));
	for (int turn = 0; turn < 12; turn++)
		REQUIRE(plain.engine->endTurn(plain.active(), plain.now).ok);
	CHECK(plain.state().orbs.empty());

	// Avec : au tour 3, sur une paire de cases symétriques de la zone centrale.
	Arena arena({ { GUERRIER, { 0, 4 } } }, { { ARCHER, { 12, 8 } } }, openMap(13), 1, {}, true);
	CHECK(arena.state().bonuses);
	while (arena.state().round < gameData().bonuses.firstRound)
		REQUIRE(arena.engine->endTurn(arena.active(), arena.now).ok);
	const std::vector<Orb> & orbs = arena.state().orbs;
	REQUIRE(!orbs.empty());
	CHECK(gameData().findOrb(orbs[0].kind) != nullptr);
	std::vector<std::vector<Cell>> spots = orbSpots(arena.map);
	bool known = false;
	for (const std::vector<Cell> & spot : spots)
	{
		bool same = spot.size() == orbs.size();
		for (const Orb & orb : orbs)
			same = same && std::find(spot.begin(), spot.end(), orb.cell) != spot.end();
		known = known || same;
		if (spot.size() == 2)
			CHECK(spot[0].x + spot[1].x == 12);	// Carte symétrique : (x, y) et (12 - x, 8 - y + 4)
	}
	CHECK(known);
}

TEST_CASE("Picking up an orb applies its effect")
{
	Arena arena({ { GUERRIER, { 2, 5 } } }, { { ARCHER, { 12, 5 } } }, openMap(), 1, {}, true);
	arena.playUntilTurnOf(0);
	BattleState state = arena.state();
	Fighter & warrior = *state.findFighter(0);
	warrior.hp -= 30;
	int ap = warrior.ap;
	state.orbs.push_back({ 900, "soin", { 3, 5 } });
	state.orbs.push_back({ 901, "energie", { 4, 5 } });
	state.orbs.push_back({ 902, "protection", { 8, 5 } });
	BattleEngine engine(gameData(), arena.map, state, 1);

	// Deux orbes ramassés au passage (soin, puis énergie).
	REQUIRE(engine.move(0, { { 3, 5 }, { 4, 5 }, { 5, 5 } }, 0).ok);
	const Fighter & after = *engine.getState().findFighter(0);
	CHECK(after.hp == warrior.hp + gameData().findOrb("soin")->heal);
	CHECK(after.ap == ap + gameData().findOrb("energie")->ap);
	REQUIRE(engine.getState().orbs.size() == 1);
	CHECK(after.record.healed == 0);	// Un soin d'orbe ne compte pas dans le bilan

	// L'Archer s'arrête sur l'orbe de protection : il le ramasse.
	BattleState next = engine.getState();
	next.findFighter(1)->position = { 10, 5 };
	BattleEngine archer(gameData(), arena.map, next, 1);
	REQUIRE(archer.endTurn(0, 0).ok);
	REQUIRE(archer.getState().activeFighterId() == 1);
	REQUIRE(archer.move(1, { { 9, 5 }, { 8, 5 } }, 0).ok);
	CHECK(archer.getState().orbs.empty());
	CHECK(archer.getState().findFighter(1)->shield == gameData().findOrb("protection")->shield);

	// Repoussé sur un orbe : ramassé à l'arrivée.
	BattleState pushed = arena.state();
	pushed.orbs.push_back({ 903, "soin", { 6, 5 } });
	pushed.findFighter(1)->position = { 3, 5 };
	pushed.findFighter(1)->hp -= 20;
	BattleEngine push(gameData(), arena.map, pushed, 1);
	REQUIRE(push.cast(0, 0, { 3, 5 }, 0).ok);	// Taillade, pas de poussée : rien
	CHECK(push.getState().orbs.size() == 1);
}

TEST_CASE("The hard AI plans a move then an attack, is deterministic, and retreats when wounded")
{
	BotOptions hard;
	hard.planner = true;

	// Archer affaibli hors de portée : le Guerrier avance puis frappe (Taillade), KO dans le tour.
	Arena arena({ { GUERRIER, { 2, 5 } } }, { { ARCHER, { 6, 5 } } }, openMap(), 1, { { 0, 2, 4, 5 } });
	arena.playUntilTurnOf(0);
	BattleState state = arena.state();
	state.findFighter(1)->hp = 12;
	BattleEngine engine(gameData(), arena.map, state, 1);

	// Même décision quel que soit l'état du générateur : pas de tirage.
	std::mt19937 first(1);
	std::mt19937 second(99);
	BotAction a = chooseBotAction(engine.getState(), arena.map, gameData(), 0, first, hard);
	BotAction b = chooseBotAction(engine.getState(), arena.map, gameData(), 0, second, hard);
	CHECK((a.kind == b.kind));
	CHECK(a.path == b.path);
	CHECK(a.slot == b.slot);
	CHECK((a.kind == BotAction::Kind::MOVE));

	std::mt19937 rng(1);
	for (int step = 0; step < 6 && engine.getState().findFighter(1)->alive && engine.getState().activeFighterId() == 0; step++)
	{
		BotAction action = chooseBotAction(engine.getState(), arena.map, gameData(), 0, rng, hard);
		if (action.kind == BotAction::Kind::MOVE)
			REQUIRE(engine.move(0, action.path, 0).ok);
		else if (action.kind == BotAction::Kind::CAST)
			REQUIRE(engine.cast(0, action.slot, action.target, 0).ok);
		else
			break;
	}
	CHECK_FALSE(engine.getState().findFighter(1)->alive);

	// Archer blessé sans PA, Guerrier tout proche : il s'éloigne hors d'atteinte.
	Arena retreat({ { ARCHER, { 5, 5 } } }, { { GUERRIER, { 8, 5 } } }, openMap(), 1);
	retreat.playUntilTurnOf(0);
	BattleState hurt = retreat.state();
	hurt.findFighter(0)->hp = 15;
	hurt.findFighter(0)->ap = 0;
	BotAction away = chooseBotAction(hurt, retreat.map, gameData(), 0, rng, hard);
	REQUIRE((away.kind == BotAction::Kind::MOVE));
	CHECK(manhattan(away.path.back(), { 8, 5 }) > manhattan({ 5, 5 }, { 8, 5 }));
}

TEST_CASE("Vague de flammes spares allies and Prison de glace freezes")
{
	// Vague vers la droite depuis (2, 5) : cases (3, 5), (4, 5) et (5, 5).
	Arena arena({ { MAGE, { 2, 5 } }, { GUERRIER, { 4, 5 } } }, { { ARCHER, { 3, 5 } }, { PROTECTEUR, { 5, 5 } }, { ARCHER, { 4, 7 } } },
		openMap(), 1, { { 4, 5, 0, 1 } });
	arena.playUntilTurnOf(0);
	int ally = arena.fighter(1).hp;
	int first = arena.fighter(2).hp;
	int last = arena.fighter(3).hp;
	int aside = arena.fighter(4).hp;
	REQUIRE(arena.engine->cast(0, arena.slotOf(0, "vague_de_flammes"), { 3, 5 }, arena.now).ok);
	CHECK(arena.fighter(1).hp == ally);
	CHECK(arena.fighter(2).hp < first);
	CHECK(arena.fighter(3).hp < last);
	CHECK(arena.fighter(4).hp == aside);
	CHECK(arena.fighter(2).hasState("brule"));
	CHECK(arena.fighter(3).hasState("brule"));
	CHECK_FALSE(arena.fighter(1).hasState("brule"));

	Arena prison({ { MAGE, { 2, 5 } } }, { { GUERRIER, { 6, 5 } } }, openMap(), 1, { { 5, 0, 1, 2 } });
	prison.playUntilTurnOf(0);
	REQUIRE(prison.engine->cast(0, prison.slotOf(0, "prison_de_glace"), { 6, 5 }, prison.now).ok);
	CHECK(prison.fighter(1).hasState("gele"));
	prison.playUntilTurnOf(1);
	CHECK(prison.fighter(1).mp == gameData().findClass(GUERRIER)->baseStats.get(Stat::MP) - 3);
}

TEST_CASE("Barriere blocks pushes and Lien de vie heals over the next turns")
{
	Arena arena({ { PROTECTEUR, { 2, 5 } } }, { { ARCHER, { 5, 5 } } }, openMap(), 1, { { 4, 5, 0, 1 } });
	arena.playUntilTurnOf(0);
	int resistance = effectiveStat(arena.state(), gameData(), arena.fighter(0), Stat::RESISTANCE);
	REQUIRE(arena.engine->cast(0, arena.slotOf(0, "barriere"), { 2, 5 }, arena.now).ok);
	REQUIRE(arena.engine->cast(0, arena.slotOf(0, "lien_de_vie"), { 2, 5 }, arena.now).ok);
	CHECK(arena.fighter(0).hasState("unmovable"));
	CHECK(effectiveStat(arena.state(), gameData(), arena.fighter(0), Stat::RESISTANCE) == resistance + 25);

	// Inamovible : la Flèche de recul blesse sans repousser.
	arena.playUntilTurnOf(1);
	REQUIRE(arena.engine->cast(1, spellIndex(ARCHER, "fleche_recul"), { 2, 5 }, arena.now).ok);
	CHECK(arena.fighter(0).position == Cell{ 2, 5 });
	int hp = arena.fighter(0).hp;
	CHECK(hp < arena.fighter(0).maxHp);

	// Lien de vie : soin au début de son tour suivant.
	arena.playUntilTurnOf(0);
	CHECK(arena.fighter(0).hp > hp);
}

TEST_CASE("Spell preview separates what the shield absorbs from the HP lost")
{
	Arena arena({ { GUERRIER, { 5, 5 } } }, { { ARCHER, { 5, 6 } } });
	arena.playUntilTurnOf(0);
	int taillade = spellIndex(GUERRIER, "taillade");

	BattleState shielded = arena.state();
	ActiveEffect shield;
	shield.uid = shielded.nextUid++;
	shield.type = EffectType::SHIELD;
	shield.value = 10;
	shield.remainingTurns = 2;
	shield.positive = true;
	Fighter & target = *shielded.findFighter(1);
	target.effects.push_back(shield);
	target.shield = 10;

	std::vector<TargetPreview> previews = previewSpell(shielded, arena.map, gameData(), 0, taillade, { 5, 6 });
	REQUIRE(previews.size() == 1);
	CHECK(previews[0].minAbsorbed == 10);
	CHECK(previews[0].maxAbsorbed == 10);
	CHECK(previews[0].minDamage > 10);

	// Sans bouclier : mêmes dégâts, rien d'absorbé.
	std::vector<TargetPreview> bare = previewSpell(arena.state(), arena.map, gameData(), 0, taillade, { 5, 6 });
	REQUIRE(bare.size() == 1);
	CHECK(bare[0].maxAbsorbed == 0);
	CHECK(bare[0].minDamage == previews[0].minDamage);
	CHECK(bare[0].maxDamage == previews[0].maxDamage);
}

TEST_CASE("Tournament talents add their bonuses and their start-of-fight effects")
{
	const GameData & data = gameData();
	REQUIRE(data.talents.size() == 10);
	REQUIRE(data.findTalent("garde") != nullptr);
	CHECK(data.findTalent("inconnu") == nullptr);

	CHECK(validTalentChoice(data, { "garde", "garde", "inconnu", "force", "elan" }, 2) == std::vector<std::string>{ "garde", "force" });
	std::mt19937 rng(5);
	std::vector<std::string> random = randomTalentChoice(data, 3, rng);
	CHECK(random.size() == 3);
	CHECK(validTalentChoice(data, random, 3) == random);
	CHECK(randomTalentChoice(data, 0, rng).empty());

	// Guerrier avec Robustesse, Garde et Élan (les doublons et inconnus sont ignorés) ; Archer sans talent.
	BattleMap map = openMap();
	map.startCells[1] = { { 2, 2 } };
	map.startCells[2] = { { 8, 8 } };
	BattleEngine engine(data, map, 1);
	engine.addFighter(1, GUERRIER, "A", {}, { "robustesse", "garde", "elan", "garde", "?" });
	engine.addFighter(2, ARCHER, "B");
	int baseHp = data.findClass(GUERRIER)->baseStats.get(Stat::MAX_HP);
	int baseMp = data.findClass(GUERRIER)->baseStats.get(Stat::MP);
	const Fighter & warrior = *engine.getState().findFighter(0);
	CHECK(warrior.talents == std::vector<std::string>{ "robustesse", "garde", "elan" });
	CHECK(warrior.maxHp == baseHp + 15);
	CHECK(warrior.hp == baseHp + 15);
	CHECK(warrior.shield == 0);

	engine.startPlacement(0);
	REQUIRE(engine.setReady(0, true, 0).ok);
	REQUIRE(engine.setReady(1, true, 0).ok);
	REQUIRE((engine.getState().phase == BattlePhase::FIGHT));
	CHECK(warrior.shield == 15);
	// Un bouclier de talent ne compte pas dans le bilan des boucliers donnés.
	CHECK(warrior.record.shielded == 0);

	// Élan : +1 PM pendant le premier tour seulement.
	auto playUntilWarrior = [&engine]() {
		for (int guard = 0; engine.getState().activeFighterId() != 0; guard++)
		{
			REQUIRE(guard < 5);
			REQUIRE(engine.endTurn(engine.getState().activeFighterId(), 0).ok);
		}
	};
	playUntilWarrior();
	CHECK(warrior.mp == baseMp + 1);
	REQUIRE(engine.endTurn(0, 0).ok);
	playUntilWarrior();
	CHECK(warrior.mp == baseMp);

	// Les clients reçoivent les talents.
	BattleState mirror;
	BattleMap mirrorMap;
	BattleMirror::applySnapshot(mirror, mirrorMap, engine.snapshot(-1, 0));
	CHECK(mirror.findFighter(0)->talents == warrior.talents);
}

TEST_CASE("Appearances unlock from account progress and keep the team hue")
{
	const GameData & data = gameData();
	PlayerProgress progress;
	std::vector<std::string> unlocked = unlockedAppearances(data, progress);
	REQUIRE(unlocked.size() == 1);
	CHECK(unlocked[0] == "classique");

	// Énigmes, haut fait, victoires, MVP : chacun débloque sa propre apparence.
	progress.puzzles = { "a", "b" };
	CHECK(allowedAppearance(data, progress, "givre").empty());
	progress.puzzles.insert("c");
	CHECK(allowedAppearance(data, progress, "givre") == "givre");
	CHECK(allowedAppearance(data, progress, "braise").empty());
	progress.achievements.insert("first_blood");
	CHECK(allowedAppearance(data, progress, "braise") == "braise");
	progress.wins = 4;
	CHECK(allowedAppearance(data, progress, "nuit").empty());
	progress.wins = 5;
	CHECK(allowedAppearance(data, progress, "nuit") == "nuit");
	progress.mvp = 1;
	CHECK(allowedAppearance(data, progress, "or") == "or");
	CHECK(allowedAppearance(data, progress, "inconnue").empty());
	unlocked = unlockedAppearances(data, progress);
	CHECK(unlocked == std::vector<std::string>({ "classique", "givre", "braise", "nuit", "or" }));

	// Chaque apparence verrouillée dit comment la débloquer.
	for (const AppearanceDef & appearance : data.appearances)
		CHECK_FALSE(unlockCondition(appearance).empty());
	CHECK(unlockCondition(*data.findAppearance("givre")) == u8"3 énigmes réussies");

	// La teinte reste celle de l'équipe : bleu reste bleu, rouge reste rouge.
	const int blue[3] = { 0, 166, 214 };
	const int red[3] = { 120, 17, 17 };
	for (const AppearanceDef & appearance : data.appearances)
	{
		int out[3];
		armorColor(&appearance, blue, out);
		CHECK(out[2] >= out[0]);
		CHECK(out[1] >= out[0]);
		armorColor(&appearance, red, out);
		CHECK(out[0] >= out[1]);
		CHECK(out[0] >= out[2]);
	}
	int same[3];
	armorColor(nullptr, blue, same);
	CHECK(same[0] == 0);
	CHECK(same[1] == 166);
	CHECK(same[2] == 214);
}

TEST_CASE("A fighter's appearance travels through the snapshot to the mirror")
{
	BattleMap map = openMap();
	map.startCells[1].push_back({ 2, 2 });
	map.startCells[2].push_back({ 10, 10 });
	BattleEngine engine(gameData(), map, 3);
	int first = engine.addFighter(1, MAGE, "Givre", {}, {}, "givre");
	int second = engine.addFighter(2, ARCHER, "Inconnue", {}, {}, "pas-une-apparence");
	CHECK(engine.getState().findFighter(first)->appearance == "givre");
	CHECK(engine.getState().findFighter(second)->appearance.empty());

	BattleState mirror;
	BattleMap mirrorMap;
	BattleMirror::applySnapshot(mirror, mirrorMap, engine.snapshot(0, 0));
	CHECK(mirror.findFighter(first)->appearance == "givre");
	CHECK(mirror.findFighter(second)->appearance.empty());
}

namespace
{
	// Premier chemin où deux documents JSON diffèrent (vide : identiques).
	std::string firstDifference(const nlohmann::json & a, const nlohmann::json & b, const std::string & path = "")
	{
		if (a.type() != b.type())
			return path + " (type)";
		if (a.is_object())
		{
			for (auto it = a.begin(); it != a.end(); ++it)
			{
				if (!b.contains(it.key()))
					return path + "/" + it.key() + " (absent)";
				std::string inner = firstDifference(it.value(), b[it.key()], path + "/" + it.key());
				if (!inner.empty())
					return inner;
			}
			return a.size() == b.size() ? std::string() : path + " (clés en trop)";
		}
		if (a.is_array())
		{
			if (a.size() != b.size())
				return path + " (taille)";
			for (std::size_t i = 0; i < a.size(); i++)
			{
				std::string inner = firstDifference(a[i], b[i], path + "/" + std::to_string(i));
				if (!inner.empty())
					return inner;
			}
			return std::string();
		}
		return a == b ? std::string() : path + " : " + a.dump() + " / " + b.dump();
	}
}

TEST_CASE("The state rebuilt by the mirror serializes exactly like the engine snapshot")
{
	std::mt19937 rng(77);
	int classIds[] = { MAGE, ARCHER, PROTECTEUR, GUERRIER };
	for (int battle = 0; battle < 40; battle++)
	{
		CAPTURE(battle);
		BattleMap map = openMap(13);
		for (int i = 0; i < 10; i++)
			map.setCell({ (int)(rng() % 9) + 2, (int)(rng() % 13) }, false, rng() % 2 == 0);
		std::vector<std::vector<int>> spells;
		int picked[4];
		for (int i = 0; i < 4; i++)
		{
			picked[i] = classIds[rng() % 4];
			spells.push_back(randomSpellChoice(*gameData().findClass(picked[i]), rng));
		}
		Arena arena({ { picked[0], { 0, 4 } }, { picked[1], { 0, 8 } } }, { { picked[2], { 12, 4 } }, { picked[3], { 12, 8 } } },
			map, (std::uint32_t)(1000 + battle), spells, battle % 2 == 1);

		// Comme une rediffusion : l'état de départ, puis les lots d'événements.
		BattleState mirror;
		BattleMap mirrorMap;
		BattleMirror::applySnapshot(mirror, mirrorMap, nlohmann::json::parse(arena.engine->snapshot(-1, arena.now).dump()));
		std::uint64_t seq = 0;
		auto compare = [&]() {
			nlohmann::json batch = nlohmann::json::parse(arena.engine->flushEvents().dump());
			seq = batch.value("seq", seq);
			for (const nlohmann::json & event : batch["ev"])
				BattleMirror::applyEvent(mirror, event);
			nlohmann::json expected = arena.engine->snapshot(-1, arena.now);
			nlohmann::json rebuilt = statejson::snapshot(mirror, mirrorMap, seq, -1, expected.value("ms", (std::int64_t)0));
			// Les bilans (dégâts, soins…) ne sont transmis qu'avec la fin du combat ("end"), et les
			// ressources d'un combattant hors combat ne s'affichent plus.
			for (nlohmann::json * fighters : { &expected["fighters"], &rebuilt["fighters"] })
			{
				for (nlohmann::json & fighter : *fighters)
				{
					if (!arena.engine->isOver())
						fighter.erase("record");
					if (!fighter.value("alive", true))
					{
						for (const char * key : { "ap", "mp", "casts", "cooldowns", "effects" })
							fighter.erase(key);
					}
				}
			}
			std::string difference = firstDifference(expected, rebuilt);
			INFO(batch.dump());
			REQUIRE(difference == "");
		};

		for (int action = 0; action < 3000 && !arena.engine->isOver(); action++)
		{
			compare();
			int id = arena.active();
			const Fighter & fighter = arena.fighter(id);
			int choice = rng() % 3;
			if (choice == 0 && fighter.mp > 0)
			{
				std::vector<Cell> reachable = reachableCells(arena.state(), arena.engine->getMap(), fighter);
				if (!reachable.empty())
				{
					arena.engine->move(id, findPath(arena.state(), arena.engine->getMap(), fighter, reachable[rng() % reachable.size()]), arena.now);
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
						arena.engine->cast(id, index, cells[rng() % cells.size()], arena.now);
						continue;
					}
				}
			}
			arena.now += 1000;
			arena.engine->endTurn(id, arena.now);
		}
		compare();
	}
}

TEST_CASE("Highlights of a recorded battle: double KO, combo and big hit, with their windows")
{
	Arena arena({ { MAGE, { 1, 1 } }, { GUERRIER, { 1, 3 } } }, { { ARCHER, { 8, 1 } }, { PROTECTEUR, { 8, 3 } } });
	nlohmann::json start = nlohmann::json::parse(arena.engine->snapshot(-1, arena.now).dump());
	using nlohmann::json;
	std::vector<std::pair<std::int64_t, json>> batches = {
		{ 0, { { "seq", 1 }, { "ev", json::array({ { { "t", "turn" }, { "f", 0 }, { "round", 1 } } }) } } },
		{ 1000, { { "seq", 2 }, { "ev", json::array({ { { "t", "damage" }, { "f", 2 }, { "src", 0 }, { "amount", 30 }, { "absorbed", 0 }, { "hp", 50 }, { "maxHp", 80 }, { "shield", 0 } } }) } } },
		{ 6000, { { "seq", 3 }, { "ev", json::array({ { { "t", "combo" }, { "f", 3 }, { "src", 1 }, { "name", "Brise-glace" } } }) } } },
		{ 12000, { { "seq", 4 }, { "ev", json::array({
			{ { "t", "turn" }, { "f", 0 }, { "round", 2 } },
			{ { "t", "damage" }, { "f", 2 }, { "src", 0 }, { "amount", 20 }, { "absorbed", 0 }, { "hp", 0 }, { "maxHp", 80 }, { "shield", 0 } },
			{ { "t", "death" }, { "f", 2 } },
			{ { "t", "damage" }, { "f", 3 }, { "src", 0 }, { "amount", 18 }, { "absorbed", 0 }, { "hp", 0 }, { "maxHp", 100 }, { "shield", 0 } },
			{ { "t", "death" }, { "f", 3 } } }) } } },
		{ 13000, { { "seq", 5 }, { "ev", json::array({ { { "t", "end" }, { "winner", 1 }, { "reason", "KO" }, { "round", 2 } } }) } } },
	};
	std::vector<Highlight> highlights = detectHighlights(start, batches, "Les Bleus", "Les Rouges");
	REQUIRE(highlights.size() == 3);
	CHECK(highlights[0].kind == "big_hit");
	CHECK(highlights[0].from == 0);
	CHECK(highlights[0].to == 1);
	CHECK(highlights[1].kind == "combo");
	CHECK(highlights[1].title.find("Brise-glace") != std::string::npos);
	CHECK(highlights[1].from == 2);
	CHECK(highlights[1].to == 2);
	CHECK(highlights[2].kind == "double_ko");
	CHECK(highlights[2].title.find(arena.fighter(0).name) != std::string::npos);
	CHECK(highlights[2].from == 3);
	CHECK(highlights[2].to == 4);
	CHECK(highlights[2].score > highlights[1].score);

	// Un seul temps fort demandé : le mieux noté.
	std::vector<Highlight> best = detectHighlights(start, batches, "Les Bleus", "Les Rouges", 1);
	REQUIRE(best.size() == 1);
	CHECK(best[0].kind == "double_ko");
}
