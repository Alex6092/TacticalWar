#include "Message.h"

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
