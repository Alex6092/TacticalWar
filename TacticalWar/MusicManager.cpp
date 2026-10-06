#include "MusicManager.h"
#include "ClientConfig.h"

MusicManager * MusicManager::instance = NULL;

MusicManager * MusicManager::getInstance()
{
	if (instance == NULL)
		instance = new MusicManager();

	return instance;
}

MusicManager::MusicManager()
{
	enabled = ClientConfig::get().soundEnabled;
	iTakeDamageInstance = 0;

	if (enabled)
		load();
}

void MusicManager::load()
{
	if (loaded)
		return;
	loaded = true;
	menuMusic.reset(new sf::Music());
	battleMusic.reset(new sf::Music());
	menuMusic->openFromFile("./assets/music/SAM1_BGM16_sence_anxiety.wav");
	battleMusic->openFromFile("./assets/music/SAM1_BGM04_battle_battle1.wav");
	takeDamageSoundBuffer.loadFromFile("./assets/sound/McOof.wav");

	for (int i = 0; i < 4; i++)
		takeDamageSoundInstances[i].reset(new sf::Sound());

	menuMusic->setLoop(true);
	battleMusic->setLoop(true);

	menuMusic->setVolume(50.0);
	battleMusic->setVolume(50.0);
}

void MusicManager::setEnabled(bool value)
{
	if (value == enabled)
		return;
	enabled = value;
	if (!enabled)
	{
		if (loaded)
		{
			menuMusic->stop();
			battleMusic->stop();
		}
		return;
	}
	load();
	if (current == 1)
		menuMusic->play();
	else if (current == 2)
		battleMusic->play();
}

void MusicManager::setMenuMusic()
{
	current = 1;
	if (!enabled)
		return;

	if (battleMusic->getStatus() == sf::Music::Status::Playing)
	{
		battleMusic->stop();
	}

	if (menuMusic->getStatus() != sf::Music::Status::Playing)
		menuMusic->play();
}

void MusicManager::setBattleMusic()
{
	current = 2;
	if (!enabled)
		return;

	if (menuMusic->getStatus() == sf::Music::Status::Playing)
	{
		menuMusic->stop();
	}

	if (battleMusic->getStatus() != sf::Music::Status::Playing)
		battleMusic->play();
}

void MusicManager::stopMusic()
{
	current = 0;
	if (!enabled)
		return;

	menuMusic->stop();
	battleMusic->stop();
}

void MusicManager::playTakeDamageSound()
{
	if (!enabled)
		return;

	takeDamageSoundInstances[iTakeDamageInstance]->setBuffer(takeDamageSoundBuffer);
	takeDamageSoundInstances[iTakeDamageInstance]->play();

	iTakeDamageInstance++;
	if (iTakeDamageInstance >= 4)
		iTakeDamageInstance = 0;
}
