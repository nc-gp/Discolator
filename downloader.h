#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

class ListDownloader
{
public:
    static const wchar_t* DefaultUrl();   // https://discord.com/api/v9/applications/detectable

    ListDownloader() = default;
    ~ListDownloader();
    ListDownloader(const ListDownloader&) = delete;
    ListDownloader& operator=(const ListDownloader&) = delete;

    // Starts downloading url and saves the converted list to outFile.
    // Returns false if a download is already running.
    bool Start(const std::filesystem::path& outFile);

    // Gives up on a running download. Never waits for the network: the worker thread
    // finishes (or times out) on its own and its result is thrown away.
    void Cancel();

    bool               IsRunning() const;
    unsigned long long BytesReceived() const;

    // Call every frame. Returns true exactly once per finished download.
    bool PollFinished(std::string& message, bool& success);

private:
    // Shared with the worker thread, which keeps it alive even if this object is gone.
    struct Shared
    {
        std::atomic<bool>               running{ true };
        std::atomic<bool>               finished{ false };
        std::atomic<bool>               cancel{ false };
        std::atomic<unsigned long long> bytes{ 0 };
        std::atomic<void*>              activeRequest{ nullptr };   // HINTERNET, closed on cancel (best effort)

        std::mutex  mutex;
        std::string message;
        bool        success = false;
    };

    static void Run(std::shared_ptr<Shared> state, std::wstring url, std::filesystem::path outFile);
    static bool Fetch(Shared& state, const std::wstring& url, std::string& body, std::string& error);

    std::shared_ptr<Shared> state_;
};