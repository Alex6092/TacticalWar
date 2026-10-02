#include "ClientConfig.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace
{
	const char * CONFIG_PATH = "client.json";
}

ClientConfig & ClientConfig::get()
{
	static ClientConfig * instance = NULL;
	if (instance == NULL)
	{
		instance = new ClientConfig();
		instance->load();
	}
	return *instance;
}

void ClientConfig::load()
{
	std::ifstream file(CONFIG_PATH, std::ios::binary);
	if (!file)
	{
		save();
		return;
	}

	std::stringstream content;
	content << file.rdbuf();
	nlohmann::json json = nlohmann::json::parse(content.str(), nullptr, false);
	if (json.is_discarded() || !json.is_object())
	{
		std::cout << CONFIG_PATH << " invalide : valeurs par défaut utilisées." << std::endl;
		return;
	}

	serverHost = json.value("serverHost", serverHost);
	serverPort = json.value("serverPort", serverPort);
}

void ClientConfig::save() const
{
	nlohmann::json json = {
		{ "serverHost", serverHost },
		{ "serverPort", serverPort }
	};

	std::ofstream file(CONFIG_PATH, std::ios::binary | std::ios::trunc);
	file << json.dump(2) << "\n";
}

std::string ClientConfig::getServerAddress() const
{
	if (serverPort == 12345)
		return serverHost;
	return serverHost + ":" + std::to_string(serverPort);
}

bool ClientConfig::setServerAddress(const std::string & address)
{
	std::string host = address;
	unsigned short port = 12345;

	std::size_t colon = address.rfind(':');
	if (colon != std::string::npos)
	{
		host = address.substr(0, colon);
		int parsedPort = std::atoi(address.substr(colon + 1).c_str());
		if (parsedPort <= 0 || parsedPort > 65535)
			return false;
		port = (unsigned short)parsedPort;
	}

	if (host.empty())
		return false;

	serverHost = host;
	serverPort = port;
	return true;
}
