#include "stub.h"

#include <windows.h>
#include <string>
#include <vector>

namespace
{
    unsigned long g_parentPid = 0;

    bool ParentAlive()
    {
        if (g_parentPid == 0)
            return true;                       // no parent given: stay alive
        HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, g_parentPid);
        if (!h)
            return false;                      // process no longer exists
        const bool alive = (WaitForSingleObject(h, 0) == WAIT_TIMEOUT);
        CloseHandle(h);
        return alive;
    }

    LRESULT CALLBACK StubProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_TIMER:
            if (!ParentAlive())
                DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    // Splits the command line into tokens, skipping the program name (first token).
    std::vector<std::wstring> ArgsAfterProgramName()
    {
        const std::wstring cmd = GetCommandLineW();
        size_t i = 0;
        if (!cmd.empty() && cmd[0] == L'"')
        {
            i = cmd.find(L'"', 1);
            i = (i == std::wstring::npos) ? cmd.size() : i + 1;
        }
        else
        {
            i = cmd.find(L' ');
            if (i == std::wstring::npos) i = cmd.size();
        }

        std::vector<std::wstring> args;
        while (i < cmd.size())
        {
            while (i < cmd.size() && cmd[i] == L' ') ++i;
            const size_t start = i;
            while (i < cmd.size() && cmd[i] != L' ') ++i;
            if (i > start)
                args.push_back(cmd.substr(start, i - start));
        }
        return args;
    }
}

StubOptions ParseStubCommandLine()
{
    StubOptions o;
    const auto args = ArgsAfterProgramName();
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == L"--stub")        o.stub = true;
        else if (args[i] == L"--hidden") o.hidden = true;
        else if (args[i] == L"--parent" && i + 1 < args.size())
            o.parentPid = wcstoul(args[i + 1].c_str(), nullptr, 10);
    }
    return o;
}

int RunStub(const StubOptions& options)
{
    g_parentPid = options.parentPid;

    WNDCLASSEXW wc = { sizeof(wc), 0, StubProc, 0, 0, GetModuleHandleW(nullptr),
                       nullptr, LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)),  // IDC_ARROW
                       (HBRUSH)(COLOR_WINDOW + 1), nullptr, L"GameStubWindow", nullptr };
    RegisterClassExW(&wc);

    // Default: normal window, minimized. Hidden: tool window (no taskbar button), far off-screen.
    const DWORD exStyle = options.hidden ? WS_EX_TOOLWINDOW : 0;
    const int   pos = options.hidden ? -20000 : 50;

    HWND hwnd = CreateWindowExW(exStyle, wc.lpszClassName, L"Game", WS_OVERLAPPEDWINDOW,
        pos, pos, 200, 80, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd)
        return 1;

    SetTimer(hwnd, 1, 2000, nullptr);   // check every 2 s whether the simulator still exists
    ShowWindow(hwnd, options.hidden ? SW_SHOWNOACTIVATE : SW_SHOWMINNOACTIVE);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}