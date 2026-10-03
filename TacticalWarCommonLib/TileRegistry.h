#pragma once

#include <map>
#include <string>
#include <vector>

namespace tw
{
	// Catégorie d'une tuile : règles de jeu par défaut et ordre dans la palette de l'éditeur.
	//   GROUND   : sol praticable ;
	//   OBSTACLE : bloque le passage et la ligne de vue (rocher, arbre) ;
	//   LIQUID   : bloque le passage mais pas la vue (eau, lave) ;
	//   EMPTY    : trou, rien n'est dessiné.
	enum class TileCategory { GROUND, OBSTACLE, LIQUID, EMPTY };

	const char * toString(TileCategory category);
	TileCategory tileCategoryFromString(const std::string & text);

	// Règles de jeu d'une case, tirées de sa tuile. C'est la seule définition : le moteur de combat,
	// les cartes envoyées aux clients, l'éditeur et la recherche de chemin s'en servent.
	// Praticable et bloque la vue sont indépendants : les hautes herbes sont praticables et cachent,
	// l'eau n'est pas praticable et ne cache pas, un rocher ne laisse rien passer.
	struct TileRules
	{
		bool walkable = true;
		bool blocksLineOfSight = false;
		// Effet au début du tour du combattant qui s'y trouve : dégâts (braises), soins (source).
		int turnDamage = 0;
		int turnHeal = 0;

		bool hasTurnEffect() const { return turnDamage > 0 || turnHeal > 0; }

		// Règles par défaut d'une catégorie de tuile.
		static TileRules forCategory(TileCategory category);
		// Tuile inconnue : ni praticable, ni transparente.
		static TileRules unknown();
	};

	struct TileDef
	{
		std::string id;
		std::string name;			// UTF-8
		TileCategory category = TileCategory::GROUND;
		std::string texture;		// Chemin relatif au dossier du jeu
		float anchorX = 0;			// Pixel de la texture placé au centre de la case
		float anchorY = 0;
		TileRules rules;
		std::string group;			// Groupe de la palette de l'éditeur (UTF-8)
		std::string shader;			// "water", "lava" ou vide
	};

	// Registre des tuiles (assets/tiles/tileset.json). Sans dépendance à SFML : utilisé par le
	// serveur (règles de jeu), le client (rendu) et l'éditeur (palette).
	class TileRegistry
	{
	public:
		static const char * DEFAULT_PATH;

		// Tuiles des cartes au format v1 (sol, obstacle, trou) et tuile par défaut.
		static const char * LEGACY_GROUND;
		static const char * LEGACY_OBSTACLE;
		static const char * LEGACY_HOLE;

		// Registre partagé, chargé depuis DEFAULT_PATH au premier appel (tuiles intégrées si absent).
		static TileRegistry & get();

		bool load(const std::string & path, std::string * error = nullptr);
		bool loadFromString(const std::string & json, std::string * error = nullptr);
		void loadBuiltins();

		const TileDef * find(const std::string & id) const;
		const std::vector<TileDef> & all() const { return tiles; }

		// Tuile équivalente aux indicateurs d'une case v1.
		static const char * legacyTile(bool walkable, bool obstacle);

	private:
		void add(const TileDef & tile);

		std::vector<TileDef> tiles;
		std::map<std::string, std::size_t> index;
	};
}
