#pragma once

#include <BaseCharacterModel.h>
#include <Environment.h>

class Protecteur : public tw::BaseCharacterModel
{
private:

	int tour = 1;
	int compt1, compt2, compt3;
	// Durées de relance (valeur initiale du cooldown) :
	int cd1, cd2, cd3;

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
		return "./assets/Protecteur/";
	}

	// Retourne la valeur du maximum de point de vie de base (sans altération d'effet). C'est une caractéristique de base de la classe.
	virtual int getBaseMaxLife()
	{
		return 75;
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

	virtual bool canDoAttack(int spellId)
	{
		if (spellId == 1)
		{
			return compt1 <= 0;
		}
		else if (spellId == 2)
		{
			return compt2 <= 0;
		}
		else if (spellId == 3)
		{
			return compt3 <= 0;
		}

		return true;
	}

	virtual int getAttackCooldown(int spellId)
	{
		if (spellId == 1)
		{
			return compt1;
		}
		else if (spellId == 2)
		{
			return compt2;
		}
		else if (spellId == 3)
		{
			return compt3;
		}

		return 0;
	}

	virtual void setAttackCooldown(int spellId, int value)
	{
		if (spellId == 1)
		{
			compt1 = value;
		}
		else if (spellId == 2)
		{
			compt2 = value;
		}
		else if (spellId == 3)
		{
			compt3 = value;
		}
	}

	virtual void onSpellCast(int spellId)
	{
		// Réarme le cooldown du sort lancé :
		if (spellId == 1)
			compt1 = cd1;
		else if (spellId == 2)
			compt2 = cd2;
		else if (spellId == 3)
			compt3 = cd3;
	}

	virtual void turnStart()
	{
		BaseCharacterModel::turnStart();

		// Décrémentation des cooldowns :
		if (compt1 > 0)
			compt1--;
		if (compt2 > 0)
			compt2--;
		if (compt3 > 0)
			compt3--;
	}

	//Passif : +3 HP sur soi-même pour chaque auto attaque
	virtual std::vector<tw::AttackDamageResult> doAttack1(int targetX, int targetY)
	{
		return std::vector<tw::AttackDamageResult>();
	}

	//Sort 1 : Réanimation (Mana : 5 / Ciblé / DPS : 0 / Cd : usage unique)
	virtual std::vector<tw::AttackDamageResult> doAttack2(int targetX, int targetY)
	{

		return std::vector<tw::AttackDamageResult>();
	}

	//Sort 2 : Heal (Mana : 5 / Ciblé / Heal : +20% des HP max / Cd : 3t / )
	virtual std::vector<tw::AttackDamageResult> doAttack3(int targetX, int targetY)
	{
		return std::vector<tw::AttackDamageResult>();
	}

	//Sort 3 : Purification d'âme (Mana : 2 / Pigme (4) / DPS : 10 / Cd : 2t) Supprime 1 tour de malus chez les alliés touchés
	virtual std::vector<tw::AttackDamageResult> doAttack4(int targetX, int targetY)
	{
		std::vector<tw::AttackDamageResult> result;
		std::vector<tw::Point2D> impactZone = getImpactZoneForSpell(4, targetX, targetY);
		std::vector<tw::BaseCharacterModel*> impactedEntities = getMapKnowledge()->getAliveCharactersInZone(impactZone);

		for (int i = 0; i < impactedEntities.size(); i++)
		{
			result.push_back(tw::AttackDamageResult(impactedEntities[i], 7));
		}

		return result;
	}

	//Auto attaque (Mana : 0 / Corps à corps / DPS : 5 / Cd : 0t)
	virtual std::vector<tw::AttackDamageResult> doAttack5(int targetX, int targetY)
	{
		return std::vector<tw::AttackDamageResult>();
	}

	Protecteur(tw::Environment * environment, int teamId, int currentX, int currentY, tw::IMapKnowledge * map)
		: BaseCharacterModel(environment, teamId, currentX, currentY, map)
	{
		initializeValues();
		compt1 = 99;
		compt2 = 3;
		compt3 = 2;
		cd1 = compt1;
		cd2 = compt2;
		cd3 = compt3;
	}


	// Méthodes rajoutées :

	virtual std::string getClassName()
	{
		return "Protecteur";
	}

	virtual std::string getClassDescription()
	{
		return "Qui vise à protéger, à sauvegarder : Société protectrice des animaux. Qui marque un désir de protection condescendante : Prendre un ton protecteur. Se dit d'un système qui, par des prohibitions, des taxes à l'entrée sur les produits étrangers, favorise l'industrie nationale.";
	}

	virtual std::string getClassIconPath()
	{
		return "./assets/classicons/Priest.png";
	}

	virtual std::string getClassPreviewPath()
	{
		return "./assets/classpreview/Protecteur.png";
	}


	//------------------------------------

	virtual std::string getSpell1Name()
	{
		return "Protecteur sort 1";
	}

	virtual std::string getSpell2Name()
	{
		return "Protecteur sort 2";
	}

	virtual std::string getSpell3Name()
	{
		return "Protecteur sort 3";
	}

	virtual std::string getSpell4Name()
	{
		return "Protecteur sort 4";
	}


	//------------------------------------
	virtual std::string getSpell1Description()
	{
		return "Protecteur, description sort 1 .................................................................................................................................";
	}

	virtual std::string getSpell2Description()
	{
		return "Protecteur, description sort 2 .................................................................................................................................";
	}

	virtual std::string getSpell3Description()
	{
		return "Protecteur, description sort 3 .................................................................................................................................";
	}

	virtual std::string getSpell4Description()
	{
		return "Protecteur, description sort 4 .................................................................................................................................";
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

	//------------------------------------

	virtual std::string getSpell1AnimationPath()
	{
		return "./assets/spellsprites/ballbig1_red";
	}

	virtual std::string getSpell2AnimationPath()
	{

		return "./assets/spellsprites/ballbig1_red";
	}

	virtual std::string getSpell3AnimationPath()
	{
		return "./assets/spellsprites/claw1_red";
	}

	virtual std::string getSpell4AnimationPath()
	{
		return "./assets/spellsprites/claw1_red";
	}

	//------------------------------------

	virtual std::string getSpell1SoundPath()
	{
		return "./assets/sound/explosion1.wav";
	}

	virtual std::string getSpell2SoundPath()
	{
		return "./assets/sound/explosion1.wav";
	}

	virtual std::string getSpell3SoundPath()
	{
		return "./assets/sound/punch3.wav";
	}

	virtual std::string getSpell4SoundPath()
	{
		return "./assets/sound/punch3.wav";
	}

	//------------------------------------

	virtual tw::Animation getSpell1AttackerAnimation()
	{
		return tw::Animation::ATTACK2;
	}

	virtual tw::Animation getSpell2AttackerAnimation()
	{
		return tw::Animation::ATTACK2;
	}

	virtual tw::Animation getSpell3AttackerAnimation()
	{
		return tw::Animation::ATTACK1;
	}

	virtual tw::Animation getSpell4AttackerAnimation()
	{
		return tw::Animation::ATTACK1;
	}
	//--------------------------------------------------------------------
};

