#pragma once

#include <MoveActionAnimationEventListener.h>
#include "BattleActionToAnimation.h"
#include "IScreenActionCallback.h"

class CharacterMoveAction : public BattleActionToAnimation, MoveActionAnimationEventListener
{
private:
	int persoId;
	std::vector<tw::Point2D> path;
	bool moveAnimationFinished;
	IScreenActionCallback * screen;
	bool firstUpdate;
public:
	CharacterMoveAction(IScreenActionCallback * screen, int persoId, std::vector<tw::Point2D> path)
	{
		this->persoId = persoId;
		this->screen = screen;
		this->path = path;
		moveAnimationFinished = false;
		firstUpdate = true;
	}

	virtual void update(float deltatime)
	{
		if (firstUpdate)
		{
			screen->applyCharacterMove(persoId, path, this);
			firstUpdate = false;
		}
	}

	virtual void onMoveFinished()
	{
		moveAnimationFinished = true;
		notifyAnimationFinished(0);
	}
};

