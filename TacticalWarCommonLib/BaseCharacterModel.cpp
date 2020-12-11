#include "pch.h"
#include "BaseCharacterModel.h"

using namespace tw;

BaseCharacterModel::BaseCharacterModel(Environment* environment, int teamId, int currentX, int currentY)
{
	this->neededAnimation = Animation::IDLE;
	this->animationDuration = -1;
	this->reinitViewTime = false;

	this->currentMoveCallback = NULL;

	this->teamId = teamId;
	this->environment = environment;
	this->currentX = currentX;
	this->currentY = currentY;

	setNoTargetPosition();
}

BaseCharacterModel::~BaseCharacterModel()
{

}