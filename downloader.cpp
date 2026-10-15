#include "downloader.h"

#include "./game/games.h"
#include "./config/paths.h"

#include <windows.h>
#include <winhttp.h>

#include <fstream>
#include <thread>

#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;

namespace
{
    // Closes a WinHTTP handle when it goes out of scope.
    struct Handle
    {
        HINTERNET h = nullptr;
        explicit Handle(HINTERNET handle = nullptr) : h(handle) {}
        ~Handle() { if (h) WinHttpCloseHandle(h); }
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        explicit operator bool() const { return h != nullptr; }
    };

    std::string DescribeError(DWORD code)
    {
        switch (code)
        {
        case ERROR_WINHTTP_NAME_NOT_RESOLVED:     return "could not find the server (no internet connection?)";
        case ERROR_WINHTTP_CANNOT_CONNECT:        return "could not connect to the server";
        case ERROR_WINHTTP_CONNECTION_ERROR:      return "the connection was interrupted";
        case ERROR_WINHTTP_TIMEOUT:               return "the server took too long to answer";
        case ERROR_WINHTTP_SECURE_FAILURE:        return "secure connection failed (check the date/time on this PC)";
        default:                                  return "network error " + std::to_string(code);
        }
    }

    // Writes to a temp file first, so a failed download never destroys the old list.
    bool WriteFileAtomic(const fs::path& file, const std::string& data, std::string& error)
    {
        const fs::path tmp = file.wstring() + L".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                error = "could not write " + paths::ToUtf8(tmp);
                return false;
            }
            out.write(data.data(), (std::streamsize)data.size());
            if (!out)
            {
                error = "could not write the list to disk";
                return false;
            }
        }
        if (!MoveFileExW(tmp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            error = "could not replace " + paths::ToUtf8(file);
            return false;
        }
        return true;
    }
}

ListDownloader::~ListDownloader()
{
    Cancel();
}

bool ListDownloader::IsRunning() const
{
    return state_ && state_->running;
}

unsigned long long ListDownloader::BytesReceived() const
{
    return state_ ? state_->bytes.load() : 0;
}

void ListDownloader::Cancel()
{
    if (!state_)
        return;
    state_->cancel = true;
    // Closing the request may make a blocked call return sooner. Not guaranteed, so we never wait for it.
    if (void* request = state_->activeRequest.exchange(nullptr))
        WinHttpCloseHandle((HINTERNET)request);
    state_.reset(); // the worker keeps its own reference and cleans up by itself
}

bool ListDownloader::Start(const fs::path& outFile)
{
    if (IsRunning())
        return false;

    state_ = std::make_shared<Shared>();
    std::thread(&ListDownloader::Run, state_, L"https://discord.com/api/v9/applications/detectable", outFile).detach();
    return true;
}

bool ListDownloader::PollFinished(std::string& message, bool& success)
{
    if (!state_ || !state_->finished.exchange(false))
        return false;

    std::lock_guard<std::mutex> lock(state_->mutex);
    message = state_->message;
    success = state_->success;
    return true;
}

void ListDownloader::Run(std::shared_ptr<Shared> state, std::wstring url, fs::path outFile)
{
    std::string body, error, cache;
    size_t count = 0;

    bool ok = Fetch(*state, url, body, error);
    if (!ok && state->cancel)
        error = "cancelled";
    if (ok && state->cancel)
    {
        ok = false;
        error = "cancelled";
    }
    if (ok) ok = ConvertDetectableList(body, cache, count, error);
    if (ok) ok = WriteFileAtomic(outFile, cache, error);

    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->success = ok;
        state->message = ok ? "Downloaded a fresh game list: " + std::to_string(count) + " games."
            : "Download failed: " + error;
    }
    state->running = false;
    state->finished = true;
}

bool ListDownloader::Fetch(Shared& state, const std::wstring& url, std::string& body, std::string& error)
{
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    URL_COMPONENTSW uc = {};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = (DWORD)(sizeof(host) / sizeof(host[0]));
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = (DWORD)(sizeof(path) / sizeof(path[0]));
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
    {
        error = "invalid address";
        return false;
    }
    const bool secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    Handle session(WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) Discolator/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session)
    {
        error = DescribeError(GetLastError());
        return false;
    }
    WinHttpSetTimeouts(session.h, 8000, 8000, 15000, 20000); // resolve, connect, send, receive (ms)

    Handle connection(WinHttpConnect(session.h, host, uc.nPort, 0));
    if (!connection)
    {
        error = DescribeError(GetLastError());
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(connection.h, L"GET", path, nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request)
    {
        error = DescribeError(GetLastError());
        return false;
    }

    // Whoever takes the handle out of the slot first (us or Cancel()) closes it.
    struct RequestOwner
    {
        std::atomic<void*>& slot;
        ~RequestOwner() { if (void* h = slot.exchange(nullptr)) WinHttpCloseHandle((HINTERNET)h); }
    };
    state.activeRequest = request;
    RequestOwner owner{ state.activeRequest };
    if (state.cancel)
    {
        error = "cancelled";
        return false;
    }

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr))
    {
        error = DescribeError(GetLastError());
        return false;
    }

    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    if (status != 200)
    {
        error = "the server answered with HTTP " + std::to_string(status);
        return false;
    }

    constexpr size_t kMaxBytes = 200u * 1024u * 1024u; // sanity limit
    for (;;)
    {
        if (state.cancel)
        {
            error = "cancelled";
            return false;
        }

        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available))
        {
            error = DescribeError(GetLastError());
            return false;
        }
        if (available == 0)
            break;

        const size_t old = body.size();
        if (old + available > kMaxBytes)
        {
            error = "the answer is unexpectedly large";
            return false;
        }
        body.resize(old + available);

        DWORD read = 0;
        if (!WinHttpReadData(request, &body[old], available, &read))
        {
            error = DescribeError(GetLastError());
            return false;
        }
        body.resize(old + read);
        state.bytes = body.size();
    }

    if (body.empty())
    {
        error = "the server sent an empty answer";
        return false;
    }
    if (body.size() >= 2 && (unsigned char)body[0] == 0x1F && (unsigned char)body[1] == 0x8B)
    {
        error = "the server sent a compressed answer, which isn't supported";
        return false;
    }
    return true;
}