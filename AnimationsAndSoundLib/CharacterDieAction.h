#pragma once
#include <MoveActionAnimationEventListener.h>
#include "BattleActionToAnimation.h"
#include "IScreenActionCallback.h"

class CharacterDieAction :  public BattleActionToAnimation, MoveActionAnimationEventListener
{
private:
	int persoId;
	bool moveAnimationFinished;
	IScreenActionCallback * screen;
	bool firstUpdate;
public:
	CharacterDieAction(IScreenActionCallback * screen, int persoId)
	{
		this->persoId = persoId;
		this->screen = screen;
		moveAnimationFinished = false;
		firstUpdate = true;
	}

	virtual void update(float deltatime)
	{
		if (firstUpdate)
		{
			screen->applyCharacterDie(persoId);
			firstUpdate = false;
		}
	}

	virtual void onMoveFinished()
	{
		moveAnimationFinished = true;
		notifyAnimationFinished(0);
	}

};

