# Checklist de l'événement

## Avant l'événement

- [ ] Préparer les paquets : `powershell -ExecutionPolicy Bypass -File tools\package.ps1 -ServerHost <IP du serveur>`
      (compile en Release, lance les tests, crée `dist\*.zip`).
- [ ] **Réseau** : un réseau local filaire ou un point d'accès Wi-Fi dédié. Le Wi-Fi des établissements
      isole souvent les postes entre eux (« isolation des clients ») : le jeu ne fonctionne pas dans ce cas.
- [ ] Choisir le PC serveur, lui donner une adresse IP fixe si possible.
- [ ] Sur le PC serveur, lancer `Ouvrir-pare-feu.bat` en administrateur (ports 12345 et 8080).
- [ ] Lancer le serveur une première fois et **noter le mot de passe admin** affiché.
- [ ] Créer les équipes (client connecté en `admin`, onglet Équipes), puis imprimer les fiches
      d'identifiants générées dans `data\exports\fiches-equipes.html`.
- [ ] Vérifier la licence des musiques (`assets\music\SAM1_*`) avant une diffusion publique.
- [ ] **Répétition générale** : tous les PC sur le réseau de l'événement, avec le projecteur.
- [ ] Proposer aux élèves de faire le **tutoriel** (bouton « Tutoriel » de l'écran de connexion,
      quelques minutes, sans serveur), puis de s'entraîner : bouton « Entraînement » (voir
      `docs/regles-du-jeu.md`).

## Test grandeur nature sans joueurs

1. Lancer le serveur, créer 4 à 8 équipes et un tournoi.
2. Lancer un bot par joueur : `TacticalWarBot.exe --login <login> --password <mdp> --server <IP> --delay 800`.
3. Suivre l'avancée sur `http://<IP>:8080/` et avec un client en mode réalisateur.

## Le jour J

1. Démarrer le serveur. Il affiche ses adresses sur le réseau local.
2. Sur chaque PC joueur, lancer `TacticalWar.exe` et se connecter avec la fiche de l'équipe
   (l'adresse du serveur se règle sur l'écran de connexion).
3. **Écran projeté** :
   - arbre du tournoi et résultats en direct : navigateur sur `http://<IP>:8080/?rotate=20` (plein écran : F11).
     La page montre aussi les derniers combats avec leur MVP, et un onglet « Meilleurs joueurs ».
     À la fin du tournoi, elle passe sur la « Cérémonie » (podium, MVP du tournoi, hauts faits rares) ;
   - combats en direct : `Spectateur-realisateur.bat` (suit le combat le plus serré) ;
   - temps forts : chaque combat est enregistré (`data\replays\`) et peut être revu depuis l'écran
     spectateur, onglet « Rediffusions » (client connecté sans identifiants).
4. Administration (client connecté en `admin`) : onglet Tournoi pour créer et démarrer le tournoi.
   Les matchs se lancent automatiquement dès que les deux équipes sont libres. Réglage « Combats » :
   au KO, ou « Zone à tenir » (premier au nombre de points choisi, voir `docs/regles-du-jeu.md`).
   Réglage « Talents par joueur » : un talent gagné par match joué, 3 au plus par défaut (0 : aucun).
   Réglage « Bannissement » : chaque équipe interdit une classe à l'autre avant le match (aucun, phase
   finale ou tous les matchs) ; prévoir 20 s de plus par match concerné.
   Réglage « Cartes » : classiques, à cases spéciales (braises, sources, hautes herbes) ou toutes.
5. Accueil et attente (facultatif) : un PC en entraînement libre pour les équipes qui attendent leur
   match, ou en démonstration (`TacticalWar.exe --training-autoplay`, combats entre ordinateurs).

## En cas de problème

| Situation | Que faire |
|---|---|
| Un joueur est déconnecté | Il se reconnecte avec les mêmes identifiants et retrouve son combat. Son tour passe automatiquement en attendant ; une équipe absente pendant 90 s perd par forfait. |
| Le serveur s'est arrêté | Le relancer : le tournoi reprend, les matchs en cours sont rejoués. |
| Résultat contesté ou match à rejouer | Onglet Tournoi : « Victoire A/B », « Arrêter (PV) » ou « Rejouer ». Le journal `data\results.jsonl` garde chaque résultat avec la graine du combat. |
| Une équipe est en retard | « Suspendre » arrête le lancement de nouveaux matchs. |
| Un PC n'a pas de son | Lancer le client avec `--no-sound`. |
| Les émotes des joueurs gênent | Mettre `"emotes": false` dans `server.json`, puis relancer le serveur. |

## Après le tournoi

1. La page projetée passe sur la **cérémonie** : podium, MVP du tournoi, hauts faits les plus rares.
2. **Diplômes** : onglet Tournoi, sélectionner le tournoi, bouton « Diplômes » (ou
   `http://<IP>:8080/diplomes.html?tournament=<numéro>`). Une page A4 par joueur : équipe et classement,
   matchs joués et gagnés, bilan, classes jouées, hauts faits et titres de MVP. Bouton « Imprimer »
   (ou impression vers PDF) ; les en-têtes et pieds de page du navigateur peuvent être désactivés dans
   ses options d'impression.
3. Sauvegarder le dossier `data\` du serveur, puis supprimer les fiches d'identifiants.
