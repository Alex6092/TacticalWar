#pragma once

#include <cmath>

#include "ClientConfig.h"

namespace tw
{
	namespace ui
	{
		// Taille du texte choisie dans les options (100, 115 ou 130 %), appliquée aux surfaces de lecture :
		// journal, ligne d'aide, détails du combattant, aide, descriptions des sorts, bandeau des personnages.
		inline float scale()
		{
			int percent = ClientConfig::get().textScale;
			return (percent < 100 || percent > 150 ? 100 : percent) / 100.f;
		}

		inline unsigned int text(unsigned int size)
		{
			return (unsigned int)std::lround(size * scale());
		}
	}
}
