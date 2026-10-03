#pragma once

#include "PreparedSampleData.h"
#include <algorithm>
#include <cmath>

namespace randomchop
{
inline float clampStretchSpeed(float speed) noexcept
{
    if (!std::isfinite(speed))
        return 1.0f;
    const auto bounded = std::clamp(speed, 0.25f, 2.0f);
    return std::clamp(std::round(bounded * 4.0f) * 0.25f, 0.25f, 2.0f);
}

PreparedSamplePtr prepareStretch(
    const std::shared_ptr<const juce::AudioBuffer<float>>& decodedAudio,
    double sampleRate, float playbackSpeed, uint64_t revision);
}
