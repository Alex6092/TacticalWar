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
		bool hard = false;			// Difficulté « Difficile » (--level hard)
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
	// Classe (celle demandée, sinon au hasard parmi les autorisées), 4 sorts et talents au hasard.
	void pickClass(int forbiddenClass, bool forTeammate = false);
	// Coéquipier absent depuis 10 s (une coupure brève ne compte pas), ou second personnage d'un bot
	// seul dans son équipe : le bot choisit aussi sa classe, puis joue son personnage.
	void pickForAbsentMate(std::int64_t now);
	void act(std::int64_t now);
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
	int talentSlots = 0;		// Talents à choisir pour le prochain match (HC)
	int forbidden = 0;			// Classe interdite à l'équipe (bannissement)
	bool ownPicked = false;
	bool mateAbsent = false;
	std::int64_t mateAbsentSince = 0;
	bool mateStandIn = false;
	bool mateLocked = false;
	bool matePickSent = false;
};
