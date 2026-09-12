#pragma once

#include <JuceHeader.h>
#include "HostGrid.h"
#include "RandomizationEngine.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace randomchop
{
struct ScrambleSettings final
{
    float amountPercent = 0.0f;
};

class ScrambleProcessor final
{
public:
    void prepare(double newSampleRate);
    void reset() noexcept;
    void setSeed(uint64_t seed) noexcept;
    void process(juce::AudioBuffer<float>&, const GridBoundaries&, int gridChoice,
                 ScrambleSettings) noexcept;

    int getLastCaptureFrames() const noexcept { return lastCaptureFrames; }
    int getLastManipulatedSlices() const noexcept { return lastManipulatedSlices; }
    int getLastPitchedSlices() const noexcept { return lastPitchedSlices; }
    uint64_t getActivationCount() const noexcept { return activationCount; }
    bool isActive() const noexcept { return active; }
    bool isArmed() const noexcept { return armed; }
    float getEventProgress() const noexcept
    {
        return active && eventFrames > 0
            ? std::clamp(static_cast<float>(eventFrame) / static_cast<float>(eventFrames),
                         0.0f, 1.0f)
            : 0.0f;
    }
    uint32_t getActiveSliceMask() const noexcept
    {
        uint32_t mask = 0;
        for (int slice = 0; slice < sliceCount; ++slice)
            if (manipulated[static_cast<std::size_t>(slice)])
                mask |= uint32_t { 1 } << static_cast<uint32_t>(slice);
        return active ? mask : 0;
    }

private:
    static constexpr int maximumSlices = 8;
    void beginEvent(const GridBoundaries&, int, float amount) noexcept;
    void configureSlice(int slice, float amount) noexcept;
    float readCaptured(int channel, double logicalFrame) const noexcept;
    void endEvent() noexcept;
    void invalidateHistory() noexcept;

    juce::AudioBuffer<float> history;
    std::array<double, maximumSlices> readOrigin {};
    std::array<double, maximumSlices> readOffset {};
    std::array<double, maximumSlices> readIncrement {};
    std::array<int, maximumSlices> loopFrames {};
    std::array<float, maximumSlices> sliceWet {};
    std::array<bool, maximumSlices> manipulated {};
    RandomizationEngine random;
    double sampleRate = 44100.0;
    int writeFrame = 0;
    int validFrames = 0;
    int captureStart = 0;
    int captureFrames = 0;
    int sliceFrames = 1;
    int sliceCount = 1;
    int eventFrame = 0;
    int eventFrames = 0;
    int eventBoundariesRemaining = 0;
    int fadeFrames = 1;
    int lastCaptureFrames = 0;
    int lastManipulatedSlices = 0;
    int lastPitchedSlices = 0;
    uint64_t activationCount = 0;
    bool recordDuringEvent = false;
    bool active = false;
    bool enabled = false;
    bool armed = false;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassGain { 0.0f };
};
}

