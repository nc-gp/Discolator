#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Game
{
    std::string              name;
    std::string              nameLower;     // ASCII-lowercased, for searching
    std::vector<std::string> executables;   // Windows exe names exactly as Discord lists them

    // Folder name made from the game name (illegal characters removed).
    std::filesystem::path FolderName() const;

    // <FolderName>\<first exe>, e.g. "Valorant\VALORANT.exe" or "Battlefield 1\bf1\bf1.exe".
    // Empty if the game has no executables.
    std::filesystem::path RelativeExePath() const;

    bool MatchesSearch(const std::string& lowerQuery) const;
};

struct GameListResult
{
    std::vector<Game> games;
    std::string       error;        // empty on success
    unsigned long long fileBytes = 0;  // size of the file on disk
    size_t            entryCount = 0;  // entries found in the file (usable or not)
};

GameListResult LoadGameList(const std::filesystem::path& file);

// Turns the raw answer of Discord's "detectable applications" API into our small cache
// format (name + Windows exe names). Returns false and sets error if nothing usable.
bool ConvertDetectableList(const std::string& rawJson, std::string& cacheJson,
    size_t& gameCount, std::string& error);

// ASCII lowercase copy of a string.
std::string ToLowerAscii(std::string s);