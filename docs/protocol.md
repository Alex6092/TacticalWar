# Protocole réseau

> Fichier généré par `py tools/gen_protocol_doc.py` à partir de `ProtocolLib/Opcodes.h` : ne pas le modifier à la main.

Le client et le serveur échangent des **lignes de texte UTF-8** sur TCP (port 12345 par défaut).
Chaque ligne commence par un **opcode de 2 caractères**, suivi du contenu du message :

- pour la plupart des messages, un objet **JSON compact** sur une seule ligne (`CL{"slot":0,"x":4,"y":7}`) ;
- pour quelques messages hérités de la première version, un texte simple (`HG<identifiant>;<mot de passe>`, `HG<numéro de carte>`).

Le serveur fait autorité : il valide chaque action et diffuse des **événements** aux valeurs absolues
(PV, bouclier, PA, PM, positions) par lots numérotés (`BV`, champ `seq`). Un client qui détecte un trou
dans la numérotation redemande l'état complet (`BR`, réponse `BI`).

Version du protocole : **8**. Une page web de suivi du tournoi est servie en HTTP sur le port 8080
(`/`, `/api/state`, `/api/events` en Server-Sent Events, `/api/health`).

**Rôle requis** : rôle minimal du client pour envoyer le message au serveur (le serveur ignore les messages
non autorisés). « Spectateur » inclut les joueurs et l'administrateur.


## Système

| Opcode | Sens | Rôle requis | Description |
|---|---|---|---|
| `ZP` | S → C | tous | Keepalive (ping) |
| `ZQ` | C → S | tous | Keepalive (pong) |

## Connexion / changement d'écran

