#pragma once
#include "Environment.h"
#include <vector>

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
		// Pour g�rer le d�placement :
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
		}


		std::vector<CharacterEventListener*> listeners;

		void notifyPositionChanged(int newPositionX, int newPositionY)
		{
			for (int i = 0; i < listeners.size(); i++)
			{
				listeners[i]->onPositionChanged(this, newPositionX, newPositionY);
			}
		}


	public:
		BaseCharacterModel(Environment* environment, int teamId, int currentX, int currentY);
		virtual ~BaseCharacterModel();


		virtual int getClassId() = 0;
		virtual std::string getGraphicsPath() = 0;

		// Retourne la valeur du maximum de point de vie de base (sans alt�ration d'effet). C'est une caract�ristique de base de la classe.
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
				// 1) D�terminer la direction
				// 2) Effectuer le mouvement dans la bonne direction sur les coordonn�es interpol�es
				// 3) D�terminer si le mouvement vers la cible est termin�
				// Si le mouvement est termin� :
				//		4) Mettre � jour la position courante
				//		5) Appeler setNoTargetPosition si le mouvement est termin�

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

					// On ne notifie qu'� la fin du d�placement (but : �viter les freeze � chaque
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

		void setPath(std::vector<Point2D> path)
		{
			this->path = path;
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