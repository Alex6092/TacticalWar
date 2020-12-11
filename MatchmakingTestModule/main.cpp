#include <PlayerManager.h>
#include <iostream>
#include <string>
#include <map>

using namespace tw;
using namespace std;

int main(int argc, char ** argv)
{
	vector<Player*> players = PlayerManager::loadPlayers();
	vector<Player*> team;

	map<int, vector<Player*>> teamIdToPlayers;

	int indexTeam = 1;
	int indexMatch = 1;
	int indexTour = 1;
	int equipe = players.size() / 2;
	int matchPlay;
	

	cout << "Liste des joueurs : \n" << endl;
	for (int i = 0; i < players.size(); i++)
	{
		cout << "Login : " << players[i]->getPseudo() << endl;
		cout << "Password : " << players[i]->getPassword() << endl;
		cout << "Team : " << players[i]->getTeamNumber() << endl << endl;
	}

	if (equipe / 2 > 1)
	{
		cout << "il y a " << equipe << " equipes en jeu" << endl << endl;
	}
	else
	{
		cout << "il y a " << equipe << " equipe en jeu" << endl << endl;
	}

	for (int i = 0; i < players.size() ; i+=2)
	{
		if (indexTeam == players[i]->getTeamNumber())
		{
			std::vector<Player*> joueurs;
			joueurs.push_back(players[i]);
			joueurs.push_back(players[i + 1]);
			teamIdToPlayers[indexTeam] = joueurs;

			cout << "equipe : " << indexTeam << " -> " << teamIdToPlayers[indexTeam][0]->getPseudo() << " et " << teamIdToPlayers[indexTeam][1]->getPseudo() << endl;
		}
		indexTeam += 1;
	}
	cout << "---------------------------------" << endl;
	indexTeam = 1;
	map<int, vector<Player*>> TeamRest;
	map<int, vector<Player*>> TeamWinMatch;
	map<int, vector<Player*>> TeamLosMatch;
	TeamRest = teamIdToPlayers;
	//Nombre de tour !!
	for (int x = 0; x < equipe - 1; x++)
	{
		//Nombtre de match
			for (int i = 0; i < equipe; i++)
			{
				if (teamIdToPlayers == TeamRest) {
					// Si une équipe n'a pas d'adversaire on mes l'équipe sur le banc sinon on l'a fait joué contre une autre équipe.
					if (TeamRest.find(indexTeam + 1) == TeamRest.end())
					{
						indexTeam = TeamRest.size();
						cout << "Tour " << indexTour << " - ";
						cout << "equipe " << TeamRest[indexTeam][0]->getTeamNumber() << " sur le banc ! ;)" << endl;
						TeamWinMatch[i+1] = TeamRest[indexTeam];
						TeamRest = TeamWinMatch;
					}
					else
					{
						cout << "Tour " << indexTour << " - ";
						cout << "equipe " << TeamRest[indexTeam][0]->getTeamNumber() << " vs " << "equipe " << TeamRest[indexTeam + 1][0]->getTeamNumber() << endl;
						// On force la 1ere equipe à win.
						TeamWinMatch[i+1] = TeamRest[indexTeam];
						// On force la seconde à lose.
						TeamLosMatch[i+1] = TeamRest[indexTeam + 1];
						indexTeam += 2;
					}
				}
				else
				{
					if (teamIdToPlayers.size() <= indexTeam) {
						indexTeam = 1;
					}
					if(TeamRest.size() & 1)
					{
						TeamWinMatch.clear();
						int idmax = TeamRest.size();
						int copyindexTeam = 1, xy = 0;
						for (int y = 0; y < i; y++) {
							if (TeamRest.size() & 1) {
								int indexcopy = copyindexTeam + 2;
								if (TeamRest.find(indexcopy) == TeamRest.end())
								{
									cout << "Tour " << indexTour << " - ";
									cout << "equipe " << TeamRest[indexTeam][0]->getTeamNumber() << " sur le banc ! ;)" << endl;
									TeamWinMatch[xy + 1] = TeamRest[indexTeam];
									TeamRest = TeamWinMatch;
									indexcopy = 0;
									copyindexTeam = 1;
									idmax = TeamRest.size();
									indexTeam = 1;
									xy = 0;
									TeamWinMatch.clear();
								}
								else 
								{
									cout << "Tour " << indexTour << " - ";
									cout << "equipe " << TeamRest[idmax][0]->getTeamNumber() << " vs " << "equipe " << TeamRest[indexTeam][0]->getTeamNumber() << endl;
									TeamWinMatch[xy + 1] = TeamRest[idmax];
									TeamLosMatch[i + 1] = TeamRest[indexTeam];
									indexTeam += 1;
									idmax = TeamRest.size() - indexTeam + 1;
									xy += 1;
									copyindexTeam = indexTeam + 1;
								}
							}
						}
					}
					else
					{
						TeamWinMatch.clear();
						int idmax = TeamRest.size();
						int copyindexTeam = 1, xy =0;
						for (int y = 0; y < i; y++) {
							if (TeamRest.size() & 1)
							{
								idmax = 1;
								// TODO calcul à faire. (ptetre)
							}
							// Tant qu'il ne reste pas une equipe en jeu onn continu de faire des matchs.
							if (TeamRest.size() == 1) {
								indexTeam = 1;
								cout << "Win de l'equipe " << TeamRest[indexTeam][0]->getTeamNumber() << endl;
								y = i;
							}
							else
							{
								cout << "Tour " << indexTour << " - ";
								cout << "equipe " << TeamRest[idmax][0]->getTeamNumber() << " vs " << "equipe " << TeamRest[indexTeam][0]->getTeamNumber() << endl;
								TeamWinMatch[xy + 1] = TeamRest[idmax];
								TeamLosMatch[i + 1] = TeamRest[indexTeam];
								indexTeam += 1;
								idmax = TeamRest.size() - indexTeam  +1;
								xy += 1;
								copyindexTeam = indexTeam + 1;
								if (TeamRest.size() <= copyindexTeam) {
									xy = 0;
									indexTeam = 1;
									TeamRest = TeamWinMatch;
									if (TeamWinMatch.size() != 1) {
										TeamWinMatch.clear();
									}
								}
							}
						}
						i = equipe;
						x = equipe - 1;
					}
				}
			}
			cout << "---------------------------------" << endl;
			indexTour += 1;
	}

	return 0;
}