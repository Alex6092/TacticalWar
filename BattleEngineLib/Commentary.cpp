#include "Commentary.h"

#include <algorithm>
#include <utility>
#include <vector>

using namespace tw::battle;

namespace
{
	// Priorités : la phrase la plus importante passe d'abord.
	const int END = 100;
	const int DOUBLE_KO = 90;
	const int KO = 80;
	const int COMEBACK = 70;
	const int COMBO = 60;
	const int BIG_HIT_PRIORITY = 50;
	const int DANGER = 45;
	const int WALL_DOWN = 35;
	const int ZONE_POINT = 30;
	const int ORB = 25;
	const int WALL_UP = 20;
	const int SHRINK = 32;

	std::string fill(std::string text, const std::vector<std::pair<std::string, std::string>> & values)
	{
		for (const auto & value : values)
		{
			std::size_t at;
			while ((at = text.find(value.first)) != std::string::npos)
				text.replace(at, value.first.size(), value.second);
		}
		return text;
	}

	double teamHpPercent(const BattleState & state, int team)
	{
		int hp = 0;
		int maxHp = 0;
		for (const Fighter & fighter : state.fighters)
		{
			if (fighter.team != team)
				continue;
			hp += fighter.alive ? std::max(0, fighter.hp) : 0;
			maxHp += std::max(1, fighter.maxHp);
		}
		return maxHp > 0 ? 100.0 * hp / maxHp : 0.0;
	}

	// Orbes de gamedata.json ("bonuses") : soin, energie, protection.
	std::string orbLabel(const std::string & kind)
	{
		if (kind == "soin")
			return u8"de soin";
		if (kind == "energie")
			return u8"d'énergie";
		if (kind == "protection")
			return u8"de protection";
		return u8"« " + kind + u8" »";
	}
}

Commentary::Commentary(std::uint32_t seed)
	: rng(seed), lastSpoken(-MIN_INTERVAL_MS)
{
	teamNames[1] = u8"l'équipe 1";
	teamNames[2] = u8"l'équipe 2";
}

void Commentary::setTeamNames(const std::string & team1, const std::string & team2)
{
	if (!team1.empty())
		teamNames[1] = team1;
	if (!team2.empty())
		teamNames[2] = team2;
}

std::string Commentary::pick(std::initializer_list<const char *> variants)
{
	std::uniform_int_distribution<int> choice(0, (int)variants.size() - 1);
	return *(variants.begin() + choice(rng));
}

std::string Commentary::fighterName(const BattleState & state, int id) const
{
	const Fighter * fighter = state.findFighter(id);
	return fighter != nullptr ? fighter->name : std::string("?");
}

std::string Commentary::team(int team) const
{
	return teamNames[team == 2 ? 2 : 1];
}

void Commentary::offer(Candidate & best, int priority, const std::string & text, std::int64_t nowMs)
{
	if (priority > best.priority)
	{
		best.priority = priority;
		best.text = text;
		best.at = nowMs;
	}
}

std::string Commentary::say(const Candidate & candidate, std::int64_t nowMs)
{
	// Copie d'abord : "candidate" peut être la phrase en attente, effacée ici.
	std::string text = candidate.text;
	lastSpoken = nowMs;
	pending = Candidate();
	return text;
}

