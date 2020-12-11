#pragma once
#include <MoveActionAnimationEventListener.h>
#include "BattleActionToAnimation.h"
#include "IScreenActionCallback.h"

class LaunchSpellAction : public BattleActionToAnimation, MoveActionAnimationEventListener
{
private:
	int persoId;
	int spellId;
	int x, y;
	bool moveAnimationFinished;
	IScreenActionCallback * screen;
	bool firstUpdate;

	bool hasResetAttackAnimation;
public:
	LaunchSpellAction(IScreenActionCallback * screen, int persoId, int spellId, int x,int y)
	{
		this->persoId = persoId;
		this->spellId = spellId;
		this->screen = screen;
		this->x = x;
		this->y = y;
		moveAnimationFinished = false;
		firstUpdate = true;
		hasResetAttackAnimation = false;
	}

	virtual void update(float deltatime)
	{
		elapseTime(deltatime);
		if (firstUpdate)
		{
			tw::Player * srcPlayer = screen->getCharacter(persoId);
			srcPlayer->getCharacter()->startAttack1Animation(1);
			firstUpdate = false;
		}

		if (getEllapsedTime() > 1000 && !hasResetAttackAnimation)
		{
			tw::Player * srcPlayer = screen->getCharacter(persoId);
			srcPlayer->getCharacter()->resetAnimation();
			hasResetAttackAnimation = true;

			screen->applyCharacterLaunchSpell(persoId, x, y, spellId);
			notifyAnimationFinished(0);
			delete this;
		}

		/*
		if (hasResetAttackAnimation && !? ? ? ? )
		{

		}
		*/
	}

	virtual void onMoveFinished()
	{
		moveAnimationFinished = true;
		notifyAnimationFinished(0);
	}
};