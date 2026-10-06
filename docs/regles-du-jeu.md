# Aides en combat

Ce que les joueurs voient à l'écran pour comprendre leurs options. Les règles elles-mêmes (sorts, classes,
équilibrage) sont dans `assets/data/gamedata.json`, voir `docs/equilibrage.md`.

## Aide des commandes

En combat, le bouton **« ? »** (à droite de « Émotes ») ou la touche **H** ouvre un panneau qui
rappelle les commandes (déplacement, sorts, signaux, émotes, caméra) et les règles à retenir (PA et PM,
relance, bouclier, tacle, ligne de vue, cases spéciales, combinaisons, réserve de temps). Il se ferme
par « Fermer », par H ou par Échap.

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

## Boucliers

Un bouclier absorbe les dégâts **avant** les points de vie. Il est affiché partout où l'on regarde :
- **au-dessus du personnage** : un écusson bleu avec sa valeur, posé sur le cœur des PV ;
- **dans la frise des tours** : « PV 93/98 +20 » (le bouclier en bleu), et une barre de vie rouge prolongée
  en bleu par le bouclier ;
- **dans le panneau de détails** : « Bouclier 20 : absorbe les dégâts en premier » ;
- **pendant la visée** : l'aperçu sépare les PV perdus (« -6 à -9 PV ») de la part absorbée (« Bouclier -10 ») ;
- **à chaque coup** : un texte bleu « Bouclier -10 », puis un texte rouge pour les PV perdus ; le journal dit ce
  qu'il reste du bouclier ou qu'il est brisé ;
- **sur la page projetée** : « +20 » en bleu à côté des PV.

L'écusson est dessiné par `tools/ui/make_overlay_icons.py` (`assets/ui/characterdata/shield_bg.png`).

## Anticiper les déplacements

Survoler un combattant montre où il pourra aller à son prochain tour, sans compter le tacle :
- en orange pour un ennemi ;
- en turquoise pour un allié.

Ça marche aussi pendant le tour des autres et en mode spectateur. Pendant son propre tour, les déplacements
possibles du joueur sont en vert.

## Communiquer avec son équipe

- **Signal** : Alt+clic sur une case ouvre une roue de 4 signaux ; le clic molette envoie directement « Ici ».

  | Signal | Couleur | Journal |
  |---|---|---|
  | Ici | doré | « Léa désigne Cible » ou « Léa signale une case » |
  | Attaquez | rouge | « Léa : attaquez Cible ! » |
  | Repli | bleu | « Léa : repli ! » |
  | Danger | orange | « Léa : attention à Cible ! » ou « Léa : danger ici ! » |

  Un anneau et une flèche de la couleur du signal apparaissent sur la case, avec son icône et son nom
  pendant 3 secondes. Échap ou un clic ailleurs referme la roue.
  Message : `CG{"x", "y", "kind"}` (0 ici, 1 attaquez, 2 repli, 3 danger).
  - Seuls les coéquipiers le voient : ni les adversaires ni les spectateurs (l'écran projeté est
    visible des joueurs), et il n'est pas enregistré dans les rediffusions.
  - Au plus un signal par seconde (trois toutes les cinq secondes côté serveur).
- **Émotes** : bouton « Émotes » à côté de « Passer le tour », ou touches F1 à F6 :
  « Bien joué ! », « Merci ! », « GG », « Oups ! », « Attention ! », « À l'attaque ! ».
  - Une bulle s'affiche à côté du personnage, pour tout le monde, spectateurs et rediffusions compris.
  - Pas de texte libre ; une émote toutes les trois secondes au plus.
  - L'organisateur peut les couper : `"emotes": false` dans `server.json`.

## Réserve de temps

Un tour dure 25 secondes (`"turnSeconds"` dans les règles de `assets/data/gamedata.json` ; 40 avant la
séance de test d'octobre 2026, trop long pour finir les poules d'un tournoi de 16 équipes en une heure).
Chaque combattant dispose en plus d'une **réserve de 30 secondes** pour tout le combat :
- au-delà des 25 secondes, le minuteur passe en orange, « réserve 22 s », et la réserve s'entame ;
- le temps utilisé est perdu pour les tours suivants ; la réserve restante figure dans les détails du
  combattant (« Réserve 18 s ») ;
- réserve épuisée : le tour s'arrête à la fin des 25 secondes, comme avant ;
- un joueur absent (5 secondes par tour) n'utilise pas sa réserve, sauf si son coéquipier le pilote.

Réglage : `"timeBankSeconds"` dans les règles de `assets/data/gamedata.json` (0 : pas de réserve).

## Sorts au choix

Chaque classe a **6 sorts** ; chaque joueur en emporte **4** dans son combat, choisis sur l'écran de choix
de classe : un clic sur un sort l'ajoute ou le retire, et le compteur indique « 4/4 ». Le dernier choix fait
pour chaque classe est retenu (`client.json`) et proposé la fois suivante. Sans choix, ce sont les 4 premiers.

