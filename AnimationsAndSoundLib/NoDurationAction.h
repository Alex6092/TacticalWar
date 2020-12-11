#pragma once
#include <iostream>
#include "BattleActionToAnimation.h"
class NoDurationAction : public BattleActionToAnimation
{

public : 
 virtual void update(float deltatime);
 virtual void update()=0;
	
};

