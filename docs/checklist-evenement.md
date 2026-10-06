# Checklist de l'événement

## Avant l'événement

- [ ] Préparer les paquets : `powershell -ExecutionPolicy Bypass -File tools\package.ps1 -ServerHost <IP du serveur>`
      (compile en Release, lance les tests, crée `dist\*.zip`). Le zip du client est aussi déposé dans
      le paquet du serveur : les élèves peuvent le télécharger sur `http://<IP>:8080/telecharger.html`.
- [ ] **Version des clients** : un client d'une autre version que le serveur est refusé à la connexion,
      avec l'adresse de la page de téléchargement. Distribuer le nouveau zip après chaque mise à jour.
- [ ] **Réseau** : un réseau local filaire ou un point d'accès Wi-Fi dédié. Le Wi-Fi des établissements
      isole souvent les postes entre eux (« isolation des clients ») : le jeu ne fonctionne pas dans ce cas.
- [ ] Choisir le PC serveur, lui donner une adresse IP fixe si possible.
- [ ] Sur le PC serveur, lancer `Ouvrir-pare-feu.bat` en administrateur (ports 12345 et 8080).
- [ ] Lancer le serveur une première fois et **noter le mot de passe admin** affiché.
- [ ] Créer les équipes (client connecté en `admin`, onglet Équipes), puis imprimer les fiches
      d'identifiants générées dans `data\exports\fiches-equipes.html`. Nombre impair d'élèves : une
      équipe peut n'avoir qu'un joueur (champs du joueur 2 laissés vides), qui joue alors les deux
      personnages.
- [ ] Vérifier la licence des musiques (`assets\music\SAM1_*`) avant une diffusion publique.
- [ ] **Répétition générale** : tous les PC sur le réseau de l'événement, avec le projecteur.
- [ ] Proposer aux élèves de faire le **tutoriel** (bouton « Tutoriel » de l'écran de connexion,
      quelques minutes, sans serveur), puis de s'entraîner : bouton « Entraînement », et ses
      « Énigmes » (voir `docs/regles-du-jeu.md`).
- [ ] Conserver le `data\profiles.json` d'un événement à l'autre : il garde les apparences débloquées
      par chaque joueur (hauts faits, victoires, énigmes).
- [ ] **Imprimer le guide du joueur**, un par joueur, en recto-verso : `http://<IP>:8080/guide.html`
      (serveur lancé) ou bouton « Guide » de l'onglet Tournoi. Il peut être distribué avant
      l'événement avec le tutoriel.

## Test grandeur nature sans joueurs

1. Lancer le serveur, créer 4 à 8 équipes et un tournoi.
2. Lancer un bot par joueur : `TacticalWarBot.exe --login <login> --password <mdp> --server <IP> --delay 800`.
3. Suivre l'avancée sur `http://<IP>:8080/` (onglet « Combats » : mosaïque des combats en cours ;
   panneau « Commentaire ») et avec un client en mode réalisateur.
4. Le réalisateur rejoue les moments des combats en cours quelques secondes après, puis, une fois des
   combats terminés, leurs temps forts, un combat après l'autre.

## Le jour J

1. Démarrer le serveur. Il affiche ses adresses sur le réseau local.
2. Sur chaque PC joueur, lancer `TacticalWar.exe` et se connecter avec la fiche de l'équipe
   (l'adresse du serveur se règle sur l'écran de connexion).
