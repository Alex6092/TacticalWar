#pragma once

#include <map>
#include <string>
#include <vector>

#include "Team.h"

namespace tw
{
	// Équipes et comptes joueurs, persistés dans un fichier JSON (data/teams.json).
	// Toutes les opérations de modification retournent un message d'erreur (vide si succès)
	// et ne modifient rien en cas d'erreur. L'appelant sauvegarde avec save().
	class TeamStore
	{
	public:
		explicit TeamStore(const std::string & filePath);

		// Fichier absent : le store est vide et load() réussit.
		bool load(std::string & error);
		bool save(std::string * error = nullptr) const;
		bool fileExists() const;

		const std::vector<Team> & getTeams() const { return teams; }
		const Team * findTeam(int id) const;
		// Recherche insensible à la casse.
		const Team * findTeamByLogin(const std::string & login, int * playerIndex = nullptr) const;

		// clearPasswords reçoit, pour chaque mot de passe défini ou généré, login -> mot de passe en clair.
		std::string createTeam(const TeamInput & input, int & newTeamId, std::map<std::string, std::string> & clearPasswords);
		std::string updateTeam(int teamId, const TeamInput & input, std::map<std::string, std::string> & clearPasswords);
		std::string resetPassword(const std::string & login, std::string & newClearPassword);
		std::string setActive(int teamId, bool active);
		std::string deleteTeam(int teamId);

		// Retourne le login tel qu'enregistré (casse d'origine) dans canonicalLogin.
		bool authenticate(const std::string & login, const std::string & password, std::string * canonicalLogin = nullptr, int * teamId = nullptr) const;

		// Règles de validation, exposées pour les tests et l'interface.
		static std::string validateLogin(const std::string & login);
		static std::string validatePassword(const std::string & password);

	private:
		std::string validate(const TeamInput & input, int ignoredTeamId) const;
		Team * findTeamMutable(int id);

		std::string filePath;
		std::vector<Team> teams;
		int nextId;
	};
}
