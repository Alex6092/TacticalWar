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
