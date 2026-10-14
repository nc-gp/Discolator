#pragma once

struct AutoStopPreset
{
    const char* label;
    int         minutes;   // 0 = off, -1 = use the custom value
};

inline constexpr AutoStopPreset kAutoStopPresets[] = {
    { "Off",        0 },
    { "15 min",    15 },
    { "30 min",    30 },
    { "1 hour",    60 },
    { "2 hours",  120 },
    { "4 hours",  240 },
    { "8 hours",  480 },
    { "Custom...", -1 },
};
inline constexpr int kAutoStopPresetCount = (int)(sizeof(kAutoStopPresets) / sizeof(kAutoStopPresets[0]));

struct Config
{
    bool deleteOnStop = true;
    bool hideTaskbar = false;
    int  autoStopIndex = 0;    // index into kAutoStopPresets
    int  autoStopCustomMinutes = 90;

    // Reads config.json (missing or broken file -> defaults).
    static Config Load();
    void Save() const;

    // Keeps values inside valid ranges.
    void Sanitize();

    // Auto-stop duration for newly started games, 0 = never.
    int AutoStopSeconds() const;
};