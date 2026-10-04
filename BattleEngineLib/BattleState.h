#pragma once

#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "GameData.h"

namespace tw
{
	namespace battle
	{
		struct Cell
		{
			int x = 0;
			int y = 0;

			bool operator==(const Cell & other) const { return x == other.x && y == other.y; }
			bool operator!=(const Cell & other) const { return !(*this == other); }
			bool operator<(const Cell & other) const { return y != other.y ? y < other.y : x < other.x; }
		};

		inline int manhattan(const Cell & a, const Cell & b)
		{
			int dx = a.x - b.x;
			int dy = a.y - b.y;
			return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
		}

		// Carte vue par le moteur : uniquement ce qui compte pour les règles.
		class BattleMap
		{
		public:
			BattleMap() {}
			BattleMap(int width, int height);

			int getWidth() const { return width; }
			int getHeight() const { return height; }
			bool contains(const Cell & cell) const { return cell.x >= 0 && cell.y >= 0 && cell.x < width && cell.y < height; }

			bool isWalkable(const Cell & cell) const;
			bool blocksSight(const Cell & cell) const;
			void setCell(const Cell & cell, bool walkable, bool blocksSight);

			// Effet de la case au début du tour de qui s'y trouve (braises, source) : 0 si aucun.
			int turnDamage(const Cell & cell) const;
			int turnHeal(const Cell & cell) const;
			void setTurnEffect(const Cell & cell, int damage, int heal);
			bool hasTurnEffects() const { return !turnEffects.empty(); }

			// Cellules de départ de chaque équipe (1 et 2).
			std::vector<Cell> startCells[3];
			// Zone à tenir peinte dans l'éditeur (vide : zone calculée, voir objectiveZone).
			std::vector<Cell> zoneCells;

		private:
			int width = 0;
			int height = 0;
			std::vector<std::uint8_t> flags;
			std::map<int, std::pair<int, int>> turnEffects;	// Index de case -> dégâts, soins
		};

		struct ActiveEffect
		{
			int uid = 0;
			int casterId = -1;
			std::string spellId;
			std::string name;
			EffectType type = EffectType::STAT_MOD;
			Stat stat = Stat::POWER;
			int value = 0;			// Modification de caractéristique, bouclier restant...
			int minValue = 0;		// DOT / HOT : valeurs tirées à chaque tour
			int maxValue = 0;
			int casterPower = 0;	// Puissance du lanceur au moment de l'application (DOT / HOT)
			int remainingTurns = 0;
			// L'effet a été appliqué pendant le tour de son porteur : ce tour ne compte pas.
			bool skipNextDecrement = false;
			bool positive = false;
			std::string state;
		};

		// Bilan d'un combattant sur tout le combat (écran de fin, page projetée).
		struct FighterRecord
		{
			int dealt = 0;		// Dégâts infligés aux ennemis, bouclier compris
			int taken = 0;		// Dégâts subis, bouclier compris
			int healed = 0;		// PV rendus (lui compris)
			int shielded = 0;	// Boucliers donnés
			int kills = 0;		// Ennemis mis hors combat
			int casts = 0;		// Sorts lancés
			int combos = 0;		// Combinaisons déclenchées
			int zonePoints = 0;	// Points de zone marqués en étant dans la zone
			// Hauts faits (identifiants de Achievements.h), décernés à la fin du combat.
			std::vector<std::string> badges;
		};

		struct Fighter
		{
			int id = 0;
			int team = 0;			// 1 ou 2
			int classId = 0;
			std::string name;
			Cell position;
			Stats baseStats;
			int hp = 0;
			int maxHp = 0;			// PV max actuels (diminués par l'érosion)
			int shield = 0;
			int ap = 0;
			int mp = 0;
			bool alive = true;
			bool ready = false;
			bool connected = true;

			// Sorts emportés : indices dans les sorts de la classe, dans l'ordre de la barre de sorts
			// (emplacements 0 à 3). Vide (ancien instantané) : les sorts de la classe dans l'ordre.
			std::vector<int> spells;
			// Talents de tournoi (identifiants), déjà ajoutés aux caractéristiques de base.
			std::vector<std::string> talents;
			std::map<std::string, int> cooldowns;			// sort -> tours restants
			std::map<std::string, int> castsThisTurn;
			std::map<std::string, std::map<int, int>> castsOnTarget;	// sort -> combattant -> lancers ce tour
			std::vector<ActiveEffect> effects;
			FighterRecord record;

			int initialMaxHp() const { return baseStats.get(Stat::MAX_HP); }
			bool hasState(const std::string & state) const;
		};

		struct Glyph
		{
			int uid = 0;
			int casterId = -1;
			int team = 0;
			std::string spellId;
			std::string name;
			std::vector<Cell> cells;
			int remainingTurns = 0;		// Décompté au début des tours du lanceur
			TargetFilter targets = TargetFilter::ENEMIES;
			std::vector<EffectDef> effects;
		};

		enum class BattlePhase
		{
			PLACEMENT,
			FIGHT,
			ENDED
		};

		enum class EndReason
		{
			NONE,
			KO,
			ROUND_LIMIT,
			FORFEIT,
			ADMIN,
			OBJECTIVE		// Zone à tenir : l'équipe a atteint le score demandé
		};

		// Mode "zone à tenir" : à la fin de chaque tour complet, une équipe marque un point si elle a
		// au moins un combattant vivant dans la zone et l'autre aucun. Un KO fait toujours gagner.
		struct ZoneState
		{
			bool enabled = false;
			std::vector<Cell> cells;
			int pointsToWin = 0;
			int scores[3] = { 0, 0, 0 };	// Par équipe (1 et 2)
			int holder = 0;					// Équipe qui a marqué au dernier décompte (0 : personne)

			bool contains(const Cell & cell) const;
		};

		struct BattleState
		{
			BattlePhase phase = BattlePhase::PLACEMENT;
			std::vector<Fighter> fighters;
			std::vector<int> turnOrder;
			int turnIndex = 0;
			int round = 0;
			std::vector<Glyph> glyphs;
			std::int64_t deadlineMs = 0;	// Fin du tour (ou du placement) en cours
			int winnerTeam = 0;
			EndReason endReason = EndReason::NONE;
			int mvpFighterId = -1;			// Meilleur combattant, connu à la fin du combat
			int firstBloodFighterId = -1;	// Auteur du premier KO du combat
			ZoneState zone;
			int nextUid = 1;

			int activeFighterId() const;
			Fighter * findFighter(int id);
			const Fighter * findFighter(int id) const;
			const Fighter * fighterAt(const Cell & cell) const;	// Combattant vivant sur la cellule
		};

		const char * toString(BattlePhase phase);
		const char * toString(EndReason reason);
	}
}
