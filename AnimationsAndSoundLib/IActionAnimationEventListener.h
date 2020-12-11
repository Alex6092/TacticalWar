#pragma once
class IActionAnimationEventListener
{
public:
	virtual void onAnimationFinished(float remainingTime) = 0;
};

