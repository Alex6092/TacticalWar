#pragma once

#include <map>
#include <string>
#include <vector>

#include "Team.h"

namespace tw
{
	// Mots de passe EN CLAIR des joueurs, conservés pour imprimer une fiche par équipe.
	// Ces fichiers (credentials.json et la fiche HTML) sont à supprimer après l'événement.
	class CredentialSheet
	{
	public:
		CredentialSheet(const std::string & jsonPath, const std::string & htmlPath);

		void load();
		void set(const std::string & login, const std::string & clearPassword);
		void remove(const std::string & login);
		std::string get(const std::string & login) const;

		// Écrit le fichier JSON et régénère la fiche HTML imprimable.
		bool save(const std::vector<Team> & teams, std::string * error = nullptr) const;

		const std::string & getHtmlPath() const { return htmlPath; }

		static std::string renderHtml(const std::vector<Team> & teams, const std::map<std::string, std::string> & passwords);

	private:
		std::string jsonPath;
		std::string htmlPath;
		std::map<std::string, std::string> passwords;
	};
}
