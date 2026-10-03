#pragma once

#include <string>
#include <vector>

#include "BattleState.h"

namespace tw
{
	namespace battle
	{
		// Règles de consultation partagées par le serveur (validation) et le client
		// (prévisualisation). Elles ne modifient jamais l'état.

		// Caractéristique effective : base + effets + auras des alliés.
		int effectiveStat(const BattleState & state, const GameData & data, const Fighter & fighter, Stat stat);

		// Dégâts d'un tick de poison ou de brûlure : jet "value", puissance du lanceur au moment du
		// lancer, résistance actuelle du porteur (bornée).
		int periodicDamage(const BattleState & state, const GameData & data, const Fighter & bearer, int value, int casterPower);

		// Ligne de vue entre deux cellules (extrémités exclues). Les obstacles et les
		// combattants vivants bloquent la vue.
		bool hasLineOfSight(const BattleState & state, const BattleMap & map, const Cell & from, const Cell & to);

		const SpellDef * spellOf(const GameData & data, const Fighter & fighter, int spellIndex);
		int effectiveMaxRange(const BattleState & state, const GameData & data, const Fighter & fighter, const SpellDef & spell);

		// Cellules de la zone de lancer (forme et portée, sans la ligne de vue).
		std::vector<Cell> launchCells(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & fighter, const SpellDef & spell);

		// Pourquoi le combattant ne peut pas lancer ce sort ce tour-ci (PA, relance, lancers par tour).
		// Chaîne vide si les ressources sont suffisantes.
		std::string checkSpellResources(const Fighter & fighter, const SpellDef & spell);

		// Pourquoi la cellule n'est pas une cible valide (zone, ligne de vue, contrainte de cible).
		std::string checkTarget(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & fighter, const SpellDef & spell, const Cell & target);

		// Cellules ciblables pour la prévisualisation.
		std::vector<Cell> castableCells(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & fighter, const SpellDef & spell);

		// Cellules touchées par la zone d'effet (dans la carte).
		std::vector<Cell> impactCells(const BattleMap & map, const Cell & caster, const Cell & target, const ZoneDef & zone);

		// Déplacement : cellules atteignables avec les PM actuels (sans tenir compte du tacle).
		std::vector<Cell> reachableCells(const BattleState & state, const BattleMap & map, const Fighter & fighter);
		// Plus court chemin (cellule de départ exclue), vide si inaccessible.
		std::vector<Cell> findPath(const BattleState & state, const BattleMap & map, const Fighter & fighter, const Cell & target);

		struct TackleLoss
		{
			int stepIndex = 0;	// Avant le pas numéro stepIndex
			int lostMp = 0;
			int lostAp = 0;
		};

		struct MovePreview
		{
			std::string error;					// Chemin invalide
			std::vector<Cell> path;				// Chemin réellement parcouru (raccourci par le tacle)
			int mpAfter = 0;
			int apAfter = 0;
			std::vector<TackleLoss> tackles;
		};

		// Simule un déplacement (tacle compris) sans modifier l'état.
		MovePreview previewMove(const BattleState & state, const BattleMap & map, const GameData & data, const Fighter & fighter, const std::vector<Cell> & path);

		// Direction dominante de "from" vers "to" (pas en x et y : -1, 0 ou 1).
		Cell directionBetween(const Cell & from, const Cell & to);
	}
}
