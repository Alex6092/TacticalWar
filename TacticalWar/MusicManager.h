#pragma once

#include <memory>
#include <SFML/Audio.hpp>

// Musiques et sons communs. Rien n'est chargé ni joué si le son est désactivé
// (client.json "sound": false ou option --no-sound).
class MusicManager
{
private:
	MusicManager();
	static MusicManager * instance;

	bool enabled;
	std::unique_ptr<sf::Music> menuMusic;
	std::unique_ptr<sf::Music> battleMusic;
	sf::SoundBuffer takeDamageSoundBuffer;
	std::unique_ptr<sf::Sound> takeDamageSoundInstances[4];
	int iTakeDamageInstance;
	// Musique en cours (0 : aucune, 1 : menus, 2 : combat), reprise quand le son est réactivé.
	int current = 0;
	bool loaded = false;
	void load();

public:
	static MusicManager * getInstance();

	bool isEnabled() const { return enabled; }
	// Options : coupe ou relance la musique (chargée au premier besoin).
	void setEnabled(bool value);

	void setMenuMusic();
	void setBattleMusic();
	void stopMusic();
	void playTakeDamageSound();
};
