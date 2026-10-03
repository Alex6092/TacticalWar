#pragma once

#include <string>
#include <vector>

namespace tw
{
	namespace battle
	{
		struct BattleState;
		struct Fighter;

		// Hauts faits de fin de combat, décernés d'après le bilan de chaque combattant. Seul
		// l'identifiant circule (événement "end", résultats du tournoi) : il ne doit pas changer.
		// Chaque écran affiche le nom et la description correspondants.
		struct AchievementDef
		{
			const char * id;
			const char * name;
			const char * description;
		};

		const int ACHIEVEMENT_COUNT = 9;
		inline const AchievementDef ACHIEVEMENTS[ACHIEVEMENT_COUNT] = {
			{ "first_blood", u8"Premier sang", u8"Premier ennemi mis hors combat du combat." },
			{ "double_ko", u8"Coup double", u8"Au moins 2 ennemis mis hors combat." },
			{ "combo_master", u8"Maître des combos", u8"Au moins 2 combinaisons déclenchées." },
			{ "demolisher", u8"Démolisseur", u8"Au moins 150 dégâts infligés." },
			{ "guardian_angel", u8"Ange gardien", u8"Au moins 60 PV rendus ou protégés (soins et boucliers)." },
			{ "untouchable", u8"Intouchable", u8"Aucun dégât subi, et debout à la fin du combat." },
			{ "last_standing", u8"Dernier debout", u8"Seul survivant de l'équipe gagnante." },
			{ "zone_keeper", u8"Gardien de la zone", u8"Dans la zone pour au moins 3 points marqués." },
			{ "lightning", u8"Victoire éclair", u8"Victoire en 5 tours ou moins." }
		};

		const AchievementDef * findAchievement(const std::string & id);

		// Hauts faits du combattant (identifiants, dans l'ordre de ACHIEVEMENTS), combat terminé.
		std::vector<std::string> earnedAchievements(const BattleState & state, const Fighter & fighter);
	}
}
