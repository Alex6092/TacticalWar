#pragma once
#include "Environment.h"
#include <vector>
#include "MoveActionAnimationEventListener.h"
#include "Effect.h"
#include "TypeZoneLaunch.h"
#include "IMapKnowledge.h"
#include "IZoneAndSightCalculator.h"


#include <chrono>
#include <iostream>
#include <ctime>
#include <math.h>

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679
#endif

namespace tw
{
	class BaseCharacterModel;

	enum class Animation
	{
		IDLE,
		RUN,
		ATTACK1,
		ATTACK2,
		DIE,
		TAKE_DAMAGE
	};

	class CharacterEventListener
	{
	public:
		virtual void onPositionChanged(BaseCharacterModel * c, int newPositionX, int newPositionY) = 0;
		virtual void onLookAt(int targetX, int targetY) {}
	};

	class AttackDamageResult
	{
	private:
		BaseCharacterModel * character;
		int damage;

	public:
		AttackDamageResult()
		{
			character = NULL;
			damage = 0;
		}

		AttackDamageResult(BaseCharacterModel * character, int damage)
		{
			this->character = character;
			this->damage = damage;
		}

		BaseCharacterModel * getCharacter()
		{
			return character;
		}

		int getDamage()
		{
			return damage;
		}
	};

	class BaseCharacterModel
	{
	private:
		Animation neededAnimation;
		float animationDuration;
		bool reinitViewTime;

		Environment* environment;

		int teamId;

		int currentX;
		int currentY;

		int colorNumber;	// Colorisation

		//---------------------------------
		// Pour gérer le déplacement :
		float interpolatedX;
		float interpolatedY;

		int currentTargetX;
		int currentTargetY;

		std::vector<Point2D> path;
		//---------------------------------

		bool isReady;

		std::string pseudo;
		int currentLife;
		int currentPM;
		int currentPA;

		void consumePM(int nb)
		{
			currentPM -= nb;
			if (currentPM < 0)
			{
				currentPM = 0;
			}
		}

		void consumePA(int nb)
		{
			currentPA -= nb;
			if (currentPA < 0)
			{
				currentPA = 0;
			}
		}

		void setNextPositionFromPath()
		{
			if (!hasTargetPosition() && path.size() > 0)
			{
				Point2D nextPosition = path.back();
				path.pop_back();
				setTargetPosition(nextPosition.getX(), nextPosition.getY());
			}
			else if (!hasTargetPosition() && path.size() == 0)
			{
				if (currentMoveCallback != NULL)
				{
					currentMoveCallback->onMoveFinished();
					currentMoveCallback = NULL;
				}
			}
		}


		std::vector<CharacterEventListener*> listeners;

		void notifyPositionChanged(int newPositionX, int newPositionY)
		{
			for (int i = 0; i < listeners.size(); i++)
			{
				listeners[i]->onPositionChanged(this, newPositionX, newPositionY);
			}
		}

		MoveActionAnimationEventListener * currentMoveCallback;


		// Liste des effets appliqués sur le personnage :
		std::vector<Effect *> appliedEffects;

		// Gestion du mouvement (server side) :
		long long lastMoveEndTime;

		IMapKnowledge * map;
		IZoneAndSightCalculator * zoneCalculator;

	protected:
		IMapKnowledge * getMapKnowledge()
		{
			return map;
		}

		std::vector<Point2D> getImpactZoneForSpell(int spellId, int targetX, int targetY)
		{
			int spellMinPO = -1;
			int spellMaxPO = -1;
			TypeZoneLaunch zoneType = TypeZoneLaunch::NORMAL;

			switch (spellId)
			{
			case 1:
				spellMinPO = getSpell1ImpactZoneMinPO();
				spellMaxPO = getSpell1ImpactZoneMaxPO();
				zoneType = getSpell1ImpactZoneType();
				break;

			case 2:
				spellMinPO = getSpell2ImpactZoneMinPO();
				spellMaxPO = getSpell2ImpactZoneMaxPO();
				zoneType = getSpell2ImpactZoneType();
				break;

			case 3:
				spellMinPO = getSpell3ImpactZoneMinPO();
				spellMaxPO = getSpell3ImpactZoneMaxPO();
				zoneType = getSpell3ImpactZoneType();
				break;

			case 4:
				spellMinPO = getSpell4ImpactZoneMinPO();
				spellMaxPO = getSpell4ImpactZoneMaxPO();
				zoneType = getSpell4ImpactZoneType();
				break;
			}
			

			std::vector<Point2D> impactZone = zoneCalculator->generateZone(
				targetX,
				targetY,
				spellMinPO,
				spellMaxPO,
				zoneType);

			return impactZone;
		}

