#pragma once

#include <BaseCharacterModel.h>
#include <Environment.h>

class Guerrier : public tw::BaseCharacterModel
{
public:
	virtual int getClassId()
	{
		return 4;
	}

	virtual std::string getName()
	{
		return "Guerrier";
	}

	virtual std::string getGraphicsPath()
	{
		return "./assets/Warrior/";
	}

	// Retourne la valeur du maximum de point de vie de base (sans alt�ration d'effet). C'est une caract�ristique de base de la classe.
	virtual int getBaseMaxLife()
	{
		return 95;
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

	//Passif : -50% d'HP -> +15% de d�g�ts ; -35% d'HP -> +25% de d�g�ts
	virtual bool doAttack1(int targetX, int targetY)
	{
		return true;
	}

	//Sort 1 : Charge (Mana : 3 / Sort en ligne 4x1 / DPS : 10 / Cd : 3t)
	virtual bool doAttack2(int targetX, int targetY)
	{

		return true;
	}

	//Sort 2 : Rempart (Mana : 4 / Zone 3x3 autour + Soi-m�me / DPS : 0 / Cd : 4t) : -30% de d�g�ts re�us sur soi-m�me / -15% de d�g�ts re�us sur les alli�s
	virtual bool doAttack3(int targetX, int targetY)
	{
		return true;
	}

	//Sort 3 : Frappe bouclier (Mana : 2 / Zone 3x3 autour du joueur / DPS : 8 / Cd : 2t)
	virtual bool doAttack4(int targetX, int targetY)
	{
		return true;
	}

	//Auto attaque (Mana : 0 / Corps � corps / DPS : 5 / Cd : 0t)
	virtual bool doAttack5(int targetX, int targetY)
	{
		return true;
	}

	Guerrier(tw::Environment * environment, int teamId, int currentX, int currentY)
		: BaseCharacterModel(environment, teamId, currentX, currentY)
	{

	}
};

