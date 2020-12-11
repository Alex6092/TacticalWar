#pragma once

#include <SFML/Audio.hpp>

class MusicManager
{
private:
	MusicManager();

	static MusicManager * instance;

	sf::Music menuMusic;
	sf::Music battleMusic;

public:
	static MusicManager * getInstance();

	void setMenuMusic();
	void setBattleMusic();
	void stopMusic();
};

