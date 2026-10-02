#pragma once

#include <map>
#include <string>
#include <vector>

namespace tw
{
	namespace battle
	{
		// Caractéristiques d'un combattant.
		enum class Stat
		{
			MAX_HP,
			AP,				// PA
			MP,				// PM
			INITIATIVE,
			POWER,			// Puissance : % de dégâts en plus
			RESISTANCE,		// % de dégâts subis en moins (bornée)
			RANGE,			// Portée en plus pour les sorts à portée modifiable
			HEAL_BONUS,		// % de soins en plus
			LOCK,			// Tacle
			DODGE,			// Fuite
			EROSION,		// % des PV perdus retirés des PV max
			COUNT
		};

		const int STAT_COUNT = (int)Stat::COUNT;

		struct Stats
		{
			int values[STAT_COUNT] = {};

			int get(Stat stat) const { return values[(int)stat]; }
			void set(Stat stat, int value) { values[(int)stat] = value; }
		};

		// Zone de lancer : cellules ciblables autour du lanceur.
		enum class LaunchShape
		{
			CIRCLE,		// Distance de Manhattan entre min et max
			LINE,		// En ligne droite (même ligne ou même colonne)
			DIAGONAL,
			STAR,		// Ligne ou diagonale
			SELF		// Uniquement la cellule du lanceur
		};

		// Zone d'effet autour de la cellule ciblée.
		enum class ZoneShape
		{
			SINGLE,
			CIRCLE,			// Distance de Manhattan <= taille
			CROSS,			// Même ligne / colonne, à distance <= taille
			SQUARE,			// Distance de Tchebychev <= taille
			LINE,			// Depuis la cible, dans la direction lanceur -> cible, sur taille cases en plus
			PERPENDICULAR,	// Ligne perpendiculaire à la direction lanceur -> cible, taille cases de chaque côté
			RING			// Distance de Manhattan == taille
		};

		// Contrainte sur la cellule ciblée.
		enum class CellRequirement
		{
			ANY,
			FREE_CELL,			// Cellule praticable et libre
			FREE_CELL_OR_ALLY,
			CHARACTER,			// Un combattant vivant
			ENEMY,
			ALLY,				// Un allié (le lanceur exclu)
			ALLY_OR_SELF
		};

		// Combattants concernés par un effet, parmi ceux présents dans la zone.
		enum class TargetFilter
		{
			ENEMIES,
			ALLIES,				// Alliés, lanceur compris s'il est dans la zone
			ALL,
			CASTER				// Le lanceur, où qu'il soit
		};

		enum class EffectType
		{
			DAMAGE,
			HEAL,
			LIFESTEAL,		// Dégâts, puis le lanceur récupère "percent" % des dégâts infligés
			SHIELD,			// Points de bouclier pendant "duration" tours
			DOT,			// Dégâts au début de chacun des "duration" prochains tours du porteur
			HOT,			// Soins au début de chacun des "duration" prochains tours du porteur
			STAT_MOD,		// Modification de caractéristique pendant "duration" tours
			PUSH,			// Repousse de "min" cases (dégâts de collision si bloqué)
			PULL,			// Attire de "min" cases vers le lanceur
			DASH,			// Le lanceur bondit sur la cellule adjacente à la cible, de son côté
			TELEPORT,		// Le lanceur se téléporte sur la cellule (ou échange avec un allié si allowSwap)
			DISPEL,			// Retire les effets positifs ou négatifs
			GLYPH,			// Pose un glyphe sur la zone pendant "duration" tours du lanceur
			STATE			// Ajoute un état ("unmovable"...) pendant "duration" tours
		};

		enum class DispelMode
		{
			NEGATIVE,
			POSITIVE,
			ALL
		};

