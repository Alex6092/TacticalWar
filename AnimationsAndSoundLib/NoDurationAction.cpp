#include "pch.h"
#include "NoDurationAction.h"

void NoDurationAction::update(float deltatime)
{
	update();
	notifyAnimationFinished(deltatime);
}