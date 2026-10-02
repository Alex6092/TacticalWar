#pragma once

#include <string>

// Configuration du client, lue depuis client.json (créé avec les valeurs par défaut s'il est absent).
class ClientConfig
{
public:
	std::string serverHost = "127.0.0.1";
	unsigned short serverPort = 12345;

	static ClientConfig & get();

	// Adresse au format "hote" ou "hote:port".
	std::string getServerAddress() const;
	// Retourne false si l'adresse est invalide.
	bool setServerAddress(const std::string & address);

	void save() const;

private:
	ClientConfig() {}
	void load();
};
