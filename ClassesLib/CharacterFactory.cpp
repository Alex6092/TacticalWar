#include "pch.h"
#include "CharacterFactory.h"

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
	case 1:
		return new Mage(environment, teamId, posX, posY);
	case 2:
		return new Archer(environment, teamId, posX, posY);
	case 3:
		return new Protecteur(environment, teamId, posX, posY);
	case 4:
		return new Guerrier(environment, teamId, posX, posY);
	}
	return NULL;
}
