#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

namespace tw
{
	namespace store
	{
		// Rediffusion d'un combat : fichier JSON Lines dans data/replays/<id>.jsonl.
		//   1re ligne  {"type": "header", "title", "teams", "date", "mapId", "map", "snapshot"}
		//   puis       {"t": <ms depuis le début>, "batch": <lot d'événements BV>}
		//   fin        {"type": "end", "winner", "reason", "rounds"} (absente si le serveur a été arrêté)
		class ReplayWriter
		{
		public:
			bool open(const std::string & path, const nlohmann::json & header, std::string * error = nullptr);
			void append(std::int64_t elapsedMs, const nlohmann::json & batch);
			void finish(const nlohmann::json & end);
			bool isOpen() const { return file.is_open(); }
			const std::string & getPath() const { return path; }

		private:
			void writeLine(const nlohmann::json & line);

			std::ofstream file;
			std::string path;
		};

		struct Replay
		{
			nlohmann::json header;
			std::vector<std::pair<std::int64_t, nlohmann::json>> batches;
			nlohmann::json end;		// null si le combat n'a pas été terminé
		};

		class ReplayLibrary
		{
		public:
			explicit ReplayLibrary(const std::string & directory);

			// Identifiant de fichier pour un nouveau combat (date et heure, numéro de session).
			std::string newId(int sessionId) const;
			std::string pathOf(const std::string & id) const;

			// Résumés des rediffusions, les plus récentes d'abord :
			// {id, title, teams, date, winner, reason, rounds, complete}.
			nlohmann::json list(std::size_t max = 100) const;
			bool load(const std::string & id, Replay & replay, std::string * error = nullptr) const;
			bool remove(const std::string & id) const;

			const std::string & getDirectory() const { return directory; }

		private:
			std::string directory;
		};

		// Identifiant valide : chiffres, lettres et tirets uniquement (pas de chemin).
		bool isValidReplayId(const std::string & id);
	}
}
