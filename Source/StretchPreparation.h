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
    if (!std::isfinite(ratio) || ratio < 1.0f)
        return 0.0f;
    return std::clamp(ratio, 1.0f, 4.0f);
}

inline float stretchDurationMultiplier(float storedRatio) noexcept
{
    const auto setting = clampStretchRatio(storedRatio);
    return setting <= 1.0f ? 1.0f : setting;
}

PreparedSamplePtr prepareStretch(
    const std::shared_ptr<const juce::AudioBuffer<float>>& decodedAudio,
    double sampleRate, float durationMultiplier, uint64_t revision);
}
