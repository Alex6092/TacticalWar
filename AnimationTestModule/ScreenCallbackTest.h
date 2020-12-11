#pragma once

#include <IScreenActionCallback.h>
#include <iostream>
#include <Mage.h>
#include <BaseCharacterModel.h>

using namespace std;

class ScreenCallbackTest : public IScreenActionCallback
{
	tw::BaseCharacterModel * c1;
	tw::BaseCharacterModel * c2;
	tw::Environment * environment;
	tw::Player * p1;
	tw::Player * p2;

public:
	ScreenCallbackTest(tw::Environment * environment)
	{
		this->environment = environment;
		c1 = new Mage(environment, 1, 9, 9);
		c2 = new Mage(environment, 2, 11, 11);

		p1 = new tw::Player("Peon 1", "", 1);
		p1->setCharacter(c1);

		p2 = new tw::Player("Peon 2", "", 2);
		p2->setCharacter(c2);
	}

	virtual void applyEndOfBattle()
	{
		cout << "applyEndOfBattle()" << endl;
	}
	virtual void applyChangeTurn(float remaining, int idPerso, std::string message)
	{
		cout << "applyChangeTurn()" << endl;
	}

	virtual void applyCharacterDie(int idPerso)
	{
		cout << "applyCharacterDie()" << endl;
	}
	
	virtual void applyCharacterLaunchSpell(int persoId, int x, int y, int spellId)
	{
		cout << "applyCharacterLaunchSpell()" << endl;
	}

	virtual tw::Player* getCharacter(int persoId)
	{
		if (persoId == 1)
			return p1;
		else if (persoId == 2)
			return p2;

		return NULL;
	}

	virtual std::vector <tw::Player*> getAliveCharacters() {
		std::vector<tw::Player*> players;
		players.push_back(p1);
		players.push_back(p2);
		return players;
	}

	virtual void addAnimationToDisplay(sf::Sprite * s)
	{
		cout << "addAnimationToDisplay()" << endl;
	}

	virtual void applyCharacterMove(int persoId, std::vector<tw::Point2D> path, MoveActionAnimationEventListener * callback)
	{
		if (persoId == 1)
		{
			c1->setPath(path, callback);
		}
		else if (persoId == 2)
		{
			c2->setPath(path, callback);
		}

		cout << "applyCharacterMove()" << endl;
	}
	
	virtual void applyCharacterDisconnected(int persoId)
	{
		cout << "applyCharacterDisconnected()" << endl;
	}

	virtual void applyCharacterConnected(int persoId)
	{
		cout << "applyCharacterConnected()" << endl;
	}

	virtual void applyCharacterPosition(int persoId, int x, int y)
	{
		cout << "applyCharacterPosition()" << endl;
	}

	virtual void applyEnterBattlePhase()
	{
		cout << "applyEnterBattlePhase()" << endl;
	}
};