		struct EffectDef
		{
			EffectType type = EffectType::DAMAGE;
			TargetFilter targets = TargetFilter::ENEMIES;
			int min = 0;
			int max = 0;
			int duration = 0;
			Stat stat = Stat::POWER;
			int percent = 0;
			bool allowSwap = false;
			bool refresh = true;		// Effet non cumulable : la durée est rafraîchie
			DispelMode dispel = DispelMode::NEGATIVE;
			std::string state;
			std::string name;			// Libellé affiché (ex : "Poison")

			// Glyphe : zone, cibles et effets déclenchés au début du tour d'un combattant dedans.
			ZoneShape glyphShape = ZoneShape::SINGLE;
			int glyphSize = 0;
			std::vector<EffectDef> glyphEffects;
		};

		struct ZoneDef
		{
			ZoneShape shape = ZoneShape::SINGLE;
			int size = 0;
		};

		struct SpellDef
		{
			std::string id;
			std::string name;
			std::string description;
			std::string icon;

			int apCost = 3;
			int minRange = 1;
			int maxRange = 1;
			bool rangeModifiable = false;
			bool lineOfSight = true;
			LaunchShape launch = LaunchShape::CIRCLE;
			CellRequirement requirement = CellRequirement::ANY;
			int castsPerTurn = 0;		// 0 : illimité
			int castsPerTarget = 0;		// 0 : illimité
			int cooldown = 0;			// Tours avant de pouvoir relancer le sort
			int initialCooldown = 0;
			ZoneDef impact;
			std::vector<EffectDef> effects;

			// Rendu côté client.
			std::string fxSprite;
			std::string sound;
			std::string casterAnimation;	// "magical" ou "physical"
		};

		enum class PassiveType
		{
			NONE,
			LOW_HP_DAMAGE,		// +bonus% de dégâts sous threshold% de PV (deux paliers)
			DISTANCE_DAMAGE,	// +bonus% de dégâts si la cible est à au moins "distance" cases
			ON_CAST_POWER,		// +bonus puissance jusqu'à la fin du tour à chaque sort, cumulable maxStacks fois
			ALLY_AURA			// +bonus en "stat" pour les alliés à "distance" cases ou moins
		};

		struct PassiveDef
		{
			PassiveType type = PassiveType::NONE;
			std::string name;
			std::string description;
			int threshold = 0;
			int bonus = 0;
			int threshold2 = 0;
			int bonus2 = 0;
			int distance = 0;
			int maxStacks = 0;
			Stat stat = Stat::RESISTANCE;
		};

		struct ClassDef
		{
			int id = 0;
			std::string key;
			std::string name;
			std::string description;
			std::string graphicsPath;
			std::string icon;
			std::string preview;
			Stats baseStats;
			PassiveDef passive;
			std::vector<SpellDef> spells;
		};

		// Règles générales du combat (valeurs par défaut de l'Annexe A du plan).
		struct BattleRules
		{
			int turnSeconds = 40;
			int placementSeconds = 30;
			int disconnectedTurnSeconds = 5;
			int suddenDeathRound = 15;
			int suddenDeathPercentPerRound = 5;
			int maxRounds = 30;				// Décision aux PV au-delà
			int maxResistance = 50;
			int collisionDamagePerCell = 6;
			int collisionDamageToHit = 3;
			double tackleApFactor = 0.5;	// Part de la perte de PA par rapport à la perte de PM
		};

		struct GameData
		{
			int version = 1;
			BattleRules rules;
			std::vector<ClassDef> classes;

			const ClassDef * findClass(int classId) const;

			// Charge les données (assets/data/gamedata.json). Retourne false et renseigne error en cas de problème.
			bool loadFromJsonText(const std::string & text, std::string & error);
			bool loadFromFile(const std::string & path, std::string & error);

			// Texte JSON source, pour l'envoyer tel quel aux clients.
			const std::string & getSourceText() const { return sourceText; }

		private:
			std::string sourceText;
		};

		const char * toString(Stat stat);
		bool parseStat(const std::string & text, Stat & stat);
		bool isPositiveEffect(const EffectDef & effect);
	}
}
