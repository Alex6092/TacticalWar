#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace tw
{
	// Progression d'un joueur (par login), conservée d'une connexion et d'un événement à l'autre :
	// hauts faits obtenus au moins une fois, victoires, titres de MVP, énigmes réussies (signalées par
	// son client) et dernière apparence choisie. Elle débloque des apparences (BattleEngineLib,
	// Appearances.h).
	struct PlayerProfile
	{
		std::set<std::string> achievements;
		int wins = 0;
		int mvp = 0;
		std::set<std::string> puzzles;
		std::string appearance;
	};

	// Fichier data/profiles.json : écrit à chaque changement (écriture atomique).
	class ProfileStore
	{
	public:
		explicit ProfileStore(const std::string & path);

		// Fichier absent : aucun profil (pas une erreur).
		bool load(std::string * error = nullptr);

		// Profil d'un joueur (vide s'il n'a encore rien fait). Le login est comparé sans la casse.
		PlayerProfile get(const std::string & login) const;

		// Bilan d'un combat : hauts faits du combattant, victoire de son équipe, titre de MVP.
		void recordBattle(const std::string & login, const std::vector<std::string> & badges, bool won, bool mvp);
		// Énigmes réussies, signalées par le client (identifiants courts). Retourne true si une est nouvelle.
		bool addPuzzles(const std::string & login, const std::vector<std::string> & puzzles);
		void setAppearance(const std::string & login, const std::string & appearance);

	private:
		bool save();
		static std::string key(const std::string & login);

		std::string path;
		std::map<std::string, PlayerProfile> profiles;
	};
}
