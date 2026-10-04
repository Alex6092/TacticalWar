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
	class BattleEventView;

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
		// Animation des événements (une fonction par type d'événement) : voir BattleEventView.
		friend class BattleEventView;

		// Durée des animations d'action des personnages (attaque, dégâts, mort), en secondes.
		static constexpr float ACTION_ANIMATION_SECONDS = 1.f;

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
		// Le joueur peut agir maintenant (son tour, animations terminées, serveur à jour).
		bool isInteractive() const;
		// C'est le tour du joueur (état de référence et état affiché) : il peut déjà choisir un sort
		// pendant une animation ; viser et agir attendent isInteractive().
		bool isMyTurn() const;
		// Combattants que le joueur fait agir : le sien, et celui de son coéquipier absent qu'il pilote.
		bool controls(int fighterId) const;
		// Combattant joué en ce moment : le combattant actif s'il est contrôlé par le joueur, sinon le sien.
		int actor() const;
		bool isMouseOverHud() const;
		void selectSpell(int slot);
		// Action de jeu du joueur (une seule à la fois, en attendant la réponse du serveur).
		void sendAction(const std::string & op, const nlohmann::json & body);
		// Message au serveur. La galerie des effets (et l'entraînement) le traitent en local.
		virtual void sendToServer(const std::string & op, const nlohmann::json & body);
		// Émote prédéfinie, et signal pour son équipe sur une case (Alt+clic ou clic molette).
		void sendEmote(int emoteId);
		// Types de signal : 0 ici, 1 attaquez, 2 repli, 3 danger.
		void sendPing(const battle::Cell & cell, int kind = 0);
		void showPing(int fighterId, const battle::Cell & cell, int kind = 0);
		void drawPingMarkers(sf::RenderWindow * window);
		void drawBubbles(sf::RenderWindow * window);
		void addFloatingText(int fighterId, const sf::String & text, const sf::Color & color);
		void playSound(const std::string & path);
		sf::String fighterName(int fighterId) const;
		void showEnd();
		// Quitte l'écran de combat (l'objet est détruit).
		virtual void leave();
		sf::String teamLabel(int team) const;

		tgui::Gui * gui;
		sf::RenderWindow * window;
		IsometricRenderer * renderer;
		Environment * environment;
		BattleColorator * colorator;
		Camera camera;
		Mode mode;
		sf::String teamNames[2];
		// Case à effet (braises, source, hautes herbes) : nom de sa tuile et règle, pour l'aide au survol.
		sf::String terrainName(const battle::Cell & cell) const;
		sf::String terrainHint(const battle::Cell & cell) const;
		bool forbiddenLogged = false;
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
		std::unique_ptr<BattleEventView> eventView;
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
		struct SpeechBubble
		{
			int fighterId = -1;
			sf::String text;
			float age = 0;
		};
		std::vector<SpeechBubble> bubbles;
		// Repères des signaux reçus (icône et mot au-dessus de la case), et case de la roue ouverte.
		struct PingMarker
		{
			battle::Cell cell;
			int kind = 0;
			float age = 0;
		};
		std::vector<PingMarker> pingMarkers;
		sf::Texture pingTextures[4];
		battle::Cell pingCell = { -1, -1 };
		float emoteCooldown = 0;
		float pingCooldown = 0;
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
