// Rediffusions : chaque combat est enregistré (data/replays/<id>.jsonl) et peut être revu par un
// spectateur. La relecture renvoie le même flux qu'un combat en direct (MP, HG, BI puis les lots BV),
// au rythme d'origine : l'écran spectateur du client l'affiche sans traitement particulier.
#include "TWParser.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <JsonFile.h>
#include <Message.h>

#pragma warning(disable : 4996)	// std::localtime

namespace
{
	// Entre deux lots, l'attente est plafonnée (les longues réflexions ne sont pas rejouées).
	const std::int64_t PLAYBACK_START_DELAY_MS = 800;
	const std::int64_t PLAYBACK_MIN_GAP_MS = 30;
	const std::int64_t PLAYBACK_MAX_GAP_MS = 2500;

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
		it->second.writer->append(nowMs() - it->second.startMs, batch);
}

void TWParser::stopRecording(BattleSession * session, const nlohmann::json & end, bool keep)
{
	auto it = recordings.find(session->getId());
	if (it == recordings.end())
		return;

	std::string path = it->second.writer->getPath();
	it->second.writer->finish(end);
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

	// RP : revoir un combat.
	ReplayPlayback playback;
	std::string error;
	if (!replays.load(body.value("id", std::string()), playback.replay, &error))
	{
		send(client, encode("ER", { { "op", op }, { "message", error } }));
		return;
	}

	const nlohmann::json & header = playback.replay.header;
	std::int64_t due = PLAYBACK_START_DELAY_MS;
	std::int64_t previous = 0;
	for (const auto & batch : playback.replay.batches)
	{
		if (!playback.due.empty())
			due += std::max(PLAYBACK_MIN_GAP_MS, std::min(PLAYBACK_MAX_GAP_MS, batch.first - previous));
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

	nlohmann::json snapshot = header["snapshot"];
	snapshot["title"] = "Rediffusion - " + snapshot.value("title", std::string());
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
			it = playbacks.erase(it);
		else
			++it;
	}
}
