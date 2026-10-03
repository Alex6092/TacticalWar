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
#include <MoveActionAnimationEventListener.h>
#include <SpellView.h>
#include "BattleFx.h"
#include <BattlePreview.h>
#include <BattleState.h>

namespace tw
{
	// Écran de combat. Le serveur fait autorité :
	// - "truth" est l'état du combat tel que le serveur l'a annoncé (mis à jour dès réception) ;
	//   il sert aux prévisualisations (déplacement, zones de sort) avec les mêmes règles que le serveur ;
	// - "shown" est l'état affiché, mis à jour au rythme des animations des événements.
	class BattleScreen : public Screen, RendererEventListener, ServerMessageListener, MoveActionAnimationEventListener
	{
	public:
		// PLAYER : joueur du combat ; SPECTATOR / ADMIN : spectateur (retour à la liste des combats
		// ou à l'écran d'administration).
		enum class Mode { PLAYER, SPECTATOR, ADMIN };

		BattleScreen(tgui::Gui * gui, int environmentId, Mode mode = Mode::PLAYER);
		virtual ~BattleScreen();

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

		// MoveActionAnimationEventListener
		virtual void onMoveFinished();

	protected:
		// La galerie des effets (FxGalleryScreen) alimente cet écran avec un moteur de combat local.
		struct FloatingText
		{
			sf::String text;
			sf::Color color;
			float x = 0;
			float y = 0;
			float age = 0;
		};

		void applySnapshot(const nlohmann::json & snapshot);
		void syncView(const battle::Fighter & fighter);
		BaseCharacterModel * viewOf(int fighterId);
		float playVisual(const nlohmann::json & event, bool fast);
		void processVisuals(float deltatime);
		void refreshPreview();
		// Aperçu du sort visé sur "cell" (recalculé quand la case, le sort ou l'état changent).
		void updateAimPreview(const battle::Fighter & me, const battle::Cell & cell);
		void drawAimPreview(sf::RenderWindow * window);
		bool isInteractive() const;
		bool isMouseOverHud() const;
		void selectSpell(int slot);
		// Action du joueur : envoyée au serveur.
		virtual void sendAction(const std::string & op, const nlohmann::json & body);
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
		bool cameraFitted;
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
		// Animations d'action en cours (attaque, dégâts) : temps restant avant le retour à
		// l'animation de repos ou de course. Celui qui lance une animation d'action la termine.
		std::map<int, float> actionAnimations;
		void startActionAnimation(int fighterId, BaseCharacterModel * view, tw::Animation animation);

		std::map<int, BaseCharacterModel*> views;
		std::vector<FloatingText> floatingTexts;
		std::vector<battle::TargetPreview> aimPreviews;
		battle::Cell aimPreviewCell = { -1, -1 };
		int aimPreviewSpell = -1;
		std::uint64_t aimPreviewSeq = 0;
		BattleFx fx;
		std::string periodicSpell(const battle::Fighter & target, int sourceId, battle::EffectType type) const;
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
