#pragma once
#include "Environment.h"
#include <vector>
#include "MoveActionAnimationEventListener.h"
#include "Effect.h"
#include "TypeZoneLaunch.h"

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

		//---------------------------------
		// Pour gérer le déplacement :
		float interpolatedX;
		float interpolatedY;

		int currentTargetX;
		int currentTargetY;

		std::vector<Point2D> path;
		//---------------------------------

		int currentLife;

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

	public:
		BaseCharacterModel(Environment* environment, int teamId, int currentX, int currentY);
		virtual ~BaseCharacterModel();

		std::vector<Effect *> getAppliedEffects()
		{
			return appliedEffects;
		}

		// Méthode permettant d'appliquer des effets sur le personnage
		void addEffects(std::vector<Effect*> effects)
		{
			// TODO ...
		}


		virtual int getClassId() = 0;
		virtual std::string getGraphicsPath() = 0;

		// Méthodes rajoutées :
		virtual std::string getClassName() = 0;
		virtual std::string getClassDescription() = 0;
		virtual std::string getClassIconPath() = 0;


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
		//----------------------------------------------------------

		// Retourne la valeur du maximum de point de vie de base (sans altération d'effet). C'est une caractéristique de base de la classe.
		virtual int getBaseMaxLife() = 0;
		virtual int getBaseAttack() = 0;
		virtual int getBaseDefense() = 0;
		virtual int getBasePa() = 0;
		virtual int getBasePm() = 0;

		virtual bool doAttack1(int targetX, int targetY) = 0;
		virtual bool doAttack2(int targetX, int targetY) = 0;
		virtual bool doAttack3(int targetX, int targetY) = 0;
		virtual bool doAttack4(int targetX, int targetY) = 0;
		virtual bool doAttack5(int targetX, int targetY) = 0;

		inline int getTeamId() {
			return teamId;
		}

		inline int getCurrentX() {
			return currentX;
		}

		inline int getCurrentY() {
			return currentY;
		}

		inline Environment* getEnvironment()
		{
			return environment;
		}

		inline int getCurrentLife()
		{
			return currentLife;
		}



		inline float getInterpolatedX()
		{
			return interpolatedX;
		}

		inline float getInterpolatedY()
		{
			return interpolatedY;
		}

		inline void update(float deltatime)
		{
			float speed = 3;
			
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
				else if(currentY > currentTargetY)	
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
					if(path.size() == 0)
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



		void setPath(std::vector<Point2D> path, MoveActionAnimationEventListener * callback = NULL)
		{
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

		void resetAnimation()
		{
			neededAnimation = Animation::IDLE;
			animationDuration = -1;
		}
	};
}