3. **Écran projeté** :
   - arbre du tournoi et résultats en direct : navigateur sur `http://<IP>:8080/?carousel=1` (plein
     écran : F11). Les deux zones font défiler leurs onglets (combats, poules ou arbre, meilleurs joueurs
     à gauche ; en direct, commentaire, derniers combats, à venir, classement à droite) ; le bouton
     « Défilement » de chaque zone l'arrête ou le relance. À la fin du tournoi, la page reste sur la
     « Cérémonie » (podium, MVP du tournoi, hauts faits rares) ;
   - combats en direct : `Spectateur-realisateur.bat`. Il enchaîne les **moments forts des combats en
     cours** en léger différé (quelques secondes après), le combat le plus serré en direct, et les temps
     forts des derniers combats, en variant les combats ;
   - la page projetée montre aussi la **mosaïque** de tous les combats en cours (onglet « Combats ») et
     le **commentaire** automatique ;
   - rediffusions complètes : chaque combat est enregistré (`data\replays\`) et peut être revu depuis
     l'écran spectateur, onglet « Rediffusions » (client connecté sans identifiants).
4. Administration (client connecté en `admin`) : onglet Tournoi pour créer et démarrer le tournoi.
   Les matchs se lancent automatiquement dès que les deux équipes sont libres. Réglage « Combats » :
   au KO, ou « Zone à tenir » (premier au nombre de points choisi, voir `docs/regles-du-jeu.md`).
   Réglage « Talents par joueur » : un talent gagné par match joué, 3 au plus par défaut (0 : aucun).
   Réglage « Bannissement » : chaque équipe interdit une classe à l'autre avant le match (aucun, phase
   finale ou tous les matchs) ; prévoir 20 s de plus par match concerné.
   Réglage « Cartes » : classiques, à cases spéciales (braises, sources, hautes herbes) ou toutes.
   Réglage « Bonus sur la carte » : des orbes (soin, énergie, protection) apparaissent au centre à partir
   du tour 3 ; désactivé par défaut.
   Réglage « Rétrécissement au tour » : à partir de ce tour, un anneau de cases se ferme au bord à chaque
   tour (12 par défaut, 0 : jamais). Les tours durent 25 s.
   **Démarrer le tournoi annule les matchs amicaux** prévus ou en cours (les joueurs sont prévenus) et
   lance aussitôt les premiers matchs.
   Onglet **Matchs** : matchs amicaux hors tournoi (équipes, carte), à regarder ou annuler. Leur
   rétrécissement se règle par `"shrinkRound"` dans `server.json`.
5. Accueil et attente (facultatif) : un PC en entraînement libre pour les équipes qui attendent leur
   match (niveau Difficile pour les plus aguerris), ou en démonstration (`TacticalWar.exe
   --training-autoplay`, combats entre ordinateurs).
6. **Avant ou après le tournoi**, les équipes qui attendent peuvent se **défier** en match amical depuis
   leur écran d'attente. Les défis sont fermés pendant un tournoi en cours.

## En cas de problème

| Situation | Que faire |
|---|---|
| Un joueur est déconnecté | Il se reconnecte avec les mêmes identifiants et retrouve son combat. Son tour passe automatiquement en attendant ; une équipe absente pendant 90 s perd par forfait. |
| Le serveur s'est arrêté | Le relancer : le tournoi reprend, les matchs en cours sont rejoués. |
| Résultat contesté ou match à rejouer | Onglet Tournoi : « Victoire A/B », « Arrêter (PV) » ou « Rejouer ». Le journal `data\results.jsonl` garde chaque résultat avec la graine du combat. |
| Une équipe est en retard | « Suspendre » arrête le lancement de nouveaux matchs. |
| Un combat dure trop | Onglet Combats, « Rétrécir la carte » (ou « Rétrécir » dans l'onglet Tournoi) : un anneau de cases se ferme tout de suite, puis un à chaque tour. |
| Une équipe veut arrêter | Bouton « Abandonner » de ses joueurs (le coéquipier connecté confirme) : défaite par abandon. |
| Un PC n'a pas de son | Lancer le client avec `--no-sound`, ou décocher « Sons et musique » dans Options. |
| Un joueur distingue mal les couleurs | Options : mode daltonien (couleurs, hachures, symboles d'équipe). Texte trop petit : taille du texte 115 ou 130 %. |
| « Ce jeu ne correspond pas au serveur » | Le client n'a pas la bonne version : bouton « Ouvrir la page de téléchargement », dézipper le nouveau client, le lancer. |
| Les émotes des joueurs gênent | Mettre `"emotes": false` dans `server.json`, puis relancer le serveur. |

## Après le tournoi

1. La page projetée passe sur la **cérémonie** : podium, MVP du tournoi, hauts faits les plus rares.
2. **Diplômes** : onglet Tournoi, sélectionner le tournoi, bouton « Diplômes » (ou
   `http://<IP>:8080/diplomes.html?tournament=<numéro>`). Une page A4 par joueur : équipe et classement,
   matchs joués et gagnés, bilan, classes jouées, hauts faits et titres de MVP. Bouton « Imprimer »
   (ou impression vers PDF) ; les en-têtes et pieds de page du navigateur peuvent être désactivés dans
   ses options d'impression.
3. Sauvegarder le dossier `data\` du serveur, puis supprimer les fiches d'identifiants.
