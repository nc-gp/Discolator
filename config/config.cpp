#include "config.h"
#include "paths.h"

#include "json.hpp"

#include <algorithm>
#include <fstream>

using json = nlohmann::json;

// The key names match the Python version, so an old config.json keeps working.
Config Config::Load()
{
    Config c;
    try
    {
        std::ifstream file(paths::ConfigFile(), std::ios::binary);
        if (file)
        {
            const json j = json::parse(file);
            c.deleteOnStop = j.value("delete_on_stop", c.deleteOnStop);
            c.hideTaskbar = j.value("hide_taskbar", c.hideTaskbar);
            c.autoStopIndex = j.value("auto_idx", c.autoStopIndex);
            c.autoStopCustomMinutes = j.value("auto_custom", c.autoStopCustomMinutes);
        }
    }
    catch (...)
    {
        // Corrupt file or wrong types: fall back to defaults.
        c = Config();
    }
    c.Sanitize();
    return c;
}

void Config::Save() const
{
    json j;
    j["delete_on_stop"] = deleteOnStop;
    j["hide_taskbar"] = hideTaskbar;
    j["auto_idx"] = autoStopIndex;
    j["auto_custom"] = autoStopCustomMinutes;

    std::ofstream file(paths::ConfigFile(), std::ios::binary | std::ios::trunc);
    if (file)
        file << j.dump(2);
}

void Config::Sanitize()
{
    autoStopIndex = std::clamp(autoStopIndex, 0, kAutoStopPresetCount - 1);
    autoStopCustomMinutes = std::clamp(autoStopCustomMinutes, 1, 24 * 60);
}

int Config::AutoStopSeconds() const
{
    int minutes = kAutoStopPresets[autoStopIndex].minutes;
    if (minutes < 0)
        minutes = autoStopCustomMinutes;
    return minutes > 0 ? minutes * 60 : 0;
}