std::string Commentary::onEvents(const nlohmann::json & events, const BattleState & after, std::int64_t nowMs)
{
	Candidate best;
	if (pending.priority >= 0 && nowMs - pending.at < STALE_MS)
		best = pending;

	int lastAttacker = -1;
	for (const nlohmann::json & event : events)
	{
		std::string type = event.value("t", std::string());
		if (type == "turn")
		{
			knockoutsThisTurn = 0;
		}
		else if (type == "damage")
		{
			int amount = event.value("amount", 0) + event.value("absorbed", 0);
			int source = event.value("src", -1);
			if (source >= 0)
				lastAttacker = source;
			if (amount >= BIG_HIT && source >= 0)
			{
				std::string text = fill(pick({
					u8"Énorme coup de {a} : {n} dégâts sur {b} !",
					u8"{n} dégâts d'un coup ! {b} encaisse la frappe de {a}.",
					u8"Quelle puissance : {a} inflige {n} dégâts à {b} !" }),
					{ { "{a}", fighterName(after, source) }, { "{b}", fighterName(after, event.value("f", -1)) }, { "{n}", std::to_string(amount) } });
				offer(best, BIG_HIT_PRIORITY, text, nowMs);
			}
		}
		else if (type == "death")
		{
			knockoutsThisTurn++;
			std::string victim = fighterName(after, event.value("f", -1));
			std::string killer = lastAttacker >= 0 ? fighterName(after, lastAttacker) : std::string();
			if (knockoutsThisTurn >= 2)
			{
				std::string text = killer.empty()
					? fill(pick({ u8"Double KO ! {b} tombe à son tour.", u8"Deux combattants à terre dans le même tour !" }), { { "{b}", victim } })
					: fill(pick({ u8"Double KO pour {a} ! {b} rejoint le banc.", u8"{a} enchaîne : deuxième KO du tour, {b} est à terre !" }),
						{ { "{a}", killer }, { "{b}", victim } });
				offer(best, DOUBLE_KO, text, nowMs);
			}
			else
			{
				std::string text = killer.empty() || killer == victim
					? fill(pick({ u8"{b} est à terre !", u8"Et {b} quitte le combat !" }), { { "{b}", victim } })
					: fill(pick({ u8"{a} met {b} au tapis !", u8"KO ! {b} tombe sous les coups de {a}.", u8"{a} élimine {b} !" }),
						{ { "{a}", killer }, { "{b}", victim } });
				offer(best, KO, text, nowMs);
			}
		}
		else if (type == "combo")
		{
			std::string text = fill(pick({
				u8"Combinaison {c} ! {a} profite de la marque sur {b}.",
				u8"{a} déclenche {c} sur {b} !",
				u8"Superbe travail d'équipe : {c} sur {b} !",
				u8"La marque paie : {c} de {a} !",
				u8"{b} subit {c}, bien joué {a} !" }),
				{ { "{a}", fighterName(after, event.value("src", -1)) }, { "{b}", fighterName(after, event.value("f", -1)) },
					{ "{c}", event.value("name", std::string()) } });
			offer(best, COMBO, text, nowMs);
		}
		else if (type == "orb-" && event.value("reason", std::string()) != "shrink")
		{
			std::string text = fill(pick({ u8"{a} ramasse l'orbe {k}.", u8"Orbe {k} pour {a} !" }),
				{ { "{a}", fighterName(after, event.value("f", -1)) }, { "{k}", orbLabel(event.value("kind", std::string())) } });
			offer(best, ORB, text, nowMs);
		}
		else if (type == "block+")
		{
			const nlohmann::json & blocks = event.value("blocks", nlohmann::json::array());
			std::string name = blocks.empty() ? std::string(u8"Un mur") : blocks[0].value("name", std::string(u8"Un mur"));
			std::string text = fill(pick({ u8"{a} dresse {w} !", u8"{w} sort de terre : {a} coupe le terrain." }),
				{ { "{a}", fighterName(after, event.value("f", -1)) }, { "{w}", name } });
			offer(best, WALL_UP, text, nowMs);
		}
		else if (type == "block-" && event.value("reason", std::string()) == "destroyed")
		{
			std::string text = fill(pick({ u8"{w} cède : le passage est ouvert !", u8"Brèche ! {w} vole en éclats." }),
				{ { "{w}", event.value("name", std::string(u8"Le mur")) } });
			offer(best, WALL_DOWN, text, nowMs);
		}
		else if (type == "shrink")
		{
			offer(best, SHRINK, pick({ u8"La carte rétrécit : tout le monde se resserre !", u8"Le terrain se referme, plus de place pour fuir !" }), nowMs);
		}
		else if (type == "score")
		{
			int holder = event.value("holder", 0);
			const nlohmann::json & scores = event.value("scores", nlohmann::json::array());
			if ((holder == 1 || holder == 2) && scores.size() == 2)
			{
				std::string score = std::to_string(scores[0].get<int>()) + "-" + std::to_string(scores[1].get<int>());
				std::string text = fill(pick({ u8"Point de zone pour {t} ({s}).", u8"{t} tient la zone : {s}." }),
					{ { "{t}", team(holder) }, { "{s}", score } });
				offer(best, ZONE_POINT, text, nowMs);
			}
		}
		else if (type == "end")
		{
			int winner = event.value("winner", 0);
			std::string text = (winner == 1 || winner == 2) && event.value("reason", std::string()) == "SURRENDER"
				? fill(pick({ u8"{l} abandonne : victoire pour {t} !", u8"Drapeau blanc pour {l} : {t} remporte le combat." }),
					{ { "{t}", team(winner) }, { "{l}", team(3 - winner) } })
				: winner == 1 || winner == 2
				? fill(pick({ u8"Victoire pour {t} au tour {r} !", u8"C'est fini : {t} remporte le combat !", u8"{t} l'emporte ! Quel combat." }),
					{ { "{t}", team(winner) }, { "{r}", std::to_string(event.value("round", 0)) } })
				: std::string(u8"Fin du combat : match nul.");
			offer(best, END, text, nowMs);
		}
	}

	// Équipe en danger, et retournement (une équipe menée de plus de 40 points de % repasse devant).
	if (after.phase == BattlePhase::FIGHT)
	{
		double hp[3] = { 0, teamHpPercent(after, 1), teamHpPercent(after, 2) };
		for (int side = 1; side <= 2; side++)
		{
			double deficit = hp[3 - side] - hp[side];
			worstDeficit[side] = std::max(worstDeficit[side], deficit);
			if (!comebackSaid[side] && worstDeficit[side] > 40 && deficit < 0)
			{
				comebackSaid[side] = true;
				offer(best, COMEBACK, fill(pick({ u8"Retournement : {t} repasse devant !", u8"Incroyable remontée de {t} !" }), { { "{t}", team(side) } }), nowMs);
			}
			if (!dangerSaid[side] && hp[side] > 0 && hp[side] < 15)
			{
				dangerSaid[side] = true;
				offer(best, DANGER, fill(pick({ u8"{t} est au bord du gouffre !", u8"Plus que quelques PV pour {t}..." }), { { "{t}", team(side) } }), nowMs);
			}
		}
	}

	if (best.priority < 0)
		return std::string();
	if (best.priority >= END || nowMs - lastSpoken >= MIN_INTERVAL_MS)
		return say(best, nowMs);
	pending = best;
	return std::string();
}

std::string Commentary::poll(std::int64_t nowMs)
{
	if (pending.priority < 0)
		return std::string();
	if (nowMs - pending.at >= STALE_MS)
	{
		pending = Candidate();
		return std::string();
	}
	if (nowMs - lastSpoken < MIN_INTERVAL_MS)
		return std::string();
	return say(pending, nowMs);
}
