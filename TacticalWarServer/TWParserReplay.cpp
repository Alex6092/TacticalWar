// Rediffusions : chaque combat est enregistré (data/replays/<id>.jsonl) et peut être revu par un
// spectateur. La relecture renvoie le même flux qu'un combat en direct (MP, HG, BI puis les lots BV),
// au rythme d'origine : l'écran spectateur du client l'affiche sans traitement particulier.
#include "TWParser.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <BattleMirror.h>
#include <Highlights.h>
#include <JsonFile.h>
#include <Message.h>
#include <StateJson.h>

#pragma warning(disable : 4996)	// std::localtime

namespace
{
	// Entre deux lots, l'attente est plafonnée (les longues réflexions ne sont pas rejouées). Les
	// extraits du réalisateur s'enchaînent sans temps morts.
	const std::int64_t PLAYBACK_START_DELAY_MS = 800;
	const std::int64_t PLAYBACK_MIN_GAP_MS = 30;
	const std::int64_t PLAYBACK_MAX_GAP_MS = 2500;
	const std::int64_t EXTRACT_START_DELAY_MS = 400;
	const std::int64_t EXTRACT_MAX_GAP_MS = 800;
	// Moments d'un combat en cours : les plus récents, au plus ce nombre par combat.
	const std::size_t LIVE_HIGHLIGHTS = 8;
	// Temps forts des rediffusions proposés au réalisateur.
	const std::size_t REPLAY_HIGHLIGHTS = 24;

	std::string teamOf(const nlohmann::json & teams, std::size_t index)
	{
		return teams.is_array() && teams.size() > index && teams[index].is_string() ? teams[index].get<std::string>() : std::string();
	}

	std::string encode(const std::string & op, const nlohmann::json & body)
	{
		return tw::protocol::Message::encode(op, body);
	}

	std::string currentDate()
	{
		std::time_t now = std::time(nullptr);
		char text[32];
		std::strftime(text, sizeof(text), "%d/%m/%Y %H:%M", std::localtime(&now));
		return text;
	}
}

void TWParser::startRecording(BattleSession * session)
{
	std::string error;
	if (!tw::store::ensureDirectory(replays.getDirectory(), &error))
	{
		std::cout << "Rediffusion non enregistrée : " << error << std::endl;
		return;
	}

	nlohmann::json header = battleSnapshot(session, -1);
	nlohmann::json record = {
		{ "version", 1 },
		{ "session", session->getId() },
		{ "title", header["title"] },
		{ "teams", header["teams"] },
		{ "date", currentDate() },
		{ "mapId", session->getMapId() },
		{ "snapshot", header }
	};
	auto map = mapMessages.find(session->getMapId());
	if (map != mapMessages.end())
	{
		// Message MP : "MP" + carte JSON + fin de ligne.
		nlohmann::json mapJson = nlohmann::json::parse(map->second.substr(2), nullptr, false);
		if (!mapJson.is_discarded())
			record["map"] = mapJson;
	}

	ReplayRecording recording;
	recording.writer.reset(new tw::store::ReplayWriter());
	recording.startMs = nowMs();
	recording.snapshot = header;
	recording.teams = header["teams"];
	recording.mapId = session->getMapId();
	recording.match = header.value("title", std::string());
	if (!recording.writer->open(replays.pathOf(replays.newId(session->getId())), record, &error))
	{
		std::cout << "Rediffusion non enregistrée : " << error << std::endl;
		return;
	}
	recordings[session->getId()] = std::move(recording);
}

void TWParser::recordBatch(BattleSession * session, const nlohmann::json & batch)
{
	auto it = recordings.find(session->getId());
	if (it != recordings.end())
	{
		std::int64_t elapsed = nowMs() - it->second.startMs;
		it->second.writer->append(elapsed, batch);
		it->second.batches.push_back({ elapsed, batch });
	}
}

void TWParser::stopRecording(BattleSession * session, const nlohmann::json & end, bool keep)
{
	auto it = recordings.find(session->getId());
	if (it == recordings.end())
		return;

	std::string path = it->second.writer->getPath();
	// Temps forts du combat, rejoués par le réalisateur quand aucun combat n'est en cours.
	nlohmann::json finalEnd = end;
	if (keep && end.is_object() && !end.empty())
	{
		const nlohmann::json & teams = it->second.teams;
		nlohmann::json highlights = nlohmann::json::array();
		for (const tw::battle::Highlight & highlight : tw::battle::detectHighlights(it->second.snapshot, it->second.batches, teamOf(teams, 0), teamOf(teams, 1)))
			highlights.push_back(tw::battle::highlightJson(highlight));
		finalEnd["highlights"] = highlights;
		replayHighlightsValid = false;
	}
	it->second.writer->finish(finalEnd);
	recordings.erase(it);
	if (!keep)
		std::remove(path.c_str());
}

void TWParser::stopPlayback(ClientState * client)
{
	playbacks.erase(client->getConnId());
}

