#include "pch.h"
#include "Pathfinder.h"
#include <cstdlib>
#include <limits>
#include <math.h>
#include <time.h>


using namespace tw;

Pathfinder * Pathfinder::instance = NULL;

Pathfinder * Pathfinder::getInstance()
{
	if (instance == NULL)
		instance = new Pathfinder();

	return instance;
}

Pathfinder::Pathfinder()
{
}


Pathfinder::~Pathfinder()
{
}



int Pathfinder::TotalCostFromStartToEnd() //calcul du cout total du point de depart au point d'arrivee (heuristique)
{
	return 0;
}

Orientation Pathfinder::getOrientationFromPosition(Point2D p1, Point2D p2)
{
	// TODO : Implémenter le calcul de l'orientation du personnage à partir des positions passées en paramètres.
	// Les positions passées en paramètre représentent le mouvement du personnage de p1 vers p2.
	// A partir de ces deux positions, vous devez déterminer l'orientation résultante du mouvement.
	return Orientation::BOTTOM_RIGHT;
}

std::vector<Point2D> Pathfinder::getPath(Point2D startPosition, Point2D endPosition, Environment * environment, std::vector<Obstacle*> obstacles)
{
	std::vector<Point2D> path;
	std::map<CellData*, CellData*> Predecesseur;
	std::vector<CellData*> P;
	std::map<CellData*, float> d;
	

	for (int i = 0; i < environment->getWidth(); i++)
	{
		for (int j = 0; j < environment->getHeight(); j++)
		{
			CellData * cell = environment->getMapData(i, j);

			d[cell] = std::numeric_limits<float>::max(); //sommet à +infini
			

			if (cell->getIsObstacle() || !cell->getIsWalkable())
			{
				P.push_back(cell);
			}
		}
	}

	for (int i = 0; i < obstacles.size(); i++)
	{
		Obstacle * o = obstacles[i];
		P.push_back(environment->getMapData(o->getX(), o->getY()));
	}

	d[environment->getMapData(startPosition.getX(), startPosition.getY())] = 0;
	CellData * cell;
	float PlusCourteDistance = std::numeric_limits<float>::max();
	float NewDistance;

	while(P.size() != d.size())	//on itère tant que qu'il existe un sommet hors de P donc tant que P ne fait pas la même taille que le nombre total de sommets
	{
		PlusCourteDistance = std::numeric_limits<float>::max();
		CellData * a = NULL;
		for (int i = 0; i < environment->getWidth(); i++)
		{
			for (int j = 0; j < environment->getHeight(); j++)
			{	
				cell = environment->getMapData(i, j);
				
				if (std::find(P.begin(), P.end(), cell) == P.end()) //on verifie que le sommet n'est pas dans P
				{
					if (d[cell] < PlusCourteDistance)
					{
						PlusCourteDistance = d[cell];
						a = cell;		//on ajoute la cellule à l'ensemble P car elle est parcourue
					}
				}
			}
		}

		if (a != NULL)
		{
			P.push_back(a);
			std::vector<CellData*> Voisins;

			if (a->getX() + 1 < environment->getWidth()) //adjacent droit
			{
				CellData * voisinDroite = environment->getMapData(a->getX() + 1, a->getY());
				if (voisinDroite != NULL && voisinDroite->getIsWalkable() && !voisinDroite->getIsObstacle() && isNotDynamicObstacle(voisinDroite, obstacles))
				{
					Voisins.push_back(voisinDroite);
				}
			}

			if (a->getX() - 1 >= 0) //adjacent gauche
			{
				CellData * voisinDroite = environment->getMapData(a->getX() - 1, a->getY());
				if (voisinDroite != NULL && voisinDroite->getIsWalkable() && !voisinDroite->getIsObstacle() && isNotDynamicObstacle(voisinDroite, obstacles))
				{
					Voisins.push_back(voisinDroite);
				}
			}

			if (a->getY() + 1 < environment->getHeight()) //adjacent bas
			{
				CellData * voisinDroite = environment->getMapData(a->getX(), a->getY() + 1);
				if (voisinDroite != NULL && voisinDroite->getIsWalkable() && !voisinDroite->getIsObstacle() && isNotDynamicObstacle(voisinDroite, obstacles))
				{
					Voisins.push_back(voisinDroite);
				}
			}

			if (a->getY() - 1 >= 0) //adjacent haut
			{
				CellData * voisinDroite = environment->getMapData(a->getX(), a->getY() - 1);
				if (voisinDroite != NULL && voisinDroite->getIsWalkable() && !voisinDroite->getIsObstacle() && isNotDynamicObstacle(voisinDroite, obstacles))
				{
					Voisins.push_back(voisinDroite);
				}
			}

			//comparaison des distances voisines
			bool sorted = false;
			int i = 0;
			int compteur = 0;
			float temp;

			for (int i = 0; i < Voisins.size(); i++)
			{
				if (d[Voisins[i]] > d[a] + 1)
				{
					d[Voisins[i]] = d[a] + 1; //formule pour calculer une distance entre deux cellules	adjacentes
					Predecesseur[Voisins[i]] = a;
				}
			}
		}
		else
			break;
	}

	CellData * endCell = environment->getMapData(endPosition.getX(), endPosition.getY());
	CellData * startCell = environment->getMapData(startPosition.getX(), startPosition.getY());

	CellData * currentCell = endCell;

	if (currentCell != NULL)
	{
		std::vector<Point2D> reversedPath;
		while (currentCell != startCell)
		{
			//reversedPath.push_back(tw::Point2D(currentCell->getX(), currentCell->getY()));
			path.push_back(tw::Point2D(currentCell->getX(), currentCell->getY()));
			currentCell = Predecesseur[currentCell];

			if (currentCell == NULL)
				return std::vector<Point2D>();
		}

		// Remettre le chemin dans l'ordre :
		//while(reversedPath.size() > 0)
		//{
		//	path.push_back(reversedPath.back());
		//	reversedPath.pop_back();
		//}
	}

	// TODO : Implémenter un algorithme de recherche de plus court chemin entre startPosition et endPosition
	// qui tient compte de la topologie de l'environnement (environment : case "non marchable"; obstacles, etc...)
	// et des obstacles "dynamique" (les personnages, etc...)
	// Vous représenterez le résultat de votre calcul sous forme d'une liste de position 
	// s'enchainant de la position de départ à la position d'arrivée.

	return path;
}

bool Pathfinder::isNotDynamicObstacle(CellData * voisin, std::vector<Obstacle*> obstacles)
{
	bool isObstacle = false;
	for (int i = 0; i < obstacles.size(); i++)
	{
		Obstacle * o = obstacles[i];
		if ((*o) == (*voisin))
		{
			isObstacle = true;
			break;
		}
	}

	return !isObstacle;
}