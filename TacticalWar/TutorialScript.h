#pragma once

#include <functional>
#include <vector>

#include <SFML/System/String.hpp>
#include <BattleState.h>

namespace tw
{
	// Ce que le tutoriel observe pour savoir si une consigne est remplie (relu à chaque image).
	struct TutorialContext
	{
		const battle::BattleState * state = nullptr;	// État affiché (au rythme des animations)
		const battle::BattleMap * map = nullptr;
		int player = 0;
		int dummy = 1;
		battle::Cell fightStart = { -1, -1 };	// Case du joueur au début du combat
		battle::Cell hoveredCell = { -1, -1 };
		int hoveredFighter = -1;
		int selectedSpell = -1;
		// Actions du joueur depuis le début de l'étape en cours.
		bool turnEnded = false;
		bool pinged = false;
		bool continued = false;		// Bouton « Continuer »
		// Le mannequin a subi des dégâts (bouclier compris). Le bilan des combattants n'arrive dans
		// l'état affiché qu'à la fin du combat : l'écran le lit dans le moteur local.
		bool dummyHit = false;
	};

	struct TutorialStep
	{
		sf::String title;
		sf::String text;
		// Étape de lecture : un bouton « Continuer » la termine aussi.
		bool canContinue = false;
		std::function<bool(const TutorialContext &)> done;
	};

	// Déroulé du tutoriel : une suite d'étapes (consigne et condition de réussite), sans affichage.
	// TutorialScreen lui transmet l'état du combat et les actions du joueur à chaque image.
	class TutorialScript
	{
	public:
		TutorialScript();
		explicit TutorialScript(std::vector<TutorialStep> steps);

		// Les 9 étapes du tutoriel : placement, déplacement, cases spéciales, sorts, attaque, fin du
		// tour, anticipation, signal, victoire.
		static std::vector<TutorialStep> standardSteps();

		// Passe à l'étape suivante si la condition de l'étape en cours est remplie (une étape par
		// appel). Retourne true si l'étape a changé.
		bool update(const TutorialContext & context);
		void skipTo(int index);

		int current() const { return index; }
		int count() const { return (int)steps.size(); }
		bool finished() const { return index >= count(); }
		// Étape en cours (la dernière une fois le tutoriel terminé).
		const TutorialStep & step() const;

	private:
		std::vector<TutorialStep> steps;
		int index = 0;
	};
}
