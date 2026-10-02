#include <doctest.h>

#include <LineFramer.h>
#include <Message.h>
#include <Opcodes.h>

using namespace tw::protocol;

TEST_CASE("LineFramer extracts every complete line of a packet")
{
	LineFramer framer;
	std::string packet = "HG\nTL\nML";
	REQUIRE(framer.feed(packet.data(), packet.size()));

	std::string line;
	REQUIRE(framer.nextLine(line));
	CHECK(line == "HG");
	REQUIRE(framer.nextLine(line));
	CHECK(line == "TL");
	CHECK_FALSE(framer.nextLine(line));

	// The end of the third message arrives in another packet.
	REQUIRE(framer.feed("\n", 1));
	REQUIRE(framer.nextLine(line));
	CHECK(line == "ML");
	CHECK(framer.pendingBytes() == 0);
}

TEST_CASE("LineFramer handles messages split byte by byte and CRLF endings")
{
	LineFramer framer;
	std::string data = "CL1;2;3\r\nCt\n";
	std::vector<std::string> lines;
	std::string line;

	for (char c : data)
	{
		REQUIRE(framer.feed(&c, 1));
		while (framer.nextLine(line))
			lines.push_back(line);
	}

	REQUIRE(lines.size() == 2);
	CHECK(lines[0] == "CL1;2;3");
	CHECK(lines[1] == "Ct");
}

TEST_CASE("LineFramer keeps empty lines and rejects oversized lines")
{
	LineFramer framer(16);
	std::string line;

	REQUIRE(framer.feed("\n", 1));
	REQUIRE(framer.nextLine(line));
	CHECK(line.empty());

	std::string tooLong(32, 'x');
	CHECK_FALSE(framer.feed(tooLong.data(), tooLong.size()));
}

TEST_CASE("LineFramer compacts its buffer on long sessions")
{
	LineFramer framer;
	std::string message = std::string(1000, 'a') + "\n";
	std::string line;

	for (int i = 0; i < 1000; i++)
	{
		REQUIRE(framer.feed(message.data(), message.size()));
		REQUIRE(framer.nextLine(line));
		CHECK(line.size() == 1000);
	}
	CHECK(framer.pendingBytes() == 0);
}

TEST_CASE("Message round-trips JSON payloads in UTF-8")
{
	nlohmann::json body = { { "name", u8"Équipe Ça déchire ; , ^ |" }, { "id", 3 } };
	std::string encoded = Message::encode("TC", body);
	REQUIRE(encoded.back() == '\n');

	Message message;
	REQUIRE(Message::decode(encoded.substr(0, encoded.size() - 1), message));
	CHECK(message.op == "TC");
	REQUIRE(message.hasJsonPayload());

	nlohmann::json decoded;
	REQUIRE(message.parseJson(decoded));
	CHECK(decoded["name"].get<std::string>() == body["name"].get<std::string>());
	CHECK(decoded["id"] == 3);
}

TEST_CASE("Message keeps legacy text payloads untouched")
{
	Message message;
	REQUIRE(Message::decode("CL4;7;8", message));
	CHECK(message.op == "CL");
	CHECK(message.payload == "4;7;8");
	CHECK_FALSE(message.hasJsonPayload());

	nlohmann::json json;
	CHECK_FALSE(message.parseJson(json));
	CHECK_FALSE(Message::decode("H", message));
}

TEST_CASE("Invalid UTF-8 never makes JSON serialization throw")
{
	nlohmann::json body = { { "name", std::string("caf\xE9") } };	// CP1252, invalid UTF-8
	std::string encoded;
	CHECK_NOTHROW(encoded = Message::encode("TL", body));
	CHECK(encoded.find("caf") != std::string::npos);
}

TEST_CASE("Opcode table has unique opcodes")
{
	const std::size_t count = sizeof(OPCODES) / sizeof(OPCODES[0]);
	for (std::size_t i = 0; i < count; i++)
	{
		CHECK(std::strlen(OPCODES[i].op) == 2);
		for (std::size_t j = i + 1; j < count; j++)
			CHECK(std::strcmp(OPCODES[i].op, OPCODES[j].op) != 0);
	}

	REQUIRE(findOpcode("CL") != nullptr);
	CHECK(findOpcode("CL")->requiredRole == Role::PLAYER);
	CHECK(findOpcode("??") == nullptr);
}
