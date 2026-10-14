#pragma once

#include <windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;

class Window
{
public:
    bool Create(const wchar_t* title, int width, int height);
    void Destroy();

    // Handles Windows messages. Returns false once the window was closed.
    bool PumpMessages();

    // Starts an ImGui frame. Returns false if nothing should be drawn this frame
    // (window minimized or hidden) - in that case do NOT call EndFrame().
    bool BeginFrame();
    void EndFrame(const float clearColor[4]);

private:
    bool CreateDevice();
    void DestroyDevice();
    void CreateRenderTarget();
    void DestroyRenderTarget();

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND                    hwnd_ = nullptr;
    WNDCLASSEXW             wc_ = {};
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* context_ = nullptr;
    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11RenderTargetView* targetView_ = nullptr;

    bool         minimized_ = false;
    bool         occluded_ = false;
    unsigned int resizeWidth_ = 0;
    unsigned int resizeHeight_ = 0;
};