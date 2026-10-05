#include "SystemBrowser.h"

// Windows seulement ici : ses macros (min, max…) ne gênent pas le reste du client.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

void openInBrowser(const std::string & utf8Url)
{
	if (utf8Url.empty())
		return;
	int length = MultiByteToWideChar(CP_UTF8, 0, utf8Url.c_str(), -1, nullptr, 0);
	if (length <= 0)
		return;
	std::wstring url(length, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8Url.c_str(), -1, &url[0], length);
	ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
