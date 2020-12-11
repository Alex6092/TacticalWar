#pragma once
#include <MoveActionAnimationEventListener.h>
#include "BattleActionToAnimation.h"
#include "IScreenActionCallback.h"
class ChangeTurnAction : public BattleActionToAnimation, MoveActionAnimationEventListener
{
private:
	int persoId;
	float Remaining;
	bool moveAnimationFinished;
	std::string message;
	IScreenActionCallback * screen;
	bool firstUpdate;
public:
	ChangeTurnAction(IScreenActionCallback * screen, int persoId,std::string message, float Remaining)
	{
		this->persoId = persoId;
		this->screen = screen;
		this->message = message;
		this->Remaining = Remaining;
		moveAnimationFinished = false;
		firstUpdate = true;
	}

	virtual void update(float deltatime)
	{
		if (firstUpdate)
		{
			//screen->getCharacter(persoId)->getCharacter()->DECREMENT_COOLDOWN();
			screen->applyChangeTurn(Remaining,persoId,message);
			firstUpdate = false;
		}
	}

	virtual void onMoveFinished()
	{
		moveAnimationFinished = true;
		notifyAnimationFinished(0);
	}

};

