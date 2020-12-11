#pragma once
#include <string>
#include <vector>
#include <Player.h>
#include "StringUtils.h"
#include <Environment.h>

namespace tw
{
	enum MatchStatus
	{
		NOT_STARTED,
		STARTED,
		FINISHED
	};

	class Match;
	class MatchEventListener
	{
	public:
		virtual void onMatchStatusChanged(Match * match, MatchStatus oldStatus, MatchStatus newStatus) = 0;
	};

	class Match
	{
	private:
		//-------------------------------------------------------
		// Ne pas se soucier de ça :
		std::vector<Player> team1Client;
		std::vector<Player> team2Client;
		void clientSetTeam1(std::vector<Player> team)
		{
			team1Client = team;
		}

		void clientSetTeam2(std::vector<Player> team)
		{
			team2Client = team;
		}
	public:
		std::vector<Player> clientGetTeam1()
		{
			return team1Client;
		}

		std::vector<Player> clientGetTeam2()
		{
			return team2Client;
		}
		//-------------------------------------------------------

	private:
		static int matchId;

		int id;
		std::string name;
		std::vector<Player*> team1;
		//Player * team1PlayerTakeInit;
		std::vector<Player*> team2;
		//Player * team2PlayerTakeInit;

		MatchStatus status;
		int winnerTeam;

		void * battlePayload;

		std::vector<MatchEventListener*> listeners;

		Environment * environment;
		std::vector<Point2D> availableStartPositionTeam1;
		std::vector<Point2D> availableStartPositionTeam2;

		bool playerIsInTeam(Player * p, std::vector<Player*> & team)
		{
			return std::find(team.begin(), team.end(), p) != team.end();
		}

		void notifyStatusChanged(MatchStatus oldStatus, MatchStatus newStatus)
		{
			for (int i = 0; i < listeners.size(); i++)
			{
				listeners[i]->onMatchStatusChanged(this, oldStatus, newStatus);
			}
		}

		// A utiliser que pour la deserialisation :
		inline void setId(int id)
		{
			this->id = id;
		}

		inline void setWinnerTeam(int winnerTeam)
		{
			this->winnerTeam = winnerTeam;
		}

	public:
		Match(std::string name)
		{
			this->id = matchId++;
			this->name = name;
			status = MatchStatus::NOT_STARTED;
			winnerTeam = 0;
			battlePayload = NULL;
			environment = NULL;
		}

		int getId()
		{
			return id;
		}

		std::string getMatchName()
		{
			return name;
		}

		void setTeam1Players(Player * p1, Player * p2)
		{
			team1.clear();
			team1.push_back(p1);
			team1.push_back(p2);
		}

		void setTeam2Players(Player * p1, Player * p2)
		{
			team2.clear();
			team2.push_back(p1);
			team2.push_back(p2);
		}


		std::vector<Player*> & getTeam1() {
			return team1;
		}

		std::vector<Player*> & getTeam2() {
			return team2;
		}

		bool playerIsInTeam1(Player * p)
		{
			return playerIsInTeam(p, team1);
		}

		bool playerIsInTeam2(Player * p)
		{
			return playerIsInTeam(p, team2);
		}

		bool playerIsInThisMatch(Player * p)
		{
			return playerIsInTeam1(p) || playerIsInTeam2(p);
		}

		MatchStatus getStatus()
		{
			return status;
		}

		std::vector<Player*> * getWinnerTeam()
		{
			if (status == MatchStatus::FINISHED)
			{
				if (winnerTeam == 1)
				{
					return &team1;
				}
				else
				{
					return &team2;
				}
			}

			return NULL;
		}

		void * getBattlePayload()
		{
			return battlePayload;
		}

		void setBattlePayload(void * obj)
		{
			battlePayload = obj;
		}

		void setMatchStatus(MatchStatus status)
		{
			MatchStatus oldStatus = this->status;
			this->status = status;
			notifyStatusChanged(oldStatus, status);
		}


		Environment * getEnvironment()
		{
			return environment;
		}

