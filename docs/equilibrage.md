# Équilibrage des classes

Les caractéristiques et les sorts sont dans `assets/data/gamedata.json` : on les modifie sans recompiler
(le serveur envoie ces données aux clients à la connexion). Le guide du joueur imprimable reprend ces
chiffres : le régénérer ensuite avec `py tools/docs/make_player_guide.py` (un test le vérifie).

## Simulation

Le bot sait jouer des combats 2v2 contre lui-même, sans serveur, sur les cartes du pool de tournoi :

```
cd x64\Release
TacticalWarBot.exe --simulate 3000 --seed 1
TacticalWarBot.exe --simulate 3000 --data mon-essai.json    (autre fichier de données)
TacticalWarBot.exe --simulate 1000 --map 6                  (une seule carte)
```

Le rapport donne le taux de victoire de chaque classe et de chaque composition d'équipe, la durée des
combats, la façon dont ils se terminent, et les victoires de l'équipe 1 par carte (une carte équitable
est proche de 50 %).

L'IA (`BattleEngineLib/BotBrain.cpp`) estime la valeur de chaque sort (dégâts, soins, boucliers,
contrôle) et se place selon son rôle : les tireurs restent à distance, les combattants de mêlée vont au
contact. Elle reste simple : seuls les **grands écarts** (au-delà de 40-60 %) signalent un vrai
déséquilibre. Les tests avec de vrais joueurs restent indispensables.

## Réglages appliqués (octobre 2026)

Mesures sur 3000 combats (graine 23), taux de victoire par classe :

| Classe | Avant | Après |
|---|---|---|
| Guerrier | 30 % | 46 % |
| Archer | 67 % | 53 % |
| Mage | 46 % | 48 % |
| Protecteur | 56 % | 54 % |

Modifications :

- **Guerrier** : 135 PV (au lieu de 120), 4 PM (au lieu de 3) ; Taillade 16-19 (14-17) ;
  Charge portée 2-5 (2-4) et relance 2 tours (3) ; Provocation portée 2-6 (2-5).
- **Archer** : 80 PV (85), puissance 0 % (10 %) ; Tir précis 11-13 (12-14) ;
  Tireur d'élite +15 % (+20 %).
- **Mage** : 85 PV (80) ; Éclair 8-9 (7-8).
- **Protecteur** : Soin 14-18 (16-20).

Sans le 4e PM et la Charge améliorée, le Guerrier restait à 34 % : il a besoin de mobilité pour
rattraper un tireur qui recule.

## Combinaisons entre classes (octobre 2026)

Mesures sur 2000 combats (graine 7), avant et après l'ajout des combinaisons (voir `docs/regles-du-jeu.md`) :

| Classe | Avant | Après |
|---|---|---|
| Guerrier | 46,1 % | 46,6 % |
| Archer | 52,3 % | 52,4 % |
| Mage | 48,4 % | 51,2 % |
| Protecteur | 53,3 % | 49,7 % |

Les compositions qui disposent d'une combinaison progressent, sans dépasser 55 % : Guerrier + Mage passe
de 47 à 52 %. Pour 100 combats, l'IA déclenche Brise-glace 20 fois, Cible immobile 25 fois, Dans le
mille 32 fois et Jugement ardent 52 fois. Le rapport du simulateur donne ces chiffres à chaque essai.

## Mode « zone à tenir » (octobre 2026)

Mesures sur 2000 combats (graine 7, `--mode zone`, 5 points) :

| Classe | Taux de victoire |
|---|---|
| Guerrier | 48,7 % |
| Archer | 42,2 % |
| Mage | 56,6 % |
| Protecteur | 52,9 % |

- 29 % des combats se terminent par la zone, les autres par KO.
- Victoires de l'équipe 1 par carte entre 49 et 56 %.
- La zone calculée est symétrique sur les cartes symétriques. Avant cela, une zone placée d'un seul côté
  de la carte 2 donnait 72 % de victoires à l'équipe 1.
- L'IA des tireurs laisse la zone à un coéquipier qui la tient déjà et évite le contact. Sans ce
  réglage, les tireurs allaient se faire battre dans la zone (Archer à 33 %).

## Sorts au choix (octobre 2026)

Le simulateur fait emporter à chaque combattant 4 sorts au hasard parmi les 6 de sa classe, et donne le
taux de victoire quand chaque sort est emporté. Mesures sur 3000 combats (graine 7, au KO) :

| Classe | Taux de victoire |
|---|---|
| Guerrier | 47,5 % |
| Archer | 49,6 % |
| Mage | 49,1 % |
| Protecteur | 53,8 % |

- Les sorts d'attaque de base (Tir précis, Taillade, Éclair) ressortent à 57-62 % : l'IA les utilise à chaque
  tour.
- Les nouveaux sorts sont entre 43 et 53 % : Cri de guerre à 43 %, Prison de glace à 45 %, Barrière à 51 %.
- L'IA tire peu parti des renforcements et des marques de combinaison : les essais avec de vrais joueurs
  décideront des retouches.

## Talents de tournoi (octobre 2026)

Avec `--talents N`, chaque combattant reçoit N talents au hasard, et le rapport donne le taux de victoire
quand chaque talent est pris. Mesures sur 3000 combats (graine 7, 2 talents, au KO) :

| Classe | Taux de victoire |
|---|---|
| Guerrier | 46,4 % |
| Archer | 49,5 % |
| Mage | 48,6 % |
| Protecteur | 55,6 % |

