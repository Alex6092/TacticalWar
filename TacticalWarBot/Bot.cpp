#include <WinSock2.h>
#include <WS2tcpip.h>

#include "Bot.h"

#include <algorithm>
#include <chrono>
#include <iostream>

#include <BattleMirror.h>
#include <BattleRules.h>
#include <BotBrain.h>
#include <EnvironmentManager.h>
#include <EnvironmentMap.h>
#include <Message.h>
#include <Opcodes.h>

#pragma comment(lib, "ws2_32.lib")

using namespace tw::battle;
using nlohmann::json;

namespace
{
	std::int64_t nowMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
}

Bot::Bot(const Options & options)
	: options(options), socket(INVALID_SOCKET), rng(std::random_device()()), you(-1), seq(0),
	inBattle(false), awaiting(false), readySent(false), nextActionAt(0), battlesPlayed(0)
{
}

Bot::~Bot()
{
	if (socket != INVALID_SOCKET)
		closesocket((SOCKET)socket);
	WSACleanup();
}

void Bot::log(const std::string & text)
{
	std::cout << "[" << options.login << "] " << text << std::endl;
}

bool Bot::connectToServer()
{
	WSADATA wsData;
	if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0)
		return false;

	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	addrinfo * result = nullptr;
	if (getaddrinfo(options.host.c_str(), std::to_string(options.port).c_str(), &hints, &result) != 0)
		return false;

	SOCKET sock = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	bool connected = sock != INVALID_SOCKET && connect(sock, result->ai_addr, (int)result->ai_addrlen) == 0;
	freeaddrinfo(result);
	if (!connected)
		return false;

	u_long nonBlocking = 1;
	ioctlsocket(sock, FIONBIO, &nonBlocking);
	socket = (std::uintptr_t)sock;
	return true;
}

void Bot::send(const std::string & line)
{
	std::string data = line + "\n";
	std::size_t sent = 0;
	while (sent < data.size())
	{
		int count = ::send((SOCKET)socket, data.data() + sent, (int)(data.size() - sent), 0);
		if (count == SOCKET_ERROR)
		{
			if (WSAGetLastError() == WSAEWOULDBLOCK)
			{
				Sleep(1);
				continue;
			}
			return;
		}
		sent += count;
	}
}

int Bot::run()
{
	if (!connectToServer())
	{
		log("Connexion impossible a " + options.host + ":" + std::to_string(options.port));
		return 1;
	}

	send("HG" + tw::protocol::loginPayload(options.login, options.password));

	char buffer[16 * 1024];
	while (true)
	{
		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET((SOCKET)socket, &readSet);
		timeval timeout = { 0, 50 * 1000 };
		select(0, &readSet, nullptr, nullptr, &timeout);

		while (true)
		{
			int received = recv((SOCKET)socket, buffer, sizeof(buffer), 0);
			if (received > 0)
			{
				framer.feed(buffer, received);
				continue;
			}
			if (received == 0 || WSAGetLastError() != WSAEWOULDBLOCK)
			{
				log("Deconnecte du serveur.");
				return 0;
			}
			break;
		}

		std::string line;
		while (framer.nextLine(line))
			onLine(line);

		act(nowMs());
	}
}

void Bot::pickClass(int forbiddenClass, bool forTeammate)
{
	std::vector<int> allowed;
	for (const ClassDef & classDef : data.classes)
	{
		if (classDef.id != forbiddenClass)
			allowed.push_back(classDef.id);
	}
	int classId = options.classId != forbiddenClass ? options.classId : 0;
	if (classId == 0 && !allowed.empty())
		classId = allowed[rng() % allowed.size()];
	std::vector<int> spells;
	if (const ClassDef * classDef = data.findClass(classId))
		spells = randomSpellChoice(*classDef, rng);
	std::vector<std::string> talents = randomTalentChoice(data, talentSlots, rng);
	nlohmann::json pick = { { "class", classId }, { "spells", spells }, { "talents", talents } };
	if (forTeammate)
		pick["teammate"] = true;
	send("PC" + pick.dump());
	if (options.verbose)
		log(std::string(forTeammate ? "Choix pour le coequipier : classe " : "Choix de la classe ") + std::to_string(classId));
}

void Bot::pickForAbsentMate(std::int64_t now)
{
	const std::int64_t MATE_WAIT_MS = 10 * 1000;
	if (ownPicked && mateAbsent && !mateLocked && !matePickSent && (mateStandIn || now - mateAbsentSince >= MATE_WAIT_MS))
	{
		matePickSent = true;
		pickClass(forbidden, true);
	}
}

