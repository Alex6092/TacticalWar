#pragma once

#include <string>

namespace tw
{
	// Hachage des mots de passe avec PBKDF2-HMAC-SHA256 (API Windows CNG, aucune dépendance).
	// Format stocké : "pbkdf2-sha256$<itérations>$<sel hex>$<empreinte hex>".
	class PasswordHasher
	{
	public:
		static const int DEFAULT_ITERATIONS = 10000;

		static std::string hash(const std::string & password, int iterations = DEFAULT_ITERATIONS);

		// Vérifie un mot de passe contre une empreinte. Une empreinte vide ou mal formée ne
		// correspond à aucun mot de passe.
		static bool verify(const std::string & password, const std::string & storedHash);

		// Génère un mot de passe facile à lire et à taper : minuscules et chiffres,
		// sans les caractères ambigus (0/o, 1/l/i).
		static std::string generatePassword(int length = 6);

		// Octets aléatoires cryptographiquement sûrs, en hexadécimal.
		static std::string randomHex(int byteCount);
	};
}
