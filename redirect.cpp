// redirect.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include "redirect.h"
#include <vector>
#include <regex>
#include <cassert>
#include <tuple>

template <class T>
inline T* pointer(std::vector<T> &v)
{
	return &v.front();
}

template <class T>
inline const T* pointer(const std::vector<T> &v)
{
	return &v.front();
}

template <class T>
inline const T* pointer(const std::basic_string<T> &s)
{
	return s.c_str();
}
template <class T>
inline T* pointer(std::basic_string<T> &s)
{
	return &s.front();
}

std::tuple<std::wstring, std::wstring, std::wstring>
getConfig(LPCWSTR app, LPCWSTR lpIniFile)
{
	using namespace std;
	WCHAR buf[512] = { 0 };
	::GetPrivateProfileString(app, L"PATH", L"", buf, _countof(buf), lpIniFile);

	WCHAR pattern[512] = { 0 };
	::GetPrivateProfileString(app, L"PATTERN", L"", pattern, _countof(pattern), lpIniFile);

	WCHAR arg[512] = { 0 };
	::GetPrivateProfileString(app, L"ARG", L"", arg, _countof(arg), lpIniFile);

	return make_tuple(wstring(buf), wstring(arg), wstring(pattern));
}

std::tuple<std::wstring, std::wstring>
findApplication(LPCWSTR file, LPCWSTR lpIniFile, bool useDefault)
{
	assert(file);
	assert(lpIniFile);

	using namespace std;
	auto hWnd = GetDesktopWindow();

	auto ext = ::PathFindExtension(file);

	if (useDefault)
	{
		auto config = getConfig(ext, lpIniFile);
		return make_tuple(get<0>(config), get<1>(config));
	}

	for (int i = 0; i < 100; i++)
	{
		std::wstring app;
		app.resize(MAX_PATH);
		wnsprintfW(&app.front(), app.capacity(), L"%s.%d", ext, i);
		auto config = getConfig(app.c_str(), lpIniFile);

		const wstring& path = ref(get<0>(config));
		if (wcslen(path.c_str()) == 0)
			continue;

		try
		{
			std::wregex regex(get<2>(config).c_str(), std::regex_constants::ECMAScript | std::regex_constants::icase);
			std::wsmatch match;

			if (std::regex_search(std::wstring(file), regex))
			{
				return make_tuple(get<0>(config), get<1>(config));
			}
		}
		catch (const std::regex_error &e)
		{
			std::wstring text = L"設定に誤りがあります\n";
			::MessageBox(nullptr, text.c_str(), L"", MB_ICONEXCLAMATION);
			continue;
		}
		continue;
	}

	auto config = getConfig(ext, lpIniFile);
	return make_tuple(get<0>(config), get<1>(config));
}

std::tuple<std::wstring, std::wstring>
buildCommand(const std::tuple<std::wstring, std::wstring> &config, LPCWSTR file)
{
	using namespace std;

	std::wstring buf;
	buf.resize(MAX_PATH * 2);
	if (ExpandEnvironmentStrings(get<0>(config).c_str(), pointer(buf), buf.size()))
	{
	}
	const auto lpApplication = pointer(buf);
	auto args = get<1>(config);

	WCHAR quatedPath[MAX_PATH];
	wcscpy_s(quatedPath, file);
	::PathQuoteSpaces(quatedPath);
	if (args.length() != 0)
	{
		std::wregex re(L"%1");
		auto a = std::regex_replace(args, re, quatedPath);
		return make_tuple(lpApplication, a);
	}
	else
	{
		return make_tuple(lpApplication, quatedPath);
	}
}

void execute(const std::tuple<std::wstring, std::wstring> &config, LPCWSTR file)
{
	using namespace std;

	auto hWnd = GetDesktopWindow();
	auto cmd = buildCommand(config, file);

	std::wstring buf;
	WCHAR path[MAX_PATH];
	wcscpy_s(path, std::get<0>(cmd).c_str());
	::PathQuoteSpaces(path);
	buf.resize(MAX_PATH);

	SHELLEXECUTEINFO execInfo = { 0 };
	execInfo.cbSize = sizeof(execInfo);
	execInfo.lpFile = path;
	execInfo.lpVerb = L"open";
	execInfo.lpParameters = get<1>(cmd).c_str();
	execInfo.nShow = SW_SHOWDEFAULT;
	execInfo.fMask = SEE_MASK_WAITFORINPUTIDLE;

	if (!::ShellExecuteEx(&execInfo))
	{
		return;
	}
	//::SetForegroundWindow(execInfo.hwnd);
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	auto hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	int argc;
	auto argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
	if (argv == nullptr)
		return ::GetLastError();

	if (argc <= 1)
	{
		return 0;
	}

	TCHAR inifileName[MAX_PATH];
	wcscpy_s(inifileName, argv[0]);
	::PathRenameExtension(inifileName, L".ini");

	auto shiftKeyPressed = ::GetKeyState(VK_SHIFT) < 0;

	auto config = findApplication(::PathFindFileName(argv[1]), inifileName, shiftKeyPressed);
	if (std::get<0>(config).length() == 0)
	{
		return 0;
	}

	execute(config, argv[1]);

	::LocalFree(argv);
	return 0;
}

