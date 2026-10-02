#include "ServerConfig.h"
#include "JsonFile.h"

using namespace tw;

nlohmann::json ServerConfig::toJson() const
{
	return {
		{ "gamePort", gamePort },
		{ "httpPort", httpPort },
		{ "dataDir", dataDir },
		{ "keepaliveIntervalSeconds", keepaliveIntervalSeconds },
		{ "keepaliveTimeoutSeconds", keepaliveTimeoutSeconds },
		{ "classSelectionSeconds", classSelectionSeconds },
		{ "admin", {
			{ "login", admin.login },
			{ "passwordHash", admin.passwordHash },
			{ "allowedFrom", admin.allowedFrom }
		} }
	};
}

ServerConfig ServerConfig::fromJson(const nlohmann::json & json)
{
	ServerConfig config;
	config.gamePort = json.value("gamePort", config.gamePort);
	config.httpPort = json.value("httpPort", config.httpPort);
	config.dataDir = json.value("dataDir", config.dataDir);
	config.keepaliveIntervalSeconds = json.value("keepaliveIntervalSeconds", config.keepaliveIntervalSeconds);
	config.keepaliveTimeoutSeconds = json.value("keepaliveTimeoutSeconds", config.keepaliveTimeoutSeconds);
	config.classSelectionSeconds = json.value("classSelectionSeconds", config.classSelectionSeconds);

	if (json.contains("admin") && json["admin"].is_object())
	{
		const nlohmann::json & admin = json["admin"];
		config.admin.login = admin.value("login", config.admin.login);
		config.admin.passwordHash = admin.value("passwordHash", config.admin.passwordHash);
		config.admin.allowedFrom = admin.value("allowedFrom", config.admin.allowedFrom);
	}

	return config;
}

ServerConfig ServerConfig::loadOrCreate(const std::string & path, std::string & log)
{
	ServerConfig config;
	nlohmann::json json;
	std::string error;

	if (store::fileExists(path))
	{
		if (store::readJsonFile(path, json, &error))
		{
			config = fromJson(json);
			log = "Configuration chargée depuis " + path;
		}
		else
		{
			// Ne pas écraser un fichier existant mais invalide : l'utilisateur doit le corriger.
			log = error + " : valeurs par défaut utilisées.";
			return config;
		}
	}
	else
	{
		log = "Fichier " + path + " créé avec les valeurs par défaut.";
	}

	// Complète le fichier avec les champs manquants.
	if (json != config.toJson())
		config.save(path, nullptr);

	return config;
}

bool ServerConfig::save(const std::string & path, std::string * error) const
{
	return store::writeJsonFileAtomic(path, toJson(), error);
}
