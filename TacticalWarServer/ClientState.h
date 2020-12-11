#pragma once

#include <WS2tcpip.h>
#include <deque>

class ClientState
{
private:
	SOCKET sock;
	std::deque<unsigned char> buffer;

	std::string pseudo;
	bool isAdm;

public:
	ClientState(SOCKET sock);
	virtual ~ClientState();

	inline SOCKET getSocket() {
		return sock;
	}

	inline std::deque<unsigned char> & getBuffer() {
		return buffer;
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

