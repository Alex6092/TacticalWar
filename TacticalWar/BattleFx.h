#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <BattleState.h>
#include <GameData.h>
#include <SpellView.h>

// Effets visuels du combat : animations des sorts (lancement, projectile, impact), effets durables
// sur les combattants (poison, bouclier…), glyphes au sol et effets génériques (mort, collision).
// Ils sont décrits par le catalogue assets/spellsprites/effects.json et l'objet "visual" de chaque
// sort (assets/data/gamedata.json). L'écran de combat transmet les événements du serveur ; les
// vues obtenues sont dessinées par le rendu isométrique.
class BattleFx
{
public:
	static const char * CATALOG_PATH;
	// Geste du lanceur avant l'impact (ou avant son bond), en secondes.
	static const float WINDUP_SECONDS;
	// Vitesse des déplacements subis (poussée, attraction, bond), en cases par seconde.
	static const float SLIDE_CELLS_PER_SECOND;

	// Le lanceur bondit vers sa cible (Charge) : l'impact suit son arrivée.
	static bool dashes(const tw::battle::SpellDef & spell);

	BattleFx();

	// Les planches du catalogue ("sheet") sont relatives au dossier "assetRoot" (celui qui contient assets/).
	bool loadCatalog(const std::string & path = CATALOG_PATH, std::string * error = nullptr, const std::string & assetRoot = "./");

	// Position affichée d'un combattant, en coordonnées de case (interpolée pendant un déplacement).
	std::function<bool(int fighterId, sf::Vector2f & cell)> positionOf;
	// Joue un son (chemin du fichier).
	std::function<void(const std::string & path)> playSound;

	// Sort lancé : effet sur le lanceur, projectile (ou bond du lanceur), puis impact et son d'impact
	// à l'arrivée. Retourne le délai avant l'impact, en secondes. En mode rapide (rattrapage), rien
	// n'est joué.
	float castSpell(const tw::battle::BattleMap & map, const tw::battle::SpellDef & spell,
		int casterId, const tw::battle::Cell & casterCell, const tw::battle::Cell & target, bool fast);

	// Effets durables : un visuel en boucle par combattant et par sort, tant qu'un effet du sort reste.
	// Les états négatifs (marques de combinaison) ont leur propre visuel au sol, "combo_mark".
	void effectAdded(int fighterId, const tw::battle::ActiveEffect & effect);
	void effectRemoved(int effectUid);
	// Dégât ou soin périodique d'un sort.
	void periodic(int fighterId, const std::string & spellId);

	// Combattant hors combat : ses effets durables disparaissent.
	void fighterRemoved(int fighterId);

	void glyphAdded(int glyphUid, const std::string & spellId, const std::vector<tw::battle::Cell> & cells);
	void glyphTriggered(int glyphUid, int fighterId);
	void glyphRemoved(int glyphUid);

	// Effet générique de la section "events" du catalogue (mort, collision, poussée…).
	void playEvent(const std::string & name, int fighterId);
	// Effet du catalogue joué une fois à une position (coordonnées de case).
	void playEffect(const std::string & name, sf::Vector2f cell, float delay = 0);

	// Après un état complet : effets durables et glyphes recréés depuis l'état.
	void rebuild(const tw::battle::BattleState & state);
	void clear();

	void update(float deltatime);
	void collectViews(std::vector<tw::AbstractSpellView<sf::Sprite*>*> & views);

	bool hasEffect(const std::string & name) const { return effects.find(name) != effects.end(); }

private:
	struct EffectDef
	{
		std::string sheet;
		float fps = 24.f;
		bool loop = false;
		float scale = 1.f;
		float anchorX = 0.5f;
		float anchorY = 0.5f;
		float offsetY = 0;
		SpellView::Layer layer = SpellView::Layer::TOP;
		bool rotate = false;
		bool additive = false;
		sf::Color color = sf::Color::White;
		float duration = 0;		// Boucle jouée un temps limité (0 : jusqu'à son retrait)
	};

	struct Instance
	{
		std::unique_ptr<SpellView> view;
		const EffectDef * def = nullptr;
		float delay = 0;			// Attente avant de commencer
		bool limited = false;		// Boucle jouée un temps limité
		float remaining = 0;		// Temps restant de la boucle limitée
		int follow = -1;			// Combattant suivi
		std::string statusKey;		// Effet durable ("combattant:sort")
		int glyph = -1;				// Glyphe porteur
		// Projectile : trajet de "from" à "to" (cases), hauteur de l'arc en pixels.
		bool projectile = false;
		sf::Vector2f from;
		sf::Vector2f to;
		float flight = 0;
		float travelled = 0;
		float arc = 0;
	};

	struct Status
	{
		std::set<int> uids;
	};

	const EffectDef * find(const std::string & name) const;
	Instance * spawn(const std::string & name, sf::Vector2f cell, float delay);
	const tw::battle::SpellDef * spellById(const std::string & spellId) const;
	bool fighterCell(int fighterId, sf::Vector2f & cell) const;
	void removeWhere(const std::function<bool(const Instance &)> & predicate);

	std::map<std::string, EffectDef> effects;
	std::map<std::string, std::string> events;
	std::vector<Instance> instances;

	std::map<std::string, Status> statuses;		// "combattant:sort"
	std::map<int, std::string> statusOfUid;
	std::map<int, std::string> glyphSpells;

	struct PendingSound
	{
		float delay;
		std::string path;
	};
	std::vector<PendingSound> pendingSounds;
};
