#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "BattleEngine.h"

namespace tw
{
	namespace battle
	{
		// Énigme tactique (assets/puzzles/*.json) : une position imposée, et un objectif à atteindre pendant
		// les tours de l'équipe 1 (le joueur) : mettre hors combat toute l'équipe 2, qui ne joue pas.
		// Les sorts font leurs dégâts minimum (RollMode::MIN) : le résultat est toujours le même.
		struct PuzzleFighter
		{
			int team = 1;
			int classId = 0;
			std::string name;				// UTF-8
			Cell position;
			int hp = 0;						// 0 : PV max de la classe
			std::vector<int> spells;		// Vide : sorts par défaut de la classe
			std::vector<std::string> marks;	// Marques déjà posées ("gele", "entrave"...)
			int ap = -1;					// -1 : PA de la classe
			int mp = -1;
		};

		struct PuzzleAction
		{
			enum class Kind { MOVE, CAST, END };
			int fighter = 0;
			Kind kind = Kind::END;
			int slot = -1;
			Cell target;
			std::vector<Cell> path;
		};

		struct Puzzle
		{
			std::string id;
			int order = 0;					// Rang dans la liste des énigmes
			std::string title;				// UTF-8
			std::string goal;
			std::string hint;
			int mapId = 0;
			// L'équipe 1 joue dans l'ordre de la liste, puis l'équipe 2.
			std::vector<PuzzleFighter> fighters;
			// Solution de référence (vérifiée par les tests, jouée par la démonstration), et piège : une
			// suite d'actions tentante qui ne doit pas suffire (vérifiée par les tests).
			std::vector<PuzzleAction> solution;
			std::vector<PuzzleAction> trap;
		};

		bool parsePuzzle(const std::string & text, Puzzle & puzzle, std::string & error);

		// Combat prêt à jouer : combattants placés, marques posées, ordre de jeu (l'équipe 1 d'abord), tour
		// du premier combattant. Les combattants de l'équipe 1 après le premier sont « pilotés » : le
		// joueur les joue aussi.
		BattleState puzzleState(const GameData & data, const BattleMap & map, const Puzzle & puzzle);

		// Toute l'équipe 2 est hors combat.
		bool puzzleSolved(const BattleState & state);
		// Les tours de l'équipe 1 sont finis sans que l'objectif soit atteint.
		bool puzzleFailed(const BattleState & state);

		// Joue une action de la solution (tests, démonstration hors interface).
		ActionResult playPuzzleAction(BattleEngine & engine, const PuzzleAction & action, std::int64_t nowMs);
	}
}
