#pragma once

#include "PreparedSampleData.h"
#include <algorithm>
#include <cmath>

namespace randomchop
{
inline float clampStretchSpeed(float speed) noexcept
{
    return std::isfinite(speed) ? std::clamp(speed, 0.25f, 2.0f) : 1.0f;
}

PreparedSamplePtr prepareStretch(
    const std::shared_ptr<const juce::AudioBuffer<float>>& decodedAudio,
    double sampleRate, float playbackSpeed, uint64_t revision);
}
