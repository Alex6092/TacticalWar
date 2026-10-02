#pragma once

#include <cstring>

namespace tw
{
	namespace protocol
	{
		// Version du protocole : le client et le serveur doivent être mis à jour ensemble.
		const int PROTOCOL_VERSION = 2;

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
			{ "HC", Direction::SERVER_TO_CLIENT, Role::ANY, "Aller à la sélection de classe" },
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

			// Création de match manuelle
			{ "CM", Direction::CLIENT_TO_SERVER, Role::ADMIN, "Créer un match : nom;equipe1;equipe2" },
			{ "CO", Direction::SERVER_TO_CLIENT, Role::ADMIN, "Match créé" },
			{ "CN", Direction::SERVER_TO_CLIENT, Role::ADMIN, "Une équipe est déjà occupée" },
			{ "CF", Direction::SERVER_TO_CLIENT, Role::ADMIN, "Même équipe deux fois" },

			// Choix de classe
			{ "PC", Direction::CLIENT_TO_SERVER, Role::PLAYER, "Choisir une classe" },
			{ "PO", Direction::SERVER_TO_CLIENT, Role::PLAYER, "Classe verrouillée" },
			{ "PS", Direction::SERVER_TO_CLIENT, Role::ANY, "Statut de connexion des joueurs" },

			// Combat
			{ "CA", Direction::SERVER_TO_CLIENT, Role::ANY, "Ajout d'un personnage" },
			{ "CS", Direction::SERVER_TO_CLIENT, Role::ANY, "Personnage contrôlé par ce client" },
			{ "BS", Direction::SERVER_TO_CLIENT, Role::ANY, "État du combat" },
			{ "Cs", Direction::BOTH, Role::PLAYER, "Prêt" },
			{ "CP", Direction::BOTH, Role::PLAYER, "Position de départ" },
			{ "Cm", Direction::BOTH, Role::PLAYER, "Déplacement" },
			{ "Ct", Direction::BOTH, Role::PLAYER, "Fin de tour / début du tour d'un personnage" },
			{ "CL", Direction::BOTH, Role::PLAYER, "Lancer de sort" },
			{ "Ca", Direction::SERVER_TO_CLIENT, Role::ANY, "PA d'un personnage" },
			{ "Cp", Direction::SERVER_TO_CLIENT, Role::ANY, "PM d'un personnage" },
			{ "BE", Direction::SERVER_TO_CLIENT, Role::ANY, "Fin de combat" },
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
