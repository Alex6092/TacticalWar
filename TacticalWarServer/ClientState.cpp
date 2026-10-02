#include "ClientState.h"

ClientState::ClientState(tw::net::ConnId connId, const std::string & remoteAddress)
{
	this->connId = connId;
	this->remoteAddress = remoteAddress;
	this->pseudo = "";
	this->isAdm = false;
}

ClientState::~ClientState()
{

}
