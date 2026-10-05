#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace tw
{
	namespace protocol
	{
		// Un message du protocole : une ligne composée d'un opcode de 2 caractères suivi
		// d'un contenu. Le contenu est soit un objet/tableau JSON compact (UTF-8), soit,
		// pour les messages historiques, un texte libre avec séparateurs.
		struct Message
		{
			std::string op;
			std::string payload;

			bool hasJsonPayload() const;

			// Analyse le contenu JSON. Retourne false si le contenu n'est pas du JSON valide.
			bool parseJson(nlohmann::json & out) const;

			// Décode une ligne reçue (sans '\n'). Retourne false si la ligne est trop courte.
			static bool decode(const std::string & line, Message & out);

			// Encode un message prêt à être envoyé (terminé par '\n').
			static std::string encode(const std::string & op, const nlohmann::json & body);
			static std::string encodeRaw(const std::string & op, const std::string & rawPayload = "");
		};

		// Sérialise du JSON en remplaçant les séquences UTF-8 invalides au lieu de lever une exception.
		std::string dumpJson(const nlohmann::json & value);

		// Connexion HG<login>;<mot de passe>;v<version> : retire le champ de version de "payload" et
		// retourne la version (0 si absente : client d'avant la version 7, accepté). Le champ n'est lu
		// qu'en troisième position (deux ';'), pour ne pas confondre un mot de passe avec une version.
		int takeLoginVersion(std::string & payload);
		// Contenu de HG envoyé par le client et le bot (identifiants vides : spectateur).
		std::string loginPayload(const std::string & login, const std::string & password);
	}
}
