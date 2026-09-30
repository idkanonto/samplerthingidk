#include "FaultProcessor.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace randomchop
{
namespace
{
constexpr std::array<int, 4> denominators { 2, 4, 8, 16 };
constexpr std::array<int, 4> ticksPerDivision { 8, 4, 2, 1 };
constexpr std::array<int, 4> dustBitDepths { 12, 10, 8, 6 };
constexpr std::array<int, 4> dustHolds { 2, 3, 4, 6 };
}

float FaultProcessor::sanitise(float value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, -64.0f, 64.0f) : 0.0f;
}

void FaultProcessor::prepare(double newSampleRate)
{
    sampleRate = std::clamp(std::isfinite(newSampleRate) ? newSampleRate : 44100.0,
                            1.0, 768000.0);
    history.setSize(2, std::max(2, static_cast<int>(
        std::ceil(sampleRate * maximumHistorySeconds))), false, true, false);
    history.clear();
    fadeFrames = std::max(1, static_cast<int>(std::llround(sampleRate * 0.006)));
    reset();
}

void FaultProcessor::reset() noexcept
{
    history.clear();
    mutationCounts.fill(0);
    observedDivisionMask = 0;
    invalidateHistory();
}

void FaultProcessor::invalidateHistory() noexcept
{
    writeFrame = 0;
    validFrames = 0;
    captureStart = 0;
    captureFrames = 0;
    segmentFrame = 0;
    segmentFrames = 1;
    playbackFrames = 1;
    segmentTicksRemaining = 0;
    mutation = FaultMutation::none;
    dustCountdown = 0;
    dustHeld.fill(0.0f);
    pitchSemitones = 0;
    lastMutationSourceFrames = 0;
    lastMutationPlaybackFrames = 0;
}

void FaultProcessor::setSeed(uint64_t seed) noexcept
{
    random.setSeed(seed ^ 0x6661756c742d6473ULL);
    invalidateHistory();
}

int FaultProcessor::chooseDivision(float pressure) noexcept
{
    // A continuous center moves from half notes toward the 1/8–1/16 region.
    // Triangular weights make adjacent values crossfade instead of jumping.
    const auto center = std::clamp(pressure * 2.85f, 0.0f, 2.85f);
    std::array<double, 4> weights {};
    auto total = 0.0;
    for (int index = 0; index < 4; ++index)
    {
        const auto distance = std::abs(static_cast<double>(index) - center);
        const auto weight = std::max(0.0, 1.35 - distance);
        weights[static_cast<size_t>(index)] = weight * weight;
        total += weight * weight;
    }
    auto roll = random.unit() * std::max(total, 1.0e-9);
    for (int index = 0; index < 4; ++index)
    {
        roll -= weights[static_cast<size_t>(index)];
        if (roll <= 0.0)
            return index;
    }
    return 3;
}

FaultMutation FaultProcessor::chooseMutation(uint32_t enabledMask) noexcept
{
    std::array<FaultMutation, 3> choices {};
    int count = 0;
    if ((enabledMask & FaultMutations::pitch) != 0) choices[count++] = FaultMutation::pitch;
    if ((enabledMask & FaultMutations::bitcrush) != 0) choices[count++] = FaultMutation::bitcrush;
    if ((enabledMask & FaultMutations::reverse) != 0) choices[count++] = FaultMutation::reverse;
    return count == 0 ? FaultMutation::none
        : choices[static_cast<size_t>(random.bounded(static_cast<uint32_t>(count)))];
}

void FaultProcessor::beginSegment(double bpm, float pressure, uint32_t enabledMask) noexcept
{
    const auto division = chooseDivision(pressure);
    divisionDenominator = denominators[static_cast<size_t>(division)];
    segmentTicks = ticksPerDivision[static_cast<size_t>(division)];
    segmentTicksRemaining = segmentTicks;
    observedDivisionMask |= uint32_t { 1 } << static_cast<uint32_t>(division);
    bpm = std::clamp(std::isfinite(bpm) ? bpm : 120.0, 20.0, 400.0);
    const auto quarterNotes = 4.0 / static_cast<double>(divisionDenominator);
    segmentFrames = std::max(1, static_cast<int>(std::llround(
        sampleRate * 60.0 * quarterNotes / bpm)));
    segmentFrame = 0;
    captureFrames = std::min({ validFrames, segmentFrames, history.getNumSamples() });
    captureStart = writeFrame - captureFrames;
    if (captureStart < 0)
        captureStart += history.getNumSamples();

    const auto chance = 0.92 * std::pow(static_cast<double>(pressure), 1.25);
    mutation = pressure > 0.0f && enabledMask != 0 && random.unit() < chance
        ? chooseMutation(enabledMask) : FaultMutation::none;

    playbackFrames = segmentFrames;
    if ((mutation == FaultMutation::pitch || mutation == FaultMutation::reverse)
        && captureFrames < 2)
        mutation = FaultMutation::none;

    if (mutation == FaultMutation::pitch)
    {
        // Choose every non-zero integer interval in the requested -12..+12
        // range. This is direct sampler repitching: rate, pitch, and duration
        // remain coupled with no OLA, formant preservation, or compensation.
        const auto choice = static_cast<int>(random.bounded(24));
        pitchSemitones = choice < 12 ? choice - 12 : choice - 11;
        pitchRatio = faultPitchPlaybackRate(pitchSemitones);
        playbackFrames = faultPitchPlaybackFrames(captureFrames, pitchSemitones);
    }
    else if (mutation == FaultMutation::bitcrush)
    {
        const auto profile = static_cast<size_t>(random.bounded(4));
        dustBits = dustBitDepths[profile];
        dustHoldFrames = dustHolds[profile];
        dustCountdown = 0;
    }
    else if (mutation == FaultMutation::reverse)
        playbackFrames = captureFrames;

    if (mutation != FaultMutation::none)
    {
        lastMutationSourceFrames = captureFrames;
        lastMutationPlaybackFrames = playbackFrames;
        ++mutationCounts[static_cast<size_t>(mutation)];
    }
}

