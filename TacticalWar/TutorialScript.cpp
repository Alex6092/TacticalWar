#include "TutorialScript.h"

#include <algorithm>

using namespace tw;

namespace
{
	const battle::Fighter * fighter(const TutorialContext & c, int id)
	{
		return c.state != nullptr ? c.state->findFighter(id) : nullptr;
	}

	// Case à effet : braises, source, ou case praticable qui bloque la vue (hautes herbes).
	bool isSpecialCell(const TutorialContext & c, const battle::Cell & cell)
	{
		if (c.map == nullptr || !c.map->isWalkable(cell))
			return false;
		return c.map->turnDamage(cell) > 0 || c.map->turnHeal(cell) > 0 || c.map->blocksSight(cell);
	}

	TutorialStep makeStep(const sf::String & title, const sf::String & text, std::function<bool(const TutorialContext &)> done,
		bool canContinue = false)
	{
		TutorialStep step;
		step.title = title;
		step.text = text;
		step.done = done;
		step.canContinue = canContinue;
		return step;
	}
}

TutorialScript::TutorialScript()
	: steps(standardSteps())
{
}

TutorialScript::TutorialScript(std::vector<TutorialStep> steps)
	: steps(std::move(steps))
{
}

std::vector<TutorialStep> TutorialScript::standardSteps()
{
	std::vector<TutorialStep> steps;
	steps.push_back(makeStep(L"Placement",
		L"Avant le combat, chacun choisit sa case de départ : cliquez sur une des cases vert foncé, puis sur le bouton Prêt.",
		[](const TutorialContext & c) { return c.state != nullptr && c.state->phase != battle::BattlePhase::PLACEMENT; }));
	steps.push_back(makeStep(L"Déplacement",
		L"Le mannequin a passé son tour : à vous (cadre doré à droite). Cliquez sur une case verte pour vous déplacer. "
		L"Chaque case coûte 1 PM : le carré vert au-dessus de votre personnage indique ceux qui restent.",
		[](const TutorialContext & c) {
			const battle::Fighter * me = fighter(c, c.player);
			return me != nullptr && c.fightStart.x >= 0 && me->position != c.fightStart;
		}));
	steps.push_back(makeStep(L"Cases spéciales",
		L"Survolez les cases spéciales : les braises brûlent (8 dégâts au début du tour), la source soigne (+6 PV), "
		L"les hautes herbes cachent des tirs. La ligne d'aide, en bas, donne la règle de la case survolée.",
		[](const TutorialContext & c) { return c.continued || isSpecialCell(c, c.hoveredCell); }, true));
	steps.push_back(makeStep(L"Sorts",
		L"Choisissez un sort : touche 1 à 4, ou clic sur son icône en bas. En bleu clair, sa portée ; en bleu, "
		L"les cases où il peut être lancé d'ici.",
		[](const TutorialContext & c) { return c.selectedSpell >= 0; }));
	steps.push_back(makeStep(L"Attaque",
		L"Lancez un sort sur le mannequin. Charge (touche 2) l'atteint en ligne droite, de 2 à 5 cases ; Taillade (touche 1) "
		L"au contact. Avant de cliquer, l'aperçu montre les PV qu'il perdra et ce que son bouclier (écusson bleu) absorbera.",
		[](const TutorialContext & c) {
			const battle::Fighter * dummy = fighter(c, c.dummy);
			return dummy != nullptr && dummy->record.taken > 0;
		}));
	steps.push_back(makeStep(L"Fin du tour",
		L"Quand vous n'avez plus de PA (étoile jaune) ou plus rien à faire, cliquez sur le bouton Passer le tour. "
		L"En tournoi, un tour dure 40 secondes.",
		[](const TutorialContext & c) { return c.turnEnded; }));
	steps.push_back(makeStep(L"Anticiper",
		L"Survolez le mannequin : la zone orange montre où il pourra aller à son prochain tour. "
		L"Pratique pour se mettre hors de portée !",
		[](const TutorialContext & c) { return c.hoveredFighter == c.dummy; }));
	steps.push_back(makeStep(L"Signal",
		L"En 2 contre 2, guidez votre coéquipier : Alt + clic sur une case envoie un signal que seule votre équipe voit.",
		[](const TutorialContext & c) { return c.pinged; }));
	steps.push_back(makeStep(L"Victoire",
		L"Mettez le mannequin hors combat ! À la fin du combat, le bilan présente vos hauts faits.",
		[](const TutorialContext & c) { return c.state != nullptr && c.state->phase == battle::BattlePhase::ENDED; }));
	return steps;
}

bool TutorialScript::update(const TutorialContext & context)
{
	if (finished() || !steps[index].done || !steps[index].done(context))
		return false;
	index++;
	return true;
}

void TutorialScript::skipTo(int value)
{
	index = std::max(0, std::min(value, count()));
}

const TutorialStep & TutorialScript::step() const
{
	return steps[std::min(index, count() - 1)];
}
