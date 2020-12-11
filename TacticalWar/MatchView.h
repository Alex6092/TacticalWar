#pragma once

#include <Match.h>
#include <TGUI/TGUI.hpp>

class MatchView : public tgui::Panel
{
public:
	MatchView(tw::Match m, bool isSpectator = true);

};

