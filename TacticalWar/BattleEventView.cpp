#include "BattleEventView.h"

#include <algorithm>

#include <BattleMirror.h>
#include <BattleRules.h>
#include <Emotes.h>

#include "BattleScreen.h"
#include "ClientGameData.h"
#include "LinkToServer.h"
#include "MusicManager.h"

using namespace tw;
using nlohmann::json;

namespace
{
	sf::String num(int value)
	{
		return sf::String(std::to_string(value));
	}

	std::vector<Point2D> toLegacyPath(const json & path)
	{
		// Les vues attendent la destination en premier et le premier pas en dernier.
		std::vector<Point2D> legacy;
		for (const json & cell : path)
			legacy.insert(legacy.begin(), Point2D(cell.at(0).get<int>(), cell.at(1).get<int>()));
		return legacy;
	}
}

BattleEventView::BattleEventView(BattleScreen & screen)
	: screen(screen)
{
	handlers = {
		{ "placement", &BattleEventView::onPlacement },
		{ "place", &BattleEventView::onPlace },
		{ "fight", &BattleEventView::onFight },
		{ "turn", &BattleEventView::onTurn },
		{ "move", &BattleEventView::onMove },
		{ "cast", &BattleEventView::onCast },
		{ "damage", &BattleEventView::onDamage },
		{ "heal", &BattleEventView::onHeal },
		{ "effect+", &BattleEventView::onEffectAdded },
		{ "effect-", &BattleEventView::onEffectRemoved },
		{ "stats", &BattleEventView::onStats },
		{ "score", &BattleEventView::onScore },
		{ "combo", &BattleEventView::onCombo },
		{ "slide", &BattleEventView::onSlide },
		{ "swap", &BattleEventView::onSwap },
		{ "glyph+", &BattleEventView::onGlyphAdded },
		{ "glyph-", &BattleEventView::onGlyphRemoved },
		{ "glyph", &BattleEventView::onGlyphTriggered },
		{ "death", &BattleEventView::onDeath },
		{ "emote", &BattleEventView::onEmote },
		{ "timeout", &BattleEventView::onTimeout },
		{ "connection", &BattleEventView::onConnection },
		{ "end", &BattleEventView::onEnd },
	};
}

float BattleEventView::play(const json & event, bool fast)
{
	int fighterId = event.value("f", -1);
	Context context = { event, event.value("t", std::string()), fighterId, screen.viewOf(fighterId), screen.shown.findFighter(fighterId), fast };
	auto handler = handlers.find(context.type);
	return handler != handlers.end() ? (this->*(handler->second))(context) : 0.f;
}

void BattleEventView::syncShield(const Context & c)
{
	if (c.view != NULL && c.fighter != NULL)
		c.view->setCurrentShield(c.fighter->alive ? c.fighter->shield : 0);
}

//----------------------------------------------------------
// Placement et tours
//----------------------------------------------------------

float BattleEventView::onPlacement(const Context &)
{
	screen.colorator->setStartCells(screen.map.startCells[1], screen.map.startCells[2]);
	return 0;
}

float BattleEventView::onPlace(const Context & c)
{
	if (c.view == NULL)
		return 0;
	c.view->setCurrentX(c.event["x"].get<int>());
	c.view->setCurrentY(c.event["y"].get<int>());
	return 0;
}

float BattleEventView::onFight(const Context & c)
{
	screen.colorator->setStartCells({}, {});
	screen.hud->showMessage(L"Le combat commence !", sf::Color(255, 220, 80), 1.5f);
	screen.hud->log(L"Le combat commence.", sf::Color(255, 220, 80));
	return c.fast ? 0 : 0.8f;
}

float BattleEventView::onTurn(const Context & c)
{
	if (c.fighter == NULL)
		return 0;
	bool mine = c.fighterId == screen.you;
	sf::String text = mine ? sf::String(L"À vous de jouer !") : L"Tour de " + fromServerText(c.fighter->name);
	screen.hud->showMessage(text, mine ? sf::Color(120, 255, 120) : sf::Color(255, 220, 80), 1.2f);
	screen.hud->log(L"--- Tour " + num(screen.shown.round) + L" : " + fromServerText(c.fighter->name), sf::Color(255, 220, 80));
	screen.selectSpell(-1);
	return c.fast ? 0 : 0.4f;
}

