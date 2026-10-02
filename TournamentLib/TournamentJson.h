#pragma once

#include <nlohmann/json.hpp>

#include "Tournament.h"

namespace tw
{
	namespace tournament
	{
		// Sérialisation complète d'un tournoi (persistance data/tournaments/<id>.json).
		nlohmann::json toJson(const Tournament & tournament);
		// Retourne false si le JSON ne décrit pas un tournoi valide.
		bool fromJson(const nlohmann::json & json, Tournament & tournament, std::string * error = nullptr);

		nlohmann::json toJson(const Settings & settings);
		Settings settingsFromJson(const nlohmann::json & json);
		nlohmann::json toJson(const TMatch & match);
		nlohmann::json toJson(const MatchResult & result);
		MatchResult resultFromJson(const nlohmann::json & json);

		bool parseFormat(const std::string & text, Format & format);
		bool parseReason(const std::string & text, ResultReason & reason);
	}
}
