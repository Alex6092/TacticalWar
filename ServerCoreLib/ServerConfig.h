#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace tw
{
	struct AdminConfig
	{
		std::string login = "admin";
		// Empreinte du mot de passe (voir PasswordHasher). Vide : généré au premier lancement.
		std::string passwordHash;
		// Adresses IP autorisées à se connecter en admin (vide : toutes).
		std::vector<std::string> allowedFrom;
	};

	// Configuration du serveur, lue depuis server.json (créé avec les valeurs par défaut s'il est absent).
	struct ServerConfig
	{
		int gamePort = 12345;
		int httpPort = 8080;

		// Dossier des données persistées (équipes, tournois, résultats). Relatif au dossier de lancement.
		std::string dataDir = "data";

		int keepaliveIntervalSeconds = 10;
		int keepaliveTimeoutSeconds = 30;

		// Durée du choix des classes avant le combat (les classes manquantes sont tirées au hasard).
		int classSelectionSeconds = 90;

		AdminConfig admin;

		nlohmann::json toJson() const;
		static ServerConfig fromJson(const nlohmann::json & json);

		// Charge le fichier, ou le crée avec les valeurs par défaut. Les champs manquants
		// prennent leur valeur par défaut et le fichier est complété.
		static ServerConfig loadOrCreate(const std::string & path, std::string & log);
		bool save(const std::string & path, std::string * error = nullptr) const;
	};
}
