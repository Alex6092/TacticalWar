#pragma once

#include <memory>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "Screen.h"
#include "ServerMessageListener.h"
#include "LiveSessionsPanel.h"
#include "ReplaysPanel.h"

// Écran du mode spectateur : liste des combats en cours, "Regarder", et mode réalisateur pour un écran
// projeté. Le réalisateur choisit à chaque retour sur cet écran, dans l'ordre :
// 1. un moment d'un combat en cours pas encore vu (moins de 3 minutes), rejoué en léger différé au
//    rythme des rediffusions (les longues réflexions sont raccourcies, les actions ne sont pas
//    accélérées) ; jamais deux à la suite quand un combat est en cours ;
// 2. le combat en cours le plus serré, en direct, au moins 90 secondes avant de le quitter pour un
//    nouveau moment d'un autre combat (voir BattleScreen) ;
// 3. un temps fort des rediffusions pas encore joué (le serveur alterne les combats).
// Quitter un combat à la main arrête le mode réalisateur.
class SpectatorModeScreen : public tw::Screen, ServerMessageListener
{
private:
	sf::Font font;
	sf::Text title;
	sf::Text subtitle;

	tgui::Gui * gui;
	std::unique_ptr<LiveSessionsPanel> sessionsPanel;
	std::unique_ptr<ReplaysPanel> replaysPanel;
	tgui::Tabs::Ptr tabs;
	// Onglet affiché ("En direct" ou "Rediffusions"), conservé au retour d'un combat.
	static sf::String currentTab;
	void showTab(const sf::String & tab);
	tgui::CheckBox::Ptr directorBox;
	tgui::Label::Ptr directorHelp;

	sf::Shader shader;

	float sinceRefresh;
	float watchPending;		// Délai d'attente de la réponse à SW (secondes), 0 si aucune demande

	static bool directorMode;
	// Temps forts (HL) : moments des combats en cours puis des rediffusions, et ceux déjà joués
	// (« s<combat>:<lot> », « r<rediffusion>:<lot> ») ; conservés d'un retour à l'autre sur cet écran.
	static nlohmann::json highlights;
	static float highlightsAge;
	static std::set<std::string> played;
	// Combat regardé en direct par le réalisateur (0 : aucun) et moment du dernier changement de
	// combat (secondes de directorClock).
	static int liveSession;
	static float lastSwitch;
	// Le dernier choix était un extrait : le suivant est un combat en direct s'il y en a un.
	static bool afterExtract;
	static sf::Clock directorClock;
	float highlightRequest = 0;
	// Choix du réalisateur dès que la liste des combats et celle des temps forts sont arrivées.
	bool decidePending = false;
	bool sessionsReceived = false;
	bool highlightsReceived = false;

	void watch(int session);
	void watchLive(int session);
	void direct();
	void playExtract(const nlohmann::json & highlight);
	static std::string keyOf(const nlohmann::json & highlight);

public:
	SpectatorModeScreen(tgui::Gui * gui);
	~SpectatorModeScreen();

	static bool isDirectorMode() { return directorMode; }
	// Combat quitté à la main (bouton « Quitter » ou « Retour à la liste ») : fin du mode réalisateur.
	static void stopDirector() { directorMode = false; liveSession = 0; }
	// Le réalisateur regarde un combat en direct.
	static bool watchingLive() { return directorMode && liveSession > 0; }
	// Pendant ce direct : nouvelle liste des temps forts (HL). Vrai s'il faut le quitter pour un moment
	// pas encore vu d'un autre combat (après 90 secondes de direct).
	static bool onLiveHighlights(const nlohmann::json & body);

	virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui);
	virtual void update(float deltatime);
	virtual void render(sf::RenderWindow * window);

	virtual void onMessageReceived(std::string msg);
	virtual void onDisconnected();
};
