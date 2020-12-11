#pragma once

#include <Player.h>
#include <vector>
#include "Match.h"
#include <StringUtils.h>

namespace tw
{
	class PlayerManager
	{
		static std::vector<Player*> playersCache;
		static std::vector<Match*> matchList;

	public:
		static std::vector<Player*> loadPlayers();

		static std::vector<Match*> getAllMatchsForPlayer(Player * p);
		static Match * getCurrentOrNextMatchForPlayer(Player * p);
		static std::vector<Match*> getCurrentlyPlayingMatchs();
		static std::vector<Match*> getPlanifiedAndPlayingMatchs();


		//static void subscribeToAllMatchEvent(MatchEventListener * l);

		static void addMatch(Match * m);
	};
}