	public:
		BaseCharacterModel(Environment* environment, int teamId, int currentX, int currentY, IMapKnowledge * map = NULL)
		{
			this->map = map;
			this->isReady = false;
			this->neededAnimation = Animation::IDLE;
			this->animationDuration = -1;
			this->reinitViewTime = false;

			this->currentMoveCallback = NULL;

			this->teamId = teamId;
			this->environment = environment;
			this->currentX = currentX;
			this->currentY = currentY;

			this->colorNumber = 1;

			setNoTargetPosition();
			lastMoveEndTime = 0;
		}

		void setZoneCalculator(IZoneAndSightCalculator * calculator)
		{
			this->zoneCalculator = calculator;
		}

		std::string getPseudo()
		{
			return pseudo;
		}

		void setPseudo(std::string pseudo)
		{
			this->pseudo = pseudo;
		}

		void initializeValues()
		{
			this->currentLife = getBaseMaxLife();
			this->currentPA = getBasePa();
			this->currentPM = getBasePm();
		}

		virtual ~BaseCharacterModel()
		{

		}

		std::vector<Effect *> getAppliedEffects()
		{
			return appliedEffects;
		}

		// Méthode permettant d'appliquer des effets sur le personnage
		void addEffects(std::vector<Effect*> effects)
		{
			// TODO ...
		}

		bool isAlive()
		{
			return currentLife > 0;
		}

		void modifyCurrentLife(int value)
		{
			currentLife += value;
			if (currentLife > getBaseMaxLife())
			{
				currentLife = getBaseMaxLife();
			}
			else if (currentLife < 0)
			{
				currentLife = 0;
			}
		}

		void setCurrentLife(int life)
		{
			currentLife = life;
		}

		bool hasEnoughPM(int neededPM)
		{
			return currentPM >= neededPM;
		}

		bool hasEnoughPA(int neededPA)
		{
			return currentPA >= neededPA;
		}

		int getCurrentPM()
		{
			return currentPM;
		}

		void setCurrentPM(int pm)
		{
			currentPM = pm;
		}

		int getCurrentPA()
		{
			return currentPA;
		}

		void setCurrentPA(int pa)
		{
			currentPA = pa;
		}

		void resetPM()
		{
			currentPM = getBasePm();
		}

		void resetPA()
		{
			currentPA = getBasePa();
		}

		virtual void turnStart()
		{
			resetPA();
			resetPM();
		}

		virtual int getClassId() = 0;
		virtual std::string getGraphicsPath() = 0;

		// Méthodes rajoutées :
		virtual std::string getClassName() = 0;
		virtual std::string getClassDescription() = 0;
		virtual std::string getClassIconPath() = 0;
		virtual std::string getClassPreviewPath() = 0;




		virtual std::string getSpell1Name() = 0;
		virtual std::string getSpell2Name() = 0;
		virtual std::string getSpell3Name() = 0;
		virtual std::string getSpell4Name() = 0;

		virtual std::string getSpell1Description() = 0;
		virtual std::string getSpell2Description() = 0;
		virtual std::string getSpell3Description() = 0;
		virtual std::string getSpell4Description() = 0;

		virtual std::string getSpell1IconPath() = 0;
		virtual std::string getSpell2IconPath() = 0;
		virtual std::string getSpell3IconPath() = 0;
		virtual std::string getSpell4IconPath() = 0;

		virtual std::vector<Effect> getSpell1Effects() = 0;
		virtual std::vector<Effect> getSpell2Effects() = 0;
		virtual std::vector<Effect> getSpell3Effects() = 0;
		virtual std::vector<Effect> getSpell4Effects() = 0;

		virtual int getSpell1ManaCost() = 0;
		virtual int getSpell2ManaCost() = 0;
		virtual int getSpell3ManaCost() = 0;
		virtual int getSpell4ManaCost() = 0;

		virtual int getSpell1MinPO() = 0;
		virtual int getSpell2MinPO() = 0;
		virtual int getSpell3MinPO() = 0;
		virtual int getSpell4MinPO() = 0;

