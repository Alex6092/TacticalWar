#include "PasswordHasher.h"

#include <Windows.h>
#include <bcrypt.h>
#include <stdexcept>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

using namespace tw;

namespace
{
	const char * PREFIX = "pbkdf2-sha256";
	const int SALT_BYTES = 16;
	const int HASH_BYTES = 32;

	std::vector<unsigned char> randomBytes(int count)
	{
		std::vector<unsigned char> bytes(count);
		if (!BCRYPT_SUCCESS(BCryptGenRandom(NULL, bytes.data(), (ULONG)bytes.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
			throw std::runtime_error("BCryptGenRandom a échoué");
		return bytes;
	}

	std::string toHex(const std::vector<unsigned char> & bytes)
	{
		static const char * digits = "0123456789abcdef";
		std::string hex;
		hex.reserve(bytes.size() * 2);
		for (unsigned char b : bytes)
		{
			hex += digits[b >> 4];
			hex += digits[b & 0x0F];
		}
		return hex;
	}

	bool fromHex(const std::string & hex, std::vector<unsigned char> & bytes)
	{
		if (hex.size() % 2 != 0)
			return false;

		bytes.clear();
		for (std::size_t i = 0; i < hex.size(); i += 2)
		{
			int value = 0;
			for (int j = 0; j < 2; j++)
			{
				char c = hex[i + j];
				value <<= 4;
				if (c >= '0' && c <= '9') value |= c - '0';
				else if (c >= 'a' && c <= 'f') value |= c - 'a' + 10;
				else if (c >= 'A' && c <= 'F') value |= c - 'A' + 10;
				else return false;
			}
			bytes.push_back((unsigned char)value);
		}
		return true;
	}

	std::vector<unsigned char> pbkdf2(const std::string & password, const std::vector<unsigned char> & salt, int iterations)
	{
		BCRYPT_ALG_HANDLE algorithm = NULL;
		if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG)))
			throw std::runtime_error("BCryptOpenAlgorithmProvider a échoué");

		std::vector<unsigned char> derived(HASH_BYTES);
		NTSTATUS status = BCryptDeriveKeyPBKDF2(
			algorithm,
			(PUCHAR)password.data(), (ULONG)password.size(),
			(PUCHAR)salt.data(), (ULONG)salt.size(),
			(ULONGLONG)iterations,
			derived.data(), (ULONG)derived.size(),
			0);

		BCryptCloseAlgorithmProvider(algorithm, 0);

		if (!BCRYPT_SUCCESS(status))
			throw std::runtime_error("BCryptDeriveKeyPBKDF2 a échoué");

		return derived;
	}

	// Comparaison en temps constant.
	bool equals(const std::vector<unsigned char> & a, const std::vector<unsigned char> & b)
	{
		if (a.size() != b.size())
			return false;

		unsigned char diff = 0;
		for (std::size_t i = 0; i < a.size(); i++)
			diff |= a[i] ^ b[i];
		return diff == 0;
	}
}

std::string PasswordHasher::hash(const std::string & password, int iterations)
{
	std::vector<unsigned char> salt = randomBytes(SALT_BYTES);
	std::vector<unsigned char> derived = pbkdf2(password, salt, iterations);
	return std::string(PREFIX) + "$" + std::to_string(iterations) + "$" + toHex(salt) + "$" + toHex(derived);
}

bool PasswordHasher::verify(const std::string & password, const std::string & storedHash)
{
	std::vector<std::string> parts;
	std::size_t start = 0;
	while (true)
	{
		std::size_t end = storedHash.find('$', start);
		parts.push_back(storedHash.substr(start, end == std::string::npos ? std::string::npos : end - start));
		if (end == std::string::npos)
			break;
		start = end + 1;
	}

	if (parts.size() != 4 || parts[0] != PREFIX)
		return false;

	int iterations = std::atoi(parts[1].c_str());
	std::vector<unsigned char> salt;
	std::vector<unsigned char> expected;
	if (iterations <= 0 || !fromHex(parts[2], salt) || !fromHex(parts[3], expected) || expected.empty())
		return false;

	return equals(pbkdf2(password, salt, iterations), expected);
}

std::string PasswordHasher::generatePassword(int length)
{
	static const std::string alphabet = "abcdefghjkmnpqrstuvwxyz23456789";
	// Rejette les octets au-delà du plus grand multiple de la taille de l'alphabet (pas de biais).
	const unsigned int limit = 256 - (256 % alphabet.size());

	std::string password;
	while ((int)password.size() < length)
	{
		for (unsigned char b : randomBytes(length))
		{
			if (b < limit && (int)password.size() < length)
				password += alphabet[b % alphabet.size()];
		}
	}
	return password;
}

std::string PasswordHasher::randomHex(int byteCount)
{
	return toHex(randomBytes(byteCount));
}
