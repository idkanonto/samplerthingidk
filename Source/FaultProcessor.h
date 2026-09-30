#pragma once

#include <JuceHeader.h>
#include "HostGrid.h"
#include "RandomizationEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace randomchop
{
inline double faultPitchPlaybackRate(int semitones) noexcept
{
    return std::exp2(static_cast<double>(std::clamp(semitones, -12, 12)) / 12.0);
}

inline int faultPitchPlaybackFrames(int sourceFrames, int semitones) noexcept
{
    return std::max(1, static_cast<int>(std::ceil(
        static_cast<double>(std::max(1, sourceFrames))
            / faultPitchPlaybackRate(semitones))));
}

namespace FaultMutations
{
constexpr uint32_t pitch = 1u << 0;
constexpr uint32_t bitcrush = 1u << 1;
constexpr uint32_t reverse = 1u << 2;
constexpr uint32_t all = pitch | bitcrush | reverse;
}

enum class FaultMutation : uint8_t
{
    none = 0,
    pitch = 1,
    bitcrush = 2,
    reverse = 3
};

struct FaultSettings final
{
    float pressurePercent = 0.0f;
    uint32_t enabledMutations = FaultMutations::all;
};

// Tempo-aligned mutation engine. The caller supplies a 1/16-note boundary
// stream; FAULT groups those ticks into 1/2, 1/4, 1/8, or 1/16 segments.
// Every event choice is latched once at the segment boundary.
class FaultProcessor final
{
public:
    void prepare(double newSampleRate);
    void reset() noexcept;
    void setSeed(uint64_t seed) noexcept;
    void process(juce::AudioBuffer<float>&, const GridBoundaries&, FaultSettings) noexcept;

    FaultMutation getCurrentMutation() const noexcept { return mutation; }
    int getCurrentDivisionDenominator() const noexcept { return divisionDenominator; }
    float getSegmentProgress() const noexcept;
    bool isMutating() const noexcept { return mutation != FaultMutation::none; }
    uint64_t getMutationCount(FaultMutation type) const noexcept;
    uint32_t getObservedDivisionMask() const noexcept { return observedDivisionMask; }
    int getLastPitchSemitones() const noexcept { return pitchSemitones; }
    int getLastSourceFrames() const noexcept { return lastMutationSourceFrames; }
    int getLastPlaybackFrames() const noexcept { return lastMutationPlaybackFrames; }

private:
    static constexpr double maximumHistorySeconds = 18.1;

    void beginSegment(double bpm, float pressure, uint32_t enabledMask) noexcept;
    int chooseDivision(float pressure) noexcept;
    FaultMutation chooseMutation(uint32_t enabledMask) noexcept;
    float readCapturedSlice(int channel, double sourceFrame) const noexcept;
    void invalidateHistory() noexcept;
    static float sanitise(float value) noexcept;

    juce::AudioBuffer<float> history;
    RandomizationEngine random;
    double sampleRate = 44100.0;
    int writeFrame = 0;
    int validFrames = 0;
    int captureStart = 0;
    int captureFrames = 0;
    int segmentFrame = 0;
    int segmentFrames = 1;
    int playbackFrames = 1;
    int segmentTicksRemaining = 0;
    int segmentTicks = 1;
    int divisionDenominator = 16;
    int fadeFrames = 1;
    int dustBits = 12;
    int dustHoldFrames = 2;
    int dustCountdown = 0;
    std::array<float, 2> dustHeld {};
    double pitchRatio = 1.0;
    int pitchSemitones = 0;
    int lastMutationSourceFrames = 0;
    int lastMutationPlaybackFrames = 0;
    FaultMutation mutation = FaultMutation::none;
    std::array<uint64_t, 4> mutationCounts {};
    uint32_t observedDivisionMask = 0;
};
}
