#include "Highlights.h"
#include "BattleMirror.h"

#include <algorithm>

using namespace tw::battle;
using nlohmann::json;

namespace
{
	struct Moment
	{
		std::string kind;
		std::string title;
		int score = 0;
		std::size_t batch = 0;
	};

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

	std::string nameOf(const BattleState & state, int id)
	{
		const Fighter * fighter = state.findFighter(id);
		return fighter != nullptr ? fighter->name : std::string("?");
	}
}

json tw::battle::highlightJson(const Highlight & highlight)
{
	return { { "kind", highlight.kind }, { "title", highlight.title }, { "score", highlight.score }, { "at", highlight.atMs },
		{ "from", highlight.from }, { "to", highlight.to } };
}

std::vector<Highlight> tw::battle::detectHighlights(const json & startSnapshot, const std::vector<std::pair<std::int64_t, json>> & batches,
	const std::string & team1, const std::string & team2, std::size_t max)
{
	BattleState state;
	BattleMap map;
	BattleMirror::applySnapshot(state, map, startSnapshot);
	const std::string teams[3] = { "", team1, team2 };

	std::vector<Moment> moments;
	int knockoutsThisTurn = 0;
	int lastAttacker = -1;
	std::size_t lastDeathBatch = 0;
	double worstDeficit[3] = { 0, 0, 0 };
	bool comebackFound[3] = { false, false, false };

	for (std::size_t index = 0; index < batches.size(); index++)
	{
		const json & events = batches[index].second.contains("ev") ? batches[index].second["ev"] : json::array();
		for (const json & event : events)
		{
			// Noms lus avant l'application (un combattant mis KO garde son nom de toute façon).
			std::string type = event.value("t", std::string());
			if (type == "turn")
			{
				knockoutsThisTurn = 0;
			}
			else if (type == "damage")
			{
				int source = event.value("src", -1);
				if (source >= 0)
					lastAttacker = source;
				int amount = event.value("amount", 0) + event.value("absorbed", 0);
				if (amount >= 25 && source >= 0)
					moments.push_back({ "big_hit", u8"Gros coup de " + nameOf(state, source) + " : " + std::to_string(amount) + u8" dégâts",
						40 + std::min(30, amount / 2), index });
			}
			else if (type == "combo")
			{
				moments.push_back({ "combo", u8"Combinaison " + event.value("name", std::string()) + " de " + nameOf(state, event.value("src", -1)),
					60, index });
			}
			else if (type == "death")
			{
				knockoutsThisTurn++;
				lastDeathBatch = index;
				std::string victim = nameOf(state, event.value("f", -1));
				std::string killer = lastAttacker >= 0 ? nameOf(state, lastAttacker) : std::string();
				if (knockoutsThisTurn >= 2)
				{
					// Le double KO remplace le premier KO du tour.
					for (auto it = moments.rbegin(); it != moments.rend(); ++it)
					{
						if (it->kind == "ko")
						{
							moments.erase(std::next(it).base());
							break;
						}
					}
					moments.push_back({ "double_ko", killer.empty() ? std::string(u8"Double KO") : u8"Double KO de " + killer, 90, index });
				}
				else
				{
					moments.push_back({ "ko", killer.empty() || killer == victim ? victim + u8" est mis KO" : u8"KO de " + victim + " par " + killer,
						65, index });
				}
			}
			BattleMirror::applyEvent(state, event);
		}

		// Retournement : une équipe menée de plus de 40 points repasse devant.
		if (state.phase == BattlePhase::FIGHT)
		{
			double hp[3] = { 0, teamHpPercent(state, 1), teamHpPercent(state, 2) };
			for (int side = 1; side <= 2; side++)
			{
				double deficit = hp[3 - side] - hp[side];
				worstDeficit[side] = std::max(worstDeficit[side], deficit);
				if (!comebackFound[side] && worstDeficit[side] > 40 && deficit < 0)
				{
					comebackFound[side] = true;
					moments.push_back({ "comeback", u8"Retournement de " + teams[side], 80, index });
				}
			}
		}
	}

	// Dernier debout : le vainqueur n'a plus qu'un combattant, au moment du dernier KO.
	if (state.phase == BattlePhase::ENDED && (state.winnerTeam == 1 || state.winnerTeam == 2))
	{
		const Fighter * survivor = nullptr;
		int alive = 0;
		for (const Fighter & fighter : state.fighters)
		{
			if (fighter.team == state.winnerTeam && fighter.alive)
			{
				alive++;
				survivor = &fighter;
			}
		}
		if (alive == 1 && survivor != nullptr)
			moments.push_back({ "last_standing", u8"Dernier debout : " + survivor->name, 75, lastDeathBatch });
	}

	// Fenêtres, puis les mieux notés sans chevauchement.
	std::vector<Highlight> candidates;
	for (const Moment & moment : moments)
	{
		Highlight highlight;
		highlight.kind = moment.kind;
		highlight.title = moment.title;
		highlight.score = moment.score;
		highlight.atMs = batches[moment.batch].first;
		highlight.from = moment.batch;
		while (highlight.from > 0 && batches[highlight.from - 1].first >= highlight.atMs - HIGHLIGHT_BEFORE_MS)
			highlight.from--;
		highlight.to = moment.batch;
		while (highlight.to + 1 < batches.size() && batches[highlight.to + 1].first <= highlight.atMs + HIGHLIGHT_AFTER_MS)
			highlight.to++;
		candidates.push_back(highlight);
	}
	std::stable_sort(candidates.begin(), candidates.end(), [](const Highlight & a, const Highlight & b) { return a.score > b.score; });

	std::vector<Highlight> chosen;
	for (const Highlight & candidate : candidates)
	{
		if (chosen.size() >= max)
			break;
		bool overlaps = std::any_of(chosen.begin(), chosen.end(), [&](const Highlight & other) {
			return candidate.from <= other.to && other.from <= candidate.to;
		});
		if (!overlaps)
			chosen.push_back(candidate);
	}
	std::sort(chosen.begin(), chosen.end(), [](const Highlight & a, const Highlight & b) { return a.atMs < b.atMs; });
	return chosen;
}