| Classe | Sorts de base | Nouveaux sorts |
|---|---|---|
| Guerrier | Taillade, Charge, Rempart, Provocation | **Tourbillon** (ennemis au contact, Brise-glace), **Cri de guerre** (+20 % de puissance aux alliés proches) |
| Archer | Tir précis, Flèche empoisonnée, Flèche de recul, Flèche entravante | **Pluie de flèches** (zone), **Piège** (immobilise et entrave) |
| Mage | Éclair, Boule de feu, Glyphe de givre, Transposition | **Vague de flammes** (ligne de 3 cases, épargne les alliés, brûle), **Prison de glace** (-3 PM, gèle) |
| Protecteur | Châtiment, Soin, Bouclier sacré, Purification | **Barrière** (+25 % de résistance, inamovible), **Lien de vie** (soin sur 3 tours) |

- Les nouveaux sorts posent aussi des marques de combinaison (gelé, entravé, brûlé).
- Entraînement : les sorts se choisissent sur l'écran des réglages, sous la description de la classe.
- Les bots et l'ordinateur de l'entraînement emportent 4 sorts au hasard.
- Le message `PC` envoie la classe et les sorts : `PC{"class": 4, "spells": [0, 1, 4, 5]}`. Un choix non
  valable donne les sorts par défaut.

## Choix de classe en équipe

Sur l'écran de choix de classe, le bloc **« Votre coéquipier »** (en bas à droite) montre en direct la
classe que regarde son coéquipier, puis celle qu'il a verrouillée, et s'il est absent. Il liste les
**combinaisons possibles** entre la classe affichée et celle du coéquipier (« Brise-glace : marquez avec
Glyphe de givre ou Prison de glace, puis votre coéquipier frappe avec Taillade... »). Les adversaires
ne voient rien de ces choix.

