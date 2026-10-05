#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace tw
{
	namespace battle
	{
		// Temps fort d'un combat enregistré : un moment noté, et l'extrait à rejouer autour de lui
		// (indices des lots d'événements de la rediffusion, bornes comprises).
		struct Highlight
		{
			std::string kind;		// combo, ko, double_ko, big_hit, comeback, last_standing
			std::string title;		// UTF-8, ex. « Double KO de Léa »
			int score = 0;
			std::int64_t atMs = 0;	// Moment (ms depuis le début de l'enregistrement)
			std::size_t from = 0;
			std::size_t to = 0;
		};

		// Fenêtre de l'extrait autour du moment.
		const std::int64_t HIGHLIGHT_BEFORE_MS = 3000;
		const std::int64_t HIGHLIGHT_AFTER_MS = 4000;

		// Détecte les temps forts d'un combat : état de départ (snapshot BI de l'en-tête de la
		// rediffusion), lots (ms depuis le début, lot BV) et noms des deux équipes. Moments notés :
		// combinaison, KO et double KO (même tour), gros coup, retournement (vainqueur mené de plus de
		// 40 points de % de PV), dernier debout. Retourne au plus "max" temps forts sans
		// chevauchement, les mieux notés, dans l'ordre du combat.
		std::vector<Highlight> detectHighlights(const nlohmann::json & startSnapshot,
			const std::vector<std::pair<std::int64_t, nlohmann::json>> & batches, const std::string & team1, const std::string & team2,
			std::size_t max = 3);

		nlohmann::json highlightJson(const Highlight & highlight);
	}
}
