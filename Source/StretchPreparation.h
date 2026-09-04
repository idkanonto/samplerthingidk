#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

struct PreparedSampleData final
{
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double sampleRate = 44100.0;
    uint64_t revision = 0;
    float stretchRatio = 1.0f;
};

using PreparedSamplePtr = std::shared_ptr<const PreparedSampleData>;

namespace randomchop
{
inline float clampStretchRatio(float ratio) noexcept
{
    return std::isfinite(ratio) ? std::clamp(ratio, 0.5f, 2.0f) : 1.0f;
}

PreparedSamplePtr prepareStretch(
    const std::shared_ptr<const juce::AudioBuffer<float>>& decodedAudio,
    double sampleRate, float durationMultiplier, uint64_t revision);
}
