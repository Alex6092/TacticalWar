#include "MusicManager.h"
#include <stdlib.h>
#include <SFML/Audio.hpp>

MusicManager * MusicManager::instance = NULL;

MusicManager * MusicManager::getInstance()
{
	if (instance == NULL)
		instance = new MusicManager();

	return instance;
}

MusicManager::MusicManager()
{
	menuMusic.openFromFile("./assets/music/SAM1_BGM16_sence_anxiety.wav");
	battleMusic.openFromFile("./assets/music/SAM1_BGM04_battle_battle1.wav");

	menuMusic.setLoop(true);
	battleMusic.setLoop(true);
}

void MusicManager::setMenuMusic()
{
	if (battleMusic.getStatus() == sf::Music::Status::Playing)
	{
		battleMusic.stop();
	}
	
	menuMusic.play();
}


void MusicManager::setBattleMusic()
{
	if (menuMusic.getStatus() == sf::Music::Status::Playing)
	{
		menuMusic.stop();
	}

	battleMusic.play();
}

void MusicManager::stopMusic()
{
	menuMusic.stop();
	battleMusic.stop();
}