#pragma once

#include <nlohmann/json.hpp>

#include "BattleState.h"

namespace tw
{
	namespace battle
	{
		// Copie de l'état du combat tenue par un client à partir des messages du serveur
		// (snapshot complet puis événements). Elle sert à prévisualiser les actions avec les
		// mêmes règles que le serveur (BattleRules) ; l'affichage, lui, est animé à part.
		class BattleMirror
		{
		public:
			// Remplace tout l'état. Les cellules de départ sont recopiées dans la carte.
			static void applySnapshot(BattleState & state, BattleMap & map, const nlohmann::json & snapshot);
			static void applyEvent(BattleState & state, const nlohmann::json & event);

			static ActiveEffect effectFromJson(const nlohmann::json & json);
			static Glyph glyphFromJson(const nlohmann::json & json);
			static Cell cellFromJson(const nlohmann::json & json);
		};
	}
}
