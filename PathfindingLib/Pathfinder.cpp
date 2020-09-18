#include "pch.h"
#include "Pathfinder.h"
#include <cstdlib>


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


Orientation Pathfinder::getOrientationFromPosition(Position p1, Position p2)
{
	// TODO : Impl�menter le calcul de l'orientation du personnage � partir des positions pass�es en param�tres.
	// Les positions pass�es en param�tre repr�sentent le mouvement du personnage de p1 vers p2.
	// A partir de ces deux positions, vous devez d�terminer l'orientation r�sultante du mouvement.
	return Orientation::BOTTOM_RIGHT;
}

std::vector<Position> getPath(Position startPosition, Position endPosition, Environment * environment, std::vector<Obstacle*> obstacles)
{
	std::vector<Position> path;

	// TODO : Impl�menter un algorithme de recherche de plus court chemin entre startPosition et endPosition
	// qui tient compte de la topologie de l'environnement (environment : case "non marchable"; obstacles, etc...)
	// et des obstacles "dynamique" (les personnages, etc...)
	// Vous repr�senterez le r�sultat de votre calcul sous forme d'une liste de position 
	// s'enchainant de la position de d�part � la position d'arriv�e.

	return path;
}