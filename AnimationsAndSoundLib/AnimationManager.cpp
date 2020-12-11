#include "pch.h"
#include "AnimationManager.h"

AnimationManager * AnimationManager::instance = NULL;
AnimationManager * AnimationManager::getInstance()
{
	if (instance == NULL)
		instance = new AnimationManager();

	return instance;
}

 void AnimationManager::update(float datatime)
{
	 remainingTime = datatime * 1000.0;
	 BattleActionToAnimation * lastUpdated = NULL;

	 while (animations.size() > 0 && remainingTime > 0 && lastUpdated != animations[0])
	 {
		 if (animations.size() > 0)
		 {
			 BattleActionToAnimation * updated = animations[0];
			 updated->update(remainingTime);
			 lastUpdated = updated;
		 }
	 }
}
void AnimationManager::onAnimationFinished(float remainingTime) 
{
	this->remainingTime = remainingTime;
	animations.pop_front();

	if (animations.size() > 0)
		animations[0]->start();
}
void AnimationManager::clear()
{
	animations.clear();
}
