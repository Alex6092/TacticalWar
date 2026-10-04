#pragma once

#include <map>
#include <string>
#include <vector>
#include "CellData.h"
#include "Obstacle.h"

namespace tw
{
	class Environment
	{
	private:
		int width;
		int height;
		// Cases rangées par colonne (x * height + y). Les pointeurs restent valides toute la vie de la carte.
		std::vector<CellData*> cells;

		std::vector<Obstacle> staticObstacles;
		bool obstacleCacheInitDone;

		int id;
		std::string name;			// UTF-8
		bool tournamentPool;		// Carte tirée au sort pour les matchs de tournoi

	public:
		// Carte remplie avec la tuile donnée (par défaut : sol praticable sans tuile, comme en v1).
		Environment(int width, int height, int environmentId, const std::string & fillTile = std::string());
		~Environment();

		Environment(const Environment &) = delete;
		Environment & operator=(const Environment &) = delete;

		CellData* getMapData(int x, int y);

		// Change la tuile d'une case ; les règles de jeu viennent du registre de tuiles
		// (une tuile inconnue est traitée comme un obstacle).
		void setTile(int x, int y, const std::string & tile);
		// Variante avec des règles explicites (carte reçue du serveur).
		void setTile(int x, int y, const std::string & tile, const TileRules & rules);

		std::vector<tw::Obstacle> getObstacles()
		{
			if (!obstacleCacheInitDone)
			{
				for (int i = 0; i < getWidth(); i++)
				{
					for (int j = 0; j < getHeight(); j++)
					{
						CellData * c = getMapData(i, j);
						if (c->getIsObstacle())
						{
							staticObstacles.push_back(Obstacle(c));
						}
					}
				}
				obstacleCacheInitDone = true;
			}

			return staticObstacles;
		}

		inline int getWidth() { return width; }
		inline int getHeight() { return height; }

		inline int getId() { return id; }
		inline void setId(int id) { this->id = id; }

		inline const std::string & getName() const { return name; }
		inline void setName(const std::string & name) { this->name = name; }

		inline bool isInTournamentPool() const { return tournamentPool; }
		inline void setInTournamentPool(bool inPool) { tournamentPool = inPool; }
	};
}
