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
	// Derniers sorts choisis pour chaque classe (identifiant de classe -> indices de ses sorts),
	// proposés à nouveau au choix suivant et à l'entraînement.
	std::map<int, std::vector<int>> spellChoices;
	// Derniers talents de tournoi choisis (proposés à nouveau au match suivant et à l'entraînement).
	std::vector<std::string> talentChoice;
	// Énigmes tactiques réussies (identifiants), retenues d'une session à l'autre.
	std::set<std::string> solvedPuzzles;
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
	bool trainingZone = false;
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
	// Coéquipier absent simulé, choix du joueur déjà verrouillé (--class-screen-solo) : seconde étape.
	bool classScreenSolo = false;
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

private:
	ClientConfig() {}
	// Valeur enregistrée dans client.json (l'option --no-sound ne la modifie pas).
	bool soundInFile = true;
	void load();
};
