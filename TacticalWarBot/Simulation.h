#pragma once

#include <cstdint>
#include <string>

// Simulation d'équilibrage : joue des combats 2v2 entre IA (BotBrain) directement sur le moteur,
// sans serveur, sur les cartes du dossier ./assets/map, avec les données ./assets/data/gamedata.json.
// Affiche les taux de victoire par classe et par composition d'équipe, la durée des combats et
// leurs fins. mapId = 0 : toutes les cartes du pool de tournoi, à tour de rôle.
// dataPath : autre fichier de données à essayer (réglages d'équilibrage).
// zonePoints > 0 : mode "zone à tenir", gagné au premier à zonePoints points (0 : au KO).
// talents > 0 : chaque combattant reçoit ce nombre de talents de tournoi, tirés au hasard.
// bonuses : bonus sur la carte (orbes au centre).
// hardTeam (1 ou 2) : cette équipe joue en difficulté « Difficile » (0 : aucune), pour la comparer au niveau normal.
int runSimulation(int battles, int mapId, std::uint32_t seed, const std::string & dataPath = "./assets/data/gamedata.json",
	int zonePoints = 0, int talents = 0, bool bonuses = false, int hardTeam = 0);