float BattleEventView::onTimeout(const Context & c)
{
	screen.hud->log(L"Temps écoulé pour " + screen.fighterName(c.fighterId), sf::Color(200, 200, 200));
	return 0;
}

float BattleEventView::onConnection(const Context & c)
{
	bool connected = c.event.value("connected", true);
	screen.hud->log(screen.fighterName(c.fighterId) + (connected ? L" est revenu." : L" s'est déconnecté."), sf::Color(200, 200, 200));
	return 0;
}

float BattleEventView::onEnd(const Context &)
{
	screen.showEnd();
	return 0;
}

//----------------------------------------------------------
// Déplacements
//----------------------------------------------------------

float BattleEventView::onMove(const Context & c)
{
	if (c.view == NULL)
		return 0;

	for (const json & tackle : c.event.value("tackles", json::array()))
	{
		screen.hud->log(screen.fighterName(c.fighterId) + L" est taclé : -" + num(tackle.value("mp", 0)) + L" PM, -" + num(tackle.value("ap", 0)) + L" PA",
			sf::Color(255, 170, 90));
		screen.addFloatingText(c.fighterId, L"Taclé !", sf::Color(255, 170, 90));
	}

	const json & path = c.event["path"];
	if (!path.empty())
	{
		if (c.fast)
		{
			c.view->setCurrentX(path.back().at(0).get<int>());
			c.view->setCurrentY(path.back().at(1).get<int>());
		}
		else
		{
			// Le déplacement interrompt une animation d'action : le personnage court.
			screen.actionAnimations.erase(c.fighterId);
			c.view->resetAnimation();
			c.view->setPath(toLegacyPath(path), &screen);
			screen.waitingMove = true;
		}
	}
	c.view->setCurrentPA(c.event.value("ap", 0));
	c.view->setCurrentPM(c.event.value("mp", 0));
	return 0;
}

float BattleEventView::onSlide(const Context & c)
{
	if (c.view == NULL)
		return 0;

	int x = c.event["x"].get<int>();
	int y = c.event["y"].get<int>();
	std::string kind = c.event.value("kind", std::string());
	if (c.fast || kind == "teleport")
	{
		c.view->setCurrentX(x);
		c.view->setCurrentY(y);
	}
	else
	{
		// Poussée, attraction ou bond : le personnage glisse case par case jusqu'à l'arrivée.
		std::vector<Point2D> path;
		int cx = c.view->getCurrentX();
		int cy = c.view->getCurrentY();
		while (cx != x || cy != y)
		{
			cx += cx < x ? 1 : cx > x ? -1 : 0;
			cy += cy < y ? 1 : cy > y ? -1 : 0;
			path.insert(path.begin(), Point2D(cx, cy));
		}
		if (!path.empty())
		{
			c.view->slide(path, BattleFx::SLIDE_CELLS_PER_SECOND, &screen);
			screen.waitingMove = true;
		}
	}
	if (kind == "push" || kind == "pull")
		screen.addFloatingText(c.fighterId, kind == "push" ? L"Repoussé" : L"Attiré", sf::Color(230, 230, 230));
	if (!c.fast)
		screen.fx.playEvent(kind, c.fighterId);
	return c.fast ? 0 : 0.25f;
}

float BattleEventView::onSwap(const Context & c)
{
	if (c.view == NULL)
		return 0;
	c.view->setCurrentX(c.event["x"].get<int>());
	c.view->setCurrentY(c.event["y"].get<int>());
	BaseCharacterModel * other = screen.viewOf(c.event.value("other", -1));
	if (other != NULL)
	{
		other->setCurrentX(c.event["ox"].get<int>());
		other->setCurrentY(c.event["oy"].get<int>());
	}
	return c.fast ? 0 : 0.3f;
}

//----------------------------------------------------------
// Sorts, dégâts et soins
//----------------------------------------------------------

