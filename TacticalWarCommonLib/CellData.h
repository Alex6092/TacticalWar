#pragma once
#include <string>
#include "Point2D.h"

namespace tw
{
	class CellData : public Point2D
	{
	private:
		// Règles de jeu de la case, déduites de sa tuile (voir Environment::setTile).
		bool isWalkable;
		bool isObstacle;

		// Valeurs pour teamStartPoint :
		// - 0 = Pas un point de départ
		// - 1 = Point de départ de l'équipe 1
		// - 2 = Point de départ de l'équipe 2
		// - Par défaut = Pas un point de départ
		int teamStartPoint;

		// Identifiant de la tuile (registre TileRegistry). Vide : déduite des indicateurs (cartes v1).
		std::string tile;

	public:
		CellData(int x, int y, bool isWalkable = true, bool isObstacle = false, int teamStartPoint = 0);


		inline bool getIsWalkable()
		{
			return isWalkable;
		}

		inline void setIsWalkable(bool isWalkable) {
			this->isWalkable = isWalkable;
		}

		inline bool getIsObstacle()
		{
			return isObstacle;
		}

		inline int getTeamStartPointNumber()
		{
			return teamStartPoint;
		}

		inline void setIsObstacle(bool isObstacle) {
			this->isObstacle = isObstacle;
		}

		inline void setTeamStartPoint(int teamId)
		{
			this->teamStartPoint = teamId;
		}

		inline bool isTeam1StartPoint()
		{
			return !isObstacle && isWalkable && teamStartPoint == 1;
		}

		inline bool isTeam2StartPoint()
		{
			return !isObstacle && isWalkable && teamStartPoint == 2;
		}

		inline const std::string & getTile() const
		{
			return tile;
		}

		// Tuile affichée : celle de la case, ou l'équivalent de ses indicateurs (carte v1).
		std::string getDisplayTile() const;

		// Change la tuile et les règles de jeu de la case.
		inline void setTile(const std::string & tile, bool walkable, bool obstacle)
		{
			this->tile = tile;
			this->isWalkable = walkable;
			this->isObstacle = obstacle;
		}
	};
}
