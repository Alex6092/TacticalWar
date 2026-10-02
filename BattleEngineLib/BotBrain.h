#pragma once

#include <random>
#include <vector>

#include "BattleState.h"
#include "GameData.h"

namespace tw
{
	namespace battle
	{
		// Décision d'une IA simple pour le combattant actif : lancer le sort le plus utile (dégâts
		// sur les ennemis, soins et boucliers sur les alliés blessés), sinon se rapprocher de
		// l'ennemi le plus proche, sinon passer son tour. Utilisée par le bot réseau et par la
		// simulation d'équilibrage ; elle n'applique que des actions légales.
		struct BotAction
		{
			enum class Kind { CAST, MOVE, END_TURN };

			Kind kind = Kind::END_TURN;
			int slot = -1;
			Cell target;
			std::vector<Cell> path;
		};

		BotAction chooseBotAction(const BattleState & state, const BattleMap & map, const GameData & data, int fighterId, std::mt19937 & rng);
	}
}
