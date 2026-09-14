#pragma once

#include <JuceHeader.h>
#include "HostGrid.h"
#include "RandomizationEngine.h"
#include <array>
#include <cstdint>

namespace randomchop
{
struct MeltSettings final
{
    float amountPercent = 0.0f;
};

class MeltProcessor final
{
public:
    void prepare(double newSampleRate);
    void reset() noexcept;
    void setSeed(uint64_t seed) noexcept;
    void process(juce::AudioBuffer<float>&, const GridBoundaries&, int gridChoice,
                 MeltSettings) noexcept;

    bool isActive() const noexcept { return active; }
    bool isArmed() const noexcept { return armed; }
    float getEventProgress() const noexcept;
    float getLastStretchRatio() const noexcept { return lastStretchRatio; }
    int getLastReversedSlices() const noexcept { return lastReversedSlices; }
    uint32_t getActiveReverseMask() const noexcept;
    uint64_t getActivationCount() const noexcept { return activationCount; }

private:
    static constexpr int maximumSlices = 4;
    static constexpr int windowTableSize = 4096;
    struct Slice final
    {
        double sourceOrigin = 0.0;
        double stretchRatio = 1.0;
        float levelGain = 1.0f;
        int sourceFrames = 2;
        bool reversed = false;
    };

    static float sanitise(float value) noexcept;
    void beginEvent(const GridBoundaries&, int gridChoice, float amount) noexcept;
    void configureSlice(int slice, int outputFrames, float amount) noexcept;
    float readCaptured(int channel, double logicalFrame) const noexcept;
    float renderSliceSample(int channel, int slice, int localFrame) const noexcept;
    void endEvent() noexcept;
    void invalidateHistory() noexcept;

    juce::AudioBuffer<float> history;
    std::array<Slice, maximumSlices> slices {};
    std::array<float, windowTableSize> hannWindow {};
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
    int grainFrames = 64;
    int synthesisHop = 16;
    int edgeFadeFrames = 1;
    int lastReversedSlices = 0;
    float eventWet = 0.0f;
    float lastStretchRatio = 1.0f;
    uint64_t activationCount = 0;
    bool recordDuringEvent = false;
    bool active = false;
    bool enabled = false;
    bool armed = false;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassGain { 0.0f };
};

struct SmearSettings
{
    float amount = 0.0f;
};

class SmearProcessor final
{
public:
    static constexpr int maximumGrains = 16;

    void prepare(double newSampleRate);
    void reset() noexcept;
    void setSeed(uint64_t seed) noexcept;
    void process(juce::AudioBuffer<float>& buffer, SmearSettings settings) noexcept;

    int getActiveGrainCount() const noexcept { return activeGrainCount; }
    int getPeakActiveGrainCount() const noexcept { return peakActiveGrainCount; }
    int getLastGrainLengthFrames() const noexcept { return lastGrainLengthFrames; }
    float getLastMotionAmount() const noexcept { return lastMotionAmount; }
    float getLastOverlapGain() const noexcept { return lastOverlapGain; }

private:
    static constexpr int modulationTableSize = 2048;
    struct Grain
    {
        double readPosition = 0.0;
        double increment = 1.0;
        float pan = 0.0f;
        float pitchPhase = 0.0f;
        float pitchRate = 0.0f;
        float panPhase = 0.0f;
        float panRate = 0.0f;
        float brightness = 1.0f;
        int age = 0;
        int length = 1;
        bool active = false;
    };

    static float sanitise(float value) noexcept;
    float readDelay(int channel, double position, double increment) const noexcept;
    float lookupSine(float phase) const noexcept;
    float lookupWindow(float phase) const noexcept;
    void startGrain(float amount) noexcept;
    void resetRealtimeState() noexcept;

    juce::AudioBuffer<float> delayBuffer;
    juce::AudioBuffer<float> mediumBandDelayBuffer;
    juce::AudioBuffer<float> highBandDelayBuffer;
    std::array<Grain, maximumGrains> grains;
    std::array<float, modulationTableSize> sineTable {};
    std::array<float, modulationTableSize> hannTable {};
    std::array<float, 2> lowState { 0.0f, 0.0f };
    std::array<float, 2> feedbackState { 0.0f, 0.0f };
    std::array<std::array<float, 4>, 2> mediumFilterState {};
    std::array<std::array<float, 4>, 2> highFilterState {};
    RandomizationEngine random;
    double sampleRate = 44100.0;
    float fastEnvelope = 0.0f;
    float slowEnvelope = 0.0f;
    float motionPhase = 0.0f;
    float lastMotionAmount = 0.0f;
    float overlapEnergy = 0.0f;
    float lastOverlapGain = 0.0f;
    float mediumFilterCoefficient = 0.0f;
    float highFilterCoefficient = 0.0f;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoother { 0.0f };
    int writePosition = 0;
    int validFrames = 0;
    int grainCountdown = 0;
    int activeGrainCount = 0;
    int peakActiveGrainCount = 0;
    int lastGrainLengthFrames = 0;
};
}

