#pragma once

#include <vector>

#include <Puzzle.h>

#include "LocalBattleScreen.h"

namespace tw
{
	// Énigme tactique (assets/puzzles) : une position imposée, à résoudre pendant ses tours. Les
	// adversaires ne jouent pas ; les sorts font leurs dégâts minimum. Un panneau en haut de l'écran
	// donne l'objectif, l'indice sur demande, puis le résultat (énigme suivante, réessayer, liste).
	class PuzzleScreen : public LocalBattleScreen
	{
	public:
		// index : rang dans la liste des énigmes. demo : la solution est jouée comme par un joueur
		// (vérification, captures d'écran).
		PuzzleScreen(tgui::Gui * gui, int index, bool demo = false);

		virtual void update(float deltatime);

		// Énigmes de assets/puzzles, dans l'ordre de la liste (chargées une fois).
		static const std::vector<battle::Puzzle> & puzzles();

	protected:
		virtual void leave();

	private:
		enum class Result { PLAYING, SOLVED, FAILED };
		enum class Next { LIST, RETRY, NEXT };

		static int mapOf(int index);
		void refreshPanel();
		void layoutPanel();
		void playDemo(float deltatime);

		int index;
		battle::Puzzle puzzle;
		bool demo;
		std::size_t demoStep;
		float demoWait;
		Result result;
		Next next;
		bool hintShown;
		bool cameraPlaced;
		sf::Vector2f viewSize;
		sf::Font textFont;

		tgui::Panel::Ptr panel;
		tgui::Label::Ptr titleLabel;
		tgui::Label::Ptr textLabel;
		tgui::Button::Ptr hintButton;
		tgui::Button::Ptr retryButton;
		tgui::Button::Ptr nextButton;
		tgui::Button::Ptr listButton;
	};
}
