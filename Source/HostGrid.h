#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace randomchop
{
struct HostTiming final
{
    double bpm = 120.0;
    double ppq = 0.0;
    bool hasBpm = false;
    bool hasPpq = false;
    bool isPlaying = false;
};

struct GridBoundaries final
{
    // Large enough for unusually long offline blocks while remaining fixed-size
    // and allocation-free on the audio thread.
    static constexpr int capacity = 4096;
    std::array<int, capacity> sampleOffsets {};
    int count = 0;
    bool usedHostClock = false;
    bool transportDiscontinuity = false;
    bool gridChanged = false;
    bool truncated = false;
    double bpm = 120.0;
};

class HostGrid final
{
public:
    static constexpr double quarterNotesPerStep(int choice) noexcept
    {
        constexpr std::array<double, 3> lengths { 0.5, 0.25, 0.125 };
        return choice >= 0 && choice < static_cast<int>(lengths.size())
            ? lengths[static_cast<std::size_t>(choice)] : lengths[1];
    }

    void reset() noexcept
    {
        lastValidBpm = 120.0;
        fallbackSamplesToBoundary = 0.0;
        previousHostPpq = 0.0;
        previousHostBpm = 120.0;
        previousHostFrames = 0;
        previousHostValid = false;
        previousChoice = 1;
    }

    GridBoundaries process(double sampleRate, int numSamples, int divisionChoice,
                           HostTiming timing) noexcept
    {
        GridBoundaries result;
        const auto safeRate = std::clamp(finiteOr(sampleRate, 44100.0), 1.0, 768000.0);
        numSamples = std::max(0, numSamples);
        divisionChoice = std::clamp(divisionChoice, 0, 2);
        const auto stepPpq = quarterNotesPerStep(divisionChoice);
        const bool bpmValid = timing.hasBpm && std::isfinite(timing.bpm)
            && timing.bpm > 0.0;
        const bool ppqValid = timing.hasPpq && std::isfinite(timing.ppq);
        if (bpmValid)
            lastValidBpm = std::clamp(timing.bpm, 20.0, 400.0);
        result.bpm = lastValidBpm;

        const bool useHost = bpmValid && ppqValid && timing.isPlaying;
        result.usedHostClock = useHost;
        const bool choiceChanged = divisionChoice != previousChoice;
        result.gridChanged = choiceChanged;
        previousChoice = divisionChoice;

        if (useHost)
        {
            const auto bpm = std::clamp(timing.bpm, 20.0, 400.0);
            result.bpm = bpm;
            if (previousHostValid)
            {
                const auto expected = previousHostPpq
                    + static_cast<double>(previousHostFrames) * previousHostBpm
                        / (60.0 * safeRate);
                const auto tolerance = std::max(1.0e-7, bpm / (30.0 * safeRate));
                result.transportDiscontinuity
                    = std::abs(timing.ppq - expected) > tolerance;
            }
            else
            {
                result.transportDiscontinuity = true;
            }

            const auto samplesPerQuarter = 60.0 * safeRate / bpm;
            const auto halfSamplePpq = 0.5 / samplesPerQuarter;
            const auto epsilon = 1.0e-9;
            auto boundaryIndex = static_cast<std::int64_t>(
                std::ceil((timing.ppq - halfSamplePpq - epsilon) / stepPpq));
            for (;; ++boundaryIndex)
            {
                const auto boundaryPpq = static_cast<double>(boundaryIndex) * stepPpq;
                const auto exactOffset = (boundaryPpq - timing.ppq) * samplesPerQuarter;
                if (exactOffset >= static_cast<double>(numSamples) - 0.5)
                    break;
                if (exactOffset < -0.5)
                    continue;
                const auto offset = static_cast<int>(std::floor(exactOffset + 0.5));
                if (offset < 0 || offset >= numSamples)
                    continue;
                if (result.count > 0
                    && result.sampleOffsets[static_cast<std::size_t>(result.count - 1)] == offset)
                    continue;
                if (result.count >= GridBoundaries::capacity)
                {
                    result.truncated = true;
                    break;
                }
                result.sampleOffsets[static_cast<std::size_t>(result.count++)] = offset;
            }

            previousHostPpq = timing.ppq;
            previousHostBpm = bpm;
            previousHostFrames = numSamples;
            previousHostValid = true;
            fallbackSamplesToBoundary = samplesPerStep(safeRate, bpm, stepPpq);
            return result;
        }

        const bool switchedFromHost = previousHostValid;
        result.transportDiscontinuity = switchedFromHost;
        previousHostValid = false;
        const auto interval = samplesPerStep(safeRate, lastValidBpm, stepPpq);
        if (switchedFromHost || choiceChanged || !std::isfinite(fallbackSamplesToBoundary)
            || fallbackSamplesToBoundary < 0.0
            || fallbackSamplesToBoundary > interval)
            fallbackSamplesToBoundary = 0.0;

        auto cursor = fallbackSamplesToBoundary;
        while (cursor < static_cast<double>(numSamples) - 0.5)
        {
            if (cursor >= -0.5)
            {
                const auto offset = std::clamp(static_cast<int>(std::floor(cursor + 0.5)),
                                               0, std::max(0, numSamples - 1));
                if ((result.count == 0
                     || result.sampleOffsets[static_cast<std::size_t>(result.count - 1)] != offset)
                    && result.count < GridBoundaries::capacity)
                    result.sampleOffsets[static_cast<std::size_t>(result.count++)] = offset;
                else if (result.count >= GridBoundaries::capacity)
                {
                    result.truncated = true;
                    break;
                }
            }
            cursor += interval;
        }
        if (cursor < static_cast<double>(numSamples))
        {
            const auto skipped = std::floor(
                (static_cast<double>(numSamples) - cursor) / interval) + 1.0;
            cursor += skipped * interval;
        }
        fallbackSamplesToBoundary = cursor - static_cast<double>(numSamples);
        return result;
    }

private:
    static double finiteOr(double value, double fallback) noexcept
    {
        return std::isfinite(value) ? value : fallback;
    }

    static double samplesPerStep(double sampleRate, double bpm,
                                 double quarterNotes) noexcept
    {
        return std::max(1.0, sampleRate * 60.0 * quarterNotes
                             / std::clamp(bpm, 20.0, 400.0));
    }

    double lastValidBpm = 120.0;
    double fallbackSamplesToBoundary = 0.0;
    double previousHostPpq = 0.0;
    double previousHostBpm = 120.0;
    int previousHostFrames = 0;
    bool previousHostValid = false;
    int previousChoice = 1;
};
}

