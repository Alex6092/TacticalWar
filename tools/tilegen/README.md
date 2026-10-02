# Générateur de tuiles

Crée des variantes des tuiles peintes à la main (`assets/tiles/*.png`) : sols, rochers, arbres,
buisson, cailloux, eau profonde, lave… Les tuiles générées sont dans `assets/tiles/generated/`
et sont ajoutées au jeu de tuiles `assets/tiles/tileset.json`, utilisé par le jeu, le serveur et
l'éditeur de cartes.

## Utilisation

Depuis la racine du dépôt (Python 3 avec Pillow et numpy) :

```
py tools/tilegen/tilegen.py              # toutes les tuiles
py tools/tilegen/tilegen.py --preview    # + planche d'aperçu tools/tilegen/preview.png
py tools/tilegen/tilegen.py --only sand,lava
```

Le résultat est reproductible (tirages aléatoires seedés). Après une génération, cliquer sur
« Recharger les tuiles » dans l'éditeur.

## Recettes (`recipes.json`)

Chaque tuile part d'une **base** (peinture d'origine) et applique des **opérations** :

| Opération | Effet | Paramètres |
|---|---|---|
| `recolor` | Recoloration HSV d'une zone | `mask`, `hue` (teinte moyenne visée, en degrés), `hueShift`, `sat` et `val` (multiplicateurs), `valAdd`, `lift` (éclaircit vers le blanc) |
| `noise` | Variation de luminosité (bruit lisse) | `mask`, `amount`, `scale` (taille des taches, en pixels du jeu) |
| `highlight` | Couvre de neige les parties claires d'une zone | `mask`, `threshold`, `amount`, `color` |
| `flagstones` | Dalles : joints sur la face supérieure | `count`, `joint`, `darkness`, `variation` |
| `overlay` | Ajoute un élément d'une autre base, réduit | `from`, `mask`, `scale`, `at` (positions par rapport au centre de la case, en pixels du jeu), `tint` (recoloration de l'élément) |
| `mirror` | Symétrie horizontale | |

Zones (`mask`) : `top` (surface du bloc), `sides` (flancs de terre), `object` (rocher),
`leaves` (feuillage), `trunk` (tronc), `water` (eau), `all`. Elles sont calculées sur les couleurs
d'origine de la base, à partir de la teinte des pixels et de leur position par rapport au centre
de la case.

Les champs `name`, `category`, `group`, `shader`, `walkable` et `blocksLineOfSight` d'une recette
ne sont écrits dans `tileset.json` que s'ils n'y sont pas déjà : une retouche faite à la main
dans le jeu de tuiles est conservée. La texture et l'ancre sont toujours mises à jour.

## Catégories et règles de jeu

| Catégorie | Praticable | Bloque la vue | Exemples |
|---|---|---|---|
| `ground` | oui | non | herbe, terre, sable, neige, dalles |
| `obstacle` | non | oui | rochers, arbres |
| `liquid` | non | non | eau, lave |
| `empty` | non | non | vide (rien n'est dessiné) |

Une tuile peut changer ces règles (`walkable`, `blocksLineOfSight`) : le buisson est un obstacle
qui ne bloque pas la vue.
