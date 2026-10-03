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

## Combinaisons entre classes

Certains sorts **marquent** un ennemi, et un sort d'une autre classe lui inflige alors plus de dégâts :

| Marque | Posée par | Durée | Combinaison |
|---|---|---|---|
| Gelé | Glyphe de givre (Mage), quand l'ennemi commence son tour dedans | jusqu'à la fin de son tour suivant | **Brise-glace** : Taillade ou Charge (Guerrier) +40 %, la cible dégèle |
| Entravé | Flèche entravante (Archer) | 2 tours de la cible | **Cible immobile** : Éclair (Mage) +30 % |
| Provoqué | Provocation (Guerrier) | 2 tours de la cible | **Dans le mille** : Tir précis (Archer) +30 % |
| Brûlé | Boule de feu (Mage), ennemis seulement | 2 tours de la cible | **Jugement ardent** : Châtiment (Protecteur) +30 %, vol de vie compris |

- Une cible marquée a un **réticule doré** au sol, et la marque figure dans ses effets. Les marques
  durent assez longtemps pour que le coéquipier joue avant qu'elles s'effacent.
- Pendant la visée, l'aperçu annonce la combinaison (« Combo Brise-glace +40 % ») et compte le bonus.
- Au déclenchement : éclat doré, son, texte « Combo Brise-glace ! » et ligne de journal.
- Seule Brise-glace consomme sa marque ; les autres restent jusqu'à la fin de leur durée.
- Les marques sont des effets négatifs : la Purification d'un Protecteur les retire de ses alliés.
- L'ordinateur (bots, entraînement) recherche aussi les combinaisons.

Réglages dans `assets/data/gamedata.json` :
- la marque est un effet `STATE` avec `"negative": true` ;
- la combinaison est l'objet `"combo": { "state", "percent", "name", "consumes" }` d'un effet de dégâts.

Le simulateur d'équilibrage (`TacticalWarBot.exe --simulate`) compte les déclenchements de chaque
combinaison.

## Mode « zone à tenir »

Un autre mode de victoire, au choix de l'organisateur, en plus du KO.

- **La zone** : 5 ou 6 cases dorées au centre de la carte.
- **Les points** : à la fin de chaque tour complet (quand tout le monde a joué), une équipe marque 1 point
  si au moins un de ses combattants vivants est dans la zone et aucun adversaire.
  - Zone disputée (les deux équipes dedans) ou vide : personne ne marque.
- **La victoire** : la première équipe au score demandé (5 par défaut) gagne. Mettre toute l'équipe adverse
  hors combat fait toujours gagner.
- **La décision** : à la limite de tours ou sur arrêt de l'organisateur, on compte d'abord les points de
  zone, puis les points de vie.
- **L'affichage** :
  - le score est en haut de l'écran (« votre équipe 2 - 1 adversaires ») ;
  - un message et une ligne de journal annoncent chaque point ;
  - la page projetée montre le score dans les cartes des combats en direct.

**Où est la zone ?**
- **Zone peinte** : outils « Zone à tenir » et « Effacer la zone » de l'éditeur de cartes. La validation
  signale une zone plus proche d'une équipe que de l'autre.
- **Zone calculée** : sans zone peinte, elle est calculée au centre, à égale distance de marche des deux
  équipes. Sur une carte symétrique (cas des cartes du tournoi), la zone est elle-même symétrique. Les
  7 cartes fournies fonctionnent donc sans modification.

**Réglages** :
- tournoi : « Combats » (KO ou Zone à tenir) et « Points », dans l'onglet Tournoi de l'administration ;
- matchs hors tournoi : `"battleMode": "ZONE"` et `"zonePoints"` dans `server.json` ;
- entraînement : réglage « Mode » (ou `--training-zone`) ;
- simulateur : `TacticalWarBot.exe --simulate 2000 --mode zone [--points 5]`. Le rapport donne la zone de
  chaque carte et les distances de marche des deux équipes.

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
  hasard), la carte (au hasard parmi celles du tournoi), le mode (KO ou zone à tenir) et la difficulté.
  - **Facile** : l'ordinateur choisit parfois un sort ou un déplacement au hasard au lieu du meilleur.
  - **Normal** : l'ordinateur joue comme les bots de test du tournoi.
- **Mêmes règles qu'en tournoi** : placement puis « Prêt », minuteur de tour, aides à la visée, bilan de fin.
- En fin de combat : **« Rejouer »** (mêmes réglages, nouveau tirage) ou **« Retour »** aux réglages.
  « Quitter », en bas à droite, abandonne le combat en cours.

En ligne de commande :
- `TacticalWar.exe --training` ouvre directement les réglages ;
- `--training-start` lance un combat avec les réglages par défaut, à préciser avec `--training-class <id>`,
  `--training-map <id>`, `--training-1v1` ou `--training-zone` ;
- `--training-autoplay` fait jouer aussi le personnage du joueur par l'ordinateur, et enchaîne les combats :
  une démonstration pour un écran d'accueil.
