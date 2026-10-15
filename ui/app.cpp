#include "app.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "../config/config.h"
#include "../downloader.h"
#include "fonts.h"
#include "../game/games.h"
#include "particles.h"
#include "../config/paths.h"
#include "../game/simulator.h"

namespace
{
    struct AppState
    {
        bool                initialized = false;
        Config              config;
        Simulator           sim;
        Particles           particles;
        ListDownloader      downloader;

        std::vector<Game>   games;
        std::string         loadError;
        std::string         loadInfo;          // e.g. "1234 games  (1300 entries in file, 456 KB)"
        std::vector<int>    filtered;          // indices into games that match the search
        char                search[128] = {};
        bool                filterDirty = true;
        int                 selected = -1;  // index into games (not into filtered)

        float               footerHeight = 0;  // measured every frame: height of everything below the list
    };
    AppState g;

    const ImVec4 kGood(0.45f, 0.85f, 0.55f, 1.0f);
    const ImVec4 kBad(1.00f, 0.45f, 0.45f, 1.0f);
    const ImVec4 kTime(0.55f, 0.75f, 1.00f, 1.0f);
    const ImVec4 kAccent(0.65f, 0.72f, 1.00f, 1.0f);   // title
    const ImVec4 kLabel(0.55f, 0.62f, 0.95f, 1.0f);   // section labels

