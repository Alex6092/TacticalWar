# Effets visuels des sorts

Chaque sort a son animation, qui se règle sans recompiler, dans deux fichiers :

- `assets/spellsprites/effects.json`, le **catalogue d'effets** : chaque effet nommé y est décrit (planche
  d'images, taille, position, vitesse…) ;
- `assets/data/gamedata.json`, avec l'objet **`visual`** de chaque sort : il indique quels effets du
  catalogue jouer, et quand.

Le serveur ignore ces visuels. Les spectateurs et les rediffusions les affichent aussi : ils reçoivent
les mêmes événements.

## Objet `visual` d'un sort

```json
"visual": { "projectile": { "effect": "fireball", "speed": 10, "arc": 0.3 }, "impact": "explosion",
            "impactOn": "cells", "status": "burning", "tick": "burn_tick", "impactSound": "./assets/sound/spells/blast1.ogg" }
```

| Champ | Rôle |
|---|---|
| `cast` | Effet sur le lanceur, au lancement. |
| `projectile` | Effet qui vole du lanceur à la case visée : `speed` en cases par seconde, `arc` (hauteur de la courbe) en cases. |
| `impact` | Effet à l'arrivée, sur la case visée (`impactOn` : `target`), sur chaque case de la zone (`cells`) ou sur le lanceur (`caster`). |
| `status` | Effet en boucle sur un combattant tant qu'un effet durable du sort le touche (poison, bouclier…). |
| `tick` | Effet joué à chaque dégât ou soin périodique du sort. |
| `glyph` | Effet en boucle sur chaque case d'un glyphe du sort. |
| `glyphTrigger` | Effet quand un combattant déclenche le glyphe. |
| `impactSound` | Son joué à l'impact, en plus de `sound`, joué au lancement. |

Les dégâts et les soins s'affichent au moment de l'impact. Pour un sort qui fait bondir le lanceur
(Charge), l'impact suit son arrivée. Les poussées, attractions et bonds sont animés : le personnage
glisse jusqu'à sa case d'arrivée.

## Catalogue d'effets

```json
"burning": { "sheet": "assets/spellsprites/flame1_normal", "fps": 12, "loop": true, "scale": 0.35, "anchor": [0.5, 0.95] }
```

| Champ | Rôle |
|---|---|
| `sheet` | Planche au format atlas libGDX : `<nom>.png` et `<nom>.txt`, sans extension. |
| `fps` | Images par seconde (24 par défaut). |
| `loop` | Animation en boucle (effets durables, glyphes). |
| `scale` | Échelle de l'image. |
| `anchor` | Point de l'image placé sur la case, en fractions de sa largeur et de sa hauteur. Par défaut `[0.5, 0.5]` : le centre de l'image. |
| `offsetY` | Décalage vertical en pixels. Par exemple −45 pour viser le torse d'un personnage. |
| `layer` | `top` (au-dessus des personnages, par défaut) ou `ground` (au sol, sous les personnages). |
| `rotate` | Orienté dans le sens du trajet (projectiles dessinés pointe à droite). |
| `additive` | Mélange additif : l'effet éclaire ce qu'il recouvre et ses parties noires disparaissent. |
| `color`, `alpha` | Teinte `[r, g, b]` ou `[r, g, b, a]`, et opacité de 0 à 1. |

La section `events` associe un effet aux événements qui ne dépendent pas d'un sort :
- `death` : un combattant est mis hors combat ;
- `collision` : un combattant poussé heurte un obstacle ;
- `push`, `pull`, `dash` : poussée, attraction, bond ;
- `lifesteal` : vol de vie ;
- `combo` : une combinaison se déclenche (éclat doré sur la cible, avec le son `assets/sound/ui/combo.ogg`).

Le client joue aussi directement :
- `ping` et `ping_arrow` : le signal d'un coéquipier (anneau au sol et flèche) ;
- `combo_mark` : en boucle sous un combattant qui porte une marque de combinaison (état négatif :
  gelé, entravé, provoqué, brûlé), à la place du visuel `status` du sort qui l'a posée.

Ces planches sont dessinées par `tools/fx/import_fx.py`, comme celles de quatre sorts au choix : `whirlwind`
(Tourbillon), `arrow_rain` (Pluie de flèches), `trap` (Piège) et `ice_prison` (Prison de glace).

## Galerie de réglage

```
cd x64\Debug
TacticalWar.exe --fx-gallery
TacticalWar.exe --fx-gallery --fx-spell boule_de_feu --fx-map 5
```

La galerie fonctionne sans serveur. Elle rejoue chaque sort en boucle :
- le lanceur appartient à la classe du sort ; il fait face à une cible (Archer) et à un allié (Guerrier) ;
- un vrai moteur de combat produit les événements : effets durables, glyphes, poussées, dégâts
  périodiques au tour suivant ;
- avant chaque lancer, la visée est montrée comme en combat :
  - la portée du sort en bleu clair et les cases ciblables en bleu ;
  - une case non ciblable en gris, avec la raison dans la ligne d'aide ;
  - puis la cible en orange, avec la zone d'impact en rouge.

| Touche | Action |
|---|---|
| Gauche / droite | Sort précédent / suivant |
| R | Rejouer |
| F5 | Relire `effects.json`, `gamedata.json` et les planches |
| Échap | Quitter |

On peut aussi lancer les sorts de la barre à la souris : la boucle automatique s'arrête alors jusqu'au prochain R.

Lancée depuis `x64\Debug` ou `x64\Release`, la galerie lit les fichiers du **dépôt** (`..\..\assets`).
On règle donc directement les fichiers suivis par git, et F5 montre le résultat. Les nouveaux sons sont
copiés dans `x64\<configuration>` à la compilation suivante.

Pour une capture d'écran :

```
TacticalWar.exe --fx-spell eclair --screenshot eclair.png --screenshot-after 1.2
```

Le délai compte à partir de la première image affichée.

## Ajouter des animations et des sons

Les animations et les sons viennent du projet Exode (voir `assets/CREDITS.md`). La liste est dans
`tools/fx/selection.json`.

```
py tools/fx/import_fx.py --preview                  (planches réduites dans assets/spellsprites)
py -m pip install miniaudio soundfile
py tools/fx/convert_sounds.py                       (MP3 convertis en OGG dans assets/sound/spells)
py -m pip install numpy
py tools/fx/make_ui_sounds.py                       (sons d'interface synthétisés dans assets/sound/ui)
```

Options d'un effet dans `selection.json` :
- `as` : nom de la planche produite ;
- `grayscale` : niveaux de gris clairs, à teindre avec `color` dans le catalogue ;
- `step` : une image sur n ;
- `maxFrameSize` : taille maximale d'une image.

Les tests (`TacticalWarTests`, `FxDataTests.cpp`) vérifient :
- chaque effet cité par un sort existe dans le catalogue ;
- les planches et les sons sont présents ;
- deux sorts n'ont jamais la même animation.
