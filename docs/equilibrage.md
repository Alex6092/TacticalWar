# Équilibrage des classes

Les caractéristiques et les sorts sont dans `assets/data/gamedata.json` : on les modifie sans recompiler
(le serveur envoie ces données aux clients à la connexion).

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
