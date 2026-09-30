#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <memory>

struct PreparedSampleData final
{
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double sampleRate = 44100.0;
    uint64_t revision = 0;
    float stretchSpeed = 1.0f;
};

using PreparedSamplePtr = std::shared_ptr<const PreparedSampleData>;
