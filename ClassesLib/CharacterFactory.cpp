#include "pch.h"
#include "CharacterFactory.h"
#include <stdlib.h>

#include "Mage.h"
#include "Archer.h"
#include "Protecteur.h"
#include "Guerrier.h"

CharacterFactory * CharacterFactory::instance = NULL;

CharacterFactory * CharacterFactory::getInstance()
{
	if (instance == NULL)
		instance = new CharacterFactory();

	return instance;
}

tw::BaseCharacterModel * CharacterFactory::constructCharacter(tw::Environment * environment, int classId, int teamId, int posX, int posY)
{
	switch (classId)
	{
	case 1:	// Mage
		return new Mage(environment, teamId, posX, posY);
		break;

	case 2:	// Archer
		return new Archer(environment, teamId, posX, posY);
		break;

	case 3:	// Protecteur
		return new Protecteur(environment, teamId, posX, posY);
		break;

	case 4:	// Guerrier
		return new Guerrier(environment, teamId, posX, posY);
		break;
	}

	return NULL;
}