#pragma once

void LoadFonts(float dpiScale);

class ScopedTitleFont
{
public:
    ScopedTitleFont();
    ~ScopedTitleFont();
    ScopedTitleFont(const ScopedTitleFont&) = delete;
    ScopedTitleFont& operator=(const ScopedTitleFont&) = delete;

private:
    bool pushed_ = false;
};