void TWParser::handleReplayMessage(ClientState * client, const std::string & op, const nlohmann::json & body)
{
	if (op == "RL")
	{
		send(client, encode("RL", { { "replays", replays.list(100) } }));
		return;
	}
	if (op == "HL")
	{
		send(client, encode("HL", highlightListJson()));
		return;
	}

	// RP : revoir un combat, ou un extrait d'un combat en cours (RP{session, from, to}, léger différé).
	ReplayPlayback playback;
	std::string error;
	bool live = body.contains("session");
	if (live && !(body.contains("from") && body.contains("to")))
		error = u8"Un combat en cours se regarde en direct (SW).";
	else if (live)
		loadLiveRecording(body.value("session", 0), playback.replay, error);
	else if (!replays.load(body.value("id", std::string()), playback.replay, &error) && error.empty())
		error = u8"Rediffusion introuvable.";
	if (!error.empty())
	{
		send(client, encode("ER", { { "op", op }, { "message", error } }));
		return;
	}

	const nlohmann::json & header = playback.replay.header;
	nlohmann::json snapshot = header["snapshot"];
	snapshot["title"] = "Rediffusion - " + snapshot.value("title", std::string());

	// Extrait (temps fort) : l'état est reconstruit au début de l'extrait par le miroir, puis seuls
	// les lots de l'extrait sont rejoués, suivis de RE.
	std::size_t count = playback.replay.batches.size();
	if (body.contains("from") && body.contains("to") && count > 0)
	{
		std::size_t from = (std::size_t)std::max(0, body.value("from", 0));
		std::size_t to = (std::size_t)std::max(0, body.value("to", 0));
		if (from >= count || to < from)
		{
			send(client, encode("ER", { { "op", op }, { "message", u8"Extrait introuvable dans cette rediffusion." } }));
			return;
		}
		to = std::min(to, count - 1);
		tw::battle::BattleState state;
		tw::battle::BattleMap map;
		tw::battle::BattleMirror::applySnapshot(state, map, header["snapshot"]);
		std::uint64_t seq = header["snapshot"].value("seq", (std::uint64_t)0);
		for (std::size_t i = 0; i < from; i++)
		{
			const nlohmann::json & batch = playback.replay.batches[i].second;
			for (const nlohmann::json & event : batch.value("ev", nlohmann::json::array()))
				tw::battle::BattleMirror::applyEvent(state, event);
			seq = batch.value("seq", seq);
		}
		std::string prefix = live ? u8"À l'instant : " : u8"Temps fort : ";
		std::string title = live ? u8"À l'instant" : u8"Temps fort";
		for (const nlohmann::json & highlight : playback.replay.end.is_object() ? playback.replay.end.value("highlights", nlohmann::json::array()) : nlohmann::json::array())
		{
			if (highlight.value("from", (std::size_t)0) == from && highlight.value("to", (std::size_t)0) == to)
				title = prefix + highlight.value("title", std::string());
		}
		snapshot = tw::battle::statejson::snapshot(state, map, seq, -1, 0);
		// Champs ajoutés par le serveur à l'état d'origine (noms des équipes, classes interdites).
		for (const char * key : { "teams", "forbidden" })
		{
			if (header["snapshot"].contains(key))
				snapshot[key] = header["snapshot"][key];
		}
		// Bandeau court : le titre du temps fort (le match est rappelé par les équipes).
		snapshot["title"] = title;
		playback.replay.batches = std::vector<std::pair<std::int64_t, nlohmann::json>>(playback.replay.batches.begin() + from,
			playback.replay.batches.begin() + to + 1);
		playback.extract = true;
	}

	std::int64_t due = playback.extract ? EXTRACT_START_DELAY_MS : PLAYBACK_START_DELAY_MS;
	std::int64_t maxGap = playback.extract ? EXTRACT_MAX_GAP_MS : PLAYBACK_MAX_GAP_MS;
	std::int64_t previous = 0;
	for (const auto & batch : playback.replay.batches)
	{
		if (!playback.due.empty())
			due += std::max(PLAYBACK_MIN_GAP_MS, std::min(maxGap, batch.first - previous));
		playback.due.push_back(due);
		previous = batch.first;
	}
	playback.startMs = nowMs();

	removeSpectator(client);
	sendGameData(client);
	int mapId = header.value("mapId", 0);
	if (header.contains("map"))
		send(client, "MP" + header["map"].dump(-1, ' ', false, nlohmann::json::error_handler_t::replace) + "\n");
	send(client, "HG" + std::to_string(mapId) + "\n");

	send(client, encode("BI", snapshot));

	playbacks[client->getConnId()] = std::move(playback);
}

