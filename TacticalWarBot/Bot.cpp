#include <WinSock2.h>
#include <WS2tcpip.h>

#include "Bot.h"

#include <algorithm>
#include <chrono>
#include <iostream>

#include <BattleMirror.h>
#include <BattleRules.h>
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

	enum class SpellRole
	{
		OFFENSIVE,
		SUPPORT,
		UTILITY
	};

	SpellRole roleOf(const SpellDef & spell)
	{
		for (const EffectDef & effect : spell.effects)
		{
			if (effect.type == EffectType::DAMAGE || effect.type == EffectType::LIFESTEAL || effect.type == EffectType::DOT)
				return SpellRole::OFFENSIVE;
		}
		for (const EffectDef & effect : spell.effects)
		{
			if (effect.type == EffectType::HEAL || effect.type == EffectType::SHIELD || effect.type == EffectType::HOT)
				return SpellRole::SUPPORT;
		}
		return SpellRole::UTILITY;
	}

	bool hitsAll(const SpellDef & spell)
	{
		for (const EffectDef & effect : spell.effects)
		{
			if (effect.targets == TargetFilter::ALL)
				return true;
		}
		return false;
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

	send("HG" + options.login + ";" + options.password);

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
	else if (op == "HC")
	{
		int classId = options.classId;
		if (classId == 0 && !data.classes.empty())
			classId = data.classes[rng() % data.classes.size()].id;
		send("PC" + std::to_string(classId));
		if (options.verbose)
			log("Choix de la classe " + std::to_string(classId));
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

	if (state.phase != BattlePhase::FIGHT || state.activeFighterId() != you)
		return;

	nextActionAt = now + options.actionDelayMs;
	awaiting = true;

	if (tryCast(*me) || tryMove(*me))
		return;

	send("Ct");
}

bool Bot::tryCast(const Fighter & me)
{
	const ClassDef * classDef = data.findClass(me.classId);
	if (classDef == nullptr)
		return false;

	int bestSlot = -1;
	Cell bestCell;
	int bestScore = 0;

	for (int slot = 0; slot < (int)classDef->spells.size(); slot++)
	{
		const SpellDef & spell = classDef->spells[slot];
		if (!checkSpellResources(me, spell).empty())
			continue;

		SpellRole role = roleOf(spell);
		for (const Cell & cell : castableCells(state, map, data, me, spell))
		{
			int score = 0;
			for (const Cell & hit : impactCells(map, me.position, cell, spell.impact))
			{
				const Fighter * fighter = state.fighterAt(hit);
				if (fighter == nullptr)
					continue;

				bool ally = fighter->team == me.team;
				if (role == SpellRole::OFFENSIVE)
				{
					if (!ally)
						score += 10 + (fighter->maxHp - fighter->hp) / 10;
					else if (hitsAll(spell))
						score -= 15;
				}
				else if (role == SpellRole::SUPPORT && ally)
				{
					score += (fighter->maxHp - fighter->hp) / 3 + 2;
				}
			}

			if (role == SpellRole::UTILITY)
				score = (int)(rng() % 4);

			// Un peu de hasard pour varier les combats.
			score = score * 4 + (int)(rng() % 4);
			if (score > bestScore)
			{
				bestScore = score;
				bestSlot = slot;
				bestCell = cell;
			}
		}
	}

	if (bestSlot < 0 || bestScore < 8)
		return false;

	send("CL" + json({ { "slot", bestSlot }, { "x", bestCell.x }, { "y", bestCell.y } }).dump());
	return true;
}

bool Bot::tryMove(const Fighter & me)
{
	if (me.mp <= 0)
		return false;

	// Se rapproche de l'ennemi vivant le plus proche.
	const Fighter * target = nullptr;
	for (const Fighter & fighter : state.fighters)
	{
		if (fighter.alive && fighter.team != me.team && (target == nullptr || manhattan(fighter.position, me.position) < manhattan(target->position, me.position)))
			target = &fighter;
	}
	if (target == nullptr)
		return false;

	int currentDistance = manhattan(me.position, target->position);
	Cell best = me.position;
	int bestDistance = currentDistance;
	for (const Cell & cell : reachableCells(state, map, me))
	{
		int distance = manhattan(cell, target->position);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = cell;
		}
	}

	if (best == me.position)
		return false;

	json path = json::array();
	for (const Cell & step : findPath(state, map, me, best))
		path.push_back(json::array({ step.x, step.y }));
	send("Cm" + json({ { "path", path } }).dump());
	return true;
}
