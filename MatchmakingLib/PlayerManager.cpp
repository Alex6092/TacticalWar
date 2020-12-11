#include "pch.h"
#include "PlayerManager.h"
#include <fstream>
#include <string>
#include <iostream>
#include <deque>

using namespace std;

std::vector<tw::Player*> tw::PlayerManager::playersCache;

std::vector<tw::Match*> tw::PlayerManager::matchList;

void tw::PlayerManager::addMatch(Match * m)
{
	matchList.push_back(m);
}


std::vector<tw::Player*> tw::PlayerManager::loadPlayers()
{
	int index = 1;

	if (playersCache.size() == 0)
	{
		// Vous devrez remplacer les données de test par des 
		// données chargées depuis un fichier.
		ifstream fichierTeam("./assets/equipe.txt");

		if (fichierTeam)
		{
			cout << "Ouverture reussi\n" << endl;
			string dataFile;

			fichierTeam >> dataFile;

			// Découpage des joueurs avec un '/'.
			vector<std::string> equipe = StringUtils::explode(dataFile, '/');

			for (int i = 0; i < equipe.size(); i++)
			{
				int indexName = 0;
				int indexPassword = 1;
				int indexTeam = 2;

				int team;

				// Découpage des noms, password et nbTeam des joueurs avec une ','.
				vector<std::string> result1 = StringUtils::explode(equipe[i], ',');

				// Conversion d'un string en int.
				team = std::atoi(result1[indexTeam].c_str());

				// Envoie des infos au serveur.
				playersCache.push_back(new Player(result1[indexName], result1[indexPassword], team));
			}
		}
		else {
			cout << "Pas reussi\n" << endl;
		}
	}
  
	return playersCache;
}



std::vector<tw::Match*> tw::PlayerManager::getAllMatchsForPlayer(tw::Player * p)
{
	std::vector <tw::Match*> matchs;

	for (int i = 0; i < matchList.size(); i++)
	{
		Match * m = matchList[i];
		if (m->playerIsInThisMatch(p))
		{
			matchs.push_back(m);
		}
	}

	return matchs;
}

tw::Match * tw::PlayerManager::getCurrentOrNextMatchForPlayer(tw::Player * p)
{
	Match * match = NULL;
	// Rechercher le match courant ou le prochain pour le joueur p et le retourner. Si aucun match à venir, retourner NULL.

	for (int i = 0; i < matchList.size(); i++)
	{
		Match * m = matchList[i];
		if ((m->getStatus() == tw::MatchStatus::NOT_STARTED || m->getStatus() == tw::MatchStatus::STARTED) && m->playerIsInThisMatch(p))
		{
			match = m;

			// Un match en cours passe en priorité sur les matchs planifiés :
			if (m->getStatus() == tw::MatchStatus::STARTED)
				break;
		}
	}

	return match;
}

std::vector<tw::Match*> tw::PlayerManager::getCurrentlyPlayingMatchs()
{
	std::vector<tw::Match*> result;

	// Construire la liste des matchs en cours :
	for (int i = 0; i < matchList.size(); i++)
	{
		Match * match = matchList[i];
		if (match->getStatus() == MatchStatus::STARTED)
		{
			result.push_back(match);
		}
	}

	return result;
}

std::vector<tw::Match*> tw::PlayerManager::getPlanifiedAndPlayingMatchs()
{
	std::vector<tw::Match*> result;

	// Construire la liste des matchs en cours :
	for (int i = 0; i < matchList.size(); i++)
	{
		Match * match = matchList[i];
		if (match->getStatus() == MatchStatus::STARTED || match->getStatus() == MatchStatus::NOT_STARTED)
		{
			result.push_back(match);
		}
	}

	return result;
}

/*
void tw::PlayerManager::subscribeToAllMatchEvent(MatchEventListener * l)
{
	createTestMatchIfNotExists();

	testMatch->addEventListener(l);
}
*/