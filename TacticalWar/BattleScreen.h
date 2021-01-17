#pragma once

#include "Screen.h"
#include <IsometricRenderer.h>
#include <Environment.h>
#include <BaseCharacterModel.h>
#include "TWColorator.h"
#include "LinkToServer.h"
#include <Obstacle.h>
#include <IScreenActionCallback.h>
#include <IMapKnowledge.h>

namespace tw
{
	class BattleScreen : public Screen, RendererEventListener, CharacterEventListener, ServerMessageListener, IScreenActionCallback, IMapKnowledge
	{
	private:
		IsometricRenderer * renderer;
		Environment * environment;
		TWColorator * colorator;

		BaseCharacterModel * activeCharacter;

		std::map<int, tw::BaseCharacterModel*> characters;
		std::vector<tw::BaseCharacterModel*> timeline;
		sf::Font font;
		sf::Text FPS;

		sf::RenderWindow * window;
		tgui::Gui * gui;
		std::vector<Point2D> pathZone;


		Point2D lastStartPosition;
		Point2D lastTargetPosition;
		void invalidatePathZone();

		std::vector<Obstacle> getDynamicObstacles();

		bool readyToValidatePosition;

		int turnToken;

		bool hasInitSpellBar;

		void calculateAndSetSpellZone();
		void clearSpellZone()
		{
			colorator->setSpellLaunchZone(std::vector<tw::Point2D>());
		}

		void calculateAndSetSpellImpactZone(int targetX, int targetY);

		int selectedSpell = -1;
		void setSelectedSpell(int spellNumber)
		{
			selectedSpell = -1;
			if (activeCharacter != NULL)
			{
				if (spellNumber > 0 && spellNumber <= 4 && activeCharacter->canDoAttack(spellNumber))
				{
					selectedSpell = spellNumber;
					calculateAndSetSpellZone();
				}
			}

			if (selectedSpell == -1)
			{
				clearSpellZone();
			}
		}

		bool redirectToBattlePreparation;
		std::vector<AbstractSpellView<sf::Sprite*>*> animationsToDisplay;

		float msgRemainingTime;
		tgui::Label::Ptr msgLabel;
		void setMessage(std::string message);

	public:
		BattleScreen(tgui::Gui * gui, int environmentId);
		~BattleScreen();

		virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
		virtual void update(float deltatime);
		virtual void render(sf::RenderWindow * window);



		// Renderer event listener :
		virtual void onCellClicked(int cellX, int cellY);
		virtual void onCellHover(int cellX, int cellY);
		virtual void onCellMouseDown(int cellX, int cellY);
		virtual void onEvent(void * e);

		// CharacterEventListener :
		virtual void onPositionChanged(BaseCharacterModel * c, int newPositionX, int newPositionY);

		// ServerMessageListener :
		virtual void onMessageReceived(std::string msg);
		virtual void onDisconnected();

		// IScreenActionCallback :
		virtual void applyEndOfBattle(int winnerTeam);
		virtual void applyChangeTurn(float remaining, int idPerso, std::string message);
		virtual void applyCharacterDie(int idPerso);
		virtual void applyCharacterLaunchSpell(int persoId, int x, int y, int spellId);
		virtual tw::BaseCharacterModel* getCharacter(int persoId);
		virtual std::vector <tw::BaseCharacterModel*> getAliveCharacters();
		virtual void addAnimationToDisplay(SpellView * s);
		virtual void applyCharacterMove(int persoId, std::vector<tw::Point2D> path, MoveActionAnimationEventListener * callback);
		virtual void applyCharacterDisconnected(int persoId);
		virtual void applyCharacterConnected(int persoId);
		virtual void applyTakeDamage(int persoId);
		virtual void applyCharacterPosition(int persoId, int x, int y);
		virtual void applyEnterBattlePhase();
		virtual void applyTeleport(int playerId, int cellX, int cellY);
		virtual void applySynchroPA(int playerId, int pa);
		virtual void applySynchroPM(int playerId, int pm);
		virtual void playTakeDamageSound();

		// IMapKnowledge
		virtual std::vector<tw::BaseCharacterModel*> getAliveCharactersInZone(std::vector<tw::Point2D> zone);
	};
}

