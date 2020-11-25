#pragma once

#include <BaseCharacterModel.h>
#include <Environment.h>

class Mage : public tw::BaseCharacterModel
{
public:
	virtual int getClassId()
	{
		return 1;
	}

	virtual std::string getName()
	{
		return "Mage";
	}

	virtual std::string getGraphicsPath()
	{
		return "./assets/Warrior/";
	}

	// Retourne la valeur du maximum de point de vie de base (sans alt�ration d'effet). C'est une caract�ristique de base de la classe.
	virtual int getBaseMaxLife()
	{
		return 80;
	}

	//Fonction � ajouter
	virtual int getBasePa()
	{
		return 6;
	}

	//Fonction � ajouter
	virtual int getBasePm()
	{
		return 3;
	}

	virtual int getBaseAttack()
	{
		return 5;
	}

	virtual int getBaseDefense()
	{
		return 0;
	}


	//Passif : R�duction de mana pour les alli�s dans la zone 3x3
	virtual bool doAttack1(int targetX, int targetY)
	{
		return true;
	}

	//Sort 1 : Boule de feu (Mana : 4 / Zone �toile / DPS : 15-10(45%-25% de br�lure / Cd : 3t)
	virtual bool doAttack2(int targetX, int targetY)
	{
		
		return true;
	}

	//Sort 2 : Eclair (Mana : 2 / Zone unique / DPS : 6 / Cd : 1t)
	virtual bool doAttack3(int targetX, int targetY)
	{
		return true;
	}

	//Sort 3 : Blizzard (Mana : 3 / Joueur au centre : carr� de 3x3 autour du joueur / DPS : )
	virtual bool doAttack4(int targetX, int targetY)
	{
		return true;
	}

	virtual bool doAttack5(int targetX, int targetY)
	{
		return true;
	}

	Mage(tw::Environment * environment, int teamId, int currentX, int currentY)
		: BaseCharacterModel(environment, teamId, currentX, currentY)
	{

	}
};

