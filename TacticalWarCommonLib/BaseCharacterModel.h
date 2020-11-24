#pragma once
#include "Environment.h"
#include <vector>

namespace tw
{
	class BaseCharacterModel
	{
	private:
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


	public:
		BaseCharacterModel(Environment* environment, int teamId, int currentX, int currentY);
		virtual ~BaseCharacterModel();


		virtual int getClassId() = 0;
		virtual std::string getGraphicsPath() = 0;

		// Retourne la valeur du maximum de point de vie de base (sans alt�ration d'effet). C'est une caract�ristique de base de la classe.
		virtual int getBaseMaxLife() = 0;
		virtual int getBasePhysicalAttack() = 0;
		virtual int getBaseMagicalAttack() = 0;
		virtual int getBasePhysicalDefense() = 0;
		virtual int getBaseMagicalDefense() = 0;
		virtual bool doAttack1(int targetX, int targetY) = 0;
		virtual bool doAttack2(int targetX, int targetY) = 0;
		virtual bool doAttack3(int targetX, int targetY) = 0;
		virtual bool doAttack4(int targetX, int targetY) = 0;

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
	};
}