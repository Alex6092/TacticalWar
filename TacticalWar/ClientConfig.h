#pragma once

#include <string>

// Configuration du client, lue depuis client.json (créé avec les valeurs par défaut s'il est absent).
class ClientConfig
{
public:
	std::string serverHost = "127.0.0.1";
	unsigned short serverPort = 12345;
	// Musique et sons (désactivables, ex : poste de projection ou PC sans carte son).
	bool soundEnabled = true;

	// Connexion automatique au lancement (ligne de commande, non enregistrée) :
	//   --server hote[:port]  --login X --password Y  ou  --spectator [--director]
	bool autoConnect = false;
	// Mode réalisateur du spectateur activé au lancement (poste projeté).
	bool directorMode = false;
	std::string autoLogin;
	std::string autoPassword;

	// Outils de développement : --window LxH, --screenshot <fichier.png> [--screenshot-after <s>]
	// (la fenêtre se ferme après la capture).
	unsigned int windowWidth = 0;
	unsigned int windowHeight = 0;
	std::string screenshotPath;
	float screenshotDelaySeconds = 3;

	static ClientConfig & get();

	void applyCommandLine(int argc, char ** argv);

	// Adresse au format "hote" ou "hote:port".
	std::string getServerAddress() const;
	// Retourne false si l'adresse est invalide.
	bool setServerAddress(const std::string & address);

	void save() const;

private:
	ClientConfig() {}
	// Valeur enregistrée dans client.json (l'option --no-sound ne la modifie pas).
	bool soundInFile = true;
	void load();
};
