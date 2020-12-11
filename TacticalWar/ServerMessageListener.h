#pragma once
#include <string>
class ServerMessageListener
{
public:
	virtual void onMessageReceived(std::string msg) = 0;
	virtual void onDisconnected() = 0;
};

