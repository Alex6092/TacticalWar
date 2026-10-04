#include "TournamentJson.h"

#include <algorithm>

using namespace tw::tournament;

namespace
{
	const int FILE_VERSION = 1;

	template<typename Enum>
	bool parseEnum(const std::string & text, Enum & value, std::initializer_list<Enum> candidates)
	{
		for (Enum candidate : candidates)
		{
			if (text == toString(candidate))
			{
				value = candidate;
				return true;
			}
		}
		return false;
	}

	nlohmann::json slotToJson(const SlotRef & slot)
	{
		const char * kind = "NONE";
		switch (slot.kind)
		{
		case SlotRef::Kind::TEAM: kind = "TEAM"; break;
		case SlotRef::Kind::WINNER_OF: kind = "WINNER_OF"; break;
		case SlotRef::Kind::LOSER_OF: kind = "LOSER_OF"; break;
		case SlotRef::Kind::BYE: kind = "BYE"; break;
		default: break;
		}
		return { { "kind", kind }, { "value", slot.value } };
	}

	SlotRef slotFromJson(const nlohmann::json & json)
	{
		SlotRef slot;
		std::string kind = json.value("kind", "NONE");
		if (kind == "TEAM") slot.kind = SlotRef::Kind::TEAM;
		else if (kind == "WINNER_OF") slot.kind = SlotRef::Kind::WINNER_OF;
		else if (kind == "LOSER_OF") slot.kind = SlotRef::Kind::LOSER_OF;
		else if (kind == "BYE") slot.kind = SlotRef::Kind::BYE;
		slot.value = json.value("value", 0);
		return slot;
	}

	StageType stageTypeFromString(const std::string & text)
	{
		StageType type = StageType::ROUND_ROBIN_POOLS;
		parseEnum(text, type, { StageType::ROUND_ROBIN_POOLS, StageType::SINGLE_ELIMINATION, StageType::DOUBLE_ELIMINATION, StageType::SWISS });
		return type;
	}
}

bool tw::tournament::parseFormat(const std::string & text, Format & format)
{
	return parseEnum(text, format, { Format::POOLS_THEN_BRACKET, Format::DOUBLE_ELIMINATION, Format::SWISS });
}

bool tw::tournament::parseReason(const std::string & text, ResultReason & reason)
{
	return parseEnum(text, reason, { ResultReason::KO, ResultReason::ROUND_LIMIT, ResultReason::FORFEIT, ResultReason::ADMIN, ResultReason::BYE,
		ResultReason::OBJECTIVE });
}

nlohmann::json tw::tournament::toJson(const Settings & settings)
{
	return {
		{ "format", toString(settings.format) },
		{ "poolCount", settings.poolCount },
		{ "qualifiersPerPool", settings.qualifiersPerPool },
		{ "thirdPlaceMatch", settings.thirdPlaceMatch },
		{ "grandFinalReset", settings.grandFinalReset },
		{ "swissRounds", settings.swissRounds },
		{ "swissTopCut", settings.swissTopCut },
		{ "pointsForWin", settings.pointsForWin },
		{ "pointsForLoss", settings.pointsForLoss },
		{ "mode", settings.zoneMode ? "ZONE" : "KO" },
		{ "zonePoints", settings.zonePoints },
		{ "maxTalents", settings.maxTalents },
		{ "bans", toString(settings.bans) },
		{ "maps", toString(settings.maps) }
	};
}

