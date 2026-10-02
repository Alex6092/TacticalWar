#pragma once

#include <cstdint>
#include <random>
#include <string>

#include <BattleState.h>
#include <GameData.h>
#include <LineFramer.h>
#include <nlohmann/json.hpp>

// Client automatique : se connecte comme un joueur, choisit une classe, se place et joue
// des actions légales (en utilisant les mêmes règles que le serveur). Sert à tester le
// serveur et à simuler des tournois complets sur un seul poste.
class Bot
{
public:
	struct Options
	{
		std::string host = "127.0.0.1";
		int port = 12345;
		std::string login;
		std::string password;
		int classId = 0;			// 0 : au hasard
		int actionDelayMs = 400;	// Pause entre deux actions (pour pouvoir suivre le combat)
		bool verbose = false;
	};

	explicit Bot(const Options & options);
	~Bot();

	// Boucle principale ; rend la main si la connexion est perdue.
	int run();

private:
	bool connectToServer();
	void send(const std::string & line);
	void onLine(const std::string & line);
	void onBattleEvent(const nlohmann::json & event);
	void act(std::int64_t now);
	bool tryCast(const tw::battle::Fighter & me);
	bool tryMove(const tw::battle::Fighter & me);
	void log(const std::string & text);

	Options options;
	std::uintptr_t socket;
	tw::protocol::LineFramer framer;
	std::mt19937 rng;

	tw::battle::GameData data;
	tw::battle::BattleMap map;
	tw::battle::BattleState state;
	int you;
	std::uint64_t seq;
	bool inBattle;
	bool awaiting;
	bool readySent;
	std::int64_t nextActionAt;
	int battlesPlayed;
};
