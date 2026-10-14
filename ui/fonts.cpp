#include "fonts.h"

#include "imgui.h"
#include "../config/paths.h"

#include <windows.h>
#include <string>

namespace
{
    constexpr float kBodySize = 16.0f; // px at 100% DPI
    constexpr float kTitleSize = 24.0f;

    ImFont* g_title = nullptr;

    // Full path of a font in the Windows Fonts folder, or empty if it isn't there.
    std::string WindowsFontPath(const wchar_t* fileName)
    {
        wchar_t dir[MAX_PATH] = {};
        const UINT n = GetWindowsDirectoryW(dir, MAX_PATH);
        if (n == 0 || n >= MAX_PATH)
            return {};
        const std::wstring path = std::wstring(dir, n) + L"\\Fonts\\" + fileName;
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
            return {};
        return paths::ToUtf8(path);
    }
}

void LoadFonts(float dpiScale)
{
    ImGuiIO& io = ImGui::GetIO();
    (void)dpiScale;   // only needed for ImGui versions before 1.92 (see below)

    const std::string regular = WindowsFontPath(L"segoeui.ttf");
    const std::string bold = WindowsFontPath(L"segoeuib.ttf");
    if (regular.empty())
        return;                                   // keep ImGui's default font

#if IMGUI_VERSION_NUM >= 19200
    // ImGui 1.92+: sizes are "base sizes", scaled for DPI automatically (style.FontScaleDpi).
    io.Fonts->AddFontFromFileTTF(regular.c_str(), kBodySize);
    if (!bold.empty())
        g_title = io.Fonts->AddFontFromFileTTF(bold.c_str(), kBodySize);
#else
    // Older ImGui: fonts are baked at a fixed pixel size, so include the DPI scale here.
    io.Fonts->AddFontFromFileTTF(regular.c_str(), kBodySize * dpiScale);
    if (!bold.empty())
        g_title = io.Fonts->AddFontFromFileTTF(bold.c_str(), kTitleSize * dpiScale);
#endif
}

ScopedTitleFont::ScopedTitleFont()
{
    if (!g_title)
        return;
#if IMGUI_VERSION_NUM >= 19200
    ImGui::PushFont(g_title, kTitleSize);
#else
    ImGui::PushFont(g_title);
#endif
    pushed_ = true;
}

ScopedTitleFont::~ScopedTitleFont()
{
    if (pushed_)
        ImGui::PopFont();
}