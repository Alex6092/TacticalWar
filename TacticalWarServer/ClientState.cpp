#include "ClientState.h"



ClientState::ClientState(SOCKET sock)
{
	this->sock = sock;
	this->pseudo = "";
	this->isAdm = false;
}

ClientState::~ClientState()
{

}
