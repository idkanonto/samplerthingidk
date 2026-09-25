#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace randomchop
{
inline float outputPercentToDecibels(float percent) noexcept
{
    percent = std::isfinite(percent) ? std::clamp(percent, 0.0f, 125.0f) : 100.0f;
    if (percent <= 0.0f)
        return -std::numeric_limits<float>::infinity();

    const auto ratio = percent * 0.01f;
    auto decibels = 32.5f * std::log10(ratio);
    if (percent > 100.0f)
    {
        const auto position = (percent - 100.0f) / 25.0f;
        const auto smoothstep = position * position * (3.0f - 2.0f * position);
        constexpr auto boostCorrection = 5.6f - 32.5f * 0.09691001300805642f;
        decibels += boostCorrection * smoothstep;
    }
    return decibels;
}

inline float outputPercentToGain(float percent) noexcept
{
    const auto decibels = outputPercentToDecibels(percent);
    return std::isfinite(decibels) ? std::pow(10.0f, decibels * 0.05f) : 0.0f;
}

inline float outputDecibelsToPercent(float decibels) noexcept
{
    if (!std::isfinite(decibels))
        return decibels < 0.0f ? 0.0f : 125.0f;

    auto low = 0.0f;
    auto high = 125.0f;
    for (int iteration = 0; iteration < 32; ++iteration)
    {
        const auto middle = (low + high) * 0.5f;
        if (outputPercentToDecibels(middle) < decibels)
            low = middle;
        else
            high = middle;
    }
    return std::clamp((low + high) * 0.5f, 0.0f, 125.0f);
}
}