    // Red-tinted button style for "stop" actions. Call PopStopButton() afterwards.
    void PushStopButton()
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.62f, 0.22f, 0.28f, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.28f, 0.35f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.18f, 0.24f, 1.00f));
    }
    void PopStopButton() { ImGui::PopStyleColor(3); }

    // Small caps-style label above a card, with optional dim text on the right.
    void SectionLabel(const char* text, const char* right = nullptr)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kLabel);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
        if (right)
        {
            const float w = ImGui::CalcTextSize(right).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - w);
            ImGui::TextDisabled("%s", right);
        }
    }
    
    void ReloadGames()
    {
        const auto file = paths::GameListCacheFile();
        GameListResult r = LoadGameList(file);
        g.games = std::move(r.games);
        g.loadError = std::move(r.error);
        g.loadInfo = std::to_string(g.games.size()) + " games  (" + std::to_string(r.entryCount) +
            " entries in file, " + std::to_string(r.fileBytes / 1024) + " KB)";
        g.selected = -1;
        g.filterDirty = true;
    }

    void RebuildFilter()
    {
        const std::string q = ToLowerAscii(g.search);
        g.filtered.clear();
        for (int i = 0; i < (int)g.games.size(); ++i)
            if (g.games[i].MatchesSearch(q))
                g.filtered.push_back(i);
        g.filterDirty = false;
    }

    void StartGame(int gameIndex)
    {
        if (gameIndex >= 0 && gameIndex < (int)g.games.size())
            g.sim.Start(g.games[gameIndex], g.config.AutoStopSeconds(), g.config.hideTaskbar);
    }

    void DrawHeader()
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        {
            ScopedTitleFont title;
            ImGui::TextColored(kAccent, "Discolator");
        }
        const ImVec2 titleMin = ImGui::GetItemRectMin();
        const ImVec2 titleMax = ImGui::GetItemRectMax();
        ImGui::TextDisabled("Fake game activity for Discord Quests");

        // Status chip on the right, vertically centered on the title (drawn directly, no layout impact).
        const int  running = (int)g.sim.Sessions().size();
        char       chip[32];
        if (running > 0) std::snprintf(chip, sizeof(chip), "%d running", running);
        else             std::snprintf(chip, sizeof(chip), "idle");

        const float  dot = ImGui::GetFontSize() * 0.28f;
        const float  gap = ImGui::GetFontSize() * 0.45f;
        const float  textW = ImGui::CalcTextSize(chip).x;
        const float  right = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x;
        const float  midY = (titleMin.y + titleMax.y) * 0.5f;
        const ImU32  col = running > 0 ? IM_COL32(115, 217, 140, 255) : IM_COL32(125, 135, 180, 255);

        dl->AddCircleFilled(ImVec2(right - textW - gap - dot, midY), dot, col);
        dl->AddText(ImVec2(right - textW, midY - ImGui::GetFontSize() * 0.5f), col, chip);

        // Thin accent line fading out to the right.
        ImGui::Spacing();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float  w = ImGui::GetContentRegionAvail().x;
        const float  h = std::max(1.0f, ImGui::GetFontSize() * 0.12f);
        dl->AddRectFilledMultiColor(p, ImVec2(p.x + w, p.y + h),
            IM_COL32(140, 155, 255, 220), IM_COL32(140, 155, 255, 0),
            IM_COL32(140, 155, 255, 0), IM_COL32(140, 155, 255, 220));
        ImGui::Dummy(ImVec2(w, h));
        ImGui::Spacing();
    }

    void DrawGameList()
    {
        char count[48];
        if (g.search[0])
            std::snprintf(count, sizeof(count), "%d of %d", (int)g.filtered.size(), (int)g.games.size());
        else
            std::snprintf(count, sizeof(count), "%d games", (int)g.games.size());
        SectionLabel("GAMES", count);

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputTextWithHint("##search", "Search games...", g.search, sizeof(g.search)))
            g.filterDirty = true;
        if (g.filterDirty)
            RebuildFilter();

        // The list takes whatever height the footer (measured last frame) doesn't need.
        const float height = std::max(ImGui::GetFrameHeight() * 4, ImGui::GetContentRegionAvail().y - g.footerHeight);

        // EndChild() must always be called, even when BeginChild() returns false.
        if (ImGui::BeginChild("games", ImVec2(0, height), ImGuiChildFlags_Borders))
        {
            const float rowH = ImGui::GetFontSize() * 1.75f;
            const float pad = ImGui::GetFontSize() * 0.5f;
            const float dot = ImGui::GetFontSize() * 0.25f;

            ImGuiListClipper clipper;
            clipper.Begin((int)g.filtered.size(), rowH + ImGui::GetStyle().ItemSpacing.y);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    const int   idx = g.filtered[row];
                    const Game& game = g.games[idx];
                    const bool  running = g.sim.IsRunning(game.name);

                    const ImVec2 p = ImGui::GetCursorScreenPos();
                    const float  w = ImGui::GetContentRegionAvail().x;

                    ImGui::PushID(idx);
                    if (ImGui::Selectable("##row", g.selected == idx, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, rowH)))
                    {
                        g.selected = idx;
                        if (ImGui::IsMouseDoubleClicked(0))
                            StartGame(idx);
                    }
                    ImGui::PopID();

                    // Row content is drawn by hand: vertically centered text + a "running" badge.
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    const float textY = p.y + (rowH - ImGui::GetFontSize()) * 0.5f;
                    dl->AddText(ImVec2(p.x + pad, textY), ImGui::GetColorU32(ImGuiCol_Text), game.name.c_str());
                    if (running)
                    {
                        const char* badge = "running";
                        const float  bw = ImGui::CalcTextSize(badge).x;
                        const ImU32  green = IM_COL32(115, 217, 140, 255);
                        dl->AddText(ImVec2(p.x + w - pad - bw, textY), green, badge);
                        dl->AddCircleFilled(ImVec2(p.x + w - pad - bw - dot * 2.0f, p.y + rowH * 0.5f), dot, green);
                    }
                }
            }
        }
        ImGui::EndChild();
    }

    void DrawControls()
    {
        ImGui::Spacing();
        if (g.selected >= 0)
            ImGui::TextDisabled("Will create: games\\%s", paths::ToUtf8(g.games[g.selected].RelativeExePath()).c_str());
        else
            ImGui::TextDisabled("Select a game (double-click to start it).");

        const float h = ImGui::GetFrameHeight() * 1.2f;

        ImGui::BeginDisabled(g.selected < 0);
        if (ImGui::Button("Start simulating", ImVec2(ImGui::GetFontSize() * 11, h)))
            StartGame(g.selected);
        ImGui::EndDisabled();

        ImGui::SameLine();
        PushStopButton();
        if (ImGui::Button("Stop all", ImVec2(ImGui::GetFontSize() * 6, h)))
            g.sim.StopAll();
        PopStopButton();
    }

    void DrawRunning()
    {
        ImGui::Spacing();
        char count[16];
        std::snprintf(count, sizeof(count), "%d", (int)g.sim.Sessions().size());
        SectionLabel("RUNNING", count);

        const auto now = std::chrono::steady_clock::now();
        int stopIndex = -1;

        if (ImGui::BeginChild("running", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 3.8f), ImGuiChildFlags_Borders))
        {
            const auto& sessions = g.sim.Sessions();

            if (sessions.empty())
            {
                // Empty state: centered hint.
                const char* hint = "Nothing running";
                const ImVec2 avail = ImGui::GetContentRegionAvail();
                const ImVec2 ts = ImGui::CalcTextSize(hint);
                ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (avail.x - ts.x) * 0.5f,
                    ImGui::GetCursorPosY() + (avail.y - ts.y) * 0.5f));
                ImGui::TextDisabled("%s", hint);
            }

            for (int i = 0; i < (int)sessions.size(); ++i)
            {
                const Session& s = sessions[i];
                const long long played = std::chrono::duration_cast<std::chrono::seconds>(now - s.started).count();

                ImGui::PushID(i);
                PushStopButton();
                if (ImGui::SmallButton("Stop"))
                    stopIndex = i;
                PopStopButton();
                ImGui::PopID();

                // Green status dot.
                ImGui::SameLine();
                const float  r = ImGui::GetFontSize() * 0.25f;
                const ImVec2 p = ImGui::GetCursorScreenPos();
                const float  lh = ImGui::GetTextLineHeight();
                ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + r, p.y + lh * 0.5f), r, IM_COL32(115, 217, 140, 255));
                ImGui::Dummy(ImVec2(r * 2.0f, lh));

                ImGui::SameLine();
                ImGui::TextUnformatted(s.gameName.c_str());
                ImGui::SameLine();
                ImGui::TextColored(kTime, "%s", FormatHMS(played).c_str());
                if (s.hasEnd)
                {
                    const long long left = std::chrono::duration_cast<std::chrono::seconds>(s.endAt - now).count();
                    ImGui::SameLine();
                    ImGui::TextDisabled("(stops in %s)", FormatHMS(left).c_str());
                }
            }
        }
        ImGui::EndChild();

        if (stopIndex >= 0)           // after the loop, so the list isn't modified while drawing it
            g.sim.Stop((size_t)stopIndex);
    }

    // Returns true if a setting changed.
    bool DrawSettings()
    {
        ImGui::Spacing();
        SectionLabel("SETTINGS");

        bool changed = false;
        Config& c = g.config;

        if (ImGui::BeginChild("settings", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY))
        {
            changed |= ImGui::Checkbox("Delete fake exe when game stops", &c.deleteOnStop);
            ImGui::SameLine();
            if (ImGui::Button("Clean folder"))
                g.sim.CleanFolder();

            changed |= ImGui::Checkbox("Hide taskbar button (experimental)", &c.hideTaskbar);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Auto-stop after");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
            if (ImGui::BeginCombo("##autostop", kAutoStopPresets[c.autoStopIndex].label))
            {
                for (int i = 0; i < kAutoStopPresetCount; ++i)
                {
                    if (ImGui::Selectable(kAutoStopPresets[i].label, i == c.autoStopIndex))
                    {
                        c.autoStopIndex = i;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            if (kAutoStopPresets[c.autoStopIndex].minutes < 0)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
                if (ImGui::InputInt("min##custom", &c.autoStopCustomMinutes, 5, 30))
                {
                    c.Sanitize();
                    changed = true;
                }
            }
        }
        ImGui::EndChild();
        return changed;
    }

    void DrawStatus()
    {
        ImGui::Spacing();

        // Always takes one line, so the layout doesn't jump when the first message appears.
        if (g.downloader.IsRunning())
            ImGui::TextColored(kTime, "Downloading game list...  %.1f MB",
                (double)g.downloader.BytesReceived() / (1024.0 * 1024.0));
        else if (!g.sim.status.empty())
            ImGui::TextColored(g.sim.statusIsError ? kBad : kGood, "%s", g.sim.status.c_str());
        else
            ImGui::TextUnformatted(" ");

        if (!g.loadError.empty())
            ImGui::TextColored(kBad, "%s", g.loadError.c_str());
        else
            ImGui::TextDisabled("Loaded %s", g.loadInfo.c_str());

        if (ImGui::SmallButton("Reload list"))
            ReloadGames();
        ImGui::SameLine();
        ImGui::BeginDisabled(g.downloader.IsRunning());
        if (ImGui::SmallButton("Download fresh list"))
            g.downloader.Start(paths::GameListCacheFile());
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("Build: " __DATE__ " " __TIME__);
    }
}

void AppInit()
{
    if (g.initialized)
        return;                // safe to call more than once
    g.initialized = true;

    g.config = Config::Load();
    ReloadGames();
}

void AppShutdown()
{
    g.downloader.Cancel();     // don't leave a download thread behind
    g.sim.StopAll();           // also removes the fake exes if "delete on stop" is on
    g.config.Save();
}

void AppFrame()
{
    AppInit();   // normally already done by main(); this makes the first frame safe either way

    g.sim.deleteOnStop = g.config.deleteOnStop;
    g.sim.Update();            // exited processes + auto-stop timers

    // A finished download: reload the list from the new file and tell the user.
    {
        std::string message;
        bool        success = false;
        if (g.downloader.PollFinished(message, success))
        {
            if (success)
                ReloadGames();
            g.sim.SetStatus(message, !success);
        }
    }

    // Particles live on the background draw list: behind every window.
    {
        const ImGuiIO& io = ImGui::GetIO();
        const ImVec2 mouse = io.MousePos;
        const float  scale = ImGui::GetFontSize() / 16.0f;   // 16 px = our body font at 100% DPI
        g.particles.Update(io.DeltaTime, io.DisplaySize, ImGui::IsMousePosValid(&mouse) ? &mouse : nullptr, scale);
        g.particles.Draw(ImGui::GetBackgroundDrawList());
    }

    // One borderless window that fills the whole program window.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    // Scrollbar stays allowed: if the window is ever too small for everything, you can scroll
    // instead of losing the bottom part.
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("##main", nullptr, flags);

    DrawHeader();
    DrawGameList();

    // Everything from here on is the "footer"; its height decides the list height next frame.
    const float footerTop = ImGui::GetCursorPosY();

    DrawControls();
    DrawRunning();
    if (DrawSettings())
        g.config.Save();
    DrawStatus();

    g.footerHeight = (ImGui::GetCursorPosY() - footerTop) + 2.0f;
       
    if (ImGui::IsMouseDragging(0) && ImGui::IsWindowHovered()) {
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        ImGui::SetWindowPos(ImVec2(pos.x + delta.x, pos.y + delta.y));
    }

    ImGui::End();
}