float BattleEventView::onCast(const Context & c)
{
	if (c.view == NULL || c.fighter == NULL)
		return 0;

	int x = c.event["x"].get<int>();
	int y = c.event["y"].get<int>();
	const battle::SpellDef * spell = battle::spellOf(ClientGameData::get().data(), *c.fighter, c.event.value("slot", -1));
	c.view->setOrientationToLookAt(x, y);

	if (spell == NULL)
		return c.fast ? 0.05f : 0.7f;

	if (spell->casterAnimation == "physical")
		screen.startActionAnimation(c.fighterId, c.view, tw::Animation::ATTACK2);
	else
		screen.startActionAnimation(c.fighterId, c.view, tw::Animation::ATTACK1);

	// Les dégâts et soins (événements suivants) s'affichent à l'impact.
	float impact = screen.fx.castSpell(screen.map, *spell, c.fighterId, c.fighter->position, { x, y }, c.fast);
	screen.playSound(spell->sound);
	screen.hud->log(screen.fighterName(c.fighterId) + L" lance " + fromServerText(spell->name), sf::Color(150, 200, 255));
	// Bond : l'événement suivant est le déplacement du lanceur, qui part après son geste.
	if (BattleFx::dashes(*spell))
		return c.fast ? 0.05f : BattleFx::WINDUP_SECONDS;
	return c.fast ? 0.05f : std::max(0.4f, impact + 0.1f);
}

float BattleEventView::onDamage(const Context & c)
{
	if (c.view == NULL || c.fighter == NULL)
		return 0;

	int amount = c.event.value("amount", 0);
	int absorbed = c.event.value("absorbed", 0);
	std::string kind = c.event.value("kind", std::string());

	c.view->setDisplayMaxLife(c.fighter->maxHp);
	// Un mort reste affiché le temps de son animation.
	c.view->setCurrentLife(std::max(c.fighter->hp, c.fighter->alive ? 0 : 1));
	syncShield(c);
	if (amount <= 0)
		return 0;
	if (c.fighter->alive)
		screen.startActionAnimation(c.fighterId, c.view, tw::Animation::TAKE_DAMAGE);
	if (!c.fast && kind == "dot")
		screen.fx.periodic(c.fighterId, screen.periodicSpell(*c.fighter, c.event.value("src", -1), battle::EffectType::DOT));
	else if (!c.fast && kind == "collision")
		screen.fx.playEvent("collision", c.fighterId);

	// Le bouclier absorbe en premier : sa part en bleu, puis les PV perdus en rouge.
	int lost = amount - absorbed;
	// Case à effet (braises) : son nom accompagne les dégâts.
	sf::String terrain = kind == "terrain" ? screen.terrainName(c.fighter->position) : sf::String();
	sf::String source = kind == "dot" ? L" (effet)" : kind == "collision" ? L" (collision)" : kind == "sudden" ? L" (mort subite)"
		: kind == "terrain" ? L" (" + terrain + L")" : L"";
	if (absorbed > 0)
	{
		screen.addFloatingText(c.fighterId, L"Bouclier -" + num(absorbed), sf::Color(120, 185, 255));
		screen.hud->log(screen.fighterName(c.fighterId) + L" : le bouclier absorbe " + num(absorbed) + L" dégâts" + source
			+ (c.fighter->shield > 0 ? L" (reste " + num(c.fighter->shield) + L")" : sf::String(L" (bouclier brisé)")), sf::Color(150, 200, 255));
	}
	if (lost > 0)
	{
		screen.addFloatingText(c.fighterId, (terrain.isEmpty() ? sf::String() : terrain + L" ") + L"-" + num(lost), sf::Color(255, 80, 70));
		screen.hud->log(screen.fighterName(c.fighterId) + L" perd " + num(lost) + L" PV" + source, sf::Color(255, 130, 120));
	}
	MusicManager::getInstance()->playTakeDamageSound();
	return c.fast ? 0 : 0.35f;
}

