#pragma once

#include <cstdint>
#include <memory>
#include <random>
#include <set>

#include "BattleScreen.h"
#include <BattleEngine.h>
#include <BotBrain.h>

namespace tw
{
	// Combat sans serveur : un moteur de combat local alimente l'écran de combat (état complet BI,
	// puis lots d'événements BV) et traite les messages du joueur comme le ferait le serveur. Les
	// combattants confiés à l'IA jouent seuls. Base de la galerie des effets et de l'entraînement.
	class LocalBattleScreen : public BattleScreen
	{
	public:
		LocalBattleScreen(tgui::Gui * gui, int environmentId);

		// À appeler en dernier par les classes filles : l'écran peut se fermer pendant cet appel.
		virtual void update(float deltatime);

	protected:
		// Démarre (ou redémarre) le combat local, vu par le combattant "viewer".
		void startLocal(std::unique_ptr<battle::BattleEngine> newEngine, int viewer);
		// Transmet les événements du moteur à l'écran. Retourne true si le combat vient de finir.
		bool deliver();
		// Les animations des événements reçus sont terminées.
		bool idle() const;

		virtual void sendToServer(const std::string & op, const nlohmann::json & body);
		// Action de jeu du joueur (placement, déplacement, sort, fin de tour).
		virtual void onPlayerAction() {}
		// Fin du combat local.
		virtual void onLocalEnd() {}

		std::unique_ptr<battle::BattleEngine> engine;
		std::int64_t nowMs;
		// Fin de combat cachée : la galerie rejoue la scène au lieu d'afficher l'écran de fin.
		bool hideEnd;
		// Minuteurs du placement et des tours appliqués, comme sur le serveur.
		bool timers;
		// Combattants joués par l'IA, son niveau et le délai entre deux de ses actions.
		std::set<int> bots;
		battle::BotOptions botOptions;
		float botDelay;

	private:
		void playBots(float deltatime);

		std::mt19937 botRng;
		float botWait;
		int botTurn;
		int botActions;
	};
}
