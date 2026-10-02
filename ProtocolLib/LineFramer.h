#pragma once

#include <cstddef>
#include <string>

namespace tw
{
	namespace protocol
	{
		// Accumule les octets reçus d'une connexion et en extrait les lignes complètes
		// (terminées par '\n'). Le '\r' final éventuel est retiré.
		class LineFramer
		{
		public:
			static const std::size_t DEFAULT_MAX_LINE_LENGTH = 1024 * 1024;

			explicit LineFramer(std::size_t maxLineLength = DEFAULT_MAX_LINE_LENGTH);

			// Ajoute des octets reçus. Retourne false si une ligne dépasse la taille
			// maximale : la connexion doit alors être fermée.
			bool feed(const char * data, std::size_t length);

			// Extrait la prochaine ligne complète. Retourne false s'il n'y en a pas.
			bool nextLine(std::string & line);

			void clear();

			std::size_t pendingBytes() const { return buffer.size() - readOffset; }

		private:
			std::string buffer;
			std::size_t readOffset;
			std::size_t scanOffset;
			std::size_t maxLineLength;
		};
	}
}
