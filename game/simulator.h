#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "games.h"

struct Session
{
    std::string           gameName;
    std::filesystem::path gameDir;     // <exe folder>\games\<game>
    void* process = nullptr;   // HANDLE
    unsigned long         pid = 0;
    std::chrono::steady_clock::time_point started;
    std::chrono::steady_clock::time_point endAt;   // valid if hasEnd
    bool                  hasEnd = false;
};

std::string FormatHMS(long long seconds);   // 3725 -> "01:02:05"

class Simulator
{
public:
    Simulator();
    ~Simulator();
    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;

    // autoStopSeconds: 0 = never. Returns false (and sets status) on failure.
    bool Start(const Game& game, int autoStopSeconds, bool hideTaskbar);
    void Stop(size_t index);
    void StopAll();

    // Call every frame: notices exited processes and handles auto-stop.
    void Update();

    // Deletes every game folder that isn't running. Returns how many were removed.
    int  CleanFolder();

    bool IsRunning(const std::string& gameName) const;
    const std::vector<Session>& Sessions() const { return sessions_; }

    bool        deleteOnStop = true;   // remove the fake exe/folder when a game stops

    // The one status line shown in the UI (also used for non-simulator messages).
    void        SetStatus(std::string text, bool isError = false);
    std::string status;
    bool        statusIsError = false;

private:
    bool EnsureMaster(std::filesystem::path& master, std::string& error);
    void Remove(size_t index, bool terminate);

    std::vector<Session> sessions_;
    void* job_ = nullptr;   // HANDLE: kills all fake games if we exit or crash
};