float BattleEventView::onHeal(const Context & c)
{
	if (c.view == NULL || c.fighter == NULL)
		return 0;

	c.view->setCurrentLife(c.fighter->hp);
	int amount = c.event.value("amount", 0);
	if (amount <= 0)
		return 0;
	std::string kind = c.event.value("kind", std::string());
	if (!c.fast && kind == "hot")
		screen.fx.periodic(c.fighterId, screen.periodicSpell(*c.fighter, c.event.value("src", -1), battle::EffectType::HOT));
	else if (!c.fast && kind == "lifesteal")
		screen.fx.playEvent("lifesteal", c.fighterId);
	// Case à effet (source) : son nom accompagne les soins.
	sf::String terrain = kind == "terrain" ? screen.terrainName(c.fighter->position) : sf::String();
	screen.addFloatingText(c.fighterId, (terrain.isEmpty() ? sf::String() : terrain + L" ") + L"+" + num(amount), sf::Color(110, 255, 110));
	screen.hud->log(screen.fighterName(c.fighterId) + L" récupère " + num(amount) + L" PV" + (terrain.isEmpty() ? sf::String() : L" (" + terrain + L")"),
		sf::Color(130, 255, 130));
	return c.fast ? 0 : 0.3f;
}

float BattleEventView::onCombo(const Context & c)
{
	if (c.fighter == NULL)
		return 0;

	// Combinaison : annoncée avant les dégâts augmentés (événement suivant).
	sf::String name = fromServerText(c.event.value("name", std::string()));
	int percent = c.event.value("percent", 0);
	if (!c.fast)
	{
		screen.fx.playEvent("combo", c.fighterId);
		screen.playSound("./assets/sound/ui/combo.ogg");
	}
	screen.addFloatingText(c.fighterId, L"Combo " + name + L" !", sf::Color(255, 205, 70));
	screen.hud->log(L"Combo " + name + L" : " + screen.fighterName(c.event.value("src", -1)) + L" inflige +" + num(percent) + L" % à "
		+ screen.fighterName(c.fighterId), sf::Color(255, 205, 70));
	return c.fast ? 0 : 0.35f;
}

float BattleEventView::onDeath(const Context & c)
{
	if (c.view == NULL)
		return 0;
	screen.actionAnimations.erase(c.fighterId);
	c.view->startDieAction(BattleScreen::ACTION_ANIMATION_SECONDS);
	c.view->setCurrentShield(0);
	screen.fx.fighterRemoved(c.fighterId);
	if (!c.fast)
		screen.fx.playEvent("death", c.fighterId);
	screen.pendingDeaths[c.fighterId] = c.fast ? 0.f : 0.9f;
	screen.hud->log(screen.fighterName(c.fighterId) + L" est hors combat !", sf::Color(255, 90, 90));
	return c.fast ? 0 : 0.9f;
}

//----------------------------------------------------------
// Effets durables, caractéristiques et glyphes
//----------------------------------------------------------

float BattleEventView::onEffectAdded(const Context & c)
{
	if (c.fighter == NULL)
		return 0;

	battle::ActiveEffect effect = battle::BattleMirror::effectFromJson(c.event["effect"]);
	screen.fx.effectAdded(c.fighterId, effect);
	syncShield(c);
	if (effect.type == battle::EffectType::SHIELD)
	{
		// Bouclier reçu : sa valeur, et le total protégé.
		screen.addFloatingText(c.fighterId, L"Bouclier +" + num(effect.value), sf::Color(120, 185, 255));
		screen.hud->log(screen.fighterName(c.fighterId) + L" : " + fromServerText(effect.name) + L", bouclier +" + num(effect.value)
			+ L" (total " + num(c.fighter->shield) + L")", sf::Color(150, 200, 255));
	}
	else if (effect.spellId != "__passive")
	{
		// Marque de combinaison (état négatif) en doré, comme le réticule au sol.
		bool mark = effect.type == battle::EffectType::STATE && !effect.positive;
		sf::Color color = mark ? sf::Color(255, 205, 70) : effect.positive ? sf::Color(120, 200, 255) : sf::Color(255, 170, 90);
		screen.addFloatingText(c.fighterId, fromServerText(effect.name), color);
		screen.hud->log(screen.fighterName(c.fighterId) + L" : " + fromServerText(effect.name) + (mark ? L" (combo possible)" : L""), color);
	}
	return c.fast ? 0 : 0.2f;
}

