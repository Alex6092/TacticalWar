#include "Message.h"
#include "Opcodes.h"

#include <algorithm>
#include <cctype>

using namespace tw::protocol;

bool Message::hasJsonPayload() const
{
	return !payload.empty() && (payload[0] == '{' || payload[0] == '[');
}

bool Message::parseJson(nlohmann::json & out) const
{
	if (!hasJsonPayload())
		return false;

	out = nlohmann::json::parse(payload, nullptr, false);
	return !out.is_discarded();
}

bool Message::decode(const std::string & line, Message & out)
{
	if (line.size() < 2)
		return false;

	out.op = line.substr(0, 2);
	out.payload = line.substr(2);
	return true;
}

std::string Message::encode(const std::string & op, const nlohmann::json & body)
{
	return op + dumpJson(body) + "\n";
}

std::string Message::encodeRaw(const std::string & op, const std::string & rawPayload)
{
	return op + rawPayload + "\n";
}

std::string tw::protocol::dumpJson(const nlohmann::json & value)
{
	return value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

int tw::protocol::takeLoginVersion(std::string & payload)
{
	if (std::count(payload.begin(), payload.end(), ';') < 2)
		return 0;
	std::size_t last = payload.rfind(';');
	std::string field = payload.substr(last + 1);
	if (field.size() < 2 || field[0] != 'v' || !std::all_of(field.begin() + 1, field.end(), [](char c) { return std::isdigit((unsigned char)c) != 0; }))
		return 0;
	payload.erase(last);
	return std::atoi(field.c_str() + 1);
}

std::string tw::protocol::loginPayload(const std::string & login, const std::string & password)
{
	return login + ";" + password + ";v" + std::to_string(PROTOCOL_VERSION);
}
