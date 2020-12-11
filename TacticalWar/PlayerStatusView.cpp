#include "PlayerStatusView.h"
#include <StringUtils.h>
#include "LinkToServer.h"

std::shared_ptr<PlayerStatusView> PlayerStatusView::instance = std::make_shared<PlayerStatusView>();

std::shared_ptr<PlayerStatusView> PlayerStatusView::getInstance()
{
	return instance;
}

PlayerStatusView::PlayerStatusView()
	: tgui::Panel(tgui::Layout2d(tgui::Layout(200), tgui::Layout(100)))
{
	setPosition(10, 10);
	getRenderer()->setBackgroundColor(tgui::Color(128, 128, 128, 128));

	tgui::Label::Ptr title = tgui::Label::create("Status joueurs :");
	add(title);
	title->setPosition(5, 5);
	title->getRenderer()->setTextColor(tgui::Color::White);

	tgui::Label::Ptr joueurs = tgui::Label::create("");
	add(joueurs, "playerLabel");
	joueurs->setPosition(5, 30);
	joueurs->getRenderer()->setTextColor(tgui::Color::White);

	LinkToServer::getInstance()->addListener(this);
}

PlayerStatusView::~PlayerStatusView()
{
	LinkToServer::getInstance()->removeListener(this);
}


void PlayerStatusView::onMessageReceived(std::string msg)
{
	sf::String m = msg;

	// Status des joueurs du match :
	if (m.substring(0, 2) == "PS")
	{
		std::string toDisplay = "";
		std::vector<std::string> payloadData = StringUtils::explode(m.substring(2), ';');

		for (int i = 0; i < payloadData.size(); i++)
		{
			std::vector<std::string> playerInfo = StringUtils::explode(payloadData[i], ',');
			std::string name = playerInfo[0];
			int state = std::atoi(playerInfo[1].c_str());
			std::string stateStr = "";
			if (state == 1)
			{
				stateStr = "Connecté";
			}
			else
			{
				stateStr = "En attente";
			}

			toDisplay += " - " +  name + " (" + stateStr + ")" + "\n";
		}

		tgui::Label::Ptr playerLabel = get<tgui::Label>("playerLabel");
		playerLabel->setText(toDisplay);
	}
}

void PlayerStatusView::onDisconnected()
{
	// TODO
}
