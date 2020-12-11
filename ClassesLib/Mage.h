#pragma once

#include <BaseCharacterModel.h>
#include <Environment.h>
#include <iostream>

using namespace std;

class Mage : public tw::BaseCharacterModel
{
private:

	int tour = 1;
	int compt1, compt2, compt3;

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

	// Retourne la valeur du maximum de point de vie de base (sans altération d'effet). C'est une caractéristique de base de la classe.
	virtual int getBaseMaxLife()
	{
		return 80;
	}

	//Fonction à ajouter
	virtual int getBasePa()
	{
		return 6;
	}

	//Fonction à ajouter
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

	virtual int getAttack1()
	{
		return compt1;
	}

	virtual int getAttack2()
	{
		return compt2;
	}

	virtual int getAttack3()
	{
		return compt3;
	}

	virtual void turnStart()
	{
		// Décrémentation des cooldowns :
		if (compt1 > 0)
			compt1--;
	}

	//Passif : Réduction de mana pour les alliés dans la zone 3x3
	virtual bool doAttack1(int targetX, int targetY) // appel class  effet
	{
		return true;
	}

	//Sort 1 : Boule de feu (Mana : 4 / Zone étoile / DPS : 15-10(45%-25% de brûlure / Cd : 3t)
	virtual bool doAttack2(int targetX, int targetY)
	{
		
		return true;
	}

	//Sort 2 : Eclair (Mana : 2 / Zone unique / DPS : 6 / Cd : 1t)
	virtual bool doAttack3(int targetX, int targetY)
	{
		return true;
	}

	//Sort 3 : Blizzard (Mana : 3 / Joueur au centre : carré de 3x3 autour du joueur / DPS : )
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
		compt1 = 3;
		compt2 = 1;
		compt3 = 4;
	}


	// Méthodes rajoutées :

	virtual std::string getClassName()
	{
		return "Mage";
	}

	virtual std::string getClassDescription()
	{
		return "Description mage ...";
	}

	virtual std::string getClassIconPath()
	{
		return "./assets/classicons/Mage.png";
	}


	//------------------------------------

	virtual std::string getSpell1Name()
	{
		return "Mage sort 1";
	}

	virtual std::string getSpell2Name()
	{
		return "Mage sort 2";
	}

	virtual std::string getSpell3Name()
	{
		return "Mage sort 3";
	}

	virtual std::string getSpell4Name()
	{
		return "Mage sort 4";
	}


	//------------------------------------
	virtual std::string getSpell1Description()
	{
		return "Mage, description sort 1 ...";
	}

	virtual std::string getSpell2Description()
	{
		return "Mage, description sort 2 ...";
	}

	virtual std::string getSpell3Description()
	{
		return "Mage, description sort 3 ...";
	}

	virtual std::string getSpell4Description()
	{
		return "Mage, description sort 4 ...";
	}


	//------------------------------------
	virtual std::string getSpell1IconPath()
	{
		return "./assets/spellicons/101.png";
	}

	virtual std::string getSpell2IconPath()
	{
		return "./assets/spellicons/515.png";
	}

	virtual std::string getSpell3IconPath()
	{
		return "./assets/spellicons/516.png";
	}

	virtual std::string getSpell4IconPath()
	{
		return "./assets/spellicons/700.png";
	}


	//------------------------------------
	virtual std::vector<Effect> getSpell1Effects()
	{
		std::vector<Effect> effects;

		return effects;
	}

	virtual std::vector<Effect> getSpell2Effects()
	{
		std::vector<Effect> effects;

		return effects;
	}

	virtual std::vector<Effect> getSpell3Effects()
	{
		std::vector<Effect> effects;

		return effects;
	}

	virtual std::vector<Effect> getSpell4Effects()
	{
		std::vector<Effect> effects;

		return effects;
	}


	//------------------------------------
	virtual int getSpell1ManaCost()
	{
		return 4;
	}

	virtual int getSpell2ManaCost()
	{
		return 3;
	}

	virtual int getSpell3ManaCost()
	{
		return 2;
	}

	virtual int getSpell4ManaCost()
	{
		return 1;
	}

	//------------------------------------
	virtual int getSpell1MinPO()
	{
		return 2;
	}

	virtual int getSpell2MinPO()
	{
		return 1;
	}

	virtual int getSpell3MinPO()
	{
		return 3;
	}

	virtual int getSpell4MinPO()
	{
		return 4;
	}

	//------------------------------------

	virtual int getSpell1MaxPO()
	{
		return 5;
	}

	virtual int getSpell2MaxPO()
	{
		return 3;
	}

	virtual int getSpell3MaxPO()
	{
		return 8;
	}

	virtual int getSpell4MaxPO()
	{
		return 12;
	}

	//------------------------------------

	virtual TypeZoneLaunch getSpell1LaunchZoneType()
	{
		return TypeZoneLaunch::LINE;
	}

	virtual TypeZoneLaunch getSpell2LaunchZoneType()
	{
		return TypeZoneLaunch::NORMAL;
	}

	virtual TypeZoneLaunch getSpell3LaunchZoneType()
	{
		return TypeZoneLaunch::DIAGONAL;
	}

	virtual TypeZoneLaunch getSpell4LaunchZoneType()
	{
		return TypeZoneLaunch::STAR;
	}

	//------------------------------------

	virtual TypeZoneLaunch getSpell1ImpactZoneType()
	{
		return TypeZoneLaunch::NORMAL;
	}

	virtual TypeZoneLaunch getSpell2ImpactZoneType()
	{
		return TypeZoneLaunch::NORMAL;
	}

	virtual TypeZoneLaunch getSpell3ImpactZoneType()
	{
		return TypeZoneLaunch::NORMAL;
	}

	virtual TypeZoneLaunch getSpell4ImpactZoneType()
	{
		return TypeZoneLaunch::NORMAL;
	}

	//------------------------------------

	virtual int getSpell1ImpactZoneMinPO()
	{
		return 0;
	}

	virtual int getSpell2ImpactZoneMinPO()
	{
		return 0;
	}

	virtual int getSpell3ImpactZoneMinPO()
	{
		return 0;
	}

	virtual int getSpell4ImpactZoneMinPO()
	{
		return 0;
	}

	//------------------------------------

	virtual int getSpell1ImpactZoneMaxPO()
	{
		return 0;
	}

	virtual int getSpell2ImpactZoneMaxPO()
	{
		return 0;
	}

	virtual int getSpell3ImpactZoneMaxPO()
	{
		return 0;
	}

	virtual int getSpell4ImpactZoneMaxPO()
	{
		return 0;
	}

	//--------------------------------------------------------------------
};