float BattleEventView::onEffectRemoved(const Context & c)
{
	screen.fx.effectRemoved(c.event.value("uid", -1));
	syncShield(c);
	return 0;
}

float BattleEventView::onStats(const Context & c)
{
	if (c.view == NULL || c.fighter == NULL)
		return 0;
	c.view->setCurrentPA(c.fighter->ap);
	c.view->setCurrentPM(c.fighter->mp);
	c.view->setDisplayMaxLife(c.fighter->maxHp);
	if (c.fighter->alive)
		c.view->setCurrentLife(c.fighter->hp);
	syncShield(c);
	return 0;
}

float BattleEventView::onGlyphAdded(const Context & c)
{
	const json & glyph = c.event["glyph"];
	std::vector<battle::Cell> cells;
	for (const json & cell : glyph.value("cells", json::array()))
		cells.push_back({ cell.at(0).get<int>(), cell.at(1).get<int>() });
	screen.fx.glyphAdded(glyph.value("uid", -1), glyph.value("spell", std::string()), cells);
	screen.hud->log(L"Un glyphe est posé : " + fromServerText(glyph.value("name", std::string())), sf::Color(200, 150, 255));
	return c.fast ? 0 : 0.2f;
}

float BattleEventView::onGlyphRemoved(const Context & c)
{
	screen.fx.glyphRemoved(c.event.value("uid", -1));
	return 0;
}

float BattleEventView::onGlyphTriggered(const Context & c)
{
	if (!c.fast)
		screen.fx.glyphTriggered(c.event.value("uid", -1), c.fighterId);
	screen.hud->log(screen.fighterName(c.fighterId) + L" déclenche un glyphe", sf::Color(200, 150, 255));
	return c.fast ? 0 : 0.2f;
}

//----------------------------------------------------------
// Zone à tenir et communication
//----------------------------------------------------------

float BattleEventView::onScore(const Context & c)
{
	// Zone à tenir : point marqué à la fin d'un tour complet.
	int holder = c.event.value("holder", 0);
	sf::String score = num(screen.shown.zone.scores[1]) + L" - " + num(screen.shown.zone.scores[2]);
	const battle::Fighter * me = screen.shown.findFighter(screen.you);
	if (holder != 0)
	{
		sf::Color color = me == NULL ? sf::Color(255, 210, 80) : me->team == holder ? sf::Color(120, 255, 120) : sf::Color(255, 150, 90);
		sf::String message = me == NULL ? screen.teamLabel(holder) + L" marque un point !"
			: me->team == holder ? sf::String(L"Votre équipe marque un point !") : sf::String(L"L'adversaire marque un point !");
		screen.hud->showMessage(message, color, 1.4f);
		screen.hud->log(L"Zone : " + screen.teamLabel(holder) + L" tient la zone (+1), " + score, color);
		if (!c.fast)
			screen.playSound("./assets/sound/ui/ping.ogg");
		return c.fast ? 0 : 0.6f;
	}
	if (c.event.value("contested", false))
		screen.hud->log(L"Zone disputée : personne ne marque (" + score + L")", sf::Color(200, 200, 200));
	return 0;
}

float BattleEventView::onEmote(const Context & c)
{
	int id = c.event.value("id", -1);
	if (id < 0 || id >= battle::EMOTE_COUNT)
		return 0;

	sf::String text = fromServerText(battle::EMOTE_TEXTS[id]);
	screen.hud->log(screen.fighterName(c.fighterId) + L" : " + text, sf::Color(200, 220, 255));
	if (!c.fast)
	{
		int fighterId = c.fighterId;
		std::vector<BattleScreen::SpeechBubble> & bubbles = screen.bubbles;
		bubbles.erase(std::remove_if(bubbles.begin(), bubbles.end(),
			[fighterId](const BattleScreen::SpeechBubble & bubble) { return bubble.fighterId == fighterId; }), bubbles.end());
		BattleScreen::SpeechBubble bubble;
		bubble.fighterId = fighterId;
		bubble.text = text;
		bubbles.push_back(bubble);
		screen.playSound("./assets/sound/ui/emote.ogg");
	}
	return 0;
}
