#pragma once

#include "PlaybackRegion.h"
#include "RandomizationEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace randomchop
{
struct ChanceSettings final
{
    float reverseChance = 0.0f;
    float retriggerChance = 0.0f;
    float skipChance = 0.0f;
    float reorderChance = 0.0f;
    float bendChance = 0.0f;
    float dropChance = 0.0f;
    float retriggerSizeMilliseconds = 100.0f;
    int retriggerCount = 2;
};

struct EventDecision final
{
    bool reverseEnabled = false;
    bool retriggerEnabled = false;
    int retriggerSizeFrames = 0;
    std::uint8_t retriggerCount = 1;
    bool skipEnabled = false;
    double skipJumpFrames = 0.0;
    bool reorderEnabled = false;
    std::array<std::uint8_t, 4> reorderPermutation { 0, 1, 2, 3 };
    double reorderPieceSpanFrames = 0.0;
    bool bendEnabled = false;
    float bendDepthSemitones = 0.0f;
    bool dropEnabled = false;
    std::uint8_t dropMask = 0;

    bool operator==(const EventDecision&) const = default;
};

inline float clampChance(float chance) noexcept
{
    return std::isfinite(chance) ? std::clamp(chance, 0.0f, 100.0f) : 0.0f;
}

inline bool rollChance(float chance, RandomizationEngine& random) noexcept
{
    chance = clampChance(chance);
    if (chance <= 0.0f)
        return false;
    if (chance >= 100.0f)
        return true;
    return random.unit() * 100.0 < static_cast<double>(chance);
}

inline double reverseEventStart(FrameRegion region, double start) noexcept
{
    const auto first = static_cast<double>(region.firstFrame);
    const auto last = lastInterpolationPosition(region);
    start = std::clamp(finiteOr(start, first), first, last);
    return last - (start - first);
}

inline double resolvedEventStart(FrameRegion region, double randomStart,
                                 const EventDecision& decision) noexcept
{
    const auto first = static_cast<double>(region.firstFrame);
    const auto last = lastInterpolationPosition(region);
    auto start = decision.reverseEnabled
        ? reverseEventStart(region, randomStart)
        : std::clamp(finiteOr(randomStart, first), first, last);
    if (decision.skipEnabled)
        start += finiteOr(decision.skipJumpFrames, 0.0);
    return std::clamp(start, first, last);
}

inline EventDecision resolveChanceEvent(const ChanceSettings& settings,
                                        FrameRegion region, double randomStart,
                                        double sourceSampleRate, double hostSampleRate,
                                        RandomizationEngine& random) noexcept
{
    EventDecision decision;
    if (!region.canInterpolate())
        return decision;

    // The call order here is the fixed V2 Chance order. Every random result is
    // copied into EventDecision so rendering and future history replay do not
    // depend on the generator's call sequence.
    decision.reverseEnabled = rollChance(settings.reverseChance, random);

    decision.retriggerEnabled = rollChance(settings.retriggerChance, random);
    if (decision.retriggerEnabled)
    {
        const auto milliseconds = std::isfinite(settings.retriggerSizeMilliseconds)
            ? std::clamp(settings.retriggerSizeMilliseconds, 10.0f, 500.0f) : 100.0f;
        const auto safeHostRate = std::clamp(
            finiteOr(hostSampleRate, 44100.0), 1.0, 768000.0);
        decision.retriggerSizeFrames = std::max(1, static_cast<int>(std::llround(
            static_cast<double>(milliseconds) * safeHostRate / 1000.0)));
        decision.retriggerCount = static_cast<std::uint8_t>(
            std::clamp(settings.retriggerCount, 1, 8));
    }

    decision.skipEnabled = rollChance(settings.skipChance, random);
    auto effectiveStart = resolvedEventStart(region, randomStart, decision);
    if (decision.skipEnabled)
    {
        const auto first = static_cast<double>(region.firstFrame);
        const auto last = lastInterpolationPosition(region);
        const auto target = first + (last - first) * random.unit();
        decision.skipJumpFrames = target - effectiveStart;
        effectiveStart = target;
    }

    decision.reorderEnabled = rollChance(settings.reorderChance, random);
    if (decision.reorderEnabled)
    {
        for (int index = 3; index > 0; --index)
            std::swap(decision.reorderPermutation[static_cast<std::size_t>(index)],
                      decision.reorderPermutation[random.bounded(
                          static_cast<std::uint32_t>(index + 1))]);
        if (decision.reorderPermutation == std::array<std::uint8_t, 4> { 0, 1, 2, 3 })
            std::swap(decision.reorderPermutation[0], decision.reorderPermutation[1]);

        const auto available = decision.reverseEnabled
            ? effectiveStart - static_cast<double>(region.firstFrame)
            : lastInterpolationPosition(region) - effectiveStart;
        const auto safeSourceRate = std::clamp(
            finiteOr(sourceSampleRate, 44100.0), 1.0, 768000.0);
        const auto fragmentSpan = std::min(safeSourceRate, std::max(0.0, available));
        decision.reorderPieceSpanFrames = fragmentSpan * 0.25;
        if (decision.reorderPieceSpanFrames <= 0.0)
            decision.reorderEnabled = false;
    }

    decision.bendEnabled = rollChance(settings.bendChance, random);
    if (decision.bendEnabled)
    {
        decision.bendDepthSemitones = static_cast<float>(random.unit() * 24.0 - 12.0);
        if (std::abs(decision.bendDepthSemitones) < 0.05f)
            decision.bendDepthSemitones = decision.bendDepthSemitones < 0.0f ? -0.05f : 0.05f;
    }

    decision.dropEnabled = rollChance(settings.dropChance, random);
    if (decision.dropEnabled)
    {
        for (int slot = 0; slot < 8; ++slot)
            if (random.unit() < 0.5)
                decision.dropMask |= static_cast<std::uint8_t>(1u << slot);
        if (decision.dropMask == 0)
            decision.dropMask = static_cast<std::uint8_t>(1u << random.bounded(8));
    }
    return decision;
}
}