Messages : `PV{"class", "spells", "talents", "appearance"}` (brouillon de l'écran, voir plus bas) et
`PT{"name", "class", "viewing", "locked", "present"}`, relayé aux seuls coéquipiers.

**Délai et verrouillage** (depuis la séance de test d'octobre 2026) :
- un compte à rebours, à droite du bouton, annonce « Sans verrouillage, la classe affichée sera retenue
  dans 23 s » ;
- l'écran envoie à chaque changement la classe affichée, ses sorts, les talents et l'apparence ; à la fin
  du délai, chaque joueur non verrouillé garde ce brouillon (la classe au hasard ne sert plus qu'à un
  joueur jamais connecté) ;
- un verrouillage refusé (délai écoulé, classe interdite, déjà verrouillée) affiche la raison ;
- après avoir choisi pour un coéquipier absent, l'écran revient à sa propre classe ; le coéquipier qui
  revient voit la classe choisie pour lui, ses vrais sorts et « Choisi pour vous par Léa ». Les bots
  attendent 10 s d'absence avant de choisir pour leur coéquipier.

## Préférences de l'écran d'attente

En attendant un match, le panneau **« Mes préférences »** (à droite de « Défier une équipe ») règle la
classe préférée, ses 4 sorts (description au survol), 3 talents par ordre de préférence et l'apparence.
Tout est enregistré dans `client.json`.

L'écran de choix de classe s'ouvre ensuite sur la classe préférée, avec ces sorts, les premiers talents
(autant que le match en autorise) et l'apparence. **Rien n'est verrouillé d'avance** : le joueur peut
changer d'avis, et sans verrouillage la classe affichée est retenue à la fin du délai. Une classe
interdite par le bannissement reste interdite : l'écran passe à la classe suivante et le signale
(« Votre classe préférée est interdite pour ce match »).

## Coéquipier absent : un joueur, deux personnages

Si l'un des deux joueurs d'une équipe est absent (jamais connecté, ou déconnecté), ou si l'équipe n'a
qu'un joueur, ce joueur joue les deux personnages :
- **choix de classe** : après avoir verrouillé son choix, il choisit aussi celui de son coéquipier
  (« Personnage de <nom> »). Sans choix, la classe de l'absent est tirée au hasard ;
- **combat** : le personnage de l'absent est « piloté ». À son tour (« À vous de jouer <nom> ! »), la
  barre de sorts, la visée et les déplacements sont les siens, avec un tour complet (au lieu des 5 s
  d'un joueur déconnecté) ;
- si l'absent revient, il reprend la main sur son personnage ;
- une équipe entièrement absente perd toujours par forfait.

**Équipe d'un seul joueur** (nombre impair d'élèves) : à la création de l'équipe (onglet Équipes), laisser
vides les champs du « Joueur 2 (facultatif) » ; dans `equipe.txt`, une seule ligne pour cette équipe.
- Le second personnage s'appelle comme le joueur, suivi de « (2) » (« Léa (2) »). Sans compte, il est
  toujours joué par lui : le joueur choisit sa classe juste après la sienne (« Votre second
  personnage »), puis le joue à son tour en combat.
- Sur l'écran de choix de classe, le bloc « Votre second personnage » rappelle la classe du premier et
  liste leurs combinaisons ; la seconde étape propose d'abord une autre classe.
- Bilan : ce que fait le second personnage revient au joueur (diplôme, hauts faits), sans compter le
  match deux fois. Le classement « Meilleurs joueurs » ne compte que son premier personnage, pour rester
  comparable aux autres joueurs.
- Un second joueur peut être ajouté plus tard (équipe sans match en cours) : il reçoit un mot de passe,
  le premier garde le sien.

À l'entraînement, le format « 2 contre 2 (vous jouez les deux) » fait de même avec l'allié
(`--training-duo-control`). Avec `--training-autoplay`, l'ordinateur joue les personnages du joueur
en passant par les mêmes commandes qu'un joueur.

## Talents de tournoi

Au fil du tournoi, chaque joueur gagne des **talents**, des bonus valables pour toutes les classes :
**un talent par match joué** par son équipe (victoire, défaite ou exempt), jusqu'à 3 (réglable).

- Au premier match, personne n'a de talent ; au deuxième, chacun en a 1 ; au troisième, 2.
- Deux équipes qui se rencontrent ont presque toujours autant de matchs joués, donc autant de talents :
  pas d'effet boule de neige.
- Les talents se **choisissent librement avant chaque match**, comme les sorts : bouton « Talents (0/2) »
  sous la liste des sorts de l'écran de choix de classe. « Verrouiller mon choix » attend que les sorts
  et les talents soient choisis. Le dernier choix est retenu (`client.json`) et proposé la fois suivante.
- Un joueur qui ne choisit pas à temps reçoit des talents au hasard.
- Les talents de chaque combattant sont visibles de tous : panneau de détails du combat (clic sur un
  personnage) et cartes des combats en direct de la page projetée.

| Talent | Effet |
|---|---|
| Robustesse | +15 PV max |
| Force | +10 % de puissance |
| Carapace | +8 % de résistance |
| Célérité | +20 d'initiative et +2 de fuite |
| Ancrage | +3 de tacle |
| Allonge | +1 de portée (sorts à portée modifiable) |
| Ferveur | +15 % de soins |
| Garde | bouclier de 15 au début du combat, pendant 2 tours |
| Élan | +1 PM pendant son premier tour |
| Vigueur | +4 PV au début de chacun de ses 4 premiers tours |

**Réglages** :
- tournoi : « Talents par joueur » dans l'onglet Tournoi de l'administration (0 : pas de talents) ;
- matchs hors tournoi : pas de talents ;
- entraînement : réglage « Talents de tournoi » (0 à 3, ou `--training-talents N`). L'ordinateur reçoit
  autant de talents, tirés au hasard ;
- données : section `"talents"` de `assets/data/gamedata.json` (`stats` : bonus permanents, `effects` :
  effets posés au début du combat).

Le serveur annonce le nombre de talents dans `HC{"talents": 2}`, et le joueur répond
`PC{"class": 4, "spells": [0, 1, 4, 5], "talents": ["garde", "force"]}`.

## Bannissement de classe

Au choix de l'organisateur, un match de tournoi peut commencer par un **bannissement** : chaque équipe
interdit une classe à l'équipe adverse, qui choisit ensuite parmi les 3 autres.

- **Phase de bannissement** (20 s) : l'écran de choix de classe affiche « Bannissement », une consigne
  avec le compte à rebours, et le bouton « Bannir cette classe » pour la classe affichée.
  - Le premier joueur de l'équipe qui bannit décide pour l'équipe : son coéquipier voit « Votre équipe
    interdit : Mage. En attente de l'adversaire... ».
  - Une équipe qui ne bannit pas à temps n'interdit rien. Les deux équipes peuvent interdire la même
    classe.
- **Choix des classes** : le délai habituel repart à la fin du bannissement. La consigne rappelle la
  classe interdite par l'adversaire ; cette classe est grisée et marquée « interdite », et ne peut pas
  être verrouillée. Un joueur qui ne choisit pas à temps reçoit une classe autorisée au hasard.
- Le journal du combat (joueurs, spectateurs, rediffusions) et la page projetée indiquent la classe
  interdite à chaque équipe.

**Réglages** :
- tournoi : « Bannissement » dans l'onglet Tournoi de l'administration : aucun (par défaut), phase finale
  (tableaux à élimination, finales et petite finale, pas les poules ni les rondes suisses) ou tous les
  matchs ;
- durée de la phase : `"banSeconds"` dans `server.json` (20 par défaut) ;
- matchs hors tournoi : pas de bannissement.

Messages : `HC{"talents": 1, "ban": 20}` ouvre l'écran en mode bannissement, le joueur envoie
`PB{"class": 1}`, et le serveur répond `BB{"banned", "done", "forbidden"}` à son équipe, puis à tous
à la fin de la phase.

## Cases spéciales

Certaines cartes ont des cases qui changent le combat :

| Case | Effet |
|---|---|
| Braises | Praticable ; 8 dégâts au début du tour de qui s'y trouve (le bouclier absorbe d'abord) |
| Source | Praticable ; +6 PV au début du tour de qui s'y trouve |
| Hautes herbes | Praticable, mais bloque la ligne de vue : on s'y cache des tirs |

Les obstacles bloquent le passage ; rochers et arbres bloquent aussi la vue, mais pas le **buisson**
(un petit arbre rond) ni l'eau : on tire par-dessus. Au survol d'un obstacle, la ligne d'aide le dit
(« Buisson : bloque le passage, pas la vue (on tire par-dessus) »).

- L'effet s'applique au début du tour, après les poisons et les glyphes ; la mort subite reste en
  dernier. Traverser une case pendant un déplacement ne déclenche rien.
- Au survol, la ligne d'aide donne la règle de la case, y compris pendant un déplacement.
- Les dégâts et les soins s'affichent avec le nom de la case (« Braises -8 », « Source +6 »).
- L'ordinateur évite les braises et rejoint une source quand il est blessé.
- **Cartes** : les 7 cartes classiques n'en ont pas. Trois cartes de tournoi en ont : 9 « Cœur du
  volcan », 10 « Prairie des hautes herbes », 11 « Oasis brûlante ». Elles sont symétriques (un
  demi-tour échange les deux camps) et générées par `py tools/maps/make_special_maps.py`.
- **Réglage du tournoi** « Cartes » (onglet Tournoi) : classiques (par défaut), à cases spéciales, ou
  toutes. L'entraînement propose toutes les cartes du tournoi.
- La carte 8, « Terrain d'exercice » (hors tournoi, celle du tutoriel), les montre toutes :
  `TacticalWar.exe --training-start --training-map 8`. Pour en mettre sur une carte, utiliser le
  groupe « Cases spéciales » de la palette de l'éditeur.

## Combinaisons entre classes

Certains sorts **marquent** un ennemi, et un sort d'une autre classe lui inflige alors plus de dégâts :

| Marque | Posée par | Durée | Combinaison |
|---|---|---|---|
| Gelé | Glyphe de givre (Mage), quand l'ennemi commence son tour dedans ; Prison de glace (Mage) | jusqu'à la fin de son tour suivant ; 2 tours | **Brise-glace** : Taillade, Charge ou Tourbillon (Guerrier) +40 %, la cible dégèle |
| Entravé | Flèche entravante (Archer) ; Piège (Archer) | 2 tours ; jusqu'à son tour suivant | **Cible immobile** : Éclair (Mage) +30 % |
| Provoqué | Provocation (Guerrier) | 2 tours de la cible | **Dans le mille** : Tir précis (Archer) +30 % |
| Brûlé | Boule de feu ou Vague de flammes (Mage), ennemis seulement | 2 tours de la cible | **Jugement ardent** : Châtiment (Protecteur) +30 %, vol de vie compris |

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
  titres de MVP et de hauts faits ;
- **l'onglet « Cérémonie »**, une fois le tournoi terminé (affiché en premier) : podium des trois
  premières équipes avec leurs joueurs, MVP du tournoi (meilleur bilan cumulé) et hauts faits les plus
  rares, avec ceux qui les ont obtenus.

### Hauts faits

Le bilan décerne aussi des **hauts faits**, affichés sous le nom de chaque combattant sur l'écran de fin
(description au survol) et annoncés dans le journal :

| Haut fait | Condition |
|---|---|
| Premier sang | Premier ennemi mis hors combat du combat |
| Coup double | Au moins 2 ennemis mis hors combat |
| Maître des combos | Au moins 2 combinaisons déclenchées |
| Démolisseur | Au moins 150 dégâts infligés |
| Ange gardien | Au moins 60 PV rendus ou protégés (soins et boucliers) |
| Intouchable | Aucun dégât subi, et debout à la fin du combat |
| Dernier debout | Seul survivant de l'équipe gagnante |
| Gardien de la zone | Dans la zone pour au moins 3 points marqués (mode zone) |
| Victoire éclair | Victoire en 5 tours ou moins |

- Un forfait ou une décision de l'organisateur ne donne ni « Intouchable » ni « Victoire éclair ».
- Les hauts faits sont enregistrés avec les résultats du tournoi. La page projetée montre ceux du MVP
  dans les derniers combats.
- Liste et descriptions : `BattleEngineLib/Achievements.h` ; conditions : `Achievements.cpp`.

## Tutoriel guidé

Le bouton **« Tutoriel »** (écran de connexion, ou réglages de l'entraînement) apprend les bases en
quelques minutes, sans serveur. Le joueur est un Guerrier avec le talent Garde ; il affronte un
« Mannequin » (un Archer qui passe ses tours, protégé lui aussi par Garde) sur la carte 8, « Terrain
d'exercice ». Un panneau en haut de l'écran donne la consigne ; l'étape suivante s'affiche dès que
la consigne est remplie.

| Étape | Consigne |
|---|---|
| 1. Placement | Choisir une case de départ, puis cliquer sur Prêt |
| 2. Déplacement | Se déplacer sur une case verte (PM) |
| 3. Cases spéciales | Survoler les braises, la source ou les hautes herbes (ou Continuer) |
| 4. Sorts | Sélectionner un sort : sa portée s'affiche |
| 5. Attaque | Lancer un sort sur le mannequin, après avoir lu l'aperçu (PV et bouclier perdus) |
| 6. Fin du tour | Passer son tour |
| 7. Anticiper | Survoler le mannequin : sa portée de déplacement au prochain tour |
| 8. Signal | Envoyer un signal (Alt + clic) |
| 9. Victoire | Mettre le mannequin hors combat : le bilan présente les hauts faits |

- Pas de minuteur : chacun avance à son rythme. « Quitter » interrompt le tutoriel.
- Après le bilan, « Fermer » ouvre un écran qui présente le tournoi : choix des sorts et bloc du
  coéquipier, talents, bannissement, mode de victoire, réserve de temps, coéquipier absent, aide (H),
  signaux et combinaisons. Puis « Entraînement libre », « Énigmes » ou « Retour ».
- Captures d'écran : `TacticalWar.exe --tutorial-step N` ouvre le tutoriel à l'étape N (1 à 9), les
  étapes précédentes étant jouées automatiquement ; 10 mène le combat jusqu'au bilan, 11 ouvre l'écran
  final. `--tutorial` ouvre le tutoriel au début.

## Énigmes tactiques

Le bouton **« Énigmes »** des réglages de l'entraînement ouvre six petits défis, sans serveur : une
position imposée, et un objectif à atteindre pendant ses tours (mettre l'adversaire hors combat). Les
adversaires ne jouent pas, et les sorts font leurs dégâts minimum : le résultat est toujours le même.

| Énigme | Idée à trouver |
|---|---|
| 1. Brise-glace | Sur une cible gelée, Taillade (au contact) profite plus du gel que Charge |
| 2. Cible immobile | Avancer pour être à portée, puis trois Éclair sur la cible entravée |
| 3. Dans le mille | Reculer d'une case pour le bonus de distance de l'Archer, puis Tir précis |
| 4. Jugement ardent | Se mettre à portée, puis deux Châtiment sur la cible brûlée |
| 5. Contre le rocher | Flèche de recul contre un rocher : dégâts de collision |
| 6. Duo : Archer et Mage | Jouer les deux : Flèche entravante, puis trois Éclair (Cible immobile) |

- Panneau en haut de l'écran : objectif, « Indice », « Recommencer », « Énigmes » ; puis « Réussi ! »
  (« Énigme suivante ») ou « Raté » (« Réessayer »). Les énigmes réussies sont retenues (`client.json`)
  et marquées dans la liste.
- Données : `assets/puzzles/*.json` (carte, combattants, marques posées, objectif, indice, solution et
  « piège »). Les tests vérifient que la solution réussit, et que ne rien faire ou suivre le piège
  échoue : une énigme impossible ou trop facile est détectée.
- En ligne de commande : `--puzzles` (liste), `--puzzle N` (énigme N), `--puzzle-demo` (la solution
  est jouée automatiquement, avec les mêmes commandes qu'un joueur).

## Guide du joueur imprimable

Une feuille A4 recto-verso à distribuer à chaque joueur, à lire en plus du tutoriel et de
l'entraînement :
- **recto** : but du jeu (KO, zone, mort subite), déroulement d'un tour (placement, PA, PM, relance,
  réserve de temps), commandes, règles à savoir (aperçu, bouclier, résistance, tacle, ligne de vue,
  collision), cases spéciales, signaux et émotes, déroulement du tournoi, talents, entraînement ;
- **verso** : les 4 classes (caractéristiques, passif, 7 sorts avec coût, portée, relance, dégâts et
  marques), les combinaisons avec leur mise en pratique, et 5 astuces.

Le recto rappelle aussi les murs, les orbes, les apparences, les Options et la page de téléchargement.

La page est servie par le serveur : `http://<serveur>:8080/guide.html` (lien « Guide du joueur » en haut
de la page projetée, bouton « Guide » de l'onglet Tournoi). Bouton « Imprimer », en recto-verso.

Elle est générée par `py tools/docs/make_player_guide.py` à partir de `assets/data/gamedata.json` (règles,
classes, sorts, talents) et de `assets/tiles/tileset.json` (cases spéciales), icônes comprises. Après une
modification des données de jeu, relancer le script : le test « le guide du joueur est à jour »
(`TacticalWarTests`) compare l'empreinte de `gamedata.json` gardée dans le guide.

## Entraînement hors ligne

Le bouton **« Entraînement »** de l'écran de connexion lance un combat contre l'ordinateur, sans serveur ni
identifiants : idéal pour découvrir les classes avant le jour J, ou pour patienter entre deux matchs.

- **Réglages** : 2 contre 2 (avec un allié joué par l'ordinateur) ou 1 contre 1, la classe de chacun (ou au
  hasard), la carte (au hasard parmi celles du tournoi), le mode (KO ou zone à tenir) et la difficulté.
  - **Facile** : l'ordinateur choisit parfois un sort ou un déplacement au hasard au lieu du meilleur.
  - **Normal** : l'ordinateur joue comme les bots de test du tournoi.
  - **Difficile** : l'ordinateur prépare ses coups (voir « Ordinateur Difficile »).
- **Bonus sur la carte** : case à cocher, comme le réglage du tournoi (voir « Orbes »).
- **Mêmes règles qu'en tournoi** : placement puis « Prêt », minuteur de tour, aides à la visée, bilan de fin.
- En fin de combat : **« Rejouer »** (mêmes réglages, nouveau tirage) ou **« Retour »** aux réglages.
  « Quitter », en bas à droite, abandonne le combat en cours.

En ligne de commande :
- `TacticalWar.exe --training` ouvre directement les réglages ;
- `--training-start` lance un combat avec les réglages par défaut, à préciser avec `--training-class <id>`,
  `--training-map <id>`, `--training-1v1` ou `--training-zone` ;
- `--training-autoplay` fait jouer aussi le personnage du joueur par l'ordinateur, et enchaîne les combats :
  une démonstration pour un écran d'accueil.

## Sorts de terrain : les murs

Chaque classe a un 7e sort, un **sort de terrain** qui dresse un mur. Un mur est une invocation
statique : chaque case est un **bloc** avec sa propre barre de vie. Casser un bloc ouvre une brèche ;
les autres restent debout.

| Classe | Sort | Mur |
|---|---|---|
| Guerrier | Éboulis | 1 rocher : bloque le passage et la vue |
| Archer | Palissade | 3 pieux en travers : bloquent le passage (on tire par-dessus) |
| Mage | Mur de glace | 3 blocs en travers : bloquent le passage et la vue |
| Protecteur | Voile sacré | 3 cases en travers : bloquent la vue (on traverse, on s'y cache) |

- **Viser un bloc** : tout le monde peut viser n'importe quel bloc avec un sort de dégâts, y compris son
  propre mur pour passer. Les soins, boucliers et états ne visent pas les blocs.
- **Ce qui abîme un bloc** : les dégâts directs (ciblés ou de zone, jet et puissance du lanceur compris)
  et les collisions (un personnage poussé contre un bloc l'abîme). Rien d'autre : ni poison, ni brûlure,
  ni combinaison, ni passif, ni résistance.
- **Bilan** : les dégâts faits aux blocs ne comptent ni pour le MVP ni pour les hauts faits.
- **Durée** : un bloc non détruit disparaît après quelques tours de son lanceur, ou à sa mort.
- **À l'écran** : barre de vie au-dessus du bloc, dégâts flottants, aperçu « Détruit ! » à la visée, et au
  survol : « Mur de glace : 22/30 PV, encore 2 tours ».
- L'ordinateur pose un mur quand il protège un allié, et casse un bloc quand il lui barre la route.

## Orbes : bonus sur la carte

Réglage du tournoi **« Bonus sur la carte »** (onglet Tournoi), désactivé par défaut. Il existe aussi
pour l'entraînement, les matchs amicaux (`server.json`, `"mapBonuses": true`) et le simulateur
(`--bonuses`).

- **Apparition** : au tour 3, puis tous les 3 tours s'il n'y en a plus, sur une paire de cases
  symétriques au centre de la carte (le même orbe des deux côtés : c'est équitable).
- **Ramassage** : en passant dessus (déplacement) ou en y étant envoyé (bond, téléportation, poussée…).
- **Effets** (`gamedata.json`, `"bonuses"`) : soin (+15 PV), énergie (+1 PA tout de suite), protection
  (bouclier de 15 pendant 2 tours).
- Le journal l'annonce (« Nouveaux orbes au centre », « Léa ramasse l'orbe de soin ») ; l'ordinateur
  les recherche selon ses besoins.

## Ordinateur Difficile

Troisième niveau de l'entraînement (`--training-difficulty hard`), et des bots de test
(`TacticalWarBot.exe --level hard`). Sans hasard supplémentaire : la même graine donne les mêmes choix.

- Il **prépare son tour** sur une copie du combat : « se déplacer puis lancer un sort » contre
  « lancer tout de suite », en regardant aussi le sort suivant (Provocation puis Taillade…).
- Il **concentre ses coups** sur l'ennemi le plus blessé et pose les marques qu'un coéquipier exploitera
  avant le tour de la cible.
- **Blessé**, il finit son tour hors d'atteinte quand il le peut.
- Il gagne environ **deux combats sur trois** contre le niveau Normal (voir `equilibrage.md`).

## Apparences

Huit apparences : une variante de la couleur d'équipe (plus claire, plus sombre, plus vive… la teinte
reste celle de l'équipe, pour que les équipes restent reconnaissables) et une couleur de cheveux.

| Apparence | Se débloque avec |
|---|---|
| Classique | dès le départ |
| Givre | 3 énigmes réussies |
| Braise | haut fait « Premier sang » |
| Éclat | haut fait « Maître des combos » |
| Nuit | 5 victoires |
| Or | un titre de MVP |
| Ombre | haut fait « Intouchable » |
| Argent | 6 énigmes réussies |

- **Sur le compte** : le serveur garde la progression de chaque joueur dans `data/profiles.json`
  (hauts faits obtenus, victoires, MVP, énigmes signalées par son poste) ; elle sert d'un événement à
  l'autre si le fichier est conservé.
- **Choix** : rangée de pastilles à droite du personnage sur l'écran de classe. Les pastilles grisées
  donnent leur condition au survol. Le choix est vérifié par le serveur et retenu dans `client.json`.
- Le coéquipier voit l'apparence choisie ; une nouvelle apparence est annoncée en fin de combat.
- Hors ligne (entraînement), les apparences de la dernière connexion et celles des énigmes du poste
  restent disponibles.

## Abandonner

Bouton **« Abandonner »** en bas à droite, pendant le placement et le combat (pas à l'entraînement, au
tutoriel ni aux énigmes), avec une boîte de confirmation :
- seul joueur présent de son équipe (seul inscrit, ou coéquipier déconnecté) : l'abandon est immédiat ;
- deux joueurs présents : le coéquipier voit « Léa veut abandonner (25 s) - Cliquez « Abandonner » pour
  confirmer ». Sans sa confirmation dans les 30 s, le vote est annulé ; une connexion ou une
  déconnexion l'annule aussi.

L'autre équipe gagne ; le résultat porte la raison **abandon** (distincte du forfait d'une équipe
absente) dans le tournoi, les rediffusions, l'administration et la page projetée. Ni « Intouchable »
ni « Victoire éclair » ne sont décernés. Messages : `CQ{"vote"}` et `BQ{"from", "votes", "needed",
"expiresIn", "voted"}`.

## Carte qui rétrécit

Pour que les combats ne s'éternisent pas, à partir d'un tour réglable, **un anneau de cases se ferme à
chaque tour complet**, depuis le bord de la carte :
- les cases fermées sont assombries (hachurées en mode daltonien) et deviennent infranchissables ; les
  tirs passent toujours au-dessus ;
- les murs, orbes et cases de glyphe de l'anneau disparaissent ;
- un combattant sur l'anneau **glisse** vers la case libre la plus proche, côté centre, sans dégâts ;
- la zone centrale (celle du mode « zone à tenir ») ne se ferme jamais, et il reste toujours au moins
  12 cases ouvertes autour d'elle ;
- le journal et le commentateur l'annoncent (« La carte rétrécit ! ») ; la mosaïque de la page
  projetée montre les cases fermées en noir.

Réglages : tournoi, champ « Rétrécir au tour » (12 par défaut, 0 : jamais) ; matchs amicaux,
`"shrinkRound"` dans `server.json` (12 par défaut) ; entraînement, liste « Rétrécissement ». L'admin peut
aussi faire rétrécir la carte d'un combat qui dure trop : bouton « Rétrécir la carte » de l'onglet
Combats, ou « Rétrécir » de l'onglet Tournoi (un anneau tout de suite, puis un par tour ; message `SK`).

## Voir à travers le décor

Un personnage caché derrière un grand arbre, un rocher, un buisson, des hautes herbes ou un bloc de mur
reste visible : l'élément de devant devient transparent dans un ovale autour de lui. Option « Voir à
travers le décor » de l'écran Options (activée par défaut).

## Accessibilité : écran Options

Bouton **Options** de l'écran de connexion, et bouton « Options » de l'aide en combat (touche H).
Les réglages s'appliquent tout de suite et sont enregistrés dans `client.json`.

- **Sons et musique**.
- **Mode daltonien** : couleurs d'Okabe et Ito, distinctes pour toutes les formes de daltonisme
  (équipes bleue et orange, déplacement en vert bleuté face à la menace orange, impact vermillon face au
  bleu ciblable). Les cases d'impact sont **hachurées**, et chaque personnage porte un **symbole
  d'équipe** (rond ou triangle) devant son nom et dans l'ordre du tour. Les phrases qui citent une
  couleur prennent les noms du mode choisi.
- **Taille du texte** : 100, 115 ou 130 % pour le journal, la ligne d'aide, les détails du combattant,
  l'aide, les descriptions des sorts, l'ordre du tour et le nom des personnages.
- **Alerte de fin de tour** : pendant les 5 dernières secondes de son tour (temps normal, puis
  réserve), le minuteur clignote et un tic sonne chaque seconde.
- **Voir à travers le décor** : ovale transparent dans les éléments qui cachent un personnage.

## Onglet Matchs de l'administration

L'onglet **Matchs** crée des matchs amicaux, hors tournoi : nom (facultatif), équipe A, équipe B,
carte (au hasard ou choisie), puis « Créer le match » ; les joueurs connectés passent au choix des
classes. La liste montre les matchs amicaux avec leur carte, leur état et leur vainqueur, et les
boutons **Regarder** (match en cours), **Annuler le match** (prévu ou en cours) et **Actualiser**.

## Matchs amicaux libres : les défis

Tant qu'**aucun tournoi n'est en cours**, une équipe qui attend peut défier une autre équipe depuis
l'écran d'attente (panneau « Défier une équipe », actualisé toutes les 5 s).

- La liste montre chaque équipe, ses joueurs connectés, et si elle peut être défiée (sinon pourquoi :
  aucun joueur connecté, en match, défi en attente).
- L'équipe défiée reçoit une fenêtre avec compte à rebours : **Accepter** ou **Refuser** ; le premier
  joueur qui répond décide. Sans réponse, le défi expire au bout de 30 s.
- Une équipe n'a qu'un défi à la fois. Défi accepté : match amical, choix des classes habituel.
- Les défis en attente sont annulés quand un match ou un tournoi commence. Les bots refusent les défis.
- **Un tournoi qui démarre annule les matchs amicaux** prévus ou en cours (leurs joueurs reviennent à
  l'attente avec « Match amical annulé : le tournoi commence ») et lance tout de suite ses premiers
  matchs. Pendant le tournoi, ses équipes ne peuvent pas jouer de match amical.

## Mise à jour des clients

À la connexion, le client envoie la version du protocole. Un client d'une autre version que le
serveur est refusé avec un message clair (« Ce jeu (version 7) ne correspond pas au serveur
(version 8) ») et l'adresse de la page de téléchargement, avec un bouton **Ouvrir la page**.

La page `http://<serveur>:8080/telecharger.html` (lien « Télécharger le jeu » de la vue projetée)
donne la version et le zip du client, avec les étapes : télécharger, dézipper, lancer. Le zip est
déposé par `tools/package.ps1` dans le paquet du serveur. Un ancien client sans numéro de version
reste accepté.

## Commentateur automatique

La vue projetée affiche un panneau **Commentaire** : des phrases générées à partir des combats en cours
(combinaison, KO et double KO, gros coup, équipe en danger, retournement, orbe, mur posé ou détruit,
point de zone, fin du combat). Une phrase au plus toutes les 3 secondes par combat, la plus
importante d'abord ; les nouvelles lignes sont mises en surbrillance.

## Mosaïque des combats

Onglet **Combats** de la vue projetée, en premier dès qu'un combat est en cours (et dans la rotation
`?rotate=`) : une mini-carte vue de dessus par combat, avec les pions aux couleurs des équipes (initiale
de la classe, anneau de PV, combattant actif cerclé d'or), les murs, les orbes, la zone, les cases
fermées par le rétrécissement (en noir), le tour, le score de zone et les PV de chaque équipe.

## Vue projetée : onglets et défilement

La colonne de droite est faite d'onglets, un seul affiché à la fois sur toute la hauteur : **En direct**
(avec le nombre de combats), **Commentaire** (12 phrases), **Derniers combats** (6), **À venir** et
**Classement**. Un onglet vide est masqué.

Chaque zone (gauche et droite) peut faire défiler ses onglets toute seule : `?rotate=20` à gauche,
`?rotate-side=12` à droite, `?carousel=1` pour les deux (20 s et 12 s), ou le bouton **« Défilement »**
de chaque zone (retenu par le navigateur). Un clic sur un onglet suspend le défilement de sa zone une
minute ; un nouveau commentaire ne change pas d'onglet ; à la fin du tournoi, la cérémonie reste
affichée à gauche.

## Temps forts automatiques

À la fin de chaque combat, le serveur note ses meilleurs moments (combinaison, KO et double KO, gros
coup, retournement, dernier debout) et garde jusqu'à trois extraits d'environ 7 secondes dans la
rediffusion.

Le **mode réalisateur** du spectateur choisit à chaque retour sur la liste, dans l'ordre :
1. **un moment d'un combat en cours** pas encore vu (moins de 3 minutes) : le serveur le détecte
   pendant le combat et le propose environ 4 s après ; il est rejoué en **léger différé** (bandeau
   « À l'instant : KO de Léa par Tom ») ;
2. le combat en cours le plus serré, **en direct** ; toutes les 10 s, le réalisateur regarde s'il y a un
   nouveau moment dans un autre combat, et y passe au plus une fois par minute ;
3. les temps forts des rediffusions pas encore joués, **un combat après l'autre** (le meilleur moment de
   chaque combat, puis le deuxième...), puis il recommence.

Les extraits s'enchaînent sans attente (silences de 0,8 s au plus). Messages : `HL` (moments en direct
avec `session` et leur âge, puis ceux des rediffusions) et `RP{"session", "from", "to"}` pour un
extrait d'un combat en cours. Les calculs de l'ordinateur « Difficile » se font en tâche de fond dans
les combats locaux (entraînement) : l'image ne saccade plus (`--frame-stats` affiche la pire image).
