#pragma once

#include "BattleState.h"

namespace tw
{
	class Environment;

	namespace battle
	{
		// Carte du moteur construite depuis une carte du jeu (cases praticables, obstacles
		// qui bloquent la vue, cellules de départ des équipes). Utilisée par le serveur et le client.
		BattleMap battleMapFromEnvironment(tw::Environment * environment);
	}
}
