#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace randomchop
{
struct NormalisedRegion final
{
    double start = 0.0;
    double end = 1.0;
};

inline double finiteOr(double value, double fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

inline NormalisedRegion clampNormalisedRegion(double start, double end) noexcept
{
    start = std::clamp(finiteOr(start, 0.0), 0.0, 1.0);
    end = std::clamp(finiteOr(end, 1.0), 0.0, 1.0);
    if (end < start)
        end = start;
    return { start, end };
}

struct FrameRegion final
{
    int firstFrame = 0;
    int lastFrame = -1;

    bool canInterpolate() const noexcept { return lastFrame > firstFrame; }
};

inline FrameRegion makeFrameRegion(int sampleCount, double start, double end) noexcept
{
    if (sampleCount < 2)
        return {};

    const auto normalised = clampNormalisedRegion(start, end);
    const auto extent = static_cast<double>(sampleCount - 1);
    const auto first = std::clamp(static_cast<int>(std::ceil(normalised.start * extent)),
                                  0, sampleCount - 1);
    auto last = std::clamp(static_cast<int>(std::floor(normalised.end * extent)),
                           0, sampleCount - 1);
    if (last < first)
        last = first;
    return { first, last };
}

inline double lastInterpolationPosition(const FrameRegion& region) noexcept
{
    if (!region.canInterpolate())
        return static_cast<double>(region.firstFrame);
    return std::nextafter(static_cast<double>(region.lastFrame),
                          static_cast<double>(region.firstFrame));
}

inline bool isInterpolationPositionLegal(const FrameRegion& region, double position) noexcept
{
    if (!region.canInterpolate() || !std::isfinite(position)
        || position < static_cast<double>(region.firstFrame)
        || position >= static_cast<double>(region.lastFrame))
        return false;

    const auto index = static_cast<int>(std::floor(position));
    return index >= region.firstFrame && index + 1 <= region.lastFrame;
}

inline double maximumRandomStart(const FrameRegion& region, double sourceRate) noexcept
{
    if (!region.canInterpolate())
        return static_cast<double>(region.firstFrame);

    const auto safeRate = std::max(1.0, finiteOr(sourceRate, 1.0));
    const auto fadeMargin = std::max(2.0, safeRate * 0.003);
    return std::clamp(static_cast<double>(region.lastFrame) - fadeMargin,
                      static_cast<double>(region.firstFrame),
                      lastInterpolationPosition(region));
}

inline double resolveRandomStart(const FrameRegion& region, double sourceRate,
                                 double amount, double randomUnit) noexcept
{
    if (!region.canInterpolate())
        return static_cast<double>(region.firstFrame);

    amount = std::clamp(finiteOr(amount, 0.0), 0.0, 1.0);
    randomUnit = std::clamp(finiteOr(randomUnit, 0.0), 0.0,
                            std::nextafter(1.0, 0.0));
    const auto first = static_cast<double>(region.firstFrame);
    return first + (maximumRandomStart(region, sourceRate) - first) * amount * randomUnit;
}
}
