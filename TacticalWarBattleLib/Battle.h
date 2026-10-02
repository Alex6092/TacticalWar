#pragma once

#include <vector>
#include <Player.h>
#include <Match.h>
#include <Environment.h>
#include <EnvironmentManager.h>

enum BattleState {
	WAITING_PLAYER_PHASE,
	PREPARATION_PHASE,
	BATTLE_PHASE,
	BATTLE_PHASE_ACTIVE_PLAYER_TURN,	 //For client side
	END_PHASE
};

class Battle;

class BattleEventListener
{
public:
	virtual void onBattleStateChanged(tw::Match * m, BattleState state) = 0;
	virtual void onPlayerTurnStart(tw::Match * match, tw::Player * player) = 0;
};

class Battle
{
	BattleState state;

	std::vector<tw::Player*> team1;
	std::vector<tw::Player*> team2;


	std::vector<tw::Player*> timeline;
	int turnToken;

	tw::Environment * environment;
	tw::Match * match;

	std::vector<BattleEventListener*> listeners;

	void notifyBattleStateChanged(BattleState state)
	{
		for (int i = 0; i < listeners.size(); i++)
		{
			listeners[i]->onBattleStateChanged(match, state);
		}
	}

	void notifyPlayerTurnStart(tw::Player * p)
	{
		for (int i = 0; i < listeners.size(); i++)
		{
			listeners[i]->onPlayerTurnStart(match, p);
		}
	}

public:
	// Server side constructor :
	Battle(tw::Match * m)
	{
		this->match = m;
		std::vector<tw::Player*> & t1 = m->getTeam1();
		std::vector<tw::Player*> & t2 = m->getTeam2();

		for (int i = 0; i < t1.size(); i++)
		{
			team1.push_back(t1[i]);
		}

		for (int i = 0; i < t2.size(); i++)
		{
			team2.push_back(t2[i]);
		}

		environment = m->getEnvironment();

		state = BattleState::PREPARATION_PHASE;
		
		m->setBattlePayload(this);
		m->setMatchStatus(tw::MatchStatus::STARTED);

		int teamStart = rand() % 2;
		int team1Ini = rand() % 2;
		int team2Ini = rand() % 2;

		std::vector<tw::Player *> startTeam;
		std::vector<tw::Player*> secondTeam;

		if (teamStart == 0)
		{
			startTeam = m->getTeam1();
			secondTeam = m->getTeam2();
		}
		else
		{
			startTeam = m->getTeam2();
			secondTeam = m->getTeam1();
		}

		timeline.push_back(startTeam[team1Ini]);
		timeline.push_back(secondTeam[team2Ini]);
		timeline.push_back(startTeam[!team1Ini]);
		timeline.push_back(secondTeam[!team2Ini]);

		turnToken = 0;
	}

	inline int getIdForPlayer(tw::Player * p)
	{
		for (int i = 0; i < timeline.size(); i++)
		{
			if (timeline[i] == p)
				return i;
		}

		return -1;
	}

	inline bool isPreparationPhase()
	{
		return state == BattleState::PREPARATION_PHASE;
	}

	inline void enterBattlePhase()
	{
		state = BattleState::BATTLE_PHASE;
		notifyBattleStateChanged(state);

		playerStartTurn(timeline[turnToken]);
	}

	void playerStartTurn(tw::Player * p)
	{
		p->getCharacter()->turnStart();
		notifyPlayerTurnStart(p);
	}

	inline BattleState getBattleState()
	{
		return state;
	}

	tw::Environment * getEnvironment()
	{
		return environment;
	}
	
	tw::Match * getMatch()
	{
		return match;
	}

	std::vector<tw::Player*> getTimeline()
	{
		return timeline;
	}

	tw::Player * getActivePlayer()
	{
		return timeline[turnToken];
	}

	// Passe au prochain personnage vivant. Retourne false si plus aucun personnage n'est vivant.
	bool changeTurn()
	{
		for (int i = 0; i < timeline.size(); i++)
		{
			turnToken++;
			if (turnToken >= timeline.size())
				turnToken = 0;

			tw::BaseCharacterModel * character = getActivePlayer()->getCharacter();
			if (character != NULL && character->isAlive())
			{
				playerStartTurn(getActivePlayer());
				return true;
			}
		}

		return false;
	}

	void addEventListener(BattleEventListener * l)
	{
		listeners.push_back(l);
	}

	void removeEventListener(BattleEventListener * l)
	{
		auto it = std::find(listeners.begin(), listeners.end(), l);
		if (it != listeners.end())
		{
			listeners.erase(it);
		}
	}
};