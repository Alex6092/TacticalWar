#pragma once

#include "LocalBattleScreen.h"
#include "TutorialScript.h"

namespace tw
{
	// Tutoriel guidé, sans serveur : le joueur (Guerrier avec le talent « Garde ») affronte un mannequin
	// (Archer qui passe ses tours, protégé lui aussi par « Garde ») sur le terrain d'exercice, qui a des
	// cases spéciales. Un panneau en haut de l'écran donne la consigne de l'étape (TutorialScript). À la
	// fin, un écran présente le tournoi, puis propose l'entraînement libre ou le retour.
	class TutorialScreen : public LocalBattleScreen
	{
	public:
		// Écran d'où vient le joueur (bouton « Retour » de la fin).
		enum class Origin { LOGIN, TRAINING };

		// startStep : étape de départ (0 = la première) ; les étapes précédentes sont jouées
		// automatiquement (captures d'écran : --tutorial-step). 9 : combat mené jusqu'au bilan de fin ;
		// 10 : écran de présentation du tournoi.
		TutorialScreen(tgui::Gui * gui, Origin origin, int startStep = 0);

		virtual void update(float deltatime);

		static const int MAP_ID = 8;

	protected:
		virtual void sendToServer(const std::string & op, const nlohmann::json & body);
		virtual void leave();

	private:
		void fastForward(int step);
		void passDummyTurns();
		void refreshPanel();
		void layoutPanels();
		void showFinal();

		enum class Next { BACK, TRAINING };

		TutorialScript script;
		Origin origin;
		Next next;
		int dummy;
		battle::Cell fightStart;
		bool turnEnded;
		bool pinged;
		bool continued;
		float dummyWait;
		bool cameraPlaced;
		// Étapes jouées automatiquement : sort à sélectionner (et case visée) dès que le joueur a la main.
		int pendingSpell;
		battle::Cell pendingHover;
		bool finalRequested;
		sf::Vector2f viewSize;
		sf::Font textFont;

		tgui::Panel::Ptr panel;
		tgui::Label::Ptr stepLabel;
		tgui::Label::Ptr textLabel;
		tgui::Button::Ptr continueButton;
		tgui::Panel::Ptr finalPanel;
	};
}
