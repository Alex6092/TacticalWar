#pragma once

#include <cstdint>

namespace tw
{
	namespace battle
	{
		// Émotes prédéfinies : jamais de texte libre (public scolaire). Seul l'identifiant circule ;
		// chaque écran affiche le texte correspondant. L'ordre ne doit pas changer (rediffusions).
		const int EMOTE_COUNT = 6;
		const char * const EMOTE_TEXTS[EMOTE_COUNT] = {
			u8"Bien joué !", u8"Merci !", u8"GG", u8"Oups !", u8"Attention !", u8"À l'attaque !"
		};

		// Délai minimal entre deux émotes d'un même combattant.
		const std::int64_t EMOTE_COOLDOWN_MS = 3000;
	}
}
