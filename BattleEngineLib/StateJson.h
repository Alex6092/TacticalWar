#pragma once

#include <cstdint>

#include <nlohmann/json.hpp>

#include "BattleState.h"

namespace tw
{
	namespace battle
	{
		// Sérialisation de l'état d'un combat (format des messages BI et des événements). Utilisée
		// par le moteur (snapshot) et par le serveur pour un état reconstruit par le miroir
		// (temps forts d'une rediffusion, voir Highlights.h).
		namespace statejson
		{
			nlohmann::json cell(const Cell & cell);
			nlohmann::json effect(const ActiveEffect & effect);
			nlohmann::json glyph(const Glyph & glyph);
			nlohmann::json block(const Block & block);
			nlohmann::json record(const FighterRecord & record);
			nlohmann::json zone(const ZoneState & zone);
			nlohmann::json fighter(const Fighter & fighter);

			// État complet (BI) : seq = numéro du dernier lot d'événements inclus, viewer = combattant
			// du destinataire (-1 : spectateur), remainingMs = temps restant du tour.
			nlohmann::json snapshot(const BattleState & state, const BattleMap & map, std::uint64_t seq, int viewer, std::int64_t remainingMs);
		}
	}
}
