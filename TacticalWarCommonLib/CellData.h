#pragma once
#include <string>
#include "Point2D.h"
#include "TileRegistry.h"

namespace tw
{
	class CellData : public Point2D
	{
	private:
		// Règles de jeu de la case, déduites de sa tuile (voir Environment::setTile).
		TileRules rules;

		// Valeurs pour teamStartPoint :
		// - 0 = Pas un point de départ
		// - 1 = Point de départ de l'équipe 1
		// - 2 = Point de départ de l'équipe 2
		// - Par défaut = Pas un point de départ
		int teamStartPoint;

		// Case de la zone à tenir (mode de combat "zone"), peinte dans l'éditeur.
		bool zone = false;

		// Identifiant de la tuile (registre TileRegistry). Vide : déduite des indicateurs (cartes v1).
		std::string tile;

	public:
		CellData(int x, int y, bool isWalkable = true, bool isObstacle = false, int teamStartPoint = 0);


		inline const TileRules & getRules() const
		{
			return rules;
		}

		// Praticable : on peut s'y arrêter et la traverser.
		inline bool getIsWalkable() const
		{
			return rules.walkable;
		}

		inline void setIsWalkable(bool isWalkable) {
			rules.walkable = isWalkable;
		}

		// Obstacle à la vue : bloque la ligne de vue (une case praticable peut en être un).
		inline bool getIsObstacle() const
		{
			return rules.blocksLineOfSight;
		}

		inline int getTeamStartPointNumber()
		{
			return teamStartPoint;
		}

		inline void setIsObstacle(bool isObstacle) {
			rules.blocksLineOfSight = isObstacle;
		}

		inline bool getIsZone() const
		{
			return zone;
		}

		inline void setIsZone(bool inZone)
		{
			zone = inZone;
		}

		inline void setTeamStartPoint(int teamId)
		{
			this->teamStartPoint = teamId;
		}

		inline bool isTeam1StartPoint()
		{
			return rules.walkable && teamStartPoint == 1;
		}

		inline bool isTeam2StartPoint()
		{
			return rules.walkable && teamStartPoint == 2;
		}

		inline const std::string & getTile() const
		{
			return tile;
		}

		// Tuile affichée : celle de la case, ou l'équivalent de ses indicateurs (carte v1).
		std::string getDisplayTile() const;

		// Change la tuile et les règles de jeu de la case.
		inline void setTile(const std::string & tile, const TileRules & rules)
		{
			this->tile = tile;
			this->rules = rules;
		}
	};
}
