#include "MatchView.h"

MatchView::MatchView(tw::Match m, bool isSpectator)
{
	tgui::Label::Ptr matchName = tgui::Label::create();
	sf::String statusStr = " (";
	switch (m.getStatus())
	{
	case tw::MatchStatus::NOT_STARTED:
		statusStr += "Planifié";
		break;

	case tw::MatchStatus::STARTED:
		statusStr += "En cours";
		break;

	case tw::MatchStatus::FINISHED:
		statusStr += "Terminé";
		break;
	}
	statusStr += ")";
	matchName->setText("Match : " + sf::String(m.getMatchName()) + (isSpectator ? "" : statusStr));
	matchName->getRenderer()->setTextColor(sf::Color::Black);
	add(matchName);
	matchName->setPosition(5, 5);

	std::vector<tw::Player> team1 = m.clientGetTeam1();
	std::vector<tw::Player> team2 = m.clientGetTeam2();

	tgui::Label::Ptr vsMessage = tgui::Label::create();
	vsMessage->setText("Equipe " + std::to_string(team1[0].getTeamNumber()) + " VS équipe " + std::to_string(team2[0].getTeamNumber()));
	vsMessage->getRenderer()->setTextColor(sf::Color::Black);
	add(vsMessage);
	vsMessage->setSize(tgui::Layout("100%"), 25);
	vsMessage->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
	vsMessage->setPosition(0, 30);

	tgui::Button::Ptr joinBtn = tgui::Button::create();
	joinBtn->setText(isSpectator ? "Regarder" : "Supprimer");
	if(isSpectator)
		add(joinBtn);
	joinBtn->setPosition(tgui::Layout("78%"), 40);
	joinBtn->setSize(tgui::Layout("20%"), 40);

	for (int i = 0; i < team1.size(); i++)
	{
		tgui::Label::Ptr pseudo = tgui::Label::create();

		pseudo->setText(team1[i].getPseudo());
		pseudo->getRenderer()->setTextColor(sf::Color::Black);
		add(pseudo);
		pseudo->setPosition(5, 55 + 25 * i);
	}

	for (int i = 0; i < team2.size(); i++)
	{
		tgui::Label::Ptr pseudo = tgui::Label::create();

		pseudo->setText(team2[i].getPseudo());
		pseudo->getRenderer()->setTextColor(sf::Color::Black);
		add(pseudo);
		pseudo->setPosition(tgui::Layout("50%"), 55 + 25 * i);
	}
}