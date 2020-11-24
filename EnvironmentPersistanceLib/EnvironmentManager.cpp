#include "pch.h"
#include "EnvironmentManager.h"
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
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
	// Equipe �diteur de map : Il faudra charger les donn�es depuis le fichier environmentId.txt
	// situ� dans le dossier /assets/map/ et construire une variable de type Environment que vous retournerez.
	
	createTestEnvironmentIfNotExists();
	return testEnvironment;
}

void tw::EnvironmentManager::saveEnvironment(Environment * environment)
{
	// Equipe �diteur de map : Il faudra enregistrer dans un fichier la map
	// pass�e en param�tre dans le dossier /assets/map/.
	// L'extension du fichier sera .txt
	FILE* fichier = NULL;
	fichier = fopen("C:\\assets\\map\\map.txt", "w+");

	if (fichier != NULL)
	{
		fputc('A', fichier); // �criture du caract�re A
		fclose(fichier);
	}
	

}