void Bot::onLine(const std::string & line)
{
	tw::protocol::Message message;
	if (!tw::protocol::Message::decode(line, message))
		return;

	const std::string & op = message.op;
	if (op == tw::protocol::KEEPALIVE_PING)
	{
		send(tw::protocol::KEEPALIVE_PONG);
	}
	else if (op == "HK")
	{
		log("Identifiants refuses.");
	}
	else if (op == "GD")
	{
		std::string error;
		if (!data.loadFromJsonText(message.payload, error))
			log(error);
	}
	else if (op == "MP")
	{
		if (!tw::EnvironmentManager::getInstance()->registerReceivedMap(message.payload))
			log("Carte recue invalide.");
	}
	else if (op == "HW")
	{
		log("En attente d'un match...");
	}
	else if (op == "DI")
	{
		// Défi d'une autre équipe (match amical) : les bots refusent.
		nlohmann::json challenge = nlohmann::json::parse(message.payload, nullptr, false);
		if (challenge.is_object())
		{
			log("Defi refuse : " + challenge.value("name", std::string()));
			send("DA" + nlohmann::json({ { "from", challenge.value("from", 0) }, { "accept", false } }).dump());
		}
	}
	else if (op == "HC")
	{
		nlohmann::json selection = nlohmann::json::parse(message.payload, nullptr, false);
		if (!selection.is_object())
			selection = nlohmann::json::object();
		talentSlots = selection.value("talents", 0);
		forbidden = 0;
		ownPicked = mateAbsent = mateLocked = matePickSent = mateStandIn = false;
		if (selection.value("ban", 0) > 0 && !data.classes.empty())
		{
			// Bannissement d'abord : une classe au hasard ; le choix de classe suit le message BB.
			int banned = data.classes[rng() % data.classes.size()].id;
			send("PB" + nlohmann::json({ { "class", banned } }).dump());
			if (options.verbose)
				log("Bannissement de la classe " + std::to_string(banned));
		}
		else
		{
			pickClass(0);
			ownPicked = true;
			pickForAbsentMate(nowMs());
		}
	}
	else if (op == "PT")
	{
		nlohmann::json mate = nlohmann::json::parse(message.payload, nullptr, false);
		if (mate.is_object())
		{
			bool absent = !mate.value("present", true);
			if (absent && !mateAbsent)
				mateAbsentSince = nowMs();
			mateAbsent = absent;
			mateStandIn = mate.value("standIn", false);
			mateLocked = mate.value("locked", false);
			pickForAbsentMate(nowMs());
		}
	}
	else if (op == "BB")
	{
		nlohmann::json ban = nlohmann::json::parse(message.payload, nullptr, false);
		if (ban.is_object() && ban.value("done", false))
		{
			forbidden = ban.value("forbidden", 0);
			pickClass(forbidden);
			ownPicked = true;
			pickForAbsentMate(nowMs());
		}
	}
	else if (op == "HG")
	{
		int mapId = std::atoi(message.payload.c_str());
		tw::Environment * environment = tw::EnvironmentManager::getInstance()->loadEnvironment(mapId);
		if (environment != nullptr)
		{
			map = battleMapFromEnvironment(environment);
			delete environment;
		}
		inBattle = true;
	}
	else if (op == "BI")
	{
		json snapshot;
		if (message.parseJson(snapshot))
		{
			BattleMirror::applySnapshot(state, map, snapshot);
			you = snapshot.value("you", -1);
			seq = snapshot.value("seq", (std::uint64_t)0);
			awaiting = false;
			readySent = false;
			if (options.verbose)
				log("Etat du combat recu (combattant " + std::to_string(you) + ").");
		}
	}
	else if (op == "BV")
	{
		json batch;
		if (!message.parseJson(batch))
			return;

		std::uint64_t batchSeq = batch.value("seq", (std::uint64_t)0);
		if (batchSeq != seq + 1)
		{
			send("BR{}");
			return;
		}
		seq = batchSeq;
		awaiting = false;

		for (const json & event : batch["ev"])
		{
			BattleMirror::applyEvent(state, event);
			onBattleEvent(event);
		}
	}
	else if (op == "ER")
	{
		awaiting = false;
		if (options.verbose)
			log("Refus : " + message.payload);
	}
}

void Bot::onBattleEvent(const json & event)
{
	if (event.value("t", std::string()) == "end")
	{
		const Fighter * me = state.findFighter(you);
		bool won = me != nullptr && me->team == event.value("winner", 0);
		battlesPlayed++;
		log(std::string(won ? "Victoire" : "Defaite") + " (" + event.value("reason", std::string()) + ", tour " + std::to_string(event.value("round", 0)) + ")");
		inBattle = false;
		you = -1;
	}
}

void Bot::act(std::int64_t now)
{
	pickForAbsentMate(now);

	if (!inBattle || you < 0 || awaiting || now < nextActionAt)
		return;

	const Fighter * me = state.findFighter(you);
	if (me == nullptr)
		return;

	if (state.phase == BattlePhase::PLACEMENT)
	{
		if (!me->ready && !readySent)
		{
			send("Cs{\"ready\":true}");
			readySent = true;
		}
		return;
	}

	// Son combattant, ou celui de son coéquipier absent, qu'il pilote.
	const Fighter * active = state.findFighter(state.activeFighterId());
	if (state.phase != BattlePhase::FIGHT || active == nullptr
		|| (active->id != you && !(active->piloted && active->team == me->team)))
		return;

	nextActionAt = now + options.actionDelayMs;
	awaiting = true;

	BotOptions brain;
	brain.planner = options.hard;
	BotAction action = chooseBotAction(state, map, data, active->id, rng, brain);
	if (action.kind == BotAction::Kind::CAST)
	{
		send("CL" + json({ { "slot", action.slot }, { "x", action.target.x }, { "y", action.target.y } }).dump());
	}
	else if (action.kind == BotAction::Kind::MOVE)
	{
		json path = json::array();
		for (const Cell & step : action.path)
			path.push_back(json::array({ step.x, step.y }));
		send("Cm" + json({ { "path", path } }).dump());
	}
	else
	{
		send("Ct");
	}
}
