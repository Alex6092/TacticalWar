#pragma once

#include <set>
#include <string>
#include <vector>

#include "GameData.h"

namespace tw
{
	namespace battle
	{
		// Progression d'un joueur, retenue par le serveur sur son compte : hauts faits obtenus (au moins
		// une fois), victoires, titres de MVP et énigmes réussies. Elle débloque des apparences.
		struct PlayerProgress
		{
			std::set<std::string> achievements;
			int wins = 0;
			int mvp = 0;
			std::set<std::string> puzzles;
		};

		bool isUnlocked(const AppearanceDef & appearance, const PlayerProgress & progress);
		// Apparences débloquées, dans l'ordre des données de jeu (la première, sans condition, en tête).
		std::vector<std::string> unlockedAppearances(const GameData & data, const PlayerProgress & progress);
		// Apparence demandée par un joueur si elle est débloquée, sinon chaîne vide (classique).
		std::string allowedAppearance(const GameData & data, const PlayerProgress & progress, const std::string & wanted);
		// Condition de déblocage, en texte UTF-8 (« Haut fait « Premier sang » », « 3 énigmes réussies »…).
		std::string unlockCondition(const AppearanceDef & appearance);

		// Couleur d'armure d'un personnage : la couleur de son équipe (rgb) modifiée par son apparence
		// (luminosité et saturation ; la teinte reste celle de l'équipe). nullptr : couleur d'équipe.
		void armorColor(const AppearanceDef * appearance, const int team[3], int out[3]);
	}
}
