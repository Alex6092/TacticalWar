#pragma once

#include <IScreenActionCallback.h>
#include <iostream>
#include <Mage.h>
#include <BaseCharacterModel.h>
#include <CharacterFactory.h>

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
		c1 = CharacterFactory::getInstance()->constructCharacter(environment, 1, 1, 9, 9, NULL);
		c2 = CharacterFactory::getInstance()->constructCharacter(environment, 1, 2, 11, 11, NULL);

		p1 = new tw::Player("Peon 1", "", 1);
		p1->setCharacter(c1);

		p2 = new tw::Player("Peon 2", "", 2);
		p2->setCharacter(c2);
	}

	virtual void applyEndOfBattle(int winnerTeam)
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
	virtual void applyTakeDamage(int persoId)
	{
		cout << "applyTakeDamage()" << endl;
	}

	virtual void applyCharacterLaunchSpell(int persoId, int x, int y, int spellId)
	{
		cout << "applyCharacterLaunchSpell()" << endl;
	}

	virtual tw::BaseCharacterModel* getCharacter(int persoId)
	{
		if (persoId == 1)
			return p1->getCharacter();
		else if (persoId == 2)
			return p2->getCharacter();

		return NULL;
	}

	virtual std::vector <tw::BaseCharacterModel*> getAliveCharacters() {
		std::vector<tw::BaseCharacterModel*> players;
		players.push_back(p1->getCharacter());
		players.push_back(p2->getCharacter());
		return players;
	}

	virtual void addAnimationToDisplay(SpellView * s)
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

	virtual void CheckHpCharacter(int HP, int persoId)
	{
		if (HP == 0)
		{
			applyCharacterDie(persoId);
		}
		else
		{
			applyTakeDamage(persoId);

		}
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
	virtual void applyTakeDmgAnimation()
	{
		cout << "applyTakeDmgAnimation()" << endl;
	}
	
	virtual void applyDieAnimation()
	{
		cout << "applyDieAnimation()" << endl;
	}

	virtual void applyTeleport(int playerId, int cellX, int cellY)
	{

	}

	virtual void applySynchroPA(int playerId, int pa)
	{

	}

	virtual void applySynchroPM(int playerId, int pm)
	{

	}

	virtual void playTakeDamageSound()
	{

	}
};

