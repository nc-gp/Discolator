#include <windows.h>

#include "ui/app.h"
#include "game/stub.h"
#include "ui/window.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // Started as a fake game? Then only run the tiny stub window, no UI.
    const StubOptions stub = ParseStubCommandLine();
    if (stub.stub)
        return RunStub(stub);

    Window window;
    if (!window.Create(L"Discolator", 580, 800))
    {
        MessageBoxW(nullptr, L"Could not create the window / Direct3D device.", L"Discolator", MB_ICONERROR);
        return 1;
    }

    AppInit();

    const float clearColor[4] = { 0.07f, 0.08f, 0.13f, 1.0f };

    while (window.PumpMessages())
    {
        if (!window.BeginFrame())
            continue;           // minimized or hidden: nothing to draw

        AppFrame();
        window.EndFrame(clearColor);
    }

    AppShutdown();
    window.Destroy();
    return 0;
}