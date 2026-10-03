#include "Simulation.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <BattleEngine.h>
#include <BotBrain.h>
#include <EnvironmentManager.h>
#include <EnvironmentMap.h>
#include <GameData.h>

using namespace tw::battle;

namespace
{
	struct Tally
	{
		int games = 0;
		int wins = 0;

		double rate() const { return games > 0 ? 100.0 * wins / games : 0.0; }
	};

	std::string percent(double value)
	{
		std::ostringstream text;
		text << std::fixed << std::setprecision(1) << value << " %";
		return text.str();
	}
}

int runSimulation(int battles, int mapId, std::uint32_t seed, const std::string & dataPath, int zonePoints)
{
	GameData data;
	std::string error;
	if (!data.loadFromFile(dataPath, error))
	{
		std::cerr << "Données de jeu illisibles : " << error << std::endl;
		return 1;
	}
	if (data.classes.empty())
	{
		std::cerr << "Aucune classe dans gamedata.json." << std::endl;
		return 1;
	}

	// Cartes : celle demandée, ou toutes celles du pool de tournoi.
	std::vector<std::pair<int, BattleMap>> maps;
	tw::EnvironmentManager * manager = tw::EnvironmentManager::getInstance();
	for (int id : manager->getAlreadyExistingIds())
	{
		if (mapId != 0 && id != mapId)
			continue;
		std::unique_ptr<tw::Environment> environment(manager->loadEnvironment(id));
		if (environment != nullptr && (mapId != 0 || environment->isInTournamentPool()))
			maps.push_back({ id, battleMapFromEnvironment(environment.get()) });
	}
	if (maps.empty())
	{
		std::cerr << "Aucune carte trouvée dans ./assets/map." << std::endl;
		return 1;
	}

	std::mt19937 rng(seed);
	// Sorts emportés : tirés à part, pour que les classes et les combats d'une graine restent comparables.
	std::mt19937 spellRng(seed ^ 0x5eedu);
	std::map<std::string, Tally> bySpell;
	std::map<int, Tally> byClass;
	std::map<std::string, Tally> byComposition;
	std::map<int, Tally> byMapTeam1;
	std::map<std::string, int> endReasons;
	std::vector<int> rounds;
	double winnerHp = 0;
	int unfinished = 0;
	// Combinaisons déclenchées (nom -> nombre).
	std::map<std::string, int> combos;
	auto countCombos = [&combos](const nlohmann::json & batch) {
		for (const nlohmann::json & event : batch["ev"])
		{
			if (event.value("t", std::string()) == "combo")
				combos[event.value("name", std::string())]++;
		}
	};

	auto className = [&](int classId) {
		const ClassDef * classDef = data.findClass(classId);
		return classDef != nullptr ? classDef->name : std::to_string(classId);
	};

	for (int battle = 0; battle < battles; battle++)
	{
		const std::pair<int, BattleMap> & map = maps[battle % maps.size()];
		BattleEngine engine(data, map.second, seed * 7919u + (std::uint32_t)battle);

		int classes[4];
		for (int i = 0; i < 4; i++)
			classes[i] = data.classes[rng() % data.classes.size()].id;
		for (int i = 0; i < 4; i++)
			engine.addFighter(i < 2 ? 1 : 2, classes[i], "IA " + std::to_string(i + 1), randomSpellChoice(*data.findClass(classes[i]), spellRng));

		// Placement au hasard sur les cases de départ (le placement automatique prend les cases
		// dans l'ordre de la carte, ce qui peut grouper une équipe et disperser l'autre).
		std::int64_t now = 0;
		if (zonePoints > 0)
			engine.enableZone(zonePoints);
		engine.startPlacement(now);
		for (int team = 1; team <= 2; team++)
		{
			std::vector<Cell> starts = map.second.startCells[team];
			std::shuffle(starts.begin(), starts.end(), rng);
			int next = 0;
			for (int id = 0; id < 4; id++)
			{
				if ((id < 2 ? 1 : 2) != team)
					continue;
				while (next < (int)starts.size() && !engine.place(id, starts[next], now).ok)
					next++;
				next++;
			}
		}
		for (int id = 0; id < 4; id++)
			engine.setReady(id, true, now);

		int lastActive = -1;
		int actionsThisTurn = 0;
		for (int step = 0; step < 20000 && !engine.isOver(); step++)
		{
			// Temps simulé : bien en dessous de la durée d'un tour, aucun tour ne passe par minuteur.
			now += 100;
			engine.tick(now);
			countCombos(engine.flushEvents());

			int active = engine.getState().activeFighterId();
			if (active < 0 || engine.getState().phase != BattlePhase::FIGHT)
				continue;
			if (active != lastActive)
			{
				lastActive = active;
				actionsThisTurn = 0;
			}

			BotAction action;
			if (actionsThisTurn++ < 12)
				action = chooseBotAction(engine.getState(), engine.getMap(), data, active, rng);

			ActionResult result;
			if (action.kind == BotAction::Kind::CAST)
				result = engine.cast(active, action.slot, action.target, now);
			else if (action.kind == BotAction::Kind::MOVE)
				result = engine.move(active, action.path, now);
			else
				result = engine.endTurn(active, now);

			if (!result.ok && action.kind != BotAction::Kind::END_TURN)
				engine.endTurn(active, now);
		}

		countCombos(engine.flushEvents());
		if (!engine.isOver())
		{
			unfinished++;
			continue;
		}

		const BattleState & state = engine.getState();
		int winner = state.winnerTeam;
		rounds.push_back(state.round);
		endReasons[toString(state.endReason)]++;
		winnerHp += engine.teamHpPercent(winner);

		for (int i = 0; i < 4; i++)
		{
			Tally & tally = byClass[classes[i]];
			tally.games++;
			if ((i < 2 ? 1 : 2) == winner)
				tally.wins++;
		}

		// Sorts emportés par chaque combattant.
		for (const Fighter & fighter : state.fighters)
		{
			for (const SpellDef * spell : fighterSpells(data, fighter))
			{
				Tally & tally = bySpell[className(fighter.classId) + " : " + spell->name];
				tally.games++;
				if (fighter.team == winner)
					tally.wins++;
			}
		}

		for (int team = 1; team <= 2; team++)
		{
			std::string a = className(classes[team == 1 ? 0 : 2]);
			std::string b = className(classes[team == 1 ? 1 : 3]);
			if (b < a)
				std::swap(a, b);
			Tally & tally = byComposition[a + " + " + b];
			tally.games++;
			if (team == winner)
				tally.wins++;
		}

		Tally & side = byMapTeam1[map.first];
		side.games++;
		if (winner == 1)
			side.wins++;
	}

	int finished = (int)rounds.size();
	std::cout << "\n=== Simulation : " << finished << " combats terminés sur " << battles;
	if (unfinished > 0)
		std::cout << " (" << unfinished << " interrompus)";
	std::cout << ", " << maps.size() << " carte(s)";
	if (zonePoints > 0)
		std::cout << ", zone à tenir (" << zonePoints << " points)";
	std::cout << " ===\n";
	std::cout << "IA simple (BotBrain) : les écarts importants signalent un déséquilibre,\n"
		<< "les petits écarts ne disent rien du jeu entre humains.\n";

	std::cout << "\nTaux de victoire par classe (part des combats gagnés par l'équipe du combattant) :\n";
	for (const auto & entry : byClass)
	{
		std::cout << "  " << std::left << std::setw(12) << className(entry.first) << std::right
			<< std::setw(8) << percent(entry.second.rate()) << "   (" << entry.second.games << " participations)\n";
	}

	std::cout << "\nSorts emportés (choix au hasard, 4 sur 6) : taux de victoire quand le sort est emporté :\n";
	for (const auto & entry : bySpell)
	{
		std::cout << "  " << std::left << std::setw(34) << entry.first << std::right
			<< std::setw(8) << percent(entry.second.rate()) << "   (" << entry.second.games << " fois)\n";
	}

	std::vector<std::pair<std::string, Tally>> compositions(byComposition.begin(), byComposition.end());
	std::sort(compositions.begin(), compositions.end(), [](const auto & a, const auto & b) { return a.second.rate() > b.second.rate(); });
	std::cout << "\nCompositions d'équipe :\n";
	for (const auto & entry : compositions)
	{
		std::cout << "  " << std::left << std::setw(26) << entry.first << std::right
			<< std::setw(8) << percent(entry.second.rate()) << "   (" << entry.second.games << " combats)\n";
	}

	if (finished > 0)
	{
		std::sort(rounds.begin(), rounds.end());
		double average = 0;
		for (int value : rounds)
			average += value;
		average /= finished;
		std::cout << "\nDurée : " << std::fixed << std::setprecision(1) << average << " tours en moyenne (médiane "
			<< rounds[finished / 2] << ", min " << rounds.front() << ", max " << rounds.back() << ")\n";
		std::cout << "PV restants du vainqueur : " << percent(winnerHp / finished) << " en moyenne\n";
		std::cout << "Fins de combat :";
		for (const auto & entry : endReasons)
			std::cout << "  " << entry.first << " " << percent(100.0 * entry.second / finished);
		std::cout << "\n";
	}

	if (finished > 0)
	{
		std::cout << "\nCombinaisons déclenchées (pour 100 combats) :";
		if (combos.empty())
			std::cout << " aucune";
		std::cout << "\n";
		for (const auto & entry : combos)
		{
			std::cout << "  " << std::left << std::setw(18) << entry.first << std::right << std::setw(8) << std::fixed << std::setprecision(1)
				<< 100.0 * entry.second / finished << "   (" << entry.second << " au total)\n";
		}
	}

	std::cout << "\nVictoires de l'équipe 1 par carte (50 % = départs équitables) :\n";
	for (const auto & entry : byMapTeam1)
	{
		std::cout << "  carte " << std::setw(3) << entry.first << " : " << std::setw(8) << percent(entry.second.rate())
			<< "   (" << entry.second.games << " combats)";
		if (zonePoints > 0)
		{
			// Zone de la carte : nombre de cases et distance de marche de chaque équipe.
			for (const auto & map : maps)
			{
				if (map.first != entry.first)
					continue;
				std::vector<Cell> zone = objectiveZone(map.second);
				int distances[3];
				zoneDistances(map.second, zone, distances);
				std::cout << "   zone " << (map.second.zoneCells.empty() ? "calculée" : "peinte") << " de " << zone.size()
					<< " cases, à " << distances[1] << " / " << distances[2] << " pas";
				if (!zone.empty())
					std::cout << ", centre (" << zone[0].x << ", " << zone[0].y << ")";
			}
		}
		std::cout << "\n";
	}
	return 0;
}
