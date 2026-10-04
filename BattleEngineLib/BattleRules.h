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

		// Case praticable : terrain praticable et pas de bloc de mur infranchissable (les combattants ne
		// sont pas comptés).
		bool cellWalkable(const BattleState & state, const BattleMap & map, const Cell & cell);
		// Case qui bloque la vue : obstacle du terrain, ou bloc de mur opaque.
		bool cellBlocksSight(const BattleState & state, const BattleMap & map, const Cell & cell);

		// Ligne de vue entre deux cellules (extrémités exclues). Les obstacles, les blocs de mur opaques
		// et les combattants vivants bloquent la vue.
		bool hasLineOfSight(const BattleState & state, const BattleMap & map, const Cell & from, const Cell & to);

		// Le sort fait des dégâts directs (DAMAGE, LIFESTEAL) : il peut viser un bloc de mur et abîme
		// ceux de sa zone.
		bool damagesBlocks(const SpellDef & spell);
		// Cases où un mur de ce sort se poserait : celles de la zone, praticables, sans combattant ni bloc.
		std::vector<Cell> wallCells(const BattleState & state, const BattleMap & map, const Cell & caster, const Cell & target, const ZoneDef & zone);

		// Sorts au choix : chaque combattant emporte SPELL_SLOTS sorts parmi ceux de sa classe.
		const int SPELL_SLOTS = 4;
		// Choix par défaut : les premiers sorts de la classe.
		std::vector<int> defaultSpells(const ClassDef & classDef);
		// Choix demandé s'il est valable (SPELL_SLOTS indices distincts de sorts de la classe, ou tous
		// ses sorts si elle en a moins), sinon le choix par défaut.
		std::vector<int> validSpellChoice(const ClassDef & classDef, const std::vector<int> & requested);
		// Choix au hasard (bots, entraînement), dans l'ordre des sorts de la classe.
		std::vector<int> randomSpellChoice(const ClassDef & classDef, std::mt19937 & rng);
		// Talents de tournoi : au plus "slots" talents connus et distincts, dans l'ordre demandé.
		std::vector<std::string> validTalentChoice(const GameData & data, const std::vector<std::string> & requested, int slots);
		// Talents au hasard (bots, ordinateur de l'entraînement, emplacements laissés vides).
		std::vector<std::string> randomTalentChoice(const GameData & data, int slots, std::mt19937 & rng);

		// Sort de l'emplacement "slot" de la barre du combattant (nullptr si aucun).
		const SpellDef * spellOf(const GameData & data, const Fighter & fighter, int slot);
		// Sorts emportés par le combattant, dans l'ordre de sa barre.
		std::vector<const SpellDef *> fighterSpells(const GameData & data, const Fighter & fighter);
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

		// Zone à tenir de la carte : les cases peintes dans l'éditeur, sinon 5 ou 6 cases praticables
		// voisines, à égale distance de marche des deux équipes, au plus près du centre.
		std::vector<Cell> objectiveZone(const BattleMap & map);
		// Distance de marche de chaque équipe à la zone (depuis sa case de départ la plus proche) :
		// distances[1] et distances[2], -1 si la zone est inaccessible.
		void zoneDistances(const BattleMap & map, const std::vector<Cell> & zone, int distances[3]);
		// Équipes présentes dans la zone (combattants vivants) : present[1] et present[2].
		void zonePresence(const BattleState & state, bool present[3]);

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

		// Score du bilan d'un combattant : dégâts infligés + soins + boucliers / 2 + 25 par KO.
		int recordScore(const FighterRecord & record);
		// Meilleur combattant du combat (MVP) : plus haut score, l'équipe gagnante en cas d'égalité.
		// -1 si personne n'a rien fait.
		int chooseMvp(const BattleState & state);

		// Direction dominante de "from" vers "to" (pas en x et y : -1, 0 ou 1).
		Cell directionBetween(const Cell & from, const Cell & to);

		// Combinaison entre deux classes : des sorts d'une classe posent une marque, des sorts de l'autre
		// en profitent (choix de classe en équipe, guide du joueur).
		struct ComboLink
		{
			std::string name;						// "Brise-glace"
			int percent = 0;
			std::string state;						// Marque ("gele")
			int setterClass = 0;
			std::vector<std::string> setters;		// Noms des sorts qui posent la marque
			int finisherClass = 0;
			std::vector<std::string> finishers;		// Noms des sorts qui en profitent
		};
		// Combinaisons possibles entre deux classes, dans les deux sens (A marque et B profite, puis
		// l'inverse). Marques posées directement ou par un glyphe.
		std::vector<ComboLink> combosBetween(const GameData & data, int classA, int classB);
	}
}
