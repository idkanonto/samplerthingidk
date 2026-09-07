#pragma once

#include <JuceHeader.h>
#include "HostGrid.h"
#include "RandomizationEngine.h"
#include <array>
#include <cstdint>

namespace randomchop
{
struct FreezeSettings final
{
    float chancePercent = 0.0f;
    int sizeChoice = 1;
    int holdChoice = 1;
    float octaveChancePercent = 0.0f;
};

struct ScrambleSettings final
{
    float chancePercent = 0.0f;
    float amountPercent = 50.0f;
};

class FreezeProcessor final
{
public:
    void prepare(double newSampleRate);
    void reset() noexcept;
    void setSeed(uint64_t seed) noexcept;
    void process(juce::AudioBuffer<float>&, const GridBoundaries&, int gridChoice,
                 FreezeSettings) noexcept;

    int getLastOctaveSemitones() const noexcept { return lastOctaveSemitones; }
    int getLastCaptureFrames() const noexcept { return lastCaptureFrames; }
    uint64_t getActivationCount() const noexcept { return activationCount; }
    bool isActive() const noexcept { return active; }

private:
    bool roll(float percent) noexcept;
    void beginEvent(const GridBoundaries&, int, FreezeSettings) noexcept;
    float readCaptured(int channel, double logicalFrame) const noexcept;
    void resetRealtimeState() noexcept;

    juce::AudioBuffer<float> history;
    RandomizationEngine random;
    double sampleRate = 44100.0;
    int writeFrame = 0;
    int validFrames = 0;
    int captureStart = 0;
    int captureFrames = 0;
    double readFrame = 0.0;
    double readIncrement = 1.0;
    std::int64_t eventFrame = 0;
    std::int64_t eventFrames = 0;
    int fadeFrames = 1;
    int lastOctaveSemitones = 0;
    int lastCaptureFrames = 0;
    uint64_t activationCount = 0;
    bool active = false;
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
    uint64_t getActivationCount() const noexcept { return activationCount; }
    bool isActive() const noexcept { return active; }

private:
    static constexpr int maximumChunks = 8;
    bool roll(float percent) noexcept;
    void beginEvent(const GridBoundaries&, int, ScrambleSettings) noexcept;
    float readCaptured(int channel, int logicalFrame) const noexcept;
    void resetRealtimeState() noexcept;

    juce::AudioBuffer<float> history;
    std::array<int, maximumChunks> sourceChunk {};
    std::array<bool, maximumChunks> reverseChunk {};
    RandomizationEngine random;
    double sampleRate = 44100.0;
    int writeFrame = 0;
    int validFrames = 0;
    int captureStart = 0;
    int captureFrames = 0;
    int chunkFrames = 1;
    int chunkCount = 1;
    int eventFrame = 0;
    int fadeFrames = 1;
    int lastCaptureFrames = 0;
    uint64_t activationCount = 0;
    bool active = false;
};
}
