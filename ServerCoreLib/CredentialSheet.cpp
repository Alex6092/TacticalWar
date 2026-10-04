#include "CredentialSheet.h"
#include "JsonFile.h"

using namespace tw;

namespace
{
	std::string escapeHtml(const std::string & text)
	{
		std::string escaped;
		for (char c : text)
		{
			switch (c)
			{
			case '&': escaped += "&amp;"; break;
			case '<': escaped += "&lt;"; break;
			case '>': escaped += "&gt;"; break;
			case '"': escaped += "&quot;"; break;
			default: escaped += c;
			}
		}
		return escaped;
	}
}

CredentialSheet::CredentialSheet(const std::string & jsonPath, const std::string & htmlPath)
	: jsonPath(jsonPath), htmlPath(htmlPath)
{
}

void CredentialSheet::load()
{
	passwords.clear();

	nlohmann::json json;
	if (!store::fileExists(jsonPath) || !store::readJsonFile(jsonPath, json))
		return;

	if (json.contains("passwords") && json["passwords"].is_object())
	{
		for (auto it = json["passwords"].begin(); it != json["passwords"].end(); it++)
		{
			if (it.value().is_string())
				passwords[it.key()] = it.value().get<std::string>();
		}
	}
}

void CredentialSheet::set(const std::string & login, const std::string & clearPassword)
{
	passwords[login] = clearPassword;
}

void CredentialSheet::remove(const std::string & login)
{
	passwords.erase(login);
}

std::string CredentialSheet::get(const std::string & login) const
{
	std::map<std::string, std::string>::const_iterator it = passwords.find(login);
	return it == passwords.end() ? std::string() : it->second;
}

bool CredentialSheet::save(const std::vector<Team> & teams, std::string * error) const
{
	// Seuls les comptes encore existants sont conservés.
	std::map<std::string, std::string> current;
	for (const Team & team : teams)
	{
		for (const PlayerAccount & player : team.players)
		{
			std::map<std::string, std::string>::const_iterator it = passwords.find(player.login);
			if (!player.login.empty() && it != passwords.end())
				current[it->first] = it->second;
		}
	}

	nlohmann::json json = {
		{ "warning", "Mots de passe en clair : supprimer ce fichier après l'événement." },
		{ "passwords", current }
	};

	if (!store::writeJsonFileAtomic(jsonPath, json, error))
		return false;

	return store::writeTextFileAtomic(htmlPath, renderHtml(teams, current), error);
}

std::string CredentialSheet::renderHtml(const std::vector<Team> & teams, const std::map<std::string, std::string> & passwords)
{
	std::string html =
		"<!doctype html>\n<html lang=\"fr\">\n<head>\n<meta charset=\"utf-8\">\n"
		"<title>Tactical War - fiches des équipes</title>\n"
		"<style>\n"
		"body { font-family: Segoe UI, Arial, sans-serif; margin: 24px; color: #222; }\n"
		".warning { border: 2px solid #c0392b; color: #c0392b; padding: 8px 12px; margin-bottom: 24px; }\n"
		".card { border: 2px dashed #555; border-radius: 8px; padding: 16px 20px; margin: 0 0 20px 0; page-break-inside: avoid; }\n"
		".card h2 { margin: 0 0 12px 0; }\n"
		".tag { color: #777; font-size: 0.8em; }\n"
		"table { border-collapse: collapse; width: 100%; font-size: 1.2em; }\n"
		"td, th { text-align: left; padding: 6px 8px; border-bottom: 1px solid #ddd; }\n"
		".pwd { font-family: Consolas, monospace; font-size: 1.3em; letter-spacing: 2px; }\n"
		"@media print { .warning { display: none; } body { margin: 0; } }\n"
		"</style>\n</head>\n<body>\n"
		"<p class=\"warning\">Ce document contient les mots de passe en clair. Supprimez-le (ainsi que "
		"credentials.json) après l'événement.</p>\n";

	for (const Team & team : teams)
	{
		if (!team.active)
			continue;

		html += "<div class=\"card\">\n<h2>" + escapeHtml(team.name);
		if (!team.tag.empty())
			html += " <span class=\"tag\">[" + escapeHtml(team.tag) + "]</span>";
		html += "</h2>\n<table>\n<tr><th>Joueur</th><th>Login</th><th>Mot de passe</th></tr>\n";

		for (const PlayerAccount & player : team.players)
		{
			if (player.login.empty())
				continue;	// Équipe d'un seul joueur
			std::map<std::string, std::string>::const_iterator it = passwords.find(player.login);
			std::string password = it == passwords.end() ? "(inconnu : réinitialiser)" : it->second;
			html += "<tr><td>" + escapeHtml(player.displayName) + "</td><td>" + escapeHtml(player.login)
				+ "</td><td class=\"pwd\">" + escapeHtml(password) + "</td></tr>\n";
		}

		html += "</table>\n</div>\n";
	}

	html += "</body>\n</html>\n";
	return html;
}
