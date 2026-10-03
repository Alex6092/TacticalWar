#pragma once
#include "Environment.h"
#include <algorithm>
#include <string>
#include <vector>
#include "MoveActionAnimationEventListener.h"

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

	// Personnage affiché par le client : position (interpolée pendant les déplacements),
	// animation en cours, PV / PA / PM affichés. Les règles du jeu (sorts, dégâts, effets) sont
	// appliquées par le serveur (BattleEngineLib) : ce modèle ne fait qu'afficher leur résultat.
	// Les classes filles (ClassesLib) indiquent le dossier des graphismes de la classe.
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
		// Déplacement subi (poussée, attraction, bond) : plus rapide, sans courir ni se retourner.
		bool sliding;
		float slideSpeed;
		//---------------------------------

		bool isReady;

		std::string pseudo;
		int currentLife;
		int currentPM;
		int currentPA;
		int displayMaxLife;
		// Bouclier en cours (absorbe les dégâts avant les PV), affiché dans un écusson.
		int currentShield;

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

	public:
		BaseCharacterModel(Environment* environment, int teamId, int currentX, int currentY)
		{
			this->isReady = false;
			this->neededAnimation = Animation::IDLE;
			this->animationDuration = -1;
			this->reinitViewTime = false;

			this->currentMoveCallback = NULL;

			this->teamId = teamId;
			this->environment = environment;
			this->currentX = currentX;
			this->currentY = currentY;
			this->interpolatedX = (float)currentX;
			this->interpolatedY = (float)currentY;
			this->sliding = false;
			this->slideSpeed = 0;

			this->colorNumber = 1;
			this->currentLife = 1;
			this->currentPA = 0;
			this->currentPM = 0;
			this->displayMaxLife = -1;
			this->currentShield = 0;

			setNoTargetPosition();
		}

		virtual ~BaseCharacterModel()
		{
		}

		// Identifiant de la classe (voir assets/data/gamedata.json) et dossier de ses graphismes.
		virtual int getClassId() = 0;
		virtual std::string getGraphicsPath() = 0;

		std::string getPseudo()
		{
			return pseudo;
		}

		void setPseudo(std::string pseudo)
		{
			this->pseudo = pseudo;
		}

		bool isAlive()
		{
			return currentLife > 0;
		}

		void setCurrentLife(int life)
		{
			currentLife = life;
		}

		inline int getCurrentLife()
		{
			return currentLife;
		}

		// PV max affichés (fournis par le serveur : ils baissent avec l'érosion).
		void setDisplayMaxLife(int maxLife)
		{
			displayMaxLife = maxLife;
		}

		int getDisplayMaxLife()
		{
			return displayMaxLife > 0 ? displayMaxLife : std::max(1, currentLife);
		}

		void setCurrentShield(int shield)
		{
			currentShield = shield > 0 ? shield : 0;
		}

		int getCurrentShield()
		{
			return currentShield;
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

		// Vitesse de déplacement, en cases par seconde.
		inline float getSpeed()
		{
			return sliding ? slideSpeed : 3.0f;
		}

		inline bool isSliding()
		{
			return sliding;
		}

		inline void update(float deltatime)
		{
			float speed = getSpeed();

			setNextPositionFromPath();

			if (currentTargetX >= 0 && currentTargetY >= 0)
			{
				// Déplacement case par case : on avance les coordonnées interpolées vers la case
				// cible, puis on passe à la case suivante du chemin.
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
					{
						sliding = false;
						notifyPositionChanged(currentX, currentY);
					}

					setNextPositionFromPath();
				}
			}
			else
			{
				interpolatedX = currentX;
				interpolatedY = currentY;
			}
		}

		// Chemin à parcourir (destination en premier, premier pas en dernier).
		void setPath(std::vector<Point2D> path, MoveActionAnimationEventListener * callback = NULL)
		{
			this->path = path;
			this->currentMoveCallback = callback;
			this->sliding = false;
		}

		// Déplacement subi (poussée, attraction, bond), à "cellsPerSecond" cases par seconde : le
		// personnage garde son orientation et son animation.
		void slide(std::vector<Point2D> path, float cellsPerSecond, MoveActionAnimationEventListener * callback = NULL)
		{
			setPath(path, callback);
			this->sliding = true;
			this->slideSpeed = cellsPerSecond;
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
