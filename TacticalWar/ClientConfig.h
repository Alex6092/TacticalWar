#pragma once

#include <string>

// Configuration du client, lue depuis client.json (créé avec les valeurs par défaut s'il est absent).
class ClientConfig
{
public:
	std::string serverHost = "127.0.0.1";
	unsigned short serverPort = 12345;

	// Connexion automatique au lancement (ligne de commande, non enregistrée) :
	//   --server hote[:port]  --login X --password Y  ou  --spectator
	bool autoConnect = false;
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
	void load();
};
