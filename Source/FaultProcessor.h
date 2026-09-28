#pragma once

#include <JuceHeader.h>
#include "HostGrid.h"
#include "RandomizationEngine.h"
#include <array>
#include <cstdint>

namespace randomchop
{
namespace FaultMutations
{
constexpr uint32_t pull = 1u << 0;
constexpr uint32_t dust = 1u << 1;
constexpr uint32_t bend = 1u << 2;
constexpr uint32_t all = pull | dust | bend;
}

enum class FaultMutation : uint8_t
{
    none = 0,
    pull = 1,
    dust = 2,
    bend = 3
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

private:
    static constexpr int windowTableSize = 4096;
    static constexpr double maximumHistorySeconds = 6.1;

    void beginSegment(double bpm, float pressure, uint32_t enabledMask) noexcept;
    int chooseDivision(float pressure) noexcept;
    FaultMutation chooseMutation(uint32_t enabledMask) noexcept;
    float renderOverlapAdd(int channel, int localFrame, bool pitchShift) const noexcept;
    float readHistory(int channel, double logicalFrame) const noexcept;
    float windowAt(double phase) const noexcept;
    void invalidateHistory() noexcept;
    static float sanitise(float value) noexcept;

    juce::AudioBuffer<float> history;
    std::array<float, windowTableSize> hannWindow {};
    RandomizationEngine random;
    double sampleRate = 44100.0;
    int writeFrame = 0;
    int validFrames = 0;
    int captureStart = 0;
    int captureFrames = 0;
    int segmentFrame = 0;
    int segmentFrames = 1;
    int segmentTicksRemaining = 0;
    int segmentTicks = 1;
    int divisionDenominator = 16;
    int fadeFrames = 1;
    int grainFrames = 1024;
    int synthesisHop = 256;
    int dustBits = 12;
    int dustHoldFrames = 2;
    int dustCountdown = 0;
    std::array<float, 2> dustHeld {};
    double stretchRatio = 1.0;
    double pitchRatio = 1.0;
    FaultMutation mutation = FaultMutation::none;
    std::array<uint64_t, 4> mutationCounts {};
    uint32_t observedDivisionMask = 0;
};
}
