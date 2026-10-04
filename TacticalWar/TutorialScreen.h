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

		// startStep : étape de départ (0 = la première). Les étapes précédentes sont jouées en
		// démonstration, comme par un joueur, et doivent réussir (captures d'écran : --tutorial-step).
		// 9 : tutoriel joué jusqu'au bilan de fin ; 10 : puis écran de présentation du tournoi.
		TutorialScreen(tgui::Gui * gui, Origin origin, int startStep = 0);

		virtual void update(float deltatime);

		static const int MAP_ID = 8;

	protected:
		virtual void sendToServer(const std::string & op, const nlohmann::json & body);
		virtual void leave();

	private:
		// Action de démonstration de l'étape (placement, déplacement, sort...).
		void playDemo(int step);
		void refreshPanel();
		void layoutPanels();
		void showFinal();

		enum class Next { BACK, TRAINING, PUZZLES };

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
		// Démonstration : étapes jouées automatiquement (celles d'indice inférieur à demoUntil).
		int demoUntil;
		int demoActed;
		float demoWait;
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