Settings tw::tournament::settingsFromJson(const nlohmann::json & json)
{
	Settings settings;
	parseFormat(json.value("format", std::string()), settings.format);
	settings.poolCount = json.value("poolCount", settings.poolCount);
	settings.qualifiersPerPool = json.value("qualifiersPerPool", settings.qualifiersPerPool);
	settings.thirdPlaceMatch = json.value("thirdPlaceMatch", settings.thirdPlaceMatch);
	settings.grandFinalReset = json.value("grandFinalReset", settings.grandFinalReset);
	settings.swissRounds = json.value("swissRounds", settings.swissRounds);
	settings.swissTopCut = json.value("swissTopCut", settings.swissTopCut);
	settings.pointsForWin = json.value("pointsForWin", settings.pointsForWin);
	settings.pointsForLoss = json.value("pointsForLoss", settings.pointsForLoss);
	settings.zoneMode = json.value("mode", std::string("KO")) == "ZONE";
	settings.zonePoints = std::max(1, std::min(20, json.value("zonePoints", settings.zonePoints)));
	settings.maxTalents = std::max(0, std::min(5, json.value("maxTalents", settings.maxTalents)));
	parseEnum(json.value("bans", std::string()), settings.bans, { BanMode::NONE, BanMode::FINALS, BanMode::ALL });
	parseEnum(json.value("maps", std::string()), settings.maps, { MapPool::CLASSIC, MapPool::SPECIAL, MapPool::ALL });
	return settings;
}

nlohmann::json tw::tournament::toJson(const MatchResult & result)
{
	nlohmann::json json = {
		{ "winner", result.winnerTeamId },
		{ "reason", toString(result.reason) },
		{ "hpA", result.hpPercentA },
		{ "hpB", result.hpPercentB },
		{ "rounds", result.rounds }
	};
	if (!result.players.empty())
	{
		nlohmann::json players = nlohmann::json::array();
		for (const PlayerRecord & player : result.players)
		{
			nlohmann::json value = {
				{ "name", player.name }, { "class", player.className }, { "side", player.side }, { "dealt", player.dealt },
				{ "healed", player.healed }, { "shielded", player.shielded }, { "kills", player.kills }, { "mvp", player.mvp }
			};
			if (!player.badges.empty())
				value["badges"] = player.badges;
			if (player.standIn)
				value["standIn"] = true;
			players.push_back(value);
		}
		json["players"] = players;
	}
	return json;
}

MatchResult tw::tournament::resultFromJson(const nlohmann::json & json)
{
	MatchResult result;
	result.winnerTeamId = json.value("winner", 0);
	parseReason(json.value("reason", std::string()), result.reason);
	result.hpPercentA = json.value("hpA", 0.0);
	result.hpPercentB = json.value("hpB", 0.0);
	result.rounds = json.value("rounds", 0);
	for (const nlohmann::json & value : json.value("players", nlohmann::json::array()))
	{
		PlayerRecord player;
		player.name = value.value("name", std::string());
		player.className = value.value("class", std::string());
		player.side = value.value("side", 0);
		player.dealt = value.value("dealt", 0);
		player.healed = value.value("healed", 0);
		player.shielded = value.value("shielded", 0);
		player.kills = value.value("kills", 0);
		player.mvp = value.value("mvp", false);
		player.standIn = value.value("standIn", false);
		for (const nlohmann::json & badge : value.value("badges", nlohmann::json::array()))
		{
			if (badge.is_string())
				player.badges.push_back(badge.get<std::string>());
		}
		result.players.push_back(player);
	}
	return result;
}

nlohmann::json tw::tournament::toJson(const TMatch & match)
{
	nlohmann::json json = {
		{ "id", match.id },
		{ "stage", match.stageIndex },
		{ "bracket", match.bracket },
		{ "round", match.round },
		{ "order", match.order },
		{ "slotA", slotToJson(match.slotA) },
		{ "slotB", slotToJson(match.slotB) },
		{ "teamA", match.teamA },
		{ "teamB", match.teamB },
		{ "status", toString(match.status) },
		{ "mapId", match.mapId },
		{ "sessionId", match.sessionId }
	};

	if (match.result.has_value())
		json["result"] = toJson(*match.result);

	return json;
}

