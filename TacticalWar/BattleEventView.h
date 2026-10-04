#pragma once

#include <map>
#include <string>
#include <nlohmann/json.hpp>

#include <BaseCharacterModel.h>
#include <BattleState.h>

namespace tw
{
	class BattleScreen;

	// Animation à l'écran des événements du combat : déplacements, sorts, dégâts, effets, textes
	// flottants, journal et sons. L'événement est déjà appliqué à l'état affiché de l'écran de combat
	// (BattleMirror) quand il arrive ici. play() retourne le délai avant l'événement suivant, en
	// secondes ; en mode rapide (rattrapage), les animations sont sautées.
	class BattleEventView
	{
	public:
		explicit BattleEventView(BattleScreen & screen);

		float play(const nlohmann::json & event, bool fast);

	private:
		// Événement en cours : son type et le combattant concerné ("f"), dans l'état affiché.
		struct Context
		{
			const nlohmann::json & event;
			std::string type;
			int fighterId;
			BaseCharacterModel * view;
			const battle::Fighter * fighter;
			bool fast;
		};
		typedef float (BattleEventView::*Handler)(const Context &);

		// Écusson de bouclier au-dessus du combattant : valeur de l'état affiché.
		void syncShield(const Context & c);

		float onPlacement(const Context & c);
		float onPlace(const Context & c);
		float onFight(const Context & c);
		float onTurn(const Context & c);
		float onMove(const Context & c);
		float onCast(const Context & c);
		float onDamage(const Context & c);
		float onHeal(const Context & c);
		float onEffectAdded(const Context & c);
		float onEffectRemoved(const Context & c);
		float onStats(const Context & c);
		float onScore(const Context & c);
		float onCombo(const Context & c);
		float onSlide(const Context & c);
		float onSwap(const Context & c);
		float onGlyphAdded(const Context & c);
		float onGlyphRemoved(const Context & c);
		float onGlyphTriggered(const Context & c);
		float onBlockAdded(const Context & c);
		float onBlockHit(const Context & c);
		float onBlockRemoved(const Context & c);
		float onDeath(const Context & c);
		float onEmote(const Context & c);
		float onTimeout(const Context & c);
		float onConnection(const Context & c);
		float onEnd(const Context & c);

		BattleScreen & screen;
		std::map<std::string, Handler> handlers;
	};
}
