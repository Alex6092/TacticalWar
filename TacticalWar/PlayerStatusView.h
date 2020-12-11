#pragma once

#include <TGUI/TGUI.hpp>
#include "ServerMessageListener.h"

class PlayerStatusView : public tgui::Panel, ServerMessageListener
{
private:
	static std::shared_ptr<PlayerStatusView> instance;
	
	
public:
	PlayerStatusView();
	~PlayerStatusView();
	static std::shared_ptr<PlayerStatusView> getInstance();

	virtual void onMessageReceived(std::string msg);
	virtual void onDisconnected();
};