		virtual int getSpell1MaxPO() = 0;
		virtual int getSpell2MaxPO() = 0;
		virtual int getSpell3MaxPO() = 0;
		virtual int getSpell4MaxPO() = 0;

		virtual TypeZoneLaunch getSpell1LaunchZoneType() = 0;
		virtual TypeZoneLaunch getSpell2LaunchZoneType() = 0;
		virtual TypeZoneLaunch getSpell3LaunchZoneType() = 0;
		virtual TypeZoneLaunch getSpell4LaunchZoneType() = 0;

		virtual TypeZoneLaunch getSpell1ImpactZoneType() = 0;
		virtual TypeZoneLaunch getSpell2ImpactZoneType() = 0;
		virtual TypeZoneLaunch getSpell3ImpactZoneType() = 0;
		virtual TypeZoneLaunch getSpell4ImpactZoneType() = 0;

		virtual int getSpell1ImpactZoneMinPO() = 0;
		virtual int getSpell2ImpactZoneMinPO() = 0;
		virtual int getSpell3ImpactZoneMinPO() = 0;
		virtual int getSpell4ImpactZoneMinPO() = 0;

		virtual int getSpell1ImpactZoneMaxPO() = 0;
		virtual int getSpell2ImpactZoneMaxPO() = 0;
		virtual int getSpell3ImpactZoneMaxPO() = 0;
		virtual int getSpell4ImpactZoneMaxPO() = 0;

		virtual std::string getSpell1AnimationPath() = 0;
		virtual std::string getSpell2AnimationPath() = 0;
		virtual std::string getSpell3AnimationPath() = 0;
		virtual std::string getSpell4AnimationPath() = 0;

		virtual std::string getSpell1SoundPath() = 0;
		virtual std::string getSpell2SoundPath() = 0;
		virtual std::string getSpell3SoundPath() = 0;
		virtual std::string getSpell4SoundPath() = 0;

		virtual Animation getSpell1AttackerAnimation() = 0;
		virtual Animation getSpell2AttackerAnimation() = 0;
		virtual Animation getSpell3AttackerAnimation() = 0;
		virtual Animation getSpell4AttackerAnimation() = 0;
		//----------------------------------------------------------

		// Retourne la valeur du maximum de point de vie de base (sans altération d'effet). C'est une caractéristique de base de la classe.
		virtual int getBaseMaxLife() = 0;
		virtual int getBaseAttack() = 0;
		virtual int getBaseDefense() = 0;
		virtual int getBasePa() = 0;
		virtual int getBasePm() = 0;


		// Ces méthodes permettent de lancer les attaques 
		// (c'est à dire appliquer le cooldown quand il y en a un, 
		// trouver les cibles et leur appliquer les effets, etc...)
		virtual std::vector<AttackDamageResult> doAttack1(int targetX, int targetY) = 0;
		virtual std::vector<AttackDamageResult> doAttack2(int targetX, int targetY) = 0;
		virtual std::vector<AttackDamageResult> doAttack3(int targetX, int targetY) = 0;
		virtual std::vector<AttackDamageResult> doAttack4(int targetX, int targetY) = 0;
		virtual std::vector<AttackDamageResult> doAttack5(int targetX, int targetY) = 0;

		inline int getTeamId() {
			return teamId;
		}

		inline int getCurrentX() {
			return currentX;
		}

		inline int getCurrentY() {
			return currentY;
		}

		inline void setCurrentX(int x)
		{
			currentX = x;
		}

		inline void setCurrentY(int y)
		{
			currentY = y;
		}

		inline Environment* getEnvironment()
		{
			return environment;
		}

		inline int getCurrentLife()
		{
			return currentLife;
		}

		inline bool isPlayerReady()
		{
			return isReady;
		}

		inline void setReadyStatus(bool ready)
		{
			isReady = ready;
		}


		inline float getInterpolatedX()
		{
			return interpolatedX;
		}

		inline float getInterpolatedY()
		{
			return interpolatedY;
		}

		inline float getSpeed()
		{
			return 3.0;
		}

