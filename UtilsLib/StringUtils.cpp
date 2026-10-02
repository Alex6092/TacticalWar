#include "pch.h"
#include "StringUtils.h"



StringUtils::StringUtils()
{
}


StringUtils::~StringUtils()
{
}

bool StringUtils::startsWith(std::string mainStr, std::string toMatch)
{
	// std::string::find retourne 0 si toMatch est trouvé au début du message
	if (mainStr.find(toMatch) == 0)
		return true;
	else
		return false;
}
//cette methode permet de découper une trame
const std::vector<std::string> StringUtils::explode(const std::string& msg, const char& c)
{
	std::string buff = "";
	std::vector<std::string> v;
	for (int i = 0; i < msg.length(); i++)
	{
		char compare = msg[i];
		if (compare != c) buff += compare;
		else if (compare == c && buff != "")
		{
			v.push_back(buff);
			buff = "";
		}
	}
	if (buff != "")
		v.push_back(buff);

	return v;
}