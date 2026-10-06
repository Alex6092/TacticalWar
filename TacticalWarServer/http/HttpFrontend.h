#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>

// Serveur HTTP de la vue projetée (page web + état du tournoi en direct).
// Il tourne dans ses propres threads et ne lit que l'instantané publié par le thread de jeu
// avec publish() : il n'accède jamais à l'état du jeu.
class HttpFrontend
{
public:
	HttpFrontend(int port, const std::string & webRoot);
	~HttpFrontend();

	// Cartes compactes (JSON par identifiant), servies par /api/map/<id>. Avant start() : elles ne
	// changent plus ensuite (lues par les threads HTTP sans verrou).
	void setMaps(const std::map<int, std::string> & maps);

	bool start(std::string & error);
	void stop();

	// Publie un nouvel instantané (JSON). Appelé depuis le thread de jeu.
	void publish(const std::string & stateJson);
	std::uint64_t getVersion() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
