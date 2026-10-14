#include "particles.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kTwoPi = 6.28318530718f;

    struct Rgb { int r, g, b; };
    constexpr Rgb kPalette[] = { {120, 140, 255}, {170, 120, 255}, {90, 200, 255}, {255, 130, 200} };
}

void Particles::Spawn(ImVec2 size)
{
    std::uniform_real_distribution<float> angle(0.0f, kTwoPi);
    std::uniform_real_distribution<float> speed(35.0f, 110.0f);
    std::uniform_real_distribution<float> radius(1.5f, 3.8f);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_int_distribution<int>    color(0, (int)(sizeof(kPalette) / sizeof(kPalette[0])) - 1);

    items_.assign(kCount, Particle{});
    for (Particle& p : items_)
    {
        const float a = angle(rng_);
        p.speed = speed(rng_);
        p.vel = ImVec2(std::cos(a) * p.speed, std::sin(a) * p.speed);
        p.radius = radius(rng_);
        p.pos = ImVec2(unit(rng_) * size.x, unit(rng_) * size.y);
        const Rgb& c = kPalette[color(rng_)];
        p.rgb = (ImU32)c.r | ((ImU32)c.g << 8) | ((ImU32)c.b << 16);
    }
}

void Particles::PushTrail(Particle& p)
{
    if (p.trailCount < kTrailLen)
    {
        p.trail[(p.trailHead + p.trailCount) % kTrailLen] = p.pos;
        ++p.trailCount;
    }
    else
    {
        p.trail[p.trailHead] = p.pos;                 // overwrite the oldest
        p.trailHead = (p.trailHead + 1) % kTrailLen;
    }
}

void Particles::Update(float dt, ImVec2 size, const ImVec2* mouse, float scale)
{
    if (size.x <= 0.0f || size.y <= 0.0f)
        return;                                       // minimized
    if (items_.empty())
        Spawn(size);

    dt = std::min(dt, 0.05f);                         // no huge jumps after a stall
    lastScale_ = scale;

    acc_ += dt;
    const bool record = acc_ >= kTrailStep;
    if (record)
        acc_ = std::fmod(acc_, kTrailStep);

    const float mouseDist = kMouseDist * scale;

    for (Particle& p : items_)
    {
        // The cursor bends the path; the speed is restored right after, so dots never stall.
        if (mouse && kMousePush > 0.0f)
        {
            const float dx = p.pos.x - mouse->x;
            const float dy = p.pos.y - mouse->y;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > 0.001f && d < mouseDist)
            {
                const float k = (1.0f - d / mouseDist) * kMousePush * scale * dt / d;
                p.vel.x += dx * k;
                p.vel.y += dy * k;
                float cur = std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
                if (cur < 0.001f) cur = 1.0f;
                p.vel.x = p.vel.x / cur * p.speed;
                p.vel.y = p.vel.y / cur * p.speed;
            }
        }

        p.pos.x += p.vel.x * scale * dt;
        p.pos.y += p.vel.y * scale * dt;

        // Bounce off the window edges.
        const float r = p.radius * scale;
        if (p.pos.x < r) { p.pos.x = r;               p.vel.x = std::fabs(p.vel.x); }
        else if (p.pos.x > size.x - r) { p.pos.x = size.x - r;      p.vel.x = -std::fabs(p.vel.x); }
        if (p.pos.y < r) { p.pos.y = r;               p.vel.y = std::fabs(p.vel.y); }
        else if (p.pos.y > size.y - r) { p.pos.y = size.y - r;      p.vel.y = -std::fabs(p.vel.y); }

        if (record)
            PushTrail(p);
    }
}

void Particles::Draw(ImDrawList* dl) const
{
    for (const Particle& p : items_)
    {
        const int r = (int)(p.rgb & 0xFF), g = (int)((p.rgb >> 8) & 0xFF), b = (int)((p.rgb >> 16) & 0xFF);
        const float radius = p.radius * lastScale_;

        // Trail: oldest -> newest, then the live position as the last point.
        const int points = p.trailCount + 1;
        ImVec2 prev = p.trailCount > 0 ? p.trail[p.trailHead] : p.pos;
        for (int k = 1; k < points; ++k)
        {
            const ImVec2 cur = (k < p.trailCount) ? p.trail[(p.trailHead + k) % kTrailLen] : p.pos;
            const float  t = (float)k / (float)(points - 1);          // 0 = oldest, 1 = newest
            dl->AddLine(prev, cur, IM_COL32(r, g, b, (int)(t * t * 150.0f)),
                (0.8f + t * radius * 0.9f));
            prev = cur;
        }

        dl->AddCircleFilled(p.pos, radius, IM_COL32(r, g, b, 210));   // core
    }
}