		inline void update(float deltatime)
		{
			float speed = getSpeed();

			setNextPositionFromPath();

			if (currentTargetX >= 0 && currentTargetY >= 0)
			{
				// 1) Déterminer la direction
				// 2) Effectuer le mouvement dans la bonne direction sur les coordonnées interpolées
				// 3) Déterminer si le mouvement vers la cible est terminé
				// Si le mouvement est terminé :
				//		4) Mettre à jour la position courante
				//		5) Appeler setNoTargetPosition si le mouvement est terminé

				float moveXVector = 0;
				float moveYVector = 0;

				if (currentX < currentTargetX)
				{
					moveXVector = 1;
				}
				else if (currentX > currentTargetX)
				{
					moveXVector = -1;
				}

				if (currentY < currentTargetY)
				{
					moveYVector = 1;
				}
				else if (currentY > currentTargetY)
				{
					moveYVector = -1;
				}

				interpolatedX += moveXVector * deltatime * speed;
				interpolatedY += moveYVector * deltatime * speed;


				bool isMoveFinished = (moveXVector > 0 && interpolatedX > currentTargetX || moveXVector < 0 && interpolatedX < currentTargetX)
					||
					(moveYVector > 0 && interpolatedY > currentTargetY || moveYVector < 0 && interpolatedY < currentTargetY);

				if (isMoveFinished)
				{
					currentX = currentTargetX;
					currentY = currentTargetY;

					interpolatedX = currentX;
					interpolatedY = currentY;

					setNoTargetPosition();

					// On ne notifie qu'à la fin du déplacement (but : éviter les freeze à chaque
					// changement de cellule).
					if (path.size() == 0)
						notifyPositionChanged(currentX, currentY);

					setNextPositionFromPath();
				}
			}
			else
			{
				interpolatedX = currentX;
				interpolatedY = currentY;
			}
		}

		inline long long getLastMoveEndTime()
		{
			return lastMoveEndTime;
		}

		// Le mouvement n'est pas terminé si le temps n'est pas dépassé.
		inline bool isMoving()
		{
			return lastMoveEndTime > std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		}

		void serverSetPath(std::vector<Point2D> path)
		{
			if (path.size() > 0)
			{
				setCurrentX(path[0].getX());
				setCurrentY(path[0].getY());
				consumePM(path.size());
				long long currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				lastMoveEndTime = currentTime + (path.size() * (1000.0 / getSpeed()));
			}
		}


		void setPath(std::vector<Point2D> path, MoveActionAnimationEventListener * callback = NULL)
		{
			consumePM(path.size());
			this->path = path;
			this->currentMoveCallback = callback;
		}

		inline void setTargetPosition(int x, int y)
		{
			this->currentTargetX = x;
			this->currentTargetY = y;
		}

		inline void setNoTargetPosition()
		{
			this->currentTargetX = -1;
			this->currentTargetY = -1;
		}

		inline bool hasTargetPosition()
		{
			return (currentTargetX >= 0 && currentTargetY >= 0);
		}

		inline int getTargetX()
		{
			return currentTargetX;
		}

		inline int getTargetY()
		{
			return currentTargetY;
		}

		void addEventListener(CharacterEventListener * l)
		{
			listeners.push_back(l);
			l->onPositionChanged(this, currentX, currentY);
		}

		void removeEventListener(CharacterEventListener * l)
		{
			std::vector<CharacterEventListener*>::iterator it = std::find(listeners.begin(), listeners.end(), l);
			if (it != listeners.end())
			{
				listeners.erase(it);
			}
		}




		Animation getNeededAnimation()
		{
			return neededAnimation;
		}

		float getAnimationDuration()
		{
			return animationDuration;
		}

		bool getReinitViewTime()
		{
			bool result = reinitViewTime;
			reinitViewTime = false;
			return result;
		}

		void startAttack1Animation(float duration)
		{
			neededAnimation = Animation::ATTACK1;
			animationDuration = duration;
			reinitViewTime = true;
		}

		void startAttack2Animation(float duration)
		{
			neededAnimation = Animation::ATTACK2;
			animationDuration = duration;
			reinitViewTime = true;
		}

		void resetAnimation()
		{
			neededAnimation = Animation::IDLE;
			animationDuration = -1;
		}
		void startDieAction(float duration)
		{
			neededAnimation = Animation::DIE;
			animationDuration = duration;
			reinitViewTime = true;
		}
		void startTakeDmg(float duration)
		{
			neededAnimation = Animation::TAKE_DAMAGE;
			animationDuration = duration;
			reinitViewTime = true;
		}

