#pragma once

#include <string>
#include "net/NetServer.h"

// État d'un client connecté au serveur.
class ClientState
{
private:
	tw::net::ConnId connId;
	std::string remoteAddress;
	std::string pseudo;
	bool isAdm;

public:
	ClientState(tw::net::ConnId connId, const std::string & remoteAddress);
	virtual ~ClientState();

	inline tw::net::ConnId getConnId() {
		return connId;
	}

	inline const std::string & getRemoteAddress() {
		return remoteAddress;
	}

	inline void setPseudo(std::string pseudo)
	{
		this->pseudo = pseudo;
	}

	inline std::string getPseudo()
	{
		return pseudo;
	}

	inline bool isSpectator()
	{
		return pseudo.length() == 0;
	}

	inline bool isAdmin()
	{
		return isAdm;
	}

	inline void setIsAdmin(bool bAdmin)
	{
		this->isAdm = bAdmin;
	}
};
