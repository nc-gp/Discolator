#include "window.h"
#include "fonts.h"
#include "theme.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool Window::Create(const wchar_t* title, int width, int height)
{
    ImGui_ImplWin32_EnableDpiAwareness();

    wc_ = { sizeof(wc_), CS_CLASSDC, WndProc, 0, 0, GetModuleHandleW(nullptr),
            nullptr, LoadCursorW(nullptr, MAKEINTRESOURCEW(32512))  /* IDC_ARROW */, nullptr, nullptr, L"DiscolatorWindow", nullptr };
    RegisterClassExW(&wc_);

    const float dpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(
        MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    hwnd_ = CreateWindowW(wc_.lpszClassName, title, WS_OVERLAPPEDWINDOW, 100, 100,
        (int)(width * dpiScale), (int)(height * dpiScale),
        nullptr, nullptr, wc_.hInstance, this);
    if (!hwnd_)
        return false;

    if (!CreateDevice())
    {
        DestroyDevice();
        DestroyWindow(hwnd_);
        UnregisterClassW(wc_.lpszClassName, wc_.hInstance);
        hwnd_ = nullptr;
        return false;
    }

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;                              // no imgui.ini
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ApplyTheme();                          // colors + rounding (before scaling for DPI)
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpiScale);
#if IMGUI_VERSION_NUM >= 19200
    style.FontScaleDpi = dpiScale;
#endif
    LoadFonts(dpiScale);   // Segoe UI (older ImGui versions bake the DPI scale into the font size)

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_, context_);
    return true;
}

void Window::Destroy()
{
    if (!hwnd_)
        return;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    DestroyDevice();
    DestroyWindow(hwnd_);
    UnregisterClassW(wc_.lpszClassName, wc_.hInstance);
    hwnd_ = nullptr;
}

bool Window::PumpMessages()
{
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT)
            return false;
    }
    return true;
}

bool Window::BeginFrame()
{
    // Minimized: skip the whole frame (no NewFrame/Render), just don't burn CPU.
    if (minimized_)
    {
        Sleep(50);
        return false;
    }
    // Window fully covered / screen locked.
    if (occluded_ && swapChain_->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
    {
        Sleep(10);
        return false;
    }
    occluded_ = false;

    if (resizeWidth_ != 0 && resizeHeight_ != 0)
    {
        DestroyRenderTarget();
        swapChain_->ResizeBuffers(0, resizeWidth_, resizeHeight_, DXGI_FORMAT_UNKNOWN, 0);
        resizeWidth_ = resizeHeight_ = 0;
        CreateRenderTarget();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    return true;
}

void Window::EndFrame(const float clearColor[4])
{
    ImGui::Render();
    context_->OMSetRenderTargets(1, &targetView_, nullptr);
    context_->ClearRenderTargetView(targetView_, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    const HRESULT hr = swapChain_->Present(1, 0);   // vsync
    occluded_ = (hr == DXGI_STATUS_OCCLUDED);
}

bool Window::CreateDevice()
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd_;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL got;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION, &sd,
        &swapChain_, &device_, &got, &context_);
    if (hr == DXGI_ERROR_UNSUPPORTED)   // no GPU available -> software renderer
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
            levels, 2, D3D11_SDK_VERSION, &sd,
            &swapChain_, &device_, &got, &context_);
    if (FAILED(hr))
        return false;

    CreateRenderTarget();
    return true;
}

void Window::DestroyDevice()
{
    DestroyRenderTarget();
    if (swapChain_) { swapChain_->Release(); swapChain_ = nullptr; }
    if (context_) { context_->Release();   context_ = nullptr; }
    if (device_) { device_->Release();    device_ = nullptr; }
}

void Window::CreateRenderTarget()
{
    ID3D11Texture2D* backBuffer = nullptr;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    device_->CreateRenderTargetView(backBuffer, nullptr, &targetView_);
    backBuffer->Release();
}

void Window::DestroyRenderTarget()
{
    if (targetView_) { targetView_->Release(); targetView_ = nullptr; }
}

LRESULT CALLBACK Window::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_GETMINMAXINFO:   // don't let the window get smaller than the UI needs
    {
        const float scale = ImGui_ImplWin32_GetDpiScaleForHwnd(hWnd);
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = (LONG)(500 * scale);
        info->ptMinTrackSize.y = (LONG)(640 * scale);
        return 0;
    }
    case WM_SIZE:
        if (self)
        {
            self->minimized_ = (wParam == SIZE_MINIMIZED);
            if (wParam != SIZE_MINIMIZED)
            {
                self->resizeWidth_ = (UINT)LOWORD(lParam);
                self->resizeHeight_ = (UINT)HIWORD(lParam);
            }
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)   // disable ALT menu beep
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}