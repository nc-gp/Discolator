#include "particles.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kTwoPi = 6.28318530718f;

    // Color of a dot at a given time: slides back and forth between cyan, blue, purple and pink.
    ImU32 DotColor(float phase, float time, int alpha)
    {
        const float wave = 0.5f + 0.5f * std::sin(time * 0.25f + phase);
        const float hue = Particles::kHueMin + (Particles::kHueMax - Particles::kHueMin) * wave;
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(hue, 0.55f, 1.0f, r, g, b);
        return IM_COL32((int)(r * 255.0f), (int)(g * 255.0f), (int)(b * 255.0f), std::clamp(alpha, 0, 255));
    }
}

void Particles::Spawn(ImVec2 size)
{
    std::uniform_real_distribution<float> angle(0.0f, kTwoPi);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    items_.assign(kCount, Particle{});
    for (Particle& p : items_)
    {
        // Layers: 50% far, 30% middle, 20% near.
        const float roll = unit(rng_);
        p.depth = roll < 0.50f ? 0.45f : (roll < 0.80f ? 0.70f : 1.00f);

        const float a = angle(rng_);
        p.speed = (30.0f + unit(rng_) * 70.0f) * p.depth;           // far layers move slower
        p.vel = ImVec2(std::cos(a) * p.speed, std::sin(a) * p.speed);
        p.radius = (1.4f + unit(rng_) * 2.2f) * (0.55f + 0.45f * p.depth);
        p.pos = ImVec2(unit(rng_) * size.x, unit(rng_) * size.y);
        p.phase = unit(rng_) * kTwoPi;
    }
    // Draw far dots first so near dots end up on top.
    std::sort(items_.begin(), items_.end(), [](const Particle& a, const Particle& b) { return a.depth < b.depth; });

    bokeh_.assign(kBokehCount, Bokeh{});
    for (Bokeh& b : bokeh_)
    {
        const float a = angle(rng_);
        const float s = 6.0f + unit(rng_) * 8.0f;
        b.vel = ImVec2(std::cos(a) * s, std::sin(a) * s);
        b.radius = 45.0f + unit(rng_) * 60.0f;
        b.pos = ImVec2(unit(rng_) * size.x, unit(rng_) * size.y);
        b.phase = unit(rng_) * kTwoPi;
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
    lastSize_ = size;
    time_ += dt;

    acc_ += dt;
    const bool record = acc_ >= kTrailStep;
    if (record)
        acc_ = std::fmod(acc_, kTrailStep);

    const float mouseDist = kMouseDist * scale;

    for (Particle& p : items_)
    {
        // The cursor bends the path (near dots react more); speed is restored right after.
        if (mouse && kMousePush > 0.0f)
        {
            const float dx = p.pos.x - mouse->x;
            const float dy = p.pos.y - mouse->y;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > 0.001f && d < mouseDist)
            {
                const float k = (1.0f - d / mouseDist) * kMousePush * p.depth * scale * dt / d;
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

    // The big faint circles just drift and wrap around.
    for (Bokeh& b : bokeh_)
    {
        b.pos.x += b.vel.x * scale * dt;
        b.pos.y += b.vel.y * scale * dt;
        const float m = b.radius * scale;
        if (b.pos.x < -m)              b.pos.x = size.x + m;
        else if (b.pos.x > size.x + m) b.pos.x = -m;
        if (b.pos.y < -m)              b.pos.y = size.y + m;
        else if (b.pos.y > size.y + m) b.pos.y = -m;
    }
}

void Particles::Draw(ImDrawList* dl) const
{
    // Faint blurry circles (several stacked, smaller ones brighter, fake a soft edge).
    for (const Bokeh& b : bokeh_)
    {
        const float pulse = 0.7f + 0.3f * std::sin(time_ * 0.3f + b.phase);
        for (int ring = 0; ring < 4; ++ring)
        {
            const float f = 1.0f - 0.22f * (float)ring;
            dl->AddCircleFilled(b.pos, b.radius * lastScale_ * f,
                DotColor(b.phase, time_, (int)(5.0f * kOpacity * pulse * 2.0f)), 48);
        }
    }

    for (const Particle& p : items_)
    {
        const float twinkle = 0.75f + 0.25f * std::sin(time_ * 1.6f + p.phase * 3.0f);
        const float amount = kOpacity * p.depth * twinkle;       // far = dimmer
        const float radius = p.radius * lastScale_;

        // Trail: oldest -> newest, then the live position as the last point.
        const int points = p.trailCount + 1;
        ImVec2 prev = p.trailCount > 0 ? p.trail[p.trailHead] : p.pos;
        for (int k = 1; k < points; ++k)
        {
            const ImVec2 cur = (k < p.trailCount) ? p.trail[(p.trailHead + k) % kTrailLen] : p.pos;
            const float  t = (float)k / (float)(points - 1);       // 0 = oldest, 1 = newest
            dl->AddLine(prev, cur, DotColor(p.phase, time_, (int)(t * t * 150.0f * amount)),
                0.8f + t * radius * 0.9f);
            prev = cur;
        }

        dl->AddCircleFilled(p.pos, radius, DotColor(p.phase, time_, (int)(220.0f * amount)));
    }
}