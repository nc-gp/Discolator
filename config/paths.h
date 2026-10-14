#pragma once

#include <filesystem>
#include <string>

namespace paths
{
    namespace fs = std::filesystem;

    fs::path ExePath();            // full path of the running exe
    fs::path ExeDir();             // folder containing the exe
    fs::path GamesDir();           // <exe folder>\games  (created if missing)
    fs::path ConfigFile();         // <exe folder>\config.json
    fs::path GameListCacheFile();  // <exe folder>\detectable_cache.json

    // UTF-8 text for showing a path in ImGui (path::string() would use the ANSI codepage).
    std::string ToUtf8(const fs::path& p);
    fs::path    FromUtf8(const std::string& s);
}