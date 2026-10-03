#pragma once

#include <cstring>

namespace tw
{
	namespace protocol
	{
		// Version du protocole : le client et le serveur doivent être mis à jour ensemble.
		const int PROTOCOL_VERSION = 5;

		const int DEFAULT_GAME_PORT = 12345;
		const int DEFAULT_HTTP_PORT = 8080;

		// Keepalive : le serveur envoie KEEPALIVE_PING après une période sans trafic,
		// le client répond KEEPALIVE_PONG. Ces messages ne sont jamais transmis aux écrans.
		const char * const KEEPALIVE_PING = "ZP";
		const char * const KEEPALIVE_PONG = "ZQ";

		enum class Direction
		{
			CLIENT_TO_SERVER,
			SERVER_TO_CLIENT,
			BOTH
		};

		// Rôle minimal requis pour qu'un client puisse envoyer le message au serveur.
		enum class Role
		{
			ANY,		// Y compris un client non authentifié
			SPECTATOR,	// Spectateur, joueur ou admin
			PLAYER,		// Joueur authentifié
			ADMIN
		};

		struct OpcodeInfo
		{
			const char * op;
			Direction direction;
			Role requiredRole;
			const char * description;
		};

		// Table de référence des opcodes. Elle sert à documenter le protocole et à
		// vérifier les droits côté serveur.
		const OpcodeInfo OPCODES[] = {
			// Système
			{ "ZP", Direction::SERVER_TO_CLIENT, Role::ANY, "Keepalive (ping)" },
			{ "ZQ", Direction::CLIENT_TO_SERVER, Role::ANY, "Keepalive (pong)" },

			// Connexion / changement d'écran
			{ "HG", Direction::BOTH, Role::ANY, "C->S : login;password (vide = spectateur). S->C : entrer en combat sur la carte <id>" },
			{ "HC", Direction::SERVER_TO_CLIENT, Role::ANY, "Aller à la sélection de classe : HC{talents: nombre de talents de tournoi à choisir, ban: secondes de bannissement restantes (absent : pas de bannissement en cours)}" },
			{ "HS", Direction::SERVER_TO_CLIENT, Role::ANY, "Aller au mode spectateur" },
			{ "HW", Direction::SERVER_TO_CLIENT, Role::ANY, "Aller à l'attente de match" },
			{ "HK", Direction::SERVER_TO_CLIENT, Role::ANY, "Identifiants refusés" },
			{ "AD", Direction::SERVER_TO_CLIENT, Role::ANY, "Aller à l'écran admin" },

			// Listes
			{ "ML", Direction::BOTH, Role::SPECTATOR, "Matchs en cours" },
			{ "TL", Direction::BOTH, Role::ADMIN, "Liste des équipes (S->C : JSON {teams, readOnly, credentialSheet})" },
			{ "MC", Direction::BOTH, Role::ADMIN, "Matchs planifiés et en cours" },
			{ "MF", Direction::SERVER_TO_CLIENT, Role::ADMIN, "Matchs terminés" },

			// Administration des équipes (contenu JSON)
			{ "TC", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Créer une équipe {name, tag, seed, players:[{login, displayName, password?}]}" },
			{ "TU", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Modifier une équipe {id, name, tag, seed, players}" },
			{ "TD", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Supprimer une équipe {id} (désactivée si elle a déjà joué)" },
			{ "TA", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Activer / désactiver une équipe {id, active}" },
			{ "TK", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Générer un nouveau mot de passe {login}" },
			{ "TI", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Importer assets/equipe.txt" },
			{ "TR", Direction::SERVER_TO_CLIENT, Role::ADMIN, "Résultat d'une opération sur les équipes {ok, message, passwords}" },

			// Administration des tournois (contenu JSON)
			{ "UL", Direction::BOTH, Role::ADMIN, "Liste des tournois (S->C : {tournaments})" },
			{ "UG", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Suivre un tournoi {id} (le serveur envoie UT à chaque changement)" },
			{ "UT", Direction::SERVER_TO_CLIENT, Role::ADMIN, "État complet d'un tournoi (matchs, libellés, classements)" },
			{ "UC", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Créer un tournoi {name, settings, teams}" },
			{ "UE", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Modifier un tournoi non démarré {id, name, settings, teams}" },
			{ "UB", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Démarrer un tournoi {id}" },
			{ "UP", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Suspendre / reprendre le lancement des matchs {id, paused}" },
			{ "UD", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Supprimer un tournoi {id}" },
			{ "UF", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Imposer un vainqueur {id, match, winner, cascade}" },
			{ "US", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Arrêter un combat en cours (décision aux PV) {id, match}" },
			{ "UX", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Rejouer un match en cours {id, match}" },
			{ "UA", Direction::SERVER_TO_CLIENT, Role::ADMIN, "Résultat d'une opération sur un tournoi {ok, message, id}" },

			// Mode spectateur (contenu JSON)
			{ "SL", Direction::BOTH, Role::SPECTATOR, "Combats en cours (S->C : {sessions})" },
			{ "SW", Direction::CLIENT_TO_SERVER, Role::SPECTATOR, "Regarder un combat {session} (réponse : HG puis BI, puis le flux BV)" },
			{ "SU", Direction::CLIENT_TO_SERVER, Role::SPECTATOR, "Arrêter de regarder (combat ou rediffusion)" },
			{ "RL", Direction::BOTH, Role::SPECTATOR, "Rediffusions des combats terminés (S->C : {replays})" },
			{ "RP", Direction::CLIENT_TO_SERVER, Role::SPECTATOR, "Revoir un combat {id} (réponse : MP, HG, BI puis les lots BV au rythme du combat)" },

			// Création de match manuelle
			{ "CM", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Créer un match : nom;equipe1;equipe2" },
			{ "CO", Direction::SERVER_TO_CLIENT, Role::ADMIN, "Match créé" },
			{ "CN", Direction::SERVER_TO_CLIENT, Role::ADMIN, "Une équipe est déjà occupée" },
			{ "CF", Direction::SERVER_TO_CLIENT, Role::ADMIN, "Même équipe deux fois" },

			// Choix de classe
			{ "PC", Direction::CLIENT_TO_SERVER, Role::PLAYER, "Choisir une classe, ses sorts et ses talents : PC{class, spells:[4 indices dans les sorts de la classe], talents:[identifiants]} (PC<classId> : sorts par défaut)" },
			{ "PO", Direction::SERVER_TO_CLIENT, Role::PLAYER, "Classe verrouillée : PO<classId>" },
			{ "PB", Direction::CLIENT_TO_SERVER, Role::PLAYER, "Bannir une classe pour l'équipe adverse : PB{class} (le premier choix de l'équipe compte)" },
			{ "BB", Direction::SERVER_TO_CLIENT, Role::PLAYER, "Bannissement : BB{banned: classe interdite par son équipe (0 : aucune), done: phase terminée, forbidden: classe interdite par l'adversaire (à la fin)}" },
			{ "PS", Direction::SERVER_TO_CLIENT, Role::ANY, "Statut de connexion des joueurs" },
			{ "GD", Direction::SERVER_TO_CLIENT, Role::ANY, "Données de jeu (contenu de assets/data/gamedata.json)" },
			{ "MP", Direction::SERVER_TO_CLIENT, Role::ANY, "Carte du combat (format v2 avec les règles des tuiles), envoyée avant HG" },

			// Combat (contenu JSON). Le serveur fait autorité : il valide et diffuse des événements.
			{ "BI", Direction::SERVER_TO_CLIENT, Role::ANY, "État complet du combat {seq, you, phase, fighters...}" },
			{ "BV", Direction::SERVER_TO_CLIENT, Role::ANY, "Lot d'événements de combat {seq, ev:[...]}" },
			{ "ER", Direction::SERVER_TO_CLIENT, Role::ANY, "Action refusée {op, message}" },
			{ "BR", Direction::CLIENT_TO_SERVER, Role::SPECTATOR, "Demande de l'état complet (resynchronisation)" },
			{ "CP", Direction::CLIENT_TO_SERVER, Role::PLAYER, "Placement {x, y}" },
			{ "Cs", Direction::CLIENT_TO_SERVER, Role::PLAYER, "Prêt {ready}" },
			{ "Cm", Direction::CLIENT_TO_SERVER, Role::PLAYER, "Déplacement {path:[[x,y]...]} (sans la cellule de départ)" },
			{ "CL", Direction::CLIENT_TO_SERVER, Role::PLAYER, "Lancer de sort {slot (0 à 3), x, y}" },
			{ "Ct", Direction::CLIENT_TO_SERVER, Role::PLAYER, "Fin de tour" },
			{ "CE", Direction::CLIENT_TO_SERVER, Role::PLAYER, "Émote prédéfinie {id} (liste dans BattleEngineLib/Emotes.h), diffusée par l'événement emote" },
			{ "CG", Direction::CLIENT_TO_SERVER, Role::PLAYER, "Signal à son équipe sur une case {x, y} (3 au plus toutes les 5 s)" },
			{ "BG", Direction::SERVER_TO_CLIENT, Role::ANY, "Signal d'un coéquipier {f, x, y} : jamais envoyé aux adversaires ni aux spectateurs" },
		};

		inline const OpcodeInfo * findOpcode(const char * op)
		{
			for (const OpcodeInfo & info : OPCODES)
			{
				if (std::strncmp(info.op, op, 2) == 0)
					return &info;
			}
			return nullptr;
		}
	}
}
