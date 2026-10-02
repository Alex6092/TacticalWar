#pragma once

#include <deque>
#include <map>
#include <memory>
#include <vector>
#include <SFML/Audio.hpp>
#include <nlohmann/json.hpp>

#include "Screen.h"
#include "ServerMessageListener.h"
#include "BattleColorator.h"
#include "BattleHud.h"
#include <IsometricRenderer.h>
#include <Camera.h>
#include <Environment.h>
#include <BaseCharacterModel.h>
#include <IMapKnowledge.h>
#include <MoveActionAnimationEventListener.h>
#include <SpellView.h>
#include <BattleState.h>

namespace tw
{
	// Écran de combat. Le serveur fait autorité :
	// - "truth" est l'état du combat tel que le serveur l'a annoncé (mis à jour dès réception) ;
	//   il sert aux prévisualisations (déplacement, zones de sort) avec les mêmes règles que le serveur ;
	// - "shown" est l'état affiché, mis à jour au rythme des animations des événements.
	class BattleScreen : public Screen, RendererEventListener, ServerMessageListener, IMapKnowledge, MoveActionAnimationEventListener
	{
	public:
		// PLAYER : joueur du combat ; SPECTATOR / ADMIN : spectateur (retour à la liste des combats
		// ou à l'écran d'administration).
		enum class Mode { PLAYER, SPECTATOR, ADMIN };

		BattleScreen(tgui::Gui * gui, int environmentId, Mode mode = Mode::PLAYER);
		~BattleScreen();

		virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
		virtual void update(float deltatime);
		virtual void render(sf::RenderWindow * window);

		// RendererEventListener
		virtual void onCellClicked(int cellX, int cellY);
		virtual void onCellHover(int cellX, int cellY);
		virtual void onCellMouseDown(int cellX, int cellY);
		virtual void onEvent(void * e);

		// ServerMessageListener
		virtual void onMessageReceived(std::string msg);
		virtual void onDisconnected();

		// IMapKnowledge (non utilisé : les règles sont appliquées par le serveur)
		virtual std::vector<tw::BaseCharacterModel*> getAliveCharactersInZone(std::vector<tw::Point2D> zone);

		// MoveActionAnimationEventListener
		virtual void onMoveFinished();

	private:
		struct FloatingText
		{
			sf::String text;
			sf::Color color;
			float x = 0;
			float y = 0;
			float age = 0;
		};

		struct SpellEffect
		{
			std::unique_ptr<SpellView> view;
			float remaining = 0;
		};

		void applySnapshot(const nlohmann::json & snapshot);
		void syncView(const battle::Fighter & fighter);
		BaseCharacterModel * viewOf(int fighterId);
		float playVisual(const nlohmann::json & event, bool fast);
		void processVisuals(float deltatime);
		void refreshPreview();
		bool isInteractive() const;
		bool isMouseOverHud() const;
		void selectSpell(int slot);
		void sendAction(const std::string & op, const nlohmann::json & body);
		void addFloatingText(int fighterId, const sf::String & text, const sf::Color & color);
		void playSound(const std::string & path);
		sf::String fighterName(int fighterId) const;
		void showEnd();
		void leave();
		sf::String teamLabel(int team) const;

		tgui::Gui * gui;
		sf::RenderWindow * window;
		IsometricRenderer * renderer;
		Environment * environment;
		BattleColorator * colorator;
		Camera camera;
		Mode mode;
		sf::String teamNames[2];
		float autoCloseRemaining;
		std::unique_ptr<BattleHud> hud;
		sf::Font font;

		battle::BattleMap map;
		battle::BattleState truth;
		battle::BattleState shown;
		int you;
		std::uint64_t lastSeq;
		bool hasSnapshot;
		bool awaitingServer;

		std::deque<nlohmann::json> visualQueue;
		float stepRemaining;
		bool waitingMove;
		float waitingMoveTime;
		std::map<int, float> pendingDeaths;

		std::map<int, BaseCharacterModel*> views;
		std::vector<FloatingText> floatingTexts;
		std::vector<SpellEffect> spellEffects;
		std::map<std::string, sf::SoundBuffer> soundBuffers;
		std::vector<sf::Sound> sounds;

		sf::Clock clock;
		float deadline;		// En secondes de "clock"
		int selectedSpell;
		battle::Cell hoveredCell;
		int hoveredFighter;
		bool closeRequested;
		bool endShown;
	};
}