		void setEnvironment(Environment * environment)
		{
			this->environment = environment;

			availableStartPositionTeam1.clear();
			availableStartPositionTeam2.clear();

			if (environment != NULL)
			{
				for (int i = 0; i < environment->getWidth(); i++)
				{
					for (int j = 0; j < environment->getHeight(); j++)
					{
						CellData * cell = environment->getMapData(i, j);
						if (cell->isTeam1StartPoint())
						{
							availableStartPositionTeam1.push_back(tw::Point2D(*cell));
						}
						else if (cell->isTeam2StartPoint())
						{
							availableStartPositionTeam2.push_back(tw::Point2D(*cell));
						}
					}
				}
			}
		}

		tw::Point2D getRandomAvailableCellForTeam(int teamId, std::vector<tw::Point2D> alreadyUsed)
		{
			tw::Point2D result(-1, -1);

			std::vector<tw::Point2D> candidateCells;
			std::vector<tw::Point2D> sourceCells;
			if (teamId == 1)
			{
				sourceCells = availableStartPositionTeam1;
			}
			else
			{
				sourceCells = availableStartPositionTeam2;
			}

			for (int i = 0; i < sourceCells.size(); i++)
			{
				bool isAvailable = true;
				for (int j = 0; j < alreadyUsed.size(); j++)
				{
					if (sourceCells[i] == alreadyUsed[j])
					{
						isAvailable = false;
						break;
					}
				}

				if (isAvailable)
				{
					candidateCells.push_back(sourceCells[i]);
				}
			}

			if (candidateCells.size() > 0)
			{
				result = candidateCells[rand() % candidateCells.size()];
			}

			return result;
		}


		void addEventListener(MatchEventListener * l)
		{
			listeners.push_back(l);
		}

		void removeEventListener(MatchEventListener * l)
		{
			std::vector<MatchEventListener*>::iterator it = std::find(listeners.begin(), listeners.end(), l);

			if (it != listeners.end())
			{
				listeners.erase(it);
			}
		}

		std::string serialize()
		{
			std::string result = "";
			result = std::to_string(id) + "," + name + "," + std::to_string((int)status) + "," + std::to_string(winnerTeam) + ",";
			result += serializeTeam(team1, '|', '^') + ",";
			result += serializeTeam(team2, '|', '^');
			
			return result;
		}

		static Match * deserialize(std::string str)
		{
			Match * result = NULL;

			std::vector<std::string> splited = StringUtils::explode(str, ',');
			int i = 0;

			int matchId = std::atoi(splited[i++].c_str());
			std::string matchName = splited[i++];
			MatchStatus status = (MatchStatus)std::atoi(splited[i++].c_str());
			int winnerTeam = std::atoi(splited[i++].c_str());

			std::vector<Player> team1 = deserializeTeam(splited[i++], '|', '^');
			std::vector<Player> team2 = deserializeTeam(splited[i++], '|', '^');

			result = new Match(matchName);
			result->setMatchStatus(status);
			result->setId(matchId);
			result->setWinnerTeam(winnerTeam);

			result->clientSetTeam1(team1);
			result->clientSetTeam2(team2);

			return result;
		}




		// Serialisation team :
		static std::string serializeTeam(std::vector<Player*> & team, char separatorPlayer, char separatorInfo)
		{
			std::string result;

			for (int i = 0; i < team.size(); i++)
			{
				Player * p = team[i];
				if (i != 0)
					result += separatorPlayer;

				result += std::to_string(p->getTeamNumber()) + separatorInfo;
				result += p->getPseudo() + separatorInfo;
				result += std::to_string(p->getHasJoinBattle() ? 1 : 0);
			}

			return result;
		}

		static std::vector<Player> deserializeTeam(std::string teamInfo, char separatorPlayer, char separatorInfo)
		{
			std::vector<Player> result;
			std::vector<std::string> teamData = StringUtils::explode(teamInfo, separatorPlayer);

			for (int i = 0; i < teamData.size(); i++)
			{
				std::vector<std::string> playerData = StringUtils::explode(teamData[i], separatorInfo);

				result.push_back(Player(playerData[1], "", std::atoi(playerData[0].c_str())));
				result.back().setHasJoinBattle(std::atoi(playerData[2].c_str()));
			}

			return result;
		}
	};
}