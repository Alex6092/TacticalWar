#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace tw
{
	namespace store
	{
		// Les chemins sont en UTF-8.

		// Lit et analyse un fichier JSON. Retourne false (et renseigne error) si le fichier
		// est absent ou invalide.
		bool readJsonFile(const std::string & path, nlohmann::json & out, std::string * error = nullptr);

		// Écrit un fichier de manière atomique : le contenu est écrit dans un fichier
		// temporaire, vidé sur le disque, puis renommé à la place du fichier cible.
		// Un crash pendant l'écriture laisse donc toujours l'ancienne ou la nouvelle version.
		bool writeTextFileAtomic(const std::string & path, const std::string & content, std::string * error = nullptr);
		bool writeJsonFileAtomic(const std::string & path, const nlohmann::json & value, std::string * error = nullptr);

		// Ajoute une ligne à la fin d'un fichier (journal append-only, ex : JSON Lines).
		bool appendLine(const std::string & path, const std::string & line, std::string * error = nullptr);

		bool fileExists(const std::string & path);
		bool ensureDirectory(const std::string & path, std::string * error = nullptr);
		std::string joinPath(const std::string & directory, const std::string & fileName);
	}
}
