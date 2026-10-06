#include "ReplayStore.h"
#include "JsonFile.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>

#pragma warning(disable : 4996)	// fs::u8path (C++20) et localtime

namespace fs = std::filesystem;
using nlohmann::json;

namespace tw
{
	namespace store
	{
		bool isValidReplayId(const std::string & id)
		{
			if (id.empty() || id.size() > 64)
				return false;
			return std::all_of(id.begin(), id.end(), [](char c) {
				return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
			});
		}

		bool ReplayWriter::open(const std::string & target, const json & header, std::string * error)
		{
			path = target;
			file.open(fs::u8path(target), std::ios::binary | std::ios::trunc);
			if (!file.is_open())
			{
				if (error != nullptr)
					*error = "Impossible d'écrire " + target;
				return false;
			}
			json line = header;
			line["type"] = "header";
			writeLine(line);
			return true;
		}

		void ReplayWriter::writeLine(const json & line)
		{
			file << line.dump(-1, ' ', false, json::error_handler_t::replace) << '\n';
			file.flush();
		}

		void ReplayWriter::append(std::int64_t elapsedMs, const json & batch)
		{
			if (file.is_open())
				writeLine({ { "t", elapsedMs }, { "batch", batch } });
		}

		void ReplayWriter::finish(const json & end)
		{
			if (!file.is_open())
				return;
			json line = end;
			line["type"] = "end";
			writeLine(line);
			file.close();
		}

		ReplayLibrary::ReplayLibrary(const std::string & directory)
			: directory(directory)
		{
		}

		std::string ReplayLibrary::newId(int sessionId) const
		{
			std::time_t now = std::time(nullptr);
			char stamp[32];
			std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&now));
			return std::string(stamp) + "-" + std::to_string(sessionId);
		}

		std::string ReplayLibrary::pathOf(const std::string & id) const
		{
			return joinPath(directory, id + ".jsonl");
		}

		bool ReplayLibrary::load(const std::string & id, Replay & replay, std::string * error) const
		{
			if (!isValidReplayId(id))
			{
				if (error != nullptr)
					*error = "Rediffusion inconnue.";
				return false;
			}

			std::ifstream file(fs::u8path(pathOf(id)), std::ios::binary);
			if (!file.is_open())
			{
				if (error != nullptr)
					*error = "Rediffusion introuvable.";
				return false;
			}

			replay = Replay();
			std::string text;
			while (std::getline(file, text))
			{
				json line = json::parse(text, nullptr, false);
				if (line.is_discarded() || !line.is_object())
					continue;	// Ligne incomplète (arrêt brutal pendant l'écriture)

				std::string type = line.value("type", std::string());
				if (type == "header")
					replay.header = line;
				else if (type == "end")
					replay.end = line;
				else if (line.contains("batch"))
					replay.batches.push_back({ line.value("t", (std::int64_t)0), line["batch"] });
			}

			if (!replay.header.is_object() || !replay.header.contains("snapshot"))
			{
				if (error != nullptr)
					*error = "Rediffusion illisible.";
				return false;
			}
			return true;
		}

		json ReplayLibrary::list(std::size_t max) const
		{
			std::vector<std::string> ids;
			std::error_code code;
			for (fs::directory_iterator it(fs::u8path(directory), code), end; !code && it != end; it.increment(code))
			{
				if (it->path().extension() == ".jsonl")
					ids.push_back(it->path().stem().string());
			}
			// Les identifiants commencent par la date : l'ordre alphabétique inverse donne les plus récents.
			std::sort(ids.rbegin(), ids.rend());

			json result = json::array();
			for (const std::string & id : ids)
			{
				if (result.size() >= max)
					break;

				std::ifstream file(fs::u8path(pathOf(id)), std::ios::binary);
				std::string first;
				std::string last;
				std::string text;
				while (std::getline(file, text))
				{
					if (first.empty())
						first = text;
					if (!text.empty())
						last = text;
				}

				json header = json::parse(first, nullptr, false);
				if (header.is_discarded() || header.value("type", std::string()) != "header")
					continue;
				json end = json::parse(last, nullptr, false);
				bool complete = !end.is_discarded() && end.value("type", std::string()) == "end";

				json summary = {
					{ "id", id },
					{ "title", header.value("title", std::string()) },
					{ "teams", header.value("teams", json::array()) },
					{ "date", header.value("date", std::string()) },
					{ "complete", complete }
				};
				if (complete)
				{
					summary["winner"] = end.value("winner", 0);
					summary["reason"] = end.value("reason", std::string());
					summary["rounds"] = end.value("rounds", 0);
					if (end.contains("highlights"))
						summary["highlights"] = end["highlights"];
				}
				result.push_back(summary);
			}
			return result;
		}

		bool ReplayLibrary::remove(const std::string & id) const
		{
			if (!isValidReplayId(id))
				return false;
			std::error_code code;
			return fs::remove(fs::u8path(pathOf(id)), code);
		}
	}
}