void TWParser::tickReplays(std::int64_t now)
{
	for (auto it = playbacks.begin(); it != playbacks.end();)
	{
		ReplayPlayback & playback = it->second;
		auto client = clients.find(it->first);
		if (client == clients.end())
		{
			it = playbacks.erase(it);
			continue;
		}

		while (playback.next < playback.replay.batches.size() && now - playback.startMs >= playback.due[playback.next])
		{
			send(client->second, encode("BV", playback.replay.batches[playback.next].second));
			playback.next++;
		}

		if (playback.next >= playback.replay.batches.size())
		{
			if (playback.extract)
				send(client->second, encode("RE", nlohmann::json::object()));
			it = playbacks.erase(it);
		}
		else
			++it;
	}
}

bool TWParser::loadLiveRecording(int session, tw::store::Replay & replay, std::string & error)
{
	auto it = recordings.find(session);
	if (it == recordings.end())
	{
		error = u8"Ce combat est terminé : ses temps forts sont dans les rediffusions.";
		return false;
	}
	const ReplayRecording & recording = it->second;
	replay.header = { { "snapshot", recording.snapshot }, { "mapId", recording.mapId } };
	auto map = mapMessages.find(recording.mapId);
	if (map != mapMessages.end())
	{
		nlohmann::json mapJson = nlohmann::json::parse(map->second.substr(2), nullptr, false);
		if (!mapJson.is_discarded())
			replay.header["map"] = mapJson;
	}
	replay.batches = recording.batches;
	nlohmann::json highlights = nlohmann::json::array();
	for (const tw::battle::Highlight & highlight : recording.liveHighlights)
		highlights.push_back(tw::battle::highlightJson(highlight));
	replay.end = { { "highlights", highlights } };
	return true;
}

nlohmann::json TWParser::highlightListJson()
{
	// Rediffusions : les temps forts des 10 dernières complètes, chacune à son tour (le meilleur de
	// chaque combat, puis le deuxième...), pour que le réalisateur varie les combats.
	if (!replayHighlightsValid)
	{
		std::vector<std::vector<nlohmann::json>> perReplay;
		std::size_t complete = 0;
		for (const nlohmann::json & summary : replays.list(100))
		{
			if (!summary.value("complete", false))
				continue;
			if (++complete > 10)
				break;
			std::vector<nlohmann::json> items;
			for (const nlohmann::json & highlight : summary.value("highlights", nlohmann::json::array()))
			{
				items.push_back({ { "replay", summary.value("id", std::string()) }, { "match", summary.value("title", std::string()) },
					{ "title", highlight.value("title", std::string()) }, { "kind", highlight.value("kind", std::string()) },
					{ "score", highlight.value("score", 0) }, { "from", highlight.value("from", 0) }, { "to", highlight.value("to", 0) } });
			}
			std::stable_sort(items.begin(), items.end(), [](const nlohmann::json & a, const nlohmann::json & b) { return a.value("score", 0) > b.value("score", 0); });
			if (!items.empty())
				perReplay.push_back(items);
		}
		replayHighlights = nlohmann::json::array();
		for (std::size_t round = 0; replayHighlights.size() < REPLAY_HIGHLIGHTS; round++)
		{
			bool any = false;
			for (const std::vector<nlohmann::json> & items : perReplay)
			{
				if (round < items.size() && replayHighlights.size() < REPLAY_HIGHLIGHTS)
				{
					replayHighlights.push_back(items[round]);
					any = true;
				}
			}
			if (!any)
				break;
		}
		replayHighlightsValid = true;
	}

	// Combats en cours : moments dont la fenêtre est terminée (environ 4 s après), les plus récents
	// d'abord, avec leur âge en secondes. Rejoués en léger différé par le réalisateur (RP{session, from, to}).
	nlohmann::json live = nlohmann::json::array();
	std::int64_t now = nowMs();
	for (auto & entry : recordings)
	{
		ReplayRecording & recording = entry.second;
		if (recording.batches.size() != recording.liveBatchCount)
		{
			recording.liveBatchCount = recording.batches.size();
			recording.liveHighlights = tw::battle::detectHighlights(recording.snapshot, recording.batches, teamOf(recording.teams, 0),
				teamOf(recording.teams, 1), LIVE_HIGHLIGHTS);
		}
		std::int64_t elapsed = now - recording.startMs;
		for (const tw::battle::Highlight & highlight : recording.liveHighlights)
		{
			if (highlight.atMs + tw::battle::HIGHLIGHT_AFTER_MS > elapsed || highlight.to >= recording.batches.size())
				continue;
			nlohmann::json item = tw::battle::highlightJson(highlight);
			item["session"] = entry.first;
			item["match"] = recording.match;
			item["live"] = true;
			item["age"] = (elapsed - highlight.atMs) / 1000;
			live.push_back(item);
		}
	}
	std::stable_sort(live.begin(), live.end(), [](const nlohmann::json & a, const nlohmann::json & b) { return a.value("age", 0) < b.value("age", 0); });

	nlohmann::json all = live;
	for (const nlohmann::json & highlight : replayHighlights)
		all.push_back(highlight);
	return { { "highlights", all } };
}
