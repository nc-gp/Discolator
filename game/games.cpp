#include "games.h"
#include "../config/paths.h"

#include "json.hpp"

#include <algorithm>
#include <fstream>

using json = nlohmann::json;

std::string ToLowerAscii(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
    return s;
}

namespace
{
    // Removes characters Windows doesn't allow in file names.
    std::string SafeFolderName(const std::string& name)
    {
        std::string out;
        for (char c : name)
            if (std::string("<>:\"/\\|?*").find(c) == std::string::npos && (unsigned char)c >= 32)
                out += c;
        while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
            out.pop_back();
        while (!out.empty() && out.front() == ' ')
            out.erase(out.begin());
        return out.empty() ? "game" : out;
    }
}

std::filesystem::path Game::FolderName() const
{
    return paths::FromUtf8(SafeFolderName(name));
}

std::filesystem::path Game::RelativeExePath() const
{
    if (executables.empty())
        return {};

    std::string exe = executables.front();
    if (!exe.empty() && exe.front() == '>')   // '>' = "must match exactly", not part of the name
        exe.erase(exe.begin());

    // Entries can contain sub folders ("bf1/bf1.exe"); path handles the '/' for us.
    std::filesystem::path p = FolderName() / paths::FromUtf8(exe);
    return p.make_preferred();
}

bool Game::MatchesSearch(const std::string& lowerQuery) const
{
    return lowerQuery.empty() || nameLower.find(lowerQuery) != std::string::npos;
}

GameListResult LoadGameList(const std::filesystem::path& file)
{
    GameListResult result;

    std::error_code sizeError;
    const auto bytes = std::filesystem::file_size(file, sizeError);
    if (!sizeError)
        result.fileBytes = bytes;

    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
        result.error = "Game list not found: " + paths::ToUtf8(file);
        return result;
    }

    try
    {
        // Skip a UTF-8 byte order mark if the file has one.
        if (in.peek() == 0xEF)
        {
            char bom[3] = {};
            in.read(bom, 3);
        }

        const json root = json::parse(in);
        if (!root.is_array())
        {
            result.error = "Unexpected format: the game list must be a JSON array.";
            return result;
        }

        result.entryCount = root.size();

        for (const json& item : root)
        {
            if (!item.is_object() || !item.contains("name") || !item["name"].is_string())
                continue;
            if (!item.contains("executables") || !item["executables"].is_array())
                continue;

            Game g;
            g.name = item["name"].get<std::string>();
            for (const json& e : item["executables"])
            {
                if (e.is_string())                       // simple format: "game.exe"
                {
                    g.executables.push_back(e.get<std::string>());
                }
                else if (e.is_object() && e.contains("name") && e["name"].is_string())
                {
                    // Discord's raw format: {"name": "game.exe", "os": "win32", ...}
                    const std::string os = e.value("os", std::string("win32"));
                    if (os == "win32")
                        g.executables.push_back(e["name"].get<std::string>());
                }
            }
            if (g.name.empty() || g.executables.empty())
                continue;

            g.nameLower = ToLowerAscii(g.name);
            result.games.push_back(std::move(g));
        }

        if (result.games.empty())
        {
            std::string sample = root.empty() ? "(file is an empty list)" : root.front().dump();
            if (sample.size() > 160)
                sample = sample.substr(0, 160) + "...";
            result.error = "No usable games found in " + std::to_string(root.size()) +
                " entries. First entry: " + sample;
        }
    }
    catch (const std::exception& e)
    {
        result.games.clear();
        result.error = std::string("Could not read game list: ") + e.what();
        return result;
    }

    std::stable_sort(result.games.begin(), result.games.end(),
        [](const Game& a, const Game& b) { return a.nameLower < b.nameLower; });
    return result;
}

bool ConvertDetectableList(const std::string& rawJson, std::string& cacheJson,
    size_t& gameCount, std::string& error)
{
    gameCount = 0;
    try
    {
        const json root = json::parse(rawJson);
        if (!root.is_array())
        {
            error = "unexpected answer from the server (not a list)";
            return false;
        }

        json out = json::array();
        for (const json& item : root)
        {
            if (!item.is_object() || !item.contains("name") || !item["name"].is_string() ||
                !item.contains("executables") || !item["executables"].is_array())
                continue;

            json exes = json::array();
            for (const json& e : item["executables"])
            {
                if (e.is_string())
                    exes.push_back(e.get<std::string>());
                else if (e.is_object() && e.contains("name") && e["name"].is_string() &&
                    e.value("os", std::string()) == "win32")
                    exes.push_back(e["name"].get<std::string>());
            }
            if (exes.empty() || item["name"].get<std::string>().empty())
                continue;

            out.push_back({ { "name", item["name"] }, { "executables", exes } });
        }

        if (out.empty())
        {
            error = "the server's list contained no Windows games";
            return false;
        }
        gameCount = out.size();
        cacheJson = out.dump();
        return true;
    }
    catch (const std::exception& e)
    {
        error = std::string("could not read the server's answer: ") + e.what();
        return false;
    }
}