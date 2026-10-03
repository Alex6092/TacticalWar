#pragma once

#include <string>
#include <vector>

#include "BattleState.h"
#include "GameData.h"

namespace tw
{
	namespace battle
	{
		// Aperçu d'un sort pour un combattant touché : fourchette entre les jets les plus faibles et
		// les plus forts.
		struct TargetPreview
		{
			int fighterId = -1;
			int minDamage = 0;			// Dégâts subis, bouclier compris
			int maxDamage = 0;
			int minAbsorbed = 0;		// Part des dégâts absorbée par le bouclier de la cible
			int maxAbsorbed = 0;
			int minHeal = 0;			// PV récupérés
			int maxHeal = 0;
			int minShield = 0;			// Bouclier reçu
			int maxShield = 0;
			bool koPossible = false;	// Hors combat avec les jets les plus forts
			bool koCertain = false;		// Hors combat même avec les jets les plus faibles
			// Autres effets, en texte UTF-8 : « Poison 6/tour », « Ralenti (-1 PM) », « Repoussé ».
			std::vector<std::string> notes;
		};

		// Ce que ferait le sort de l'emplacement "slot", lancé par "casterId" sur la case "target" :
		// le moteur le lance sur une copie de l'état (mêmes règles que le serveur), une fois avec les
		// jets les plus faibles et une fois avec les plus forts. Vide si le sort ne peut pas être lancé.
		std::vector<TargetPreview> previewSpell(const BattleState & state, const BattleMap & map, const GameData & data,
			int casterId, int slot, const Cell & target);

		// Cellules que le combattant pourra atteindre à son prochain tour : ses PM d'un début de tour,
		// sans compter le tacle.
		std::vector<Cell> nextTurnReach(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & fighter);
	}
}