		// A redefinir dans les classes filles pour la gestion des cooldowns :
		// Cette méthode ne prend pas en compte le nombre de PA disponible.
		// Elle ne permet que de savoir si le cooldown est passé.
		virtual bool canDoAttack(int spellId) {
			return true;
		}

		// A redefinir dans les classes filles pour la gestion des cooldowns :
		virtual int getAttackCooldown(int spellId)
		{
			return 0;
		}

		virtual void setAttackCooldown(int spellId, int value)
		{

		}

		int getAttackPACost(int spellId)
		{
			if (spellId == 1)
			{
				return getSpell1ManaCost();
			}
			else if (spellId == 2)
			{
				return getSpell2ManaCost();
			}
			else if (spellId == 3)
			{
				return getSpell3ManaCost();
			}
			else if (spellId == 4)
			{
				return getSpell4ManaCost();
			}

			return -1;
		}

		std::vector<AttackDamageResult> doAttack(int spellId, int targetX, int targetY)
		{
			int paCost = getAttackPACost(spellId);
			if (paCost != -1)
			{
				consumePA(paCost);
				if (spellId == 1)
				{
					return doAttack1(targetX, targetY);
				}
				else if (spellId == 2)
				{
					return doAttack2(targetX, targetY);
				}
				else if (spellId == 3)
				{
					return doAttack3(targetX, targetY);
				}
				else if (spellId == 4)
				{
					return doAttack4(targetX, targetY);
				}
			}

			return std::vector<AttackDamageResult>();
		}

		std::string getSpellAnimationPath(int spellId)
		{
			if (spellId == 1)
			{
				return getSpell1AnimationPath();
			}
			else if (spellId == 2)
			{
				return getSpell2AnimationPath();
			}
			else if (spellId == 3)
			{
				return getSpell3AnimationPath();
			}
			else if (spellId == 4)
			{
				return getSpell4AnimationPath();
			}

			return "";
		}

		std::string getSpellSoundPath(int spellId)
		{
			if (spellId == 1)
			{
				return getSpell1SoundPath();
			}
			else if (spellId == 2)
			{
				return getSpell2SoundPath();
			}
			else if (spellId == 3)
			{
				return getSpell3SoundPath();
			}
			else if (spellId == 4)
			{
				return getSpell4SoundPath();
			}

			return "";
		}

		Animation getSpellAttackerAnimation(int spellId)
		{
			if (spellId == 1)
			{
				return getSpell1AttackerAnimation();
			}
			else if (spellId == 2)
			{
				return getSpell2AttackerAnimation();
			}
			else if (spellId == 3)
			{
				return getSpell3AttackerAnimation();
			}
			else if (spellId == 4)
			{
				return getSpell4AttackerAnimation();
			}

			return Animation::ATTACK1;
		}

		std::string getSpellIconPath(int spellId)
		{
			switch (spellId)
			{
			case 1:
				return getSpell1IconPath();
				break;

			case 2:
				return getSpell2IconPath();
				break;

			case 3:
				return getSpell3IconPath();
				break;

			case 4:
				return getSpell4IconPath();
				break;
			}

			return "";
		}

		std::string getSpellName(int spellId)
		{
			if (spellId == 1)
			{
				return getSpell1Name();
			}
			else if (spellId == 2)
			{
				return getSpell2Name();
			}
			else if (spellId == 3)
			{
				return getSpell3Name();
			}
			else if (spellId == 4)
			{
				return getSpell4Name();
			}

			return "Undefined";
		}

		std::string getSpellDescription(int spellId)
		{
			if (spellId == 1)
			{
				return getSpell1Description();
			}
			else if (spellId == 2)
			{
				return getSpell2Description();
			}
			else if (spellId == 3)
			{
				return getSpell3Description();
			}
			else if (spellId == 4)
			{
				return getSpell4Description();
			}

			return "Undefined";
		}

		void setOrientationToLookAt(int targetX, int targetY)
		{
			for (int i = 0; i < listeners.size(); i++)
			{
				listeners[i]->onLookAt(targetX, targetY);
			}
		}

		int getColorNumber()
		{
			return colorNumber;
		}

		void setColorNumber(int colorNumber)
		{
			this->colorNumber = colorNumber;
		}
	};
}