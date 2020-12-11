#include "pch.h"
#include "Player.h"

tw::Player::Player(std::string pseudo, std::string password, int teamId)
{
	this->pseudo = pseudo;
	this->password = password;
	this->teamNumber = teamId;
	this->hasJoinBattle = false;
	this->character = NULL;
}