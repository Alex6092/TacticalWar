#pragma once

namespace tw
{
	namespace palette
	{
		// Couleurs du jeu, en deux jeux : standard, et daltonien (Okabe-Ito : équipes bleue et orange,
		// déplacement en vert bleuté face à la menace orange, impact vermillon face au bleu ciblable).
		// Le jeu actif est choisi par le mode daltonien des options du client.
		struct Rgba
		{
			int r;
			int g;
			int b;
			int a;
		};

		enum class Role
		{
			TEAM1_ARMOR,		// Armure des personnages (multipliée par leur masque)
			TEAM2_ARMOR,
			TEAM1_PANEL,		// Fond des cartes de la frise et du bandeau
			TEAM2_PANEL,
			TEAM1_TEXT,			// Texte clair (bilan de fin, journal)
			TEAM2_TEXT,
			TEAM1_START,		// Cases de départ (teinte de la tuile)
			TEAM2_START,
			REACHABLE,			// Cases de déplacement (teinte)
			PATH,
			PATH_TRUNCATED,
			HOVER_VALID,		// Visée (calques par-dessus la tuile)
			HOVER_INVALID,
			IMPACT,
			CASTABLE,
			RANGE,
			THREAT_ENEMY,		// Déplacement possible d'un combattant survolé au prochain tour
			THREAT_ALLY,
			ZONE,
			GLYPH_ALLY,
			GLYPH_ENEMY,
			DAMAGE_TEXT,		// Textes flottants
			HEAL_TEXT,
			COUNT
		};

		void setColorblind(bool enabled);
		bool colorblind();

		Rgba color(Role role);
		Rgba color(Role role, bool colorblind);
		// Rôle d'équipe (TEAM1_x ou TEAM2_x) pour l'équipe donnée.
		Role teamRole(int team, Role team1Role);

		// Noms en texte (UTF-8), pour les phrases qui citent une couleur : « orange », « turquoise »…
		const char * name(Role role);
		// Les joueurs d'une équipe, au pluriel (« bleus », « rouges » ou « orange »).
		const char * teamPlayers(int team);

		// Couleurs des personnages (multipliées par les zones de leur masque : armure, cheveux, peau).
		// L'armure porte la couleur de l'équipe ; une apparence (Appearances.h) en donne une variante.
		void teamArmor(int team, int out[3]);
		void defaultHair(int out[3]);
		void skin(int out[3]);
	}
}
