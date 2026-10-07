#pragma once

#include <atomic>
#include <cstdint>
#include <future>
#include <memory>

#include <BotBrain.h>

namespace tw
{
	// Décision de l'IA calculée dans un fil à part, sur des copies de l'état, de la carte et des données
	// de jeu : l'image continue pendant les calculs de l'ordinateur « Difficile » (jusqu'à plusieurs
	// centaines de millisecondes en Release). Un calcul devenu inutile (tour fini, écran fermé) est
	// interrompu. Utilisée par les combats locaux (entraînement, galerie des effets).
	class AsyncBotDecision
	{
	public:
		// Moment de la décision : combattant, tour et nombre d'actions déjà jouées pendant ce tour.
		struct Key
		{
			int fighter = -1;
			int turn = -1;
			int actions = -1;

			bool operator==(const Key & other) const { return fighter == other.fighter && turn == other.turn && actions == other.actions; }
		};

		AsyncBotDecision() = default;
		AsyncBotDecision(const AsyncBotDecision &) = delete;
		AsyncBotDecision & operator=(const AsyncBotDecision &) = delete;
		// Interrompt le calcul en cours et attend sa fin (rapide une fois interrompu).
		~AsyncBotDecision();

		// Un calcul est lancé (ou prêt) pour ce moment.
		bool requested(const Key & key) const;
		// Lance le calcul pour ce moment. Un calcul lancé pour un autre moment est d'abord interrompu ;
		// tant qu'il ne s'est pas arrêté, rien n'est lancé (il faut redemander à l'image suivante).
		void request(const Key & key, const battle::BattleState & state, const battle::BattleMap & map, const battle::GameData & data,
			std::uint32_t seed, const battle::BotOptions & options);
		// Décision prête pour ce moment : true et l'action ; false si le calcul n'est pas fini.
		bool take(const Key & key, battle::BotAction & action);
		// Interrompt le calcul en cours, sans attendre.
		void cancel();

	private:
		std::future<battle::BotAction> future;
		Key running;
		std::atomic<bool> cancelled{ false };
		// Copie des données de jeu, faite une fois : un message GD reçu ne la change pas pendant un calcul.
		std::shared_ptr<const battle::GameData> data;
	};
}
