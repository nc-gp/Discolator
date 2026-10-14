#include "simulator.h"

#include "../config/paths.h"

#include <windows.h>

#include <algorithm>

namespace fs = std::filesystem;

namespace
{
    // The exe may still be locked for a moment after the process ended.
    void RemoveWithRetry(const fs::path& dir)
    {
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            std::error_code ec;
            fs::remove_all(dir, ec);
            if (!fs::exists(dir, ec))
                return;
            Sleep(50);
        }
    }
}

std::string FormatHMS(long long seconds)
{
    if (seconds < 0)
        seconds = 0;
    char buf[32];
    snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld", seconds / 3600, seconds % 3600 / 60, seconds % 60);
    return buf;
}

Simulator::Simulator()
{
    // Job object: when our process ends (even by crash), Windows kills every process in it.
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));
    }
    job_ = job;
}

Simulator::~Simulator()
{
    StopAll();
    if (job_)
        CloseHandle((HANDLE)job_);
}

void Simulator::SetStatus(std::string text, bool isError)
{
    status = std::move(text);
    statusIsError = isError;
}

bool Simulator::IsRunning(const std::string& gameName) const
{
    return std::any_of(sessions_.begin(), sessions_.end(),
        [&](const Session& s) { return s.gameName == gameName; });
}

// One master copy of ourselves lives in games\_stub; every game exe is a hardlink to it.
bool Simulator::EnsureMaster(fs::path& master, std::string& error)
{
    std::error_code ec;
    const fs::path dir = paths::GamesDir() / L"_stub";
    fs::create_directories(dir, ec);
    master = dir / L"stub.exe";

    const fs::path self = paths::ExePath();
    const bool upToDate = fs::exists(master, ec) &&
        fs::file_size(master, ec) == fs::file_size(self, ec) &&
        fs::last_write_time(master, ec) >= fs::last_write_time(self, ec);
    if (upToDate)
        return true;

    ec.clear();
    fs::copy_file(self, master, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        error = "Could not update the stub (stop all games and try again): " + ec.message();
        return false;
    }
    return true;
}

bool Simulator::Start(const Game& game, int autoStopSeconds, bool hideTaskbar)
{
    if (IsRunning(game.name))
    {
        SetStatus(game.name + " is already running.");
        return false;
    }

    const fs::path relative = game.RelativeExePath();
    if (relative.empty())
    {
        SetStatus("This game has no executable.", true);
        return false;
    }

    fs::path master;
    std::string error;
    if (!EnsureMaster(master, error))
    {
        SetStatus(error, true);
        return false;
    }

    std::error_code ec;
    const fs::path exePath = paths::GamesDir() / relative;
    fs::create_directories(exePath.parent_path(), ec);

    // A leftover exe from an older version of the stub: replace it.
    if (fs::exists(exePath, ec) && fs::file_size(exePath, ec) != fs::file_size(master, ec))
        fs::remove(exePath, ec);

    if (!fs::exists(exePath, ec))
    {
        if (!CreateHardLinkW(exePath.c_str(), master.c_str(), nullptr))   // no extra disk space
        {
            fs::copy_file(master, exePath, ec);                           // fallback: real copy
            if (ec)
            {
                SetStatus("Could not create the game exe: " + ec.message(), true);
                return false;
            }
        }
    }

    std::wstring cmd = L"\"" + exePath.wstring() + L"\" --stub --parent " +
        std::to_wstring(GetCurrentProcessId());
    if (hideTaskbar)
        cmd += L" --hidden";

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    const std::wstring workDir = exePath.parent_path().wstring();

    // Start suspended so it can be put into the job before it runs.
    if (!CreateProcessW(exePath.c_str(), cmd.data(), nullptr, nullptr, FALSE,
        CREATE_SUSPENDED, nullptr, workDir.c_str(), &si, &pi))
    {
        SetStatus("Could not start the game (error " + std::to_string(GetLastError()) + ").", true);
        return false;
    }
    if (job_)
        AssignProcessToJobObject((HANDLE)job_, pi.hProcess);   // failure is not fatal
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    Session s;
    s.gameName = game.name;
    s.gameDir = paths::GamesDir() / game.FolderName();
    s.process = pi.hProcess;
    s.pid = pi.dwProcessId;
    s.started = std::chrono::steady_clock::now();
    if (autoStopSeconds > 0)
    {
        s.hasEnd = true;
        s.endAt = s.started + std::chrono::seconds(autoStopSeconds);
    }
    sessions_.push_back(std::move(s));

    SetStatus("Simulating " + game.name + " - Discord may need a few seconds.");
    return true;
}

void Simulator::Remove(size_t index, bool terminate)
{
    if (index >= sessions_.size())
        return;

    Session s = std::move(sessions_[index]);
    sessions_.erase(sessions_.begin() + (std::ptrdiff_t)index);

    HANDLE h = (HANDLE)s.process;
    if (h)
    {
        if (terminate)
            TerminateProcess(h, 0);
        WaitForSingleObject(h, 3000);
        CloseHandle(h);
    }
    if (deleteOnStop)
        RemoveWithRetry(s.gameDir);
}

void Simulator::Stop(size_t index)
{
    if (index >= sessions_.size())
        return;
    const std::string name = sessions_[index].gameName;
    Remove(index, true);
    SetStatus("Stopped " + name + ".");
}

void Simulator::StopAll()
{
    const bool any = !sessions_.empty();
    while (!sessions_.empty())
        Remove(sessions_.size() - 1, true);
    if (any)
        SetStatus("Stopped all.");
}

void Simulator::Update()
{
    const auto now = std::chrono::steady_clock::now();
    for (size_t i = 0; i < sessions_.size();)
    {
        const Session& s = sessions_[i];
        if (WaitForSingleObject((HANDLE)s.process, 0) == WAIT_OBJECT_0)
        {
            const std::string name = s.gameName;
            Remove(i, false);                       // it exited on its own
            SetStatus(name + " stopped.");
        }
        else if (s.hasEnd && now >= s.endAt)
        {
            const std::string name = s.gameName;
            const long long   played = std::chrono::duration_cast<std::chrono::seconds>(now - s.started).count();
            Remove(i, true);
            SetStatus("Auto-stopped " + name + " after " + FormatHMS(played) + ".");
        }
        else
        {
            ++i;
        }
    }
}

int Simulator::CleanFolder()
{
    int removed = 0;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(paths::GamesDir(), ec))
    {
        if (!entry.is_directory(ec) || entry.path().filename() == L"_stub")
            continue;

        const bool active = std::any_of(sessions_.begin(), sessions_.end(), [&](const Session& s) {
            std::error_code e;
            return fs::equivalent(s.gameDir, entry.path(), e);
            });
        if (active)
            continue;

        fs::remove_all(entry.path(), ec);
        ++removed;
    }
    SetStatus("Cleaned " + std::to_string(removed) + " game folder(s).");
    return removed;
}