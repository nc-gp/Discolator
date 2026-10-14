#include "paths.h"

#include <windows.h>
#include <iterator>

namespace paths
{
    fs::path ExePath()
    {
        wchar_t buf[2048];
        const DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)std::size(buf));
        return fs::path(std::wstring(buf, n));
    }

    fs::path ExeDir()
    {
        return ExePath().parent_path();
    }

    fs::path GamesDir()
    {
        fs::path dir = ExeDir() / L"games";
        std::error_code ec;
        fs::create_directories(dir, ec);
        return dir;
    }

    fs::path ConfigFile() { return ExeDir() / L"config.json"; }
    fs::path GameListCacheFile() { return ExeDir() / L"detectable_cache.json"; }

    std::string ToUtf8(const fs::path& p)
    {
        const std::wstring w = p.wstring();
        if (w.empty())
            return {};
        const int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string out(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), out.data(), len, nullptr, nullptr);
        return out;
    }

    fs::path FromUtf8(const std::string& s)
    {
        if (s.empty())
            return {};
        const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring w(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), len);
        return fs::path(w);
    }
}