float FaultProcessor::readCapturedSlice(int channel, double sourceFrame) const noexcept
{
    if (captureFrames < 2 || history.getNumSamples() < 2)
        return 0.0f;
    sourceFrame = std::clamp(sourceFrame, 0.0,
        static_cast<double>(captureFrames - 1));
    const auto first = static_cast<int>(sourceFrame);
    const auto second = std::min(first + 1, captureFrames - 1);
    const auto fraction = static_cast<float>(sourceFrame - first);
    const auto capacity = history.getNumSamples();
    const auto firstPhysical = (captureStart + first) % capacity;
    const auto secondPhysical = (captureStart + second) % capacity;
    const auto* values = history.getReadPointer(std::clamp(channel, 0, 1));
    return values[firstPhysical]
        + fraction * (values[secondPhysical] - values[firstPhysical]);
}

void FaultProcessor::process(juce::AudioBuffer<float>& buffer,
                             const GridBoundaries& boundaries,
                             FaultSettings settings) noexcept
{
    if (buffer.getNumChannels() < 1 || history.getNumSamples() < 2)
        return;
    if (boundaries.transportDiscontinuity || boundaries.gridChanged)
        invalidateHistory();

    const auto pressure = std::clamp(std::isfinite(settings.pressurePercent)
        ? settings.pressurePercent * 0.01f : 0.0f, 0.0f, 1.0f);
    const auto enabledMask = settings.enabledMutations & FaultMutations::all;
    int boundaryIndex = 0;
    const auto channels = std::min(2, buffer.getNumChannels());
    const auto capacity = history.getNumSamples();

    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        while (boundaryIndex < boundaries.count
               && boundaries.sampleOffsets[static_cast<size_t>(boundaryIndex)] == frame)
        {
            if (segmentTicksRemaining <= 0)
            {
                if (mutation == FaultMutation::none)
                    beginSegment(boundaries.bpm, pressure, enabledMask);
            }
            else if (--segmentTicksRemaining <= 0)
            {
                if (mutation == FaultMutation::none)
                    beginSegment(boundaries.bpm, pressure, enabledMask);
            }
            ++boundaryIndex;
        }

        std::array<float, 2> dry {
            sanitise(buffer.getSample(0, frame)),
            sanitise(buffer.getSample(std::min(1, buffer.getNumChannels() - 1), frame))
        };
        std::array<float, 2> wet = dry;

        if (mutation == FaultMutation::pitch)
            for (int channel = 0; channel < channels; ++channel)
                wet[static_cast<size_t>(channel)] = readCapturedSlice(
                    channel, static_cast<double>(segmentFrame) * pitchRatio);
        else if (mutation == FaultMutation::reverse)
            for (int channel = 0; channel < channels; ++channel)
                wet[static_cast<size_t>(channel)] = readCapturedSlice(
                    channel, static_cast<double>(captureFrames - 1 - segmentFrame));
        else if (mutation == FaultMutation::bitcrush)
        {
            if (dustCountdown <= 0)
            {
                const auto levels = static_cast<float>((1u << (dustBits - 1)) - 1u);
                for (int channel = 0; channel < channels; ++channel)
                    dustHeld[static_cast<size_t>(channel)] = std::round(
                        std::clamp(dry[static_cast<size_t>(channel)], -1.0f, 1.0f) * levels) / levels;
                dustCountdown = dustHoldFrames;
            }
            --dustCountdown;
            wet = dustHeld;
        }

        auto mix = mutation == FaultMutation::none ? 0.0f : 1.0f;
        const auto edge = std::min(fadeFrames, std::max(1, playbackFrames / 2));
        if (segmentFrame < edge)
        {
            const auto phase = static_cast<double>(segmentFrame) / static_cast<double>(edge);
            mix *= static_cast<float>(std::sin(phase * juce::MathConstants<double>::halfPi)
                                      * std::sin(phase * juce::MathConstants<double>::halfPi));
        }
        const auto remaining = playbackFrames - 1 - segmentFrame;
        if (remaining < edge)
        {
            const auto phase = static_cast<double>(std::max(0, remaining))
                / static_cast<double>(edge);
            mix *= static_cast<float>(std::sin(phase * juce::MathConstants<double>::halfPi)
                                      * std::sin(phase * juce::MathConstants<double>::halfPi));
        }

        for (int channel = 0; channel < channels; ++channel)
            buffer.setSample(channel, frame, sanitise(
                dry[static_cast<size_t>(channel)]
                + mix * (wet[static_cast<size_t>(channel)] - dry[static_cast<size_t>(channel)])));

        history.setSample(0, writeFrame, dry[0]);
        history.setSample(1, writeFrame, dry[1]);
        writeFrame = (writeFrame + 1) % capacity;
        validFrames = std::min(validFrames + 1, capacity);
        ++segmentFrame;
        if (mutation != FaultMutation::none && segmentFrame >= playbackFrames)
            mutation = FaultMutation::none;
    }
}

float FaultProcessor::getSegmentProgress() const noexcept
{
    return std::clamp(static_cast<float>(segmentFrame)
        / static_cast<float>(std::max(1, playbackFrames)), 0.0f, 1.0f);
}

uint64_t FaultProcessor::getMutationCount(FaultMutation type) const noexcept
{
    const auto index = static_cast<size_t>(type);
    return index < mutationCounts.size() ? mutationCounts[index] : 0;
}
}
