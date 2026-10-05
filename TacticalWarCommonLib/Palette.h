#pragma once

namespace tw
{
	namespace palette
	{
		// Couleurs des personnages (multipliées par les zones de leur masque : armure, cheveux, peau).
		// L'armure porte la couleur de l'équipe ; une apparence (Appearances.h) en donne une variante.
		inline void teamArmor(int team, int out[3])
		{
			static const int colors[3][3] = { { 255, 255, 255 }, { 0, 166, 214 }, { 120, 17, 17 } };
			const int * color = colors[team == 1 || team == 2 ? team : 0];
			for (int i = 0; i < 3; i++)
				out[i] = color[i];
		}

		inline void defaultHair(int out[3])
		{
			out[0] = 108;
			out[1] = 70;
			out[2] = 35;
		}

		inline void skin(int out[3])
		{
			out[0] = 202;
			out[1] = 165;
			out[2] = 150;
		}
	}
}
