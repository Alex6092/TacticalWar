#pragma once

#include <cstdint>
#include <random>
#include <string>

#include <nlohmann/json.hpp>

#include "BattleState.h"

namespace tw
{
	namespace battle
	{
		// Commentateur automatique d'un combat : à partir de ses lots d'événements (BV) et de l'état
		// après chaque lot, il produit des phrases variées (tirées avec une graine : reproductibles).
		// Sujets : combinaison, KO et double KO, gros coup, équipe en danger, retournement, orbe, mur
		// posé ou détruit, point de zone, fin du combat. Au plus une phrase toutes les 3 secondes, la
		// plus importante d'abord (la fin du combat passe toujours).
		class Commentary
		{
		public:
			static const std::int64_t MIN_INTERVAL_MS = 3000;
			// Une phrase en attente (trop tôt pour la dire) est oubliée après ce délai.
			static const std::int64_t STALE_MS = 6000;
			// Gros coup : dégâts d'un seul coup (bouclier compris).
			static const int BIG_HIT = 25;

			explicit Commentary(std::uint32_t seed);

			// Noms des équipes (UTF-8), pour les phrases sur une équipe.
			void setTeamNames(const std::string & team1, const std::string & team2);

			// Lot d'événements (tableau "ev" d'un BV) et état après ce lot. Retourne la phrase à dire
			// maintenant (UTF-8), vide sinon.
			std::string onEvents(const nlohmann::json & events, const BattleState & after, std::int64_t nowMs);
			// Sans nouvel événement : phrase en attente, si le délai est passé.
			std::string poll(std::int64_t nowMs);

		private:
			struct Candidate
			{
				int priority = -1;
				std::string text;
				std::int64_t at = 0;
			};

			void offer(Candidate & best, int priority, const std::string & text, std::int64_t nowMs);
			std::string pick(std::initializer_list<const char *> variants);
			std::string fighterName(const BattleState & state, int id) const;
			std::string team(int team) const;
			std::string say(const Candidate & candidate, std::int64_t nowMs);

			std::mt19937 rng;
			std::string teamNames[3];
			std::int64_t lastSpoken;
			Candidate pending;
			// Équipe en danger (moins de 15 % de ses PV), annoncée une fois par équipe.
			bool dangerSaid[3] = { false, false, false };
			// Retournement : plus grand retard de chaque équipe (en points de % de PV), annoncé une fois.
			double worstDeficit[3] = { 0, 0, 0 };
			bool comebackSaid[3] = { false, false, false };
			// KO du tour en cours (double KO).
			int knockoutsThisTurn = 0;
		};
	}
}
