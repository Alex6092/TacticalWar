#pragma once

#include <Environment.h>
#include <map>
#include <string>
#include <vector>

namespace tw
{
	// Chargement et enregistrement des cartes.
	//
	// Format v2 (assets/map/<id>.json, seul format enregistré) :
	//   {"format": "tw-map", "version": 2, "id", "name", "width", "height", "tournament",
	//    "palette": ["grass", ...], "tiles": [[indices de la palette, x = 0..width-1], ... une ligne par y],
	//    "start": {"1": [[x, y], ...], "2": [...]}}
	// Les cartes envoyées par le serveur (message MP) ajoutent "rules" : {tuile: {"walkable", "obstacle"}},
	// pour que le client applique exactement les règles du serveur.
	//
	// Format v1 (assets/map/<id>.txt, lecture seule) : hauteur, largeur, id, puis une ligne
	// "x,y,obstacle,praticable,équipe" par case.
	class EnvironmentManager
	{
	private:
		static EnvironmentManager * instance;
		EnvironmentManager();

		std::string mapDirectory;
		// Cartes reçues du serveur (prioritaires sur les fichiers locaux).
		std::map<int, std::string> receivedMaps;

	public:
		static EnvironmentManager * getInstance();

		// Dossier des cartes (par défaut ./assets/map/).
		void setMapDirectory(const std::string & directory);
		const std::string & getMapDirectory() const { return mapDirectory; }

		// Retourne NULL si la carte n'existe pas ou est invalide.
		Environment * loadEnvironment(int environmentId);
		Environment * loadEnvironmentFrom(const std::string & directory, int environmentId);

		// Enregistre la carte au format v2 dans le dossier des cartes (ou le dossier donné).
		bool saveEnvironment(Environment * environment, std::string * error = nullptr);
		bool saveEnvironmentTo(Environment * environment, const std::string & directory, std::string * error = nullptr);

		// Sérialisation v2. withRules : ajoute les règles de jeu des tuiles (envoi aux clients).
		static std::string toJson(Environment * environment, bool withRules = false, bool compact = false);
		static Environment * fromJson(const std::string & text, std::string * error = nullptr);
		static Environment * fromV1Text(const std::string & text, int environmentId, std::string * error = nullptr);

		// Carte reçue du serveur : utilisée par loadEnvironment à la place du fichier local.
		bool registerReceivedMap(const std::string & json);

		Environment * getRandomEnvironment()
		{
			std::vector<int> ids = getAlreadyExistingIds();
			if (ids.size() > 0)
				return loadEnvironment(ids[0]);
			return NULL;
		}

		// Identifiants des cartes du dossier (fichiers .json et .txt), triés.
		std::vector<int> getAlreadyExistingIds();
		std::vector<int> getAlreadyExistingIds(const std::string & directory);

		int getAvailableId();
	};
}
