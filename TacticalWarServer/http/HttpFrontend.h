#pragma once

#include <cstdint>
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

	bool start(std::string & error);
	void stop();

	// Publie un nouvel instantané (JSON). Appelé depuis le thread de jeu.
	void publish(const std::string & stateJson);
	std::uint64_t getVersion() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