| Opcode | Sens | Rôle requis | Description |
|---|---|---|---|
| `HG` | C ↔ S | tous | C-&gt;S : login;password;v&lt;version du protocole&gt; (identifiants vides = spectateur ; sans version : ancien client, accepté). S-&gt;C : entrer en combat sur la carte &lt;id&gt; |
| `HV` | S → C | tous | Version du client différente de celle du serveur (connexion refusée) : HV{server, client, httpPort, page : page de téléchargement du client} |
| `HC` | S → C | tous | Aller à la sélection de classe : HC{talents: nombre de talents de tournoi à choisir, ban: secondes de bannissement restantes (absent : pas de bannissement en cours), seconds: secondes restantes pour choisir (pendant le choix des classes), team: équipe du joueur (1 ou 2, couleur de l'aperçu)} |
| `HS` | S → C | tous | Aller au mode spectateur |
| `HW` | S → C | tous | Aller à l'attente de match |
| `HK` | S → C | tous | Identifiants refusés |
| `AD` | S → C | tous | Aller à l'écran admin |

## Listes

| Opcode | Sens | Rôle requis | Description |
|---|---|---|---|
| `ML` | C ↔ S | spectateur | Matchs en cours |
| `TL` | C ↔ S | admin | Liste des équipes (S-&gt;C : JSON {teams, readOnly, credentialSheet}) |
| `MC` | C ↔ S | admin | Matchs planifiés et en cours |
| `MF` | S → C | admin | Matchs terminés |

## Administration des équipes (contenu JSON)

| Opcode | Sens | Rôle requis | Description |
|---|---|---|---|
| `TC` | C → S | admin | Créer une équipe {name, tag, seed, players:[{login, displayName, password?}]} |
| `TU` | C → S | admin | Modifier une équipe {id, name, tag, seed, players} |
| `TD` | C → S | admin | Supprimer une équipe {id} (désactivée si elle a déjà joué) |
| `TA` | C → S | admin | Activer / désactiver une équipe {id, active} |
| `TK` | C → S | admin | Générer un nouveau mot de passe {login} |
| `TI` | C → S | admin | Importer assets/equipe.txt |
| `TR` | S → C | admin | Résultat d'une opération sur les équipes {ok, message, passwords} |

## Administration des tournois (contenu JSON)

| Opcode | Sens | Rôle requis | Description |
|---|---|---|---|
| `UL` | C ↔ S | admin | Liste des tournois (S-&gt;C : {tournaments}) |
| `UG` | C → S | admin | Suivre un tournoi {id} (le serveur envoie UT à chaque changement) |
| `UT` | S → C | admin | État complet d'un tournoi (matchs, libellés, classements) |
| `UC` | C → S | admin | Créer un tournoi {name, settings, teams} |
| `UE` | C → S | admin | Modifier un tournoi non démarré {id, name, settings, teams} |
| `UB` | C → S | admin | Démarrer un tournoi {id} |
| `UP` | C → S | admin | Suspendre / reprendre le lancement des matchs {id, paused} |
| `UD` | C → S | admin | Supprimer un tournoi {id} |
| `UF` | C → S | admin | Imposer un vainqueur {id, match, winner, cascade} |
| `US` | C → S | admin | Arrêter un combat en cours (décision aux PV) {id, match} |
| `UX` | C → S | admin | Rejouer un match en cours {id, match} |
| `UA` | S → C | admin | Résultat d'une opération sur un tournoi {ok, message, id} |

## Mode spectateur (contenu JSON)

| Opcode | Sens | Rôle requis | Description |
|---|---|---|---|
| `SL` | C ↔ S | spectateur | Combats en cours (S-&gt;C : {sessions}) |
| `SW` | C → S | spectateur | Regarder un combat {session} (réponse : HG puis BI, puis le flux BV) |
| `SU` | C → S | spectateur | Arrêter de regarder (combat ou rediffusion) |
| `RL` | C ↔ S | spectateur | Rediffusions des combats terminés (S-&gt;C : {replays}) |
| `RP` | C → S | spectateur | Revoir un combat {id} (réponse : MP, HG, BI puis les lots BV au rythme du combat). Avec {id, from, to} : seulement l'extrait (indices des lots, temps fort), suivi de RE |
| `RE` | S → C | spectateur | Fin de l'extrait demandé par RP{id, from, to} : RE{} |
| `HL` | C ↔ S | spectateur | Temps forts des dernières rediffusions. C-&gt;S : HL{} ; S-&gt;C : HL{highlights:[{replay, match, title, kind, score, from, to}]}, les mieux notés d'abord |

## Création de match manuelle

| Opcode | Sens | Rôle requis | Description |
|---|---|---|---|
| `CM` | C → S | admin | Créer un match : nom;equipe1;equipe2 |
| `CO` | S → C | admin | Match créé |
| `CN` | S → C | admin | Une équipe est déjà occupée |
| `FL` | C ↔ S | admin | Matchs amicaux (hors tournoi). C-&gt;S : FL{} ; S-&gt;C : FL{matches:[{id, name, teamA:{id, name}, teamB, map, status: planned\|playing\|finished\|cancelled, winner, session}], maps:[{id, name}]} |
| `FC` | C → S | admin | Créer un match amical : FC{name, teamA, teamB, map (0 : au hasard)} |
| `FX` | C → S | admin | Annuler un match amical prévu ou en cours : FX{id} |
| `FR` | S → C | admin | Réponse à FC ou FX : FR{ok, message} |
| `CF` | S → C | admin | Même équipe deux fois |

## Choix de classe

| Opcode | Sens | Rôle requis | Description |
|---|---|---|---|
| `PC` | C → S | joueur | Choisir une classe, ses sorts et ses talents : PC{class, spells:[4 indices dans les sorts de la classe], talents:[identifiants], appearance, teammate: true pour le coéquipier absent ou le second personnage d'un joueur seul} (PC&lt;classId&gt; : sorts par défaut). Refus : ER{op: PC, message} |
| `PO` | S → C | joueur | Classe verrouillée : PO{class, spells, talents, appearance, by: nom du coéquipier qui a choisi pendant une absence (absent : le joueur lui-même)} |
| `PV` | C → S | joueur | Brouillon de l'écran de choix : PV{class, spells, talents, appearance, teammate: true pour le coéquipier absent}. La classe est montrée au coéquipier ; le tout est retenu si le délai expire sans verrouillage |
| `PT` | S → C | joueur | État d'un coéquipier pendant le choix des classes : PT{name, class (verrouillée, 0 sinon), viewing, locked, appearance, present, standIn : second personnage d'un joueur seul dans son équipe} |
| `PB` | C → S | joueur | Bannir une classe pour l'équipe adverse : PB{class} (le premier choix de l'équipe compte) |
| `DL` | C ↔ S | joueur | Équipes à défier (match amical hors tournoi). C-&gt;S : DL{} ; S-&gt;C : DL{teams:[{id, name, online:[noms], allowed, reason}], closed: motif si aucun défi n'est possible} |
| `DD` | C → S | joueur | Défier une équipe : DD{team} |
| `DI` | S → C | joueur | Défi reçu par l'équipe du joueur : DI{from, name, seconds} |
| `DA` | C → S | joueur | Réponse à un défi : DA{from, accept} (le premier joueur de l'équipe qui répond décide) |
| `DR` | S → C | joueur | Résultat d'un défi : DR{ok, message, from, to} |
| `PZ` | C → S | joueur | Énigmes réussies sur ce poste, pour débloquer des apparences : PZ{solved:[identifiants]} |
| `PA` | S → C | joueur | Apparences du joueur : PA{unlocked:[identifiants], selected, new:[débloquées à l'instant], progress:{wins, mvp, puzzles, achievements}} |
| `BB` | S → C | joueur | Bannissement : BB{banned: classe interdite par son équipe (0 : aucune), done: phase terminée, forbidden: classe interdite par l'adversaire (à la fin), seconds: secondes restantes pour choisir (à la fin)} |
| `PS` | S → C | tous | Statut de connexion des joueurs |
| `GD` | S → C | tous | Données de jeu (contenu de assets/data/gamedata.json) |
| `MP` | S → C | tous | Carte du combat (format v2 avec les règles des tuiles), envoyée avant HG |

## Combat (contenu JSON). Le serveur fait autorité : il valide et diffuse des événements.

| Opcode | Sens | Rôle requis | Description |
|---|---|---|---|
| `BI` | S → C | tous | État complet du combat {seq, you, phase, fighters...} |
| `BV` | S → C | tous | Lot d'événements de combat {seq, ev:[...]} |
| `ER` | S → C | tous | Action refusée {op, message} |
| `BR` | C → S | spectateur | Demande de l'état complet (resynchronisation) |
| `CP` | C → S | joueur | Placement {x, y} |
| `Cs` | C → S | joueur | Prêt {ready} |
| `Cm` | C → S | joueur | Déplacement {path:[[x,y]...]} (sans la cellule de départ) |
| `CL` | C → S | joueur | Lancer de sort {slot (0 à 3), x, y} |
| `Ct` | C → S | joueur | Fin de tour |
| `CE` | C → S | joueur | Émote prédéfinie {id} (liste dans BattleEngineLib/Emotes.h), diffusée par l'événement emote |
| `CG` | C → S | joueur | Signal à son équipe sur une case {x, y, kind : 0 ici, 1 attaquez, 2 repli, 3 danger} (3 au plus toutes les 5 s) |
| `BG` | S → C | tous | Signal d'un coéquipier {f, x, y, kind} : jamais envoyé aux adversaires ni aux spectateurs |
