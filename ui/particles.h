#pragma once

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

    static constexpr int   kCount = 80;
    static constexpr int   kTrailLen = 22;      // remembered positions per dot
    static constexpr float kTrailStep = 1.0f / 60.0f;   // seconds between remembered positions
    static constexpr float kMouseDist = 150.0f; // px: radius where the cursor bends paths
    static constexpr float kMousePush = 900.0f; // 0 = ignore the mouse

private:
    struct Particle
    {
        ImVec2 pos, vel;
        float  speed = 0;          // constant speed (px/s at scale 1)
        float  radius = 2;
        ImU32  rgb = 0;          // color without alpha, packed as R | G<<8 | B<<16
        std::array<ImVec2, kTrailLen> trail{};
        int    trailCount = 0;      // how many entries are valid
        int    trailHead = 0;      // index of the oldest entry
    };

    void Spawn(ImVec2 size);
    static void PushTrail(Particle& p);

    std::vector<Particle> items_;
    std::mt19937          rng_{ std::random_device{}() };
    float                 acc_ = 0.0f;
    float                 lastScale_ = 1.0f;
};