#include <Mage.h>
#include <Guerrier.h>
#include <Archer.h>
#include <Protecteur.h>

#include <iostream>
#include <Environment.h>
#include <EnvironmentManager.h>

using namespace std;

int main(int argc, char ** argv)
{
	tw::Environment * environment1 = tw::EnvironmentManager::getInstance()->getRandomEnvironment();
	Mage * mageTest = new Mage(environment1, 1, 0, 0);

	tw::Environment * environment2 = tw::EnvironmentManager::getInstance()->getRandomEnvironment();
	Guerrier * guerrierTest = new Guerrier(environment2, 1, 0, 0);

	tw::Environment * environment3 = tw::EnvironmentManager::getInstance()->getRandomEnvironment();
	Archer * archerTest = new Archer(environment3, 1, 0, 0);

	tw::Environment * environment4 = tw::EnvironmentManager::getInstance()->getRandomEnvironment();
	Protecteur * protecteurTest = new Protecteur(environment4, 1, 0, 0);

	int i = 0, x;

	cout << "Mage :" << endl << endl;

	cout << "Base attack : " << mageTest->getBaseAttack() << endl;
	cout << "Base defense : " << mageTest->getBaseDefense() << endl;
	cout << "Max life : " << mageTest->getBaseMaxLife() << endl << endl;

//////////////////////////////////////////////////////////////////////////

	cout << "Guerrier :" << endl << endl;

	cout << "Base attack : " << guerrierTest->getBaseAttack() << endl;
	cout << "Base defense : " << guerrierTest->getBaseDefense() << endl;
	cout << "Max life : " << guerrierTest->getBaseMaxLife() << endl << endl;

//////////////////////////////////////////////////////////////////////////

	cout << "Protecteur :" << endl << endl;

	cout << "Base attack : " << protecteurTest->getBaseAttack() << endl;
	cout << "Base defense : " << protecteurTest->getBaseDefense() << endl;
	cout << "Max life : " << protecteurTest->getBaseMaxLife() << endl << endl;

//////////////////////////////////////////////////////////////////////////

	cout << "Archer :" << endl << endl;

	cout << "Base attack : " << archerTest->getBaseAttack() << endl;
	cout << "Base defense : " << archerTest->getBaseDefense() << endl;
	cout << "Max life : " << archerTest->getBaseMaxLife() << endl << endl;

//////////////////////////////////////////////////////////////////////////

	delete mageTest;
	delete archerTest;
	delete guerrierTest;
	delete protecteurTest;

	delete environment1;
	delete environment2;
	delete environment3;
	delete environment4;

	return 0;
}