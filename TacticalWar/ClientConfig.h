#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

// Configuration du client, lue depuis client.json (créé avec les valeurs par défaut s'il est absent).
class ClientConfig
{
public:
	std::string serverHost = "127.0.0.1";
	unsigned short serverPort = 12345;
	// Musique et sons (désactivables, ex : poste de projection ou PC sans carte son).
	bool soundEnabled = true;
	// Plein écran (client.json « fullscreen », mis par le paquet de l'événement). Fenêtré par défaut
	// pour le développement ; --windowed et --window LxH forcent la fenêtre.
	bool fullscreen = false;
	bool fullscreenInFile = false;
	// Accessibilité (écran Options) : couleurs pour daltoniens (Palette.h), taille du texte des
	// surfaces de lecture en pour cent (100, 115 ou 130), alerte des 5 dernières secondes du tour.
	bool colorblind = false;
	int textScale = 100;
	bool turnAlert = true;
	// Personnages visibles à travers les arbres, rochers et murs placés devant eux.
	bool seeThrough = true;
	// Derniers sorts choisis pour chaque classe (identifiant de classe -> indices de ses sorts),
	// proposés à nouveau au choix suivant et à l'entraînement.
	std::map<int, std::vector<int>> spellChoices;
	// Derniers talents de tournoi choisis (proposés à nouveau au match suivant et à l'entraînement).
	std::vector<std::string> talentChoice;
	// Énigmes tactiques réussies (identifiants), retenues d'une session à l'autre.
	std::set<std::string> solvedPuzzles;
	// Apparences débloquées sur le compte (dernier message PA, pour l'entraînement hors ligne) et
	// apparence choisie.
	std::vector<std::string> knownAppearances;
	std::string appearance;
	// Classe préférée (écran d'attente, 0 : aucune) : le choix des classes s'ouvre sur elle.
	int preferredClass = 0;
	std::vector<int> spellChoice(int classId) const
	{
		auto it = spellChoices.find(classId);
		return it == spellChoices.end() ? std::vector<int>() : it->second;
	}

	// Connexion automatique au lancement (ligne de commande, non enregistrée) :
	//   --server hote[:port]  --login X --password Y  ou  --spectator [--director]
	bool autoConnect = false;
	// Mode réalisateur du spectateur activé au lancement (poste projeté).
	bool directorMode = false;
	std::string autoLogin;
	std::string autoPassword;

	// Outils de développement : --window LxH, --screenshot <fichier.png> [--screenshot-after <s>|end]
	// (la fenêtre se ferme après la capture ; "end" : dès l'affichage de l'écran de fin du combat).
	unsigned int windowWidth = 0;
	unsigned int windowHeight = 0;
	std::string screenshotPath;
	// Outil de développement (--frame-stats) : le client écrit toutes les 5 s la pire durée d'image et
	// le pire temps de calcul d'une image (mise à jour et dessin), pour repérer les à-coups.
	bool frameStats = false;
	float screenshotDelaySeconds = 3;
	bool screenshotAtEnd = false;
	// Galerie des effets de sorts, sans serveur : --fx-gallery [--fx-spell <id>] [--fx-map <id>].
	bool fxGallery = false;
	std::string fxSpell;
	int fxMap = 4;
	// Entraînement hors ligne : --training ouvre ses réglages ; --training-start (ou l'une des options
	// --training-class <id>, --training-map <id>, --training-1v1, --training-zone, --training-talents <n>, --training-autoplay) lance directement
	// un combat. --training-autoplay : le personnage du joueur est aussi joué par l'IA et les combats
	// s'enchaînent (démonstration sur l'écran projeté, captures).
	bool training = false;
	bool trainingStart = false;
	int trainingClass = 0;
	int trainingMap = 0;
	bool trainingDuel = false;
	bool trainingDuoControl = false;
	bool trainingAutoplay = false;
	// Captures : panneau Options ouvert sur l'écran de connexion, aide ouverte au début du combat.
	bool openOptions = false;
	bool openHelp = false;
	bool trainingZone = false;
	// Entraînement avec bonus sur la carte (--training-bonuses).
	bool trainingBonuses = false;
	// Difficulté de l'entraînement (--training-difficulty easy|normal|hard), vide : celle des réglages.
	std::string trainingDifficulty;
	// Outil de développement : écran de choix de classe sans serveur, avec N talents à choisir
	// (--class-screen N), pour les captures. -1 : désactivé. Bannissement en cours pendant S secondes
	// (--class-screen-ban S), ou terminé avec la classe interdite (--class-screen-forbidden <id>).
	int classScreenTalents = -1;
	// Tutoriel guidé (--tutorial), à partir d'une étape (--tutorial-step N, de 1 à 9) pour les captures.
	bool tutorial = false;
	// Énigmes : la liste (--puzzles), une énigme (--puzzle N, de 1 à 6), jouée par la démonstration
	// (--puzzle-demo) pour les vérifications et les captures.
	bool puzzleList = false;
	int puzzle = 0;
	bool puzzleDemo = false;
	int tutorialStep = 1;
	int classScreenBan = 0;
	int classScreenForbidden = 0;
	// Coéquipier simulé, qui a verrouillé cette classe (--class-screen-mate <id>, 0 : aucun).
	int classScreenMate = 0;
	// Équipe du joueur sur l'écran de classe de démonstration (couleur des apparences).
	int classScreenTeam = 1;
	// Coéquipier absent simulé, choix du joueur déjà verrouillé (--class-screen-solo) : seconde étape ;
	// avec --class-screen-solo-done, le choix pour le coéquipier est fait (retour à sa propre classe).
	bool classScreenSolo = false;
	bool classScreenSoloDone = false;
	// Secondes restantes pour choisir (--class-screen-seconds S, -1 : inconnues) et choix fait par le
	// coéquipier pendant une absence (--class-screen-chosen-by).
	int classScreenSeconds = -1;
	bool classScreenChosenBy = false;
	// Joueur seul dans son équipe simulé (--class-screen-alone 1 : son choix, 2 : le second personnage).
	int classScreenAlone = 0;
	// Onglet ouvert à la connexion admin (--admin-tab N : 0 Matchs, 1 Équipes, 2 Tournoi, 3 Combats),
	// pour les captures. -1 : le dernier onglet ouvert.
	int adminTab = -1;
	int trainingTalents = 0;

	static ClientConfig & get();

	void applyCommandLine(int argc, char ** argv);

	// Adresse au format "hote" ou "hote:port".
	std::string getServerAddress() const;
	// Retourne false si l'adresse est invalide.
	bool setServerAddress(const std::string & address);

	void save() const;

	// Son enregistré dans client.json, et son changement (Options) : actif tout de suite.
	bool soundSaved() const { return soundInFile; }
	void setSound(bool enabled)
	{
		soundInFile = enabled;
		soundEnabled = enabled;
	}

private:
	ClientConfig() {}
	// Valeur enregistrée dans client.json (l'option --no-sound ne la modifie pas).
	bool soundInFile = true;
	void load();
};
