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
		// Durée de la phase de bannissement des matchs de tournoi qui en ont une (réglage du tournoi).
		int banSeconds = 20;

		// Tournois : combats simultanés au maximum, repos entre deux matchs d'une équipe,
		// délai avant forfait d'une équipe absente ou déconnectée.
		int maxConcurrentMatches = 8;
		int restSeconds = 20;
		int forfeitSeconds = 90;

		// Émotes prédéfinies des joueurs pendant les combats (l'organisateur peut les couper).
		bool emotesEnabled = true;

		// Mode des combats hors tournoi : "KO" ou "ZONE" (zone à tenir, gagnée à zonePoints points).
		// Les tournois ont leur propre réglage.
		std::string battleMode = "KO";
		int zonePoints = 5;

		AdminConfig admin;

		nlohmann::json toJson() const;
		static ServerConfig fromJson(const nlohmann::json & json);

		// Charge le fichier, ou le crée avec les valeurs par défaut. Les champs manquants
		// prennent leur valeur par défaut et le fichier est complété.
		static ServerConfig loadOrCreate(const std::string & path, std::string & log);
		bool save(const std::string & path, std::string * error = nullptr) const;
	};
}
