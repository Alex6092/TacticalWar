#include "JsonFile.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <Windows.h>

namespace fs = std::filesystem;

namespace
{
	fs::path toPath(const std::string & utf8Path)
	{
		return fs::u8path(utf8Path);
	}

	void setError(std::string * error, const std::string & message)
	{
		if (error != nullptr)
			*error = message;
	}

	std::string lastErrorMessage(const std::string & context)
	{
		return context + " (erreur Windows " + std::to_string(GetLastError()) + ")";
	}

	bool isTransientError(DWORD code)
	{
		// L'antivirus ou l'indexation peuvent verrouiller brièvement un fichier.
		return code == ERROR_ACCESS_DENIED || code == ERROR_SHARING_VIOLATION || code == ERROR_LOCK_VIOLATION;
	}
}

bool tw::store::readJsonFile(const std::string & path, nlohmann::json & out, std::string * error)
{
	std::ifstream file(toPath(path), std::ios::binary);
	if (!file)
	{
		setError(error, "Impossible d'ouvrir " + path);
		return false;
	}

	std::stringstream content;
	content << file.rdbuf();

	out = nlohmann::json::parse(content.str(), nullptr, false, true);
	if (out.is_discarded())
	{
		setError(error, "JSON invalide dans " + path);
		return false;
	}

	return true;
}

bool tw::store::writeTextFileAtomic(const std::string & path, const std::string & content, std::string * error)
{
	fs::path target = toPath(path);
	if (target.has_parent_path() && !ensureDirectory(target.parent_path().u8string(), error))
		return false;

	fs::path temporary = target;
	temporary += L".tmp";

	HANDLE handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle == INVALID_HANDLE_VALUE)
	{
		setError(error, lastErrorMessage("Impossible de créer " + temporary.u8string()));
		return false;
	}

	DWORD written = 0;
	bool ok = WriteFile(handle, content.data(), (DWORD)content.size(), &written, nullptr) && written == content.size();
	ok = ok && FlushFileBuffers(handle);
	CloseHandle(handle);

	if (!ok)
	{
		setError(error, lastErrorMessage("Écriture impossible dans " + temporary.u8string()));
		DeleteFileW(temporary.c_str());
		return false;
	}

	for (int attempt = 0; attempt < 10; attempt++)
	{
		if (MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			return true;

		if (!isTransientError(GetLastError()))
			break;

		Sleep(50);
	}

	setError(error, lastErrorMessage("Impossible de remplacer " + path));
	DeleteFileW(temporary.c_str());
	return false;
}

bool tw::store::writeJsonFileAtomic(const std::string & path, const nlohmann::json & value, std::string * error)
{
	std::string content = value.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
	content += "\n";
	return writeTextFileAtomic(path, content, error);
}

bool tw::store::appendLine(const std::string & path, const std::string & line, std::string * error)
{
	fs::path target = toPath(path);
	if (target.has_parent_path() && !ensureDirectory(target.parent_path().u8string(), error))
		return false;

	std::ofstream file(target, std::ios::binary | std::ios::app);
	if (!file)
	{
		setError(error, "Impossible d'ouvrir " + path);
		return false;
	}

	file << line << "\n";
	file.flush();
	return (bool)file;
}

bool tw::store::fileExists(const std::string & path)
{
	std::error_code ec;
	return fs::exists(toPath(path), ec);
}

bool tw::store::ensureDirectory(const std::string & path, std::string * error)
{
	if (path.empty())
		return true;

	std::error_code ec;
	fs::create_directories(toPath(path), ec);
	if (ec)
	{
		setError(error, "Impossible de créer le dossier " + path + " : " + ec.message());
		return false;
	}
	return true;
}

std::string tw::store::joinPath(const std::string & directory, const std::string & fileName)
{
	if (directory.empty())
		return fileName;
	return (toPath(directory) / toPath(fileName)).u8string();
}
