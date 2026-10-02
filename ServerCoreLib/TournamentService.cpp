#include "TournamentService.h"
#include "JsonFile.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <random>
#include <set>

#include <TournamentJson.h>

using namespace tw;
using namespace tw::tournament;

TournamentService::TournamentService(const std::string & dataDirectory)
	: dataDirectory(dataDirectory), nextId(1), changed(true)
{
}

std::string TournamentService::path(int id) const
{
	return store::joinPath(store::joinPath(dataDirectory, "tournaments"), std::to_string(id) + ".json");
}

bool TournamentService::load(std::string & log)
{
	tournaments.clear();
	std::string directory = store::joinPath(dataDirectory, "tournaments");
	std::error_code ec;
	if (!std::filesystem::exists(std::filesystem::u8path(directory), ec))
	{
		log = "Aucun tournoi enregistré.";
		return true;
	}

	int loaded = 0;
	for (const auto & file : std::filesystem::directory_iterator(std::filesystem::u8path(directory), ec))
	{
		if (file.path().extension() != ".json")
			continue;

		nlohmann::json json;
		std::string error;
		Tournament tournament;
		if (!store::readJsonFile(file.path().u8string(), json, &error) || !fromJson(json, tournament, &error))
		{
			log += "Tournoi ignoré (" + file.path().filename().u8string() + ") : " + error + "\n";
			continue;
		}

		Entry entry;
		entry.engine = TournamentEngine(tournament);
		entry.paused = json.value("paused", false);
		// Les combats en cours au moment de l'arrêt du serveur sont à rejouer.
		entry.engine.recoverAfterRestart();

		tournaments[tournament.id] = entry;
		nextId = std::max(nextId, tournament.id + 1);
		save(tournament.id);
		loaded++;
	}

	log += std::to_string(loaded) + " tournoi(s) chargé(s).";
	changed = true;
	return true;
}

bool TournamentService::save(int id, std::string * error)
{
	Entry * entry = findEntry(id);
	if (entry == nullptr)
		return false;

	nlohmann::json json = toJson(entry->engine.get());
	json["paused"] = entry->paused;
	changed = true;
	return store::writeJsonFileAtomic(path(id), json, error);
}

TournamentService::Entry * TournamentService::findEntry(int id)
{
	auto it = tournaments.find(id);
	return it == tournaments.end() ? nullptr : &it->second;
}

const TournamentEngine * TournamentService::find(int id) const
{
	auto it = tournaments.find(id);
	return it == tournaments.end() ? nullptr : &it->second.engine;
}

std::vector<int> TournamentService::ids() const
{
	std::vector<int> result;
	for (const auto & entry : tournaments)
		result.push_back(entry.first);
	return result;
}

bool TournamentService::isPaused(int id) const
{
	auto it = tournaments.find(id);
	return it != tournaments.end() && it->second.paused;
}

bool TournamentService::takeChanged()
{
	bool result = changed;
	changed = false;
	return result;
}

std::string TournamentService::create(const std::string & name, const Settings & settings, const std::vector<int> & teamIds, int & newId)
{
	if (name.empty())
		return "Le nom du tournoi est obligatoire.";

	Tournament tournament;
	tournament.id = nextId;
	tournament.name = name;
	tournament.rngSeed = std::random_device()();

	Entry entry;
	entry.engine = TournamentEngine(tournament);
	std::string error = entry.engine.setSettings(settings);
	if (error.empty())
		error = entry.engine.setTeams(teamIds);
	if (!error.empty())
		return error;

	newId = nextId++;
	tournaments[newId] = entry;
	save(newId);
	return "";
}

std::string TournamentService::update(int id, const std::string & name, const Settings & settings, const std::vector<int> & teamIds)
{
	Entry * entry = findEntry(id);
	if (entry == nullptr)
		return "Tournoi introuvable.";
	if (entry->engine.get().status != TournamentStatus::DRAFT)
		return "Le tournoi a commencé : il ne peut plus être modifié.";
	if (name.empty())
		return "Le nom du tournoi est obligatoire.";

	Tournament tournament = entry->engine.get();
	tournament.name = name;
	TournamentEngine updated(tournament);
	std::string error = updated.setSettings(settings);
	if (error.empty())
		error = updated.setTeams(teamIds);
	if (!error.empty())
		return error;

	entry->engine = updated;
	save(id);
	return "";
}

std::string TournamentService::start(int id)
{
	Entry * entry = findEntry(id);
	if (entry == nullptr)
		return "Tournoi introuvable.";

	std::string error = entry->engine.start();
	if (error.empty())
		save(id);
	return error;
}

