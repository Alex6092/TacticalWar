#pragma once
#include <stdio.h>
#include "BattleActionToAnimation.h"
#include <deque>
class AnimationManager : public IActionAnimationEventListener 
{
	private :
		static AnimationManager * instance;

		std::deque<BattleActionToAnimation*> animations;
		float remainingTime;

	public:
		static AnimationManager * getInstance();
		virtual void update(float datatime);
		virtual void onAnimationFinished(float remainingTime);
		virtual void clear();

		void addAnimation(BattleActionToAnimation* anim)
		{
			animations.push_back(anim);
			anim->addlistener(this);

			// The first animation of an empty queue must be started explicitly
			// (the following ones are started by onAnimationFinished) :
			if (animations.size() == 1)
				anim->start();
		}
};

