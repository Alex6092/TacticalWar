#pragma once

#include <map>
#include <string>
#include <vector>

namespace tw
{
	class TeamStore;

	// Joueur du fichier historique assets/equipe.txt ("/login,motdepasse,equipe,/...").
	struct LegacyPlayer
	{
		std::string login;
		std::string password;
		int teamNumber = 0;
	};

	// Analyse tout le contenu du fichier (l'ancien chargeur ne lisait que le premier mot).
	// Le texte est converti en UTF-8 s'il était en Windows-1252.
	std::vector<LegacyPlayer> parseLegacyTeamFile(const std::string & content);

	// Crée une équipe "Équipe N" par numéro d'équipe complet (2 joueurs). Les équipes dont un
	// login existe déjà sont ignorées. clearPasswords reçoit login -> mot de passe en clair.
	// Retourne un compte rendu lisible.
	std::string importLegacyTeams(const std::string & content, TeamStore & store, std::map<std::string, std::string> & clearPasswords);

	// Convertit un texte Windows-1252 en UTF-8 (inchangé s'il est déjà en UTF-8 valide).
	std::string legacyTextToUtf8(const std::string & text);
	bool isValidUtf8(const std::string & text);
}
