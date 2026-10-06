#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tw
{
	// Défis entre équipes (matchs amicaux libres, hors tournoi en cours). Une équipe n'a qu'un défi à
	// la fois, envoyé ou reçu ; le défi expire au bout de 30 s. Le premier joueur de l'équipe défiée
	// qui répond décide pour elle. Sans réseau ni horloge : l'appelant donne l'heure et l'état des
	// équipes (testable).
	class ChallengeBoard
	{
	public:
		static const std::int64_t TIMEOUT_MS = 30000;

		struct Challenge
		{
			int from = 0;
			int to = 0;
			std::int64_t expiresMs = 0;
		};

		// Envoie un défi. Retourne le motif du refus (texte UTF-8), vide si le défi est enregistré.
		// tournamentRunning : un tournoi est en cours ; fromFree / toFree : l'équipe n'a pas de match
		// prévu ou en cours.
		std::string challenge(int from, int to, bool tournamentRunning, bool fromFree, bool toFree, std::int64_t nowMs);

		// Réponse de l'équipe défiée "to" au défi de "from". false si ce défi n'existe plus (expiré,
		// annulé, ou un coéquipier a déjà répondu). Le défi est retiré dans tous les cas.
		bool answer(int to, int from, std::int64_t nowMs, Challenge * answered = nullptr);

		// Retire et retourne les défis expirés.
		std::vector<Challenge> expire(std::int64_t nowMs);

		// Défi reçu ou envoyé par une équipe (nullptr s'il n'y en a pas).
		const Challenge * receivedBy(int team) const;
		const Challenge * sentBy(int team) const;

		// Tous les défis d'une équipe (match lancé par ailleurs, tournoi qui commence…) : retirés.
		std::vector<Challenge> cancelInvolving(int team);
		// Tous les défis (tournoi qui commence).
		std::vector<Challenge> cancelAll();

		const std::vector<Challenge> & pending() const { return challenges; }

	private:
		bool involved(int team) const;

		std::vector<Challenge> challenges;
	};
}
