#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "BattleRules.h"
#include "BattleState.h"
#include "GameData.h"

namespace tw
{
	namespace battle
	{
		struct ActionResult
		{
			bool ok = true;
			std::string error;

			static ActionResult success() { return ActionResult(); }
			static ActionResult failure(const std::string & message) { ActionResult result; result.ok = false; result.error = message; return result; }
		};

		// Moteur de combat faisant autorité (exécuté par le serveur).
		// Chaque action valide produit des événements (valeurs absolues : PV, bouclier, PA, PM,
		// positions) que le serveur diffuse ; les clients se contentent de les animer.
		// Le temps est fourni par l'appelant (millisecondes, horloge monotone).
		class BattleEngine
		{
		public:
			// Jets des effets (dégâts, soins, boucliers…) : au hasard, ou toujours au minimum / au
			// maximum (aperçu d'un sort).
			enum class RollMode { RANDOM, MIN, MAX };

			BattleEngine(const GameData & data, const BattleMap & map, std::uint32_t seed);
			// Moteur repris d'un état existant : simule une action sans toucher au combat en cours.
			BattleEngine(const GameData & data, const BattleMap & map, const BattleState & state, std::uint32_t seed);

			void setRollMode(RollMode mode) { rollMode = mode; }

			// Ajoute un combattant avant le placement. Retourne son identifiant (ou -1).
			int addFighter(int team, int classId, const std::string & name);

			void startPlacement(std::int64_t nowMs);

			ActionResult place(int fighterId, const Cell & cell, std::int64_t nowMs);
			ActionResult setReady(int fighterId, bool ready, std::int64_t nowMs);
			// path : cellules à parcourir, dans l'ordre, sans la cellule de départ.
			ActionResult move(int fighterId, const std::vector<Cell> & path, std::int64_t nowMs);
			ActionResult cast(int fighterId, int spellIndex, const Cell & target, std::int64_t nowMs);
			ActionResult endTurn(int fighterId, std::int64_t nowMs);
			// Émote prédéfinie (Emotes.h), visible de tous et enregistrée dans les rediffusions. À tout
			// moment du combat, au plus une toutes les EMOTE_COOLDOWN_MS par combattant.
			ActionResult emote(int fighterId, int emoteId, std::int64_t nowMs);

			// Minuteurs (placement, tour). À appeler régulièrement.
			void tick(std::int64_t nowMs);

			void setConnected(int fighterId, bool connected, std::int64_t nowMs);
			// L'équipe "team" perd par forfait.
			void forfeit(int team, std::int64_t nowMs);
			// Arrêt par l'admin : décision aux points de vie restants.
			void stopByDecision(std::int64_t nowMs);
			// Arrêt par l'admin avec un vainqueur désigné.
			void declareWinner(int winnerTeam, std::int64_t nowMs);

			bool hasPendingEvents() const { return !pendingEvents.empty(); }
			// Événements produits depuis le dernier appel : {"seq": n, "ev": [...]}.
			nlohmann::json flushEvents();
			std::uint64_t getSeq() const { return seq; }

			// État complet pour un client (reconnexion, spectateur). viewerFighterId = -1 pour un spectateur.
			nlohmann::json snapshot(int viewerFighterId, std::int64_t nowMs) const;

			const BattleState & getState() const { return state; }
			const BattleMap & getMap() const { return map; }
			const GameData & getData() const { return data; }
			bool isOver() const { return state.phase == BattlePhase::ENDED; }
			// Pourcentage de PV restants d'une équipe (par rapport aux PV max de départ).
			double teamHpPercent(int team) const;
			std::uint32_t getSeed() const { return seed; }

		private:
			void startFight(std::int64_t nowMs);
			void computeTurnOrder();
			void beginTurn(std::int64_t nowMs);
			void finishTurn(std::int64_t nowMs);
			bool checkEnd(int actingFighterId);
			void endBattle(int winnerTeam, EndReason reason);

			// Effets (BattleEffects.cpp)
			void applySpellEffect(Fighter & caster, const SpellDef & spell, const EffectDef & effect, const Cell & target, const std::vector<int> & targetIds);
			void applyEffectToTarget(Fighter & caster, const std::string & spellId, const EffectDef & effect, Fighter & target, const Cell & targetCell);
			int computeDamage(const Fighter & caster, const Fighter & target, int roll) const;
			int dealDamage(Fighter & target, int amount, int sourceId, const std::string & kind);
			int heal(Fighter & target, int amount, int sourceId, const std::string & kind);
			void addActiveEffect(Fighter & target, ActiveEffect effect, bool refresh);
			void removeEffects(Fighter & target, const std::function<bool(const ActiveEffect &)> & predicate);
			void pushFighter(Fighter & caster, Fighter & target, int distance, bool towardsCaster);
			void moveFighterTo(Fighter & fighter, const Cell & cell, const std::string & kind);
			void tickEffectsAtTurnStart(Fighter & fighter);
			void triggerGlyphs(Fighter & fighter);
			void applyOnCastPassive(Fighter & caster);
			int roll(int min, int max);

			nlohmann::json fighterJson(const Fighter & fighter) const;
			nlohmann::json effectJson(const ActiveEffect & effect) const;
			nlohmann::json glyphJson(const Glyph & glyph) const;
			void emit(const nlohmann::json & event);
			void emitStats(const Fighter & fighter);

			const GameData & data;
			BattleMap map;
			BattleState state;
			std::mt19937 rng;
			RollMode rollMode = RollMode::RANDOM;
			std::uint32_t seed;
			std::uint64_t seq;
			nlohmann::json pendingEvents;
			std::map<int, std::int64_t> lastEmoteMs;
		};
	}
}
