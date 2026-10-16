#pragma once
// Calm animated background: dots on three depth layers that bounce off the window edges,
// leave soft trails, slowly shift color and twinkle, plus a few very faint large "bokeh" circles.

#include <array>
#include <random>
#include <vector>

#include "imgui.h"

class Particles
{
public:
    // size: window size in pixels. mouse: cursor position, or nullptr if unknown.
    // scale: 1.0 at 100% DPI, so speeds and sizes look the same on every screen.
    void Update(float dt, ImVec2 size, const ImVec2* mouse, float scale);
    void Draw(ImDrawList* drawList) const;

    // ---- tweak these ----
    static constexpr float kOpacity = 0.85f;   // master brightness of everything (0..1)
    static constexpr int   kCount = 70;      // dots
    static constexpr int   kBokehCount = 5;       // big faint circles
    static constexpr int   kTrailLen = 20;      // remembered positions per dot
    static constexpr float kTrailStep = 1.0f / 60.0f;   // seconds between remembered positions
    static constexpr float kMouseDist = 150.0f;  // px: radius where the cursor bends paths
    static constexpr float kMousePush = 900.0f;  // 0 = ignore the mouse
    static constexpr float kHueMin = 0.50f;   // color range: 0.50 = cyan ... 0.90 = pink
    static constexpr float kHueMax = 0.92f;

private:
    struct Particle
    {
        ImVec2 pos, vel;
        float  speed = 0;          // constant speed (px/s at scale 1)
        float  radius = 2;
        float  depth = 1;          // 0.4 = far (small, slow, dim) ... 1.0 = near
        float  phase = 0;          // offsets color shift and twinkle
        std::array<ImVec2, kTrailLen> trail{};
        int    trailCount = 0;      // how many entries are valid
        int    trailHead = 0;      // index of the oldest entry
    };

    struct Bokeh
    {
        ImVec2 pos, vel;
        float  radius = 60;
        float  phase = 0;
    };

    void Spawn(ImVec2 size);
    static void PushTrail(Particle& p);

    std::vector<Particle> items_;
    std::vector<Bokeh>    bokeh_;
    std::mt19937          rng_{ std::random_device{}() };
    float                 acc_ = 0.0f;
    float                 time_ = 0.0f;
    float                 lastScale_ = 1.0f;
    ImVec2                lastSize_ = ImVec2(0, 0);
};