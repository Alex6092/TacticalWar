#pragma once

#include <BaseCharacterModel.h>
#include <Environment.h>

class Protecteur : public tw::BaseCharacterModel
{
public:
	virtual int getClassId()
	{
		return 3;
	}

	virtual std::string getName()
	{
		return "Protecteur";
	}

	virtual std::string getGraphicsPath()
	{
		return "./assets/Warrior/";
	}

	// Retourne la valeur du maximum de point de vie de base (sans alt�ration d'effet). C'est une caract�ristique de base de la classe.
	virtual int getBaseMaxLife()
	{
		return 75;
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

	//Passif : +3 HP sur soi-m�me pour chaque auto attaque
	virtual bool doAttack1(int targetX, int targetY)
	{
		return true;
	}

	//Sort 1 : R�animation (Mana : 5 / Cibl� / DPS : 0 / Cd : usage unique)
	virtual bool doAttack2(int targetX, int targetY)
	{

		return true;
	}

	//Sort 2 : Heal (Mana : 5 / Cibl� / Heal : +20% des HP max / Cd : 3t / )
	virtual bool doAttack3(int targetX, int targetY)
	{
		return true;
	}

	//Sort 3 : Purification d'�me (Mana : 2 / Pigme (4) / DPS : 10 / Cd : 2t) Supprime 1 tour de malus chez les alli�s touch�s
	virtual bool doAttack4(int targetX, int targetY)
	{
		return true;
	}

	//Auto attaque (Mana : 0 / Corps � corps / DPS : 5 / Cd : 0t)
	virtual bool doAttack5(int targetX, int targetY)
	{
		return true;
	}

	Protecteur(tw::Environment * environment, int teamId, int currentX, int currentY)
		: BaseCharacterModel(environment, teamId, currentX, currentY)
	{

	}
};

