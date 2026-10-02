#include "LineFramer.h"

using namespace tw::protocol;

LineFramer::LineFramer(std::size_t maxLineLength)
	: readOffset(0), scanOffset(0), maxLineLength(maxLineLength)
{
}

bool LineFramer::feed(const char * data, std::size_t length)
{
	buffer.append(data, length);

	// Une ligne trop longue sans '\n' : protection contre un client défaillant.
	if (buffer.find('\n', scanOffset) == std::string::npos && pendingBytes() > maxLineLength)
		return false;

	return true;
}

bool LineFramer::nextLine(std::string & line)
{
	std::size_t end = buffer.find('\n', scanOffset > readOffset ? scanOffset : readOffset);
	if (end == std::string::npos)
	{
		scanOffset = buffer.size();
		return false;
	}

	std::size_t lineEnd = end;
	if (lineEnd > readOffset && buffer[lineEnd - 1] == '\r')
		lineEnd--;

	line.assign(buffer, readOffset, lineEnd - readOffset);
	readOffset = end + 1;
	scanOffset = readOffset;

	// Compacte le tampon une fois que la majeure partie a été consommée.
	if (readOffset == buffer.size())
	{
		buffer.clear();
		readOffset = 0;
		scanOffset = 0;
	}
	else if (readOffset > 64 * 1024 && readOffset > buffer.size() / 2)
	{
		buffer.erase(0, readOffset);
		scanOffset -= readOffset;
		readOffset = 0;
	}

	return true;
}

void LineFramer::clear()
{
	buffer.clear();
	readOffset = 0;
	scanOffset = 0;
}
