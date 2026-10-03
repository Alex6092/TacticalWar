# Aides en combat

Ce que les joueurs voient à l'écran pour comprendre leurs options. Les règles elles-mêmes (sorts, classes,
équilibrage) sont dans `assets/data/gamedata.json`, voir `docs/equilibrage.md`.

## Viser un sort

Sélectionner un sort (touches 1 à 4, ou clic sur la barre de sorts) affiche :

- **sa portée en bleu clair** : toute sa forme et sa distance, même derrière un obstacle ou sans cible ;
- **les cases où il peut être lancé en bleu franc** ;
- **la case survolée** :
  - en orange si elle peut être ciblée, avec la zone d'impact en rouge ;
  - en gris sinon, et la ligne d'aide donne la raison (« Pas de ligne de vue », « Il faut cibler un
    ennemi »…) ;
- **un message s'il n'y a aucune case ciblable d'ici**, au lieu de ne rien afficher.

Les sorts lancés sur soi (Rempart, Purification) montrent leur zone tout de suite.

Pendant la visée, **un aperçu s'affiche à côté de chaque combattant touché** :
- dégâts (« -12 à -15 »), soins (« +14 »), bouclier (« Bouclier +20 ») ;
- effets : poison ou brûlure par tour, ralentissement, poussée, attraction, bond, passifs du lanceur ;
- « KO ! » si la cible tombe à coup sûr, « KO possible » si elle peut tomber.

Le moteur du serveur calcule ces chiffres en jouant le sort sur une copie du combat, avec les jets les plus
faibles puis les plus forts : le résultat réel est toujours dans la fourchette.

Échap annule la visée.

## Anticiper les déplacements

Survoler un combattant montre où il pourra aller à son prochain tour, sans compter le tacle :
- en orange pour un ennemi ;
- en turquoise pour un allié.

Ça marche aussi pendant le tour des autres et en mode spectateur. Pendant son propre tour, les déplacements
possibles du joueur sont en vert.

## Communiquer avec son équipe

- **Signal** : Alt+clic (ou clic molette) sur une case. Un anneau doré et une flèche apparaissent sur la
  case, avec un son, et le journal indique « Léa signale une case » ou « Léa désigne Cible ».
  - Seuls les coéquipiers le voient : ni les adversaires ni les spectateurs (l'écran projeté est
    visible des joueurs), et il n'est pas enregistré dans les rediffusions.
  - Au plus un signal par seconde (trois toutes les cinq secondes côté serveur).
- **Émotes** : bouton « Émotes » à côté de « Passer le tour », ou touches F1 à F6 :
  « Bien joué ! », « Merci ! », « GG », « Oups ! », « Attention ! », « À l'attaque ! ».
  - Une bulle s'affiche à côté du personnage, pour tout le monde, spectateurs et rediffusions compris.
  - Pas de texte libre ; une émote toutes les trois secondes au plus.
  - L'organisateur peut les couper : `"emotes": false` dans `server.json`.

## Bilan de fin de combat

À la fin du combat, chaque combattant a son bilan : dégâts infligés, soins, boucliers donnés et ennemis mis
hors combat.
- Les dégâts d'un poison ou d'une brûlure reviennent à son lanceur, ceux d'une collision au pousseur.
- Le **MVP** a le meilleur score : dégâts + soins + boucliers / 2 + 25 par KO. En cas d'égalité, c'est le
  joueur de l'équipe gagnante.

Le bilan s'affiche sur l'écran de fin (joueurs, spectateurs, rediffusions). Les bilans des matchs de
tournoi sont enregistrés avec leurs résultats, et la page projetée montre :
- **les derniers combats**, avec le vainqueur et le MVP ;
- **l'onglet « Meilleurs joueurs »** : bilan cumulé de chaque joueur sur le tournoi, avec son nombre de
  titres de MVP.

## Entraînement hors ligne

Le bouton **« Entraînement »** de l'écran de connexion lance un combat contre l'ordinateur, sans serveur ni
identifiants : idéal pour découvrir les classes avant le jour J, ou pour patienter entre deux matchs.

- **Réglages** : 2 contre 2 (avec un allié joué par l'ordinateur) ou 1 contre 1, la classe de chacun (ou au
  hasard), la carte (au hasard parmi celles du tournoi) et la difficulté.
  - **Facile** : l'ordinateur choisit parfois un sort ou un déplacement au hasard au lieu du meilleur.
  - **Normal** : l'ordinateur joue comme les bots de test du tournoi.
- **Mêmes règles qu'en tournoi** : placement puis « Prêt », minuteur de tour, aides à la visée, bilan de fin.
- En fin de combat : **« Rejouer »** (mêmes réglages, nouveau tirage) ou **« Retour »** aux réglages.
  « Quitter », en bas à droite, abandonne le combat en cours.

En ligne de commande :
- `TacticalWar.exe --training` ouvre directement les réglages ;
- `--training-start` lance un combat avec les réglages par défaut, à préciser avec `--training-class <id>`,
  `--training-map <id>` ou `--training-1v1` ;
- `--training-autoplay` fait jouer aussi le personnage du joueur par l'ordinateur, et enchaîne les combats :
  une démonstration pour un écran d'accueil.
