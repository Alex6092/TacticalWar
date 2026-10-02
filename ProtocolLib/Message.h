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
	}
}
