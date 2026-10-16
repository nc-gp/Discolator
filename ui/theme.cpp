#include "theme.h"

#include "imgui.h"

void ApplyTheme()
{
    ImGui::StyleColorsDark();

    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 8.0f;
    s.WindowPadding = ImVec2(14.0f, 12.0f);
    s.ChildBorderSize = 1.0f;
    s.ChildRounding = 8.0f;
    s.FrameRounding = 6.0f;
    s.PopupRounding = 6.0f;
    s.GrabRounding = 6.0f;
    s.ScrollbarRounding = 6.0f;
    s.FramePadding = ImVec2(8.0f, 5.0f);
    s.ItemSpacing = ImVec2(8.0f, 7.0f);

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);   // transparent: particles show through
    c[ImGuiCol_ChildBg] = ImVec4(0.04f, 0.05f, 0.09f, 0.72f);   // dark glass: text stays calm, particles still glow through
    c[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.09f, 0.15f, 0.96f);

    c[ImGuiCol_Header] = ImVec4(0.30f, 0.35f, 0.80f, 0.45f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.40f, 0.90f, 0.60f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.40f, 0.90f, 0.80f);

    c[ImGuiCol_Button] = ImVec4(0.32f, 0.36f, 0.85f, 0.80f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.38f, 0.43f, 0.95f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.32f, 0.75f, 1.00f);

    c[ImGuiCol_CheckMark] = ImVec4(0.65f, 0.72f, 1.00f, 1.00f);

    c[ImGuiCol_Text] = ImVec4(0.92f, 0.94f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.55f, 0.72f, 1.00f);
    c[ImGuiCol_Border] = ImVec4(0.40f, 0.45f, 0.90f, 0.28f);   // subtle accent outline for cards
    c[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.11f, 0.21f, 0.90f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.17f, 0.32f, 0.95f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.20f, 0.38f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_Separator] = ImVec4(0.40f, 0.45f, 0.90f, 0.22f);
}