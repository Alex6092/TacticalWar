#include "pch.h"
#include "EnvironmentManager.h"
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <StringUtils.h>
//#include <fstream>
#pragma warning(disable : 4996)
#define TAILLE_MAX 1000 // Tableau de taille 1000


using namespace std;



tw::EnvironmentManager * tw::EnvironmentManager::instance = NULL;

tw::EnvironmentManager * tw::EnvironmentManager::getInstance()
{
	if (instance == NULL)
		instance = new tw::EnvironmentManager();

	return instance;
}

tw::Environment * tw::EnvironmentManager::testEnvironment = NULL;
void tw::EnvironmentManager::createTestEnvironmentIfNotExists()
{
	if (testEnvironment == NULL)
	{
		testEnvironment = new tw::Environment(15, 15, 1);
		testEnvironment->getMapData(1, 1)->setIsWalkable(false);
		testEnvironment->getMapData(2, 2)->setIsObstacle(true);

		testEnvironment->getMapData(13, 13)->setIsWalkable(false);
		testEnvironment->getMapData(12, 12)->setIsObstacle(true);

		testEnvironment->getMapData(0, 0)->setTeamStartPoint(1);
		testEnvironment->getMapData(0, 1)->setTeamStartPoint(1);

		testEnvironment->getMapData(14, 14)->setTeamStartPoint(2);
		testEnvironment->getMapData(13, 14)->setTeamStartPoint(2);
	}
}


tw::Environment * tw::EnvironmentManager::loadEnvironment(int environmentId)
{

	Environment* environment;
	CellData* cell;
	std::map<int, std::map<CellData*, int>> map;

	FILE* fichier;
	char caractere;
	int heightenv;
	int widthenv;
	int idenv;
	int i = 0;
	long cursor;
	int x;
	int y;
	int walkable;
	int obstacle;
	int teamNumber;

	std::string filepath = "./assets/map/" + std::to_string(environmentId) + ".txt";
	fichier = fopen(filepath.c_str(), "r");

	
		do
		{
			if (i == 0)
			{
				fscanf(fichier, "%d", &heightenv);
			}

			if (i == 1)
			{
				fscanf(fichier, "%d", &widthenv);
			}

			if (i == 2)
			{
				fscanf(fichier, "%d", &idenv);
			}
			
			i++;
		} while (i<3 ); 
		
		environment = new Environment(widthenv, heightenv, idenv);

		i = 4;

		//if (ftell(fichier) ==3) //on a deja parcouru la hauteur, la largeur et l'id de l'environnement
		{
			do
			{
				fscanf(fichier, "%d,%d,%d,%d,%d", &x, &y, &obstacle, &walkable, &teamNumber);
				CellData * cell = environment->getMapData(x, y);
				cell->setIsWalkable(walkable);
				cell->setIsObstacle(obstacle);
				cell->setTeamStartPoint(teamNumber);

			} while (!feof(fichier));
		}
	
		fclose(fichier);
	

	// Equipe éditeur de map : Il faudra charger les données depuis le fichier environmentId.txt
	// situé dans le dossier /assets/map/ et construire une variable de type Environment que vous retournerez.
	
	//createTestEnvironmentIfNotExists();
	return environment;
}

void tw::EnvironmentManager::saveEnvironment(Environment * environment)
{
	// Equipe éditeur de map : Il faudra enregistrer dans un fichier la map
	// passée en paramètre dans le dossier /assets/map/.
	// L'extension du fichier sera .txt
	FILE* fichier;
	std::string filepath = "./assets/map/" + std::to_string(environment->getId()) + ".txt";
	fichier = fopen(filepath.c_str(), "w");

	
		fprintf(fichier,"%d\n",environment->getHeight());
		fprintf(fichier, "%d\n", environment->getWidth());
		fprintf(fichier, "%d\n", environment->getId());
		
	
		
		for (int i = 0; i < environment->getWidth(); i++) //pour avoir les coordonnées de chaque cellule
		{
			for (int j = 0; j < environment->getHeight(); j++)
			{
				CellData* cell = environment->getMapData(i, j);
				fprintf(fichier, "%d,%d,%d,%d,%d\n", cell->getX(), cell->getY(), cell->getIsObstacle()?1:0, cell->getIsWalkable()?1:0, cell->getTeamStartPointNumber());
			}
		}
	


	fclose(fichier);
	

}