#pragma once

#include <array>
#include <string>
#include <nlohmann/json.hpp>

namespace tw
{
	const int PLAYERS_PER_TEAM = 2;

	struct PlayerAccount
	{
		std::string login;
		std::string displayName;
		std::string passwordHash;
	};

	struct Team
	{
		int id = 0;
		std::string name;
		// Abréviation affichée dans l'arbre du tournoi (ex : "LDR").
		std::string tag;
		std::array<PlayerAccount, PLAYERS_PER_TEAM> players;
		// Tête de série (0 : non classée). Utilisée pour l'appariement du tournoi.
		int seed = 0;
		// Une équipe désactivée ne peut plus se connecter ni être inscrite à un tournoi.
		bool active = true;
	};

	// Données saisies par l'admin pour créer ou modifier une équipe.
	struct PlayerInput
	{
		std::string login;
		std::string displayName;
		// Mot de passe en clair. Vide : inchangé (modification) ou généré (création).
		std::string password;
	};

	struct TeamInput
	{
		std::string name;
		std::string tag;
		int seed = 0;
		std::array<PlayerInput, PLAYERS_PER_TEAM> players;
	};

	nlohmann::json teamToJson(const Team & team, bool includePasswordHashes);
	Team teamFromJson(const nlohmann::json & json);
	TeamInput teamInputFromJson(const nlohmann::json & json);
}
