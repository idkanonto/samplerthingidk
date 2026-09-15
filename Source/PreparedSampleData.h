#pragma once

#include <JuceHeader.h>
#include <memory>

struct PreparedSampleData final
{
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double sampleRate = 44100.0;
};

using PreparedSamplePtr = std::shared_ptr<const PreparedSampleData>;