std::string TournamentService::remove(int id)
{
	Entry * entry = findEntry(id);
	if (entry == nullptr)
		return "Tournoi introuvable.";

	if (entry->engine.get().status == TournamentStatus::RUNNING)
	{
		for (const auto & match : entry->engine.get().matches)
		{
			if (match.second.status == MatchStatus::IN_PROGRESS)
				return "Des matchs de ce tournoi sont en cours : attendez leur fin ou mettez le tournoi en pause.";
		}
	}

	tournaments.erase(id);
	std::error_code ec;
	std::filesystem::remove(std::filesystem::u8path(path(id)), ec);
	changed = true;
	return "";
}

std::string TournamentService::setPaused(int id, bool paused)
{
	Entry * entry = findEntry(id);
	if (entry == nullptr)
		return "Tournoi introuvable.";

	entry->paused = paused;
	save(id);
	return "";
}

std::string TournamentService::reportResult(int tournamentId, int matchId, const MatchResult & result, std::uint32_t battleSeed)
{
	Entry * entry = findEntry(tournamentId);
	if (entry == nullptr)
		return "Tournoi introuvable.";

	std::string error = entry->engine.reportResult(matchId, result);
	if (!error.empty())
		return error;

	save(tournamentId);

	// Journal des résultats (une ligne JSON par match), utile en cas de litige.
	const TMatch * match = entry->engine.findMatch(matchId);
	nlohmann::json line = toJson(result);
	line["time"] = (long long)std::time(nullptr);
	line["tournament"] = tournamentId;
	line["match"] = matchId;
	line["bracket"] = match->bracket;
	line["round"] = match->round;
	line["teamA"] = match->teamA;
	line["teamB"] = match->teamB;
	line["seed"] = battleSeed;
	store::appendLine(store::joinPath(dataDirectory, "results.jsonl"), line.dump());
	return "";
}

std::string TournamentService::forceResult(int tournamentId, int matchId, const MatchResult & result, bool cascade)
{
	Entry * entry = findEntry(tournamentId);
	if (entry == nullptr)
		return "Tournoi introuvable.";

	const TMatch * match = entry->engine.findMatch(matchId);
	if (match == nullptr)
		return "Match introuvable.";

	std::string error;
	if (match->status == MatchStatus::DONE)
		error = entry->engine.amendResult(matchId, result, cascade);
	else
		error = entry->engine.reportResult(matchId, result);

	if (error.empty())
	{
		save(tournamentId);
		nlohmann::json line = toJson(result);
		line["time"] = (long long)std::time(nullptr);
		line["tournament"] = tournamentId;
		line["match"] = matchId;
		line["admin"] = true;
		store::appendLine(store::joinPath(dataDirectory, "results.jsonl"), line.dump());
	}
	return error;
}

std::string TournamentService::resetMatch(int tournamentId, int matchId)
{
	Entry * entry = findEntry(tournamentId);
	if (entry == nullptr)
		return "Tournoi introuvable.";

	std::string error = entry->engine.resetMatch(matchId);
	if (error.empty())
		save(tournamentId);
	return error;
}

std::vector<TournamentService::LaunchRequest> TournamentService::nextLaunches(const std::function<bool(int)> & isTeamBusy, int runningMatches, std::int64_t nowMs, const DispatchOptions & options)
{
	std::vector<LaunchRequest> requests;
	std::set<int> reserved;

	for (auto & entry : tournaments)
	{
		const Tournament & tournament = entry.second.engine.get();
		if (tournament.status != TournamentStatus::RUNNING || entry.second.paused)
			continue;

		for (int matchId : entry.second.engine.readyMatches())
		{
			if (runningMatches + (int)requests.size() >= options.maxConcurrentMatches)
				return requests;

			const TMatch * match = entry.second.engine.findMatch(matchId);
			bool available = true;
			for (int team : { match->teamA, match->teamB })
			{
				auto rest = lastMatchEnd.find(team);
				if (reserved.count(team) > 0 || isTeamBusy(team) || (rest != lastMatchEnd.end() && nowMs - rest->second < options.restMs))
					available = false;
			}
			if (!available)
				continue;

			reserved.insert(match->teamA);
			reserved.insert(match->teamB);
			requests.push_back({ tournament.id, matchId, match->teamA, match->teamB });
		}
	}

	return requests;
}

void TournamentService::markLaunched(const LaunchRequest & request, int sessionId)
{
	Entry * entry = findEntry(request.tournamentId);
	if (entry != nullptr && entry->engine.markInProgress(request.matchId, sessionId).empty())
		save(request.tournamentId);
}

void TournamentService::markTeamFinished(int teamId, std::int64_t nowMs)
{
	lastMatchEnd[teamId] = nowMs;
}