- Les talents restent entre 46,8 % (Ancrage) et 52,8 % (Carapace) : aucun n'écrase les autres.
- Ancrage dépend du placement au contact, que l'IA cherche peu : il devrait mieux réussir entre joueurs.
- Les classes bougent peu par rapport aux mesures sans talents (Protecteur 53,8 → 55,6 %).

## Hauts faits (octobre 2026)

Le rapport du simulateur donne la part des combattants qui obtiennent chaque haut fait. Un haut fait
trop fréquent ne distingue personne : Démolisseur, d'abord à 100 dégâts, revenait à 48 % des
combattants ; à 150 dégâts, il en récompense 27 %. Mesures sur 1500 combats (graine 7) :

| Haut fait | Au KO | Mode zone |
|---|---|---|
| Premier sang | 24 % | 21 % |
| Coup double | 12 % | 8 % |
| Maître des combos | 6 % | 6 % |
| Démolisseur | 27 % | 18 % |
| Ange gardien | 24 % | 22 % |
| Intouchable | 3 % | 5 % |
| Dernier debout | 8 % | 7 % |
| Gardien de la zone | — | 19 % |
| Victoire éclair | 3 % | 4 % |

L'IA déclenche peu de combinaisons : entre joueurs qui se coordonnent, « Maître des combos » devrait
être plus fréquent.

## Cartes à cases spéciales (octobre 2026)

Les cartes 9 à 11 sont symétriques par demi-tour. Mesures sur 600 combats par carte (graine 7, au KO),
puis 1500 combats (graine 21) pour les cartes 9 et 11 :

| Carte | Équipe 1 | Classes |
|---|---|---|
| 9 Cœur du volcan | 45,3 %, puis 47,9 % | 45,8 à 54,7 % |
| 10 Prairie des hautes herbes | 50,8 % | 47,0 à 55,1 % |
| 11 Oasis brûlante | 46,5 %, puis 49,9 % | 43,7 à 54,0 % |

- En mode zone, la zone calculée est au centre, à égale distance des deux équipes (5 ou 6 pas), et
  l'équipe 1 gagne 48 à 49,5 % des combats.
- Les hautes herbes de la carte 10 (33 cases) ne pénalisent pas l'Archer plus que les rochers des
  cartes classiques (47 %).

## Sorts de terrain (octobre 2026)

Un 7e sort par classe, ajouté en fin de liste (les choix enregistrés et les énigmes restent valables).
Mesures sur 2000 combats, taux de victoire de l'équipe 1 : 50,0 % ; par classe : Mage 48,5 %,
Archer 48,1 %, Protecteur 57,2 %, Guerrier 46,2 %. Taux de victoire quand le sort est emporté (4 sorts
tirés au hasard sur 7) :

| Sort | Victoires |
|---|---|
| Palissade (Archer) | 44,7 % |
| Éboulis (Guerrier) | 43,9 % |
| Mur de glace (Mage) | 45,7 % |
| Voile sacré (Protecteur) | 54,1 % |

L'IA simple sous-estime les murs (elle les pose surtout pour protéger un allié) : ces chiffres sont
une borne basse. Par carte (200 combats chacune), l'équipe 1 gagne entre 44,5 et 56,5 %.

## Bonus sur la carte (octobre 2026)

Avec les orbes (`--bonuses`), 2000 combats : équipe 1 à 50,0 % ; Mage 50,5 %, Archer 47,1 %,
Protecteur 55,1 %, Guerrier 47,3 %. La carte 11 (la plus sensible au placement des orbes) donne 51 %
sur 1000 combats.

## Carte qui rétrécit (octobre 2026)

`TacticalWarBot.exe --simulate 2000 --seed 23 --shrink 12` : un anneau de cases se ferme à chaque tour
à partir du tour 12 (réglage par défaut des tournois).

| | Sans rétrécissement | Dès le tour 12 |
|---|---|---|
| Victoires de l'équipe 1 | 49,8 % | 49,6 % |
| Durée moyenne (médiane, maximum) | 13,2 tours (12, 25) | 12,9 tours (12, 24) |
| Mage, Archer, Protecteur, Guerrier | 52,2 / 47,5 / 54,9 / 45,7 % | 50,1 / 45,6 / 56,9 / 47,4 % |

Les départs restent équitables (cartes de 45,5 à 53,6 %). Entre IA, qui vont vite au contact, le gain
de durée est faible : la moitié des combats sont finis avant le tour 12, et la mort subite agit dès le
tour 15. Il compte surtout pour les combats humains qui s'étirent (équipes qui se fuient, soigneurs) :
plus de place pour se cacher. Le Protecteur, qui tient mieux dans un espace réduit, gagne 2 points.

## Ordinateur Difficile (octobre 2026)

`TacticalWarBot.exe --simulate 600 --hard-team 1` fait jouer l'équipe 1 au niveau Difficile contre
l'équipe 2 au niveau Normal.

| Version | Victoires du Difficile |
|---|---|
| Préparation du tour seule | 62 à 65 % (1200 combats) |
| Plus la concentration sur l'ennemi le plus blessé | **66,7 % et 66,8 %** (deux séries de 600 combats, graines différentes) |

D'autres pistes n'ont rien apporté au-delà du bruit de mesure (± 2 points) : finir son tour hors
d'atteinte même sans être blessé, prudence au déplacement, KO mieux récompensé, sort suivant compté en
entier. Le niveau Normal contre lui-même reste à 50 %.
