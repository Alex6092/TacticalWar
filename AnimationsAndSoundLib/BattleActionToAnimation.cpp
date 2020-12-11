#include "pch.h"
#include "BattleActionToAnimation.h"

void BattleActionToAnimation::notifyAnimationFinished(float remainingTime)
{
	for (int i = 0; i < Listener.size(); i++)
	{
		Listener[i]->onAnimationFinished(remainingTime);
	}
}
void BattleActionToAnimation::addlistener(IActionAnimationEventListener* l) 
{
	Listener.push_back(l);
}
void BattleActionToAnimation::removelistener(IActionAnimationEventListener* l)
{
	std::vector<IActionAnimationEventListener*>::iterator it = std::find(Listener.begin(), Listener.end(), l);
	if (it != Listener.end())
		Listener.erase(it);
}
void BattleActionToAnimation::start()
{
	elseTime = 0;
}
void BattleActionToAnimation::elapseTime(float deltatime)
{
	elseTime += deltatime;
}