nlohmann::json tw::tournament::toJson(const Tournament & tournament)
{
	nlohmann::json stages = nlohmann::json::array();
	for (const Stage & stage : tournament.stages)
	{
		stages.push_back({
			{ "type", toString(stage.type) },
			{ "name", stage.name },
			{ "built", stage.built },
			{ "finished", stage.finished },
			{ "pools", stage.pools },
			{ "seeds", stage.seeds },
			{ "bracketSize", stage.bracketSize },
			{ "totalRounds", stage.totalRounds },
			{ "currentRound", stage.currentRound }
		});
	}

	nlohmann::json matches = nlohmann::json::array();
	for (const auto & entry : tournament.matches)
		matches.push_back(toJson(entry.second));

	return {
		{ "version", FILE_VERSION },
		{ "id", tournament.id },
		{ "name", tournament.name },
		{ "settings", toJson(tournament.settings) },
		{ "status", toString(tournament.status) },
		{ "teams", tournament.teamIds },
		{ "stages", stages },
		{ "matches", matches },
		{ "nextMatchId", tournament.nextMatchId },
		{ "rngSeed", tournament.rngSeed }
	};
}

bool tw::tournament::fromJson(const nlohmann::json & json, Tournament & tournament, std::string * error)
{
	try
	{
		Tournament loaded;
		loaded.id = json.at("id").get<int>();
		loaded.name = json.value("name", std::string());
		loaded.settings = settingsFromJson(json.value("settings", nlohmann::json::object()));
		parseEnum(json.value("status", std::string()), loaded.status,
			{ TournamentStatus::DRAFT, TournamentStatus::RUNNING, TournamentStatus::FINISHED });
		loaded.teamIds = json.value("teams", std::vector<int>());
		loaded.nextMatchId = json.value("nextMatchId", 1);
		loaded.rngSeed = json.value("rngSeed", (std::uint32_t)0);

		for (const nlohmann::json & stageJson : json.value("stages", nlohmann::json::array()))
		{
			Stage stage;
			stage.type = stageTypeFromString(stageJson.value("type", std::string()));
			stage.name = stageJson.value("name", std::string());
			stage.built = stageJson.value("built", false);
			stage.finished = stageJson.value("finished", false);
			stage.pools = stageJson.value("pools", std::vector<std::vector<int>>());
			stage.seeds = stageJson.value("seeds", std::vector<int>());
			stage.bracketSize = stageJson.value("bracketSize", 0);
			stage.totalRounds = stageJson.value("totalRounds", 0);
			stage.currentRound = stageJson.value("currentRound", 0);
			loaded.stages.push_back(stage);
		}

		for (const nlohmann::json & matchJson : json.value("matches", nlohmann::json::array()))
		{
			TMatch match;
			match.id = matchJson.at("id").get<int>();
			match.stageIndex = matchJson.value("stage", 0);
			match.bracket = matchJson.value("bracket", std::string());
			match.round = matchJson.value("round", 1);
			match.order = matchJson.value("order", 0);
			match.slotA = slotFromJson(matchJson.value("slotA", nlohmann::json::object()));
			match.slotB = slotFromJson(matchJson.value("slotB", nlohmann::json::object()));
			match.teamA = matchJson.value("teamA", 0);
			match.teamB = matchJson.value("teamB", 0);
			parseEnum(matchJson.value("status", std::string()), match.status,
				{ MatchStatus::PENDING, MatchStatus::READY, MatchStatus::IN_PROGRESS, MatchStatus::DONE });
			match.mapId = matchJson.value("mapId", 0);
			match.sessionId = matchJson.value("sessionId", 0);
			if (matchJson.contains("result") && matchJson["result"].is_object())
				match.result = resultFromJson(matchJson["result"]);

			if (match.stageIndex < 0 || match.stageIndex >= (int)loaded.stages.size())
				throw std::runtime_error("match " + std::to_string(match.id) + " : phase inconnue");

			loaded.matches[match.id] = match;
			loaded.nextMatchId = std::max(loaded.nextMatchId, match.id + 1);
		}

		tournament = loaded;
		return true;
	}
	catch (const std::exception & e)
	{
		if (error != nullptr)
			*error = std::string("Tournoi invalide : ") + e.what();
		return false;
	}
}
