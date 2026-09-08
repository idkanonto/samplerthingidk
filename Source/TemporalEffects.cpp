#include "TemporalEffects.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace randomchop
{
namespace
{
constexpr double maximumHistorySeconds = 2.0;

double safeSampleRate(double value) noexcept
{
    return std::clamp(std::isfinite(value) ? value : 44100.0, 1.0, 768000.0);
}

float sanitise(float value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, -64.0f, 64.0f) : 0.0f;
}

int gridFrames(double sampleRate, double bpm, int gridChoice) noexcept
{
    bpm = std::clamp(std::isfinite(bpm) ? bpm : 120.0, 20.0, 400.0);
    const auto frames = sampleRate * 60.0
        * HostGrid::quarterNotesPerStep(gridChoice) / bpm;
    return std::max(1, static_cast<int>(std::llround(std::min(
        frames, static_cast<double>(std::numeric_limits<int>::max())))));
}

float edgeFade(int frame, int total, int fadeFrames) noexcept
{
    const auto fade = static_cast<float>(std::max(1, fadeFrames));
    const auto attack = std::clamp(static_cast<float>(frame) / fade, 0.0f, 1.0f);
    const auto remaining = std::max(0, total - 1 - frame);
    const auto release = std::clamp(static_cast<float>(remaining) / fade, 0.0f, 1.0f);
    return std::min(attack, release);
}

double wrap(double value, double length) noexcept
{
    if (length <= 0.0)
        return 0.0;
    value = std::fmod(value, length);
    return value < 0.0 ? value + length : value;
}
}

void ScrambleProcessor::prepare(double newSampleRate)
{
    sampleRate = safeSampleRate(newSampleRate);
    history.setSize(2, std::max(2, static_cast<int>(
        std::ceil(sampleRate * maximumHistorySeconds))), false, true, false);
    history.clear();
    fadeFrames = std::max(1, static_cast<int>(std::llround(sampleRate * 0.0025)));
    reset();
}

void ScrambleProcessor::reset() noexcept
{
    invalidateHistory();
    activationCount = 0;
    lastCaptureFrames = 0;
    lastManipulatedSlices = 0;
    lastPitchedSlices = 0;
}

void ScrambleProcessor::endEvent() noexcept
{
    captureStart = 0;
    captureFrames = 0;
    sliceFrames = 1;
    sliceCount = 1;
    eventFrame = 0;
    eventFrames = 0;
    recordDuringEvent = false;
    active = false;
}

void ScrambleProcessor::invalidateHistory() noexcept
{
    endEvent();
    writeFrame = 0;
    validFrames = 0;
}

void ScrambleProcessor::setSeed(uint64_t seed) noexcept
{
    random.setSeed(seed ^ 0x736372616d626c65ULL);
    invalidateHistory();
}

void ScrambleProcessor::configureSlice(int slice, float amount) noexcept
{
    const auto sourceSliceFrames = std::max(2, captureFrames / sliceCount);
    const auto sourceSlice = static_cast<int>(random.bounded(
        static_cast<uint32_t>(sliceCount)));
    const auto origin = std::min(captureFrames - 2, sourceSlice * sourceSliceFrames);
    const auto available = std::max(2, std::min(sourceSliceFrames, captureFrames - origin));
    const auto choice = random.unit();
    const auto pitchWeight = 0.02 + 0.34 * static_cast<double>(amount * amount);
    const auto holdWeight = 0.20 + 0.16 * static_cast<double>(amount);
    const auto reverseWeight = 0.18 + 0.10 * static_cast<double>(amount);

    readOrigin[static_cast<std::size_t>(slice)] = static_cast<double>(origin);
    readOffset[static_cast<std::size_t>(slice)] = 0.0;
    readIncrement[static_cast<std::size_t>(slice)] = 1.0;
    loopFrames[static_cast<std::size_t>(slice)] = available;

    if (choice < pitchWeight)
    {
        const auto upward = random.bounded(2) != 0;
        readIncrement[static_cast<std::size_t>(slice)] = upward ? 2.0 : 0.5;
        readOffset[static_cast<std::size_t>(slice)] = upward
            ? 0.0 : 0.25 * static_cast<double>(available);
        ++lastPitchedSlices;
    }
    else if (choice < pitchWeight + holdWeight)
    {
        const auto fraction = 0.72 - 0.58 * static_cast<double>(amount);
        loopFrames[static_cast<std::size_t>(slice)] = std::max(2,
            static_cast<int>(std::llround(static_cast<double>(available) * fraction)));
        readOffset[static_cast<std::size_t>(slice)] = static_cast<double>(
            random.bounded(static_cast<uint32_t>(std::max(1,
                available - loopFrames[static_cast<std::size_t>(slice)] + 1))));
    }
    else if (choice < pitchWeight + holdWeight + reverseWeight)
    {
        readOffset[static_cast<std::size_t>(slice)] = static_cast<double>(available - 1);
        readIncrement[static_cast<std::size_t>(slice)] = -1.0;
    }
    else
    {
        const auto maximumOffset = std::max(1, available / 2);
        readOffset[static_cast<std::size_t>(slice)] = static_cast<double>(
            random.bounded(static_cast<uint32_t>(maximumOffset)));
    }

    const auto baseWet = 0.20f + 0.80f * std::pow(amount, 0.62f);
    sliceWet[static_cast<std::size_t>(slice)] = std::clamp(baseWet
        * (0.88f + 0.12f * static_cast<float>(random.unit())), 0.0f, 1.0f);
}

void ScrambleProcessor::beginEvent(const GridBoundaries& boundaries, int gridChoice,
                                   float amount) noexcept
{
    if (active || validFrames < 2 || amount <= 0.0f)
        return;

    const auto step = gridFrames(sampleRate, boundaries.bpm, gridChoice);
    captureFrames = std::min({ step, validFrames, history.getNumSamples() });
    if (captureFrames < 2)
        return;
    captureStart = writeFrame - captureFrames;
    if (captureStart < 0)
        captureStart += history.getNumSamples();

    const auto longEventChance = std::max(0.0f, amount - 0.42f) * 0.72f;
    eventFrames = step * (random.unit() < longEventChance ? 2 : 1);
    recordDuringEvent = eventFrames <= history.getNumSamples() - captureFrames;
    sliceCount = std::clamp(4 + static_cast<int>(std::floor(amount * 4.0f)),
                            4, maximumSlices);
    sliceFrames = std::max(1, (eventFrames + sliceCount - 1) / sliceCount);
    manipulated.fill(false);
    sliceWet.fill(0.0f);
    readIncrement.fill(1.0);
    loopFrames.fill(1);

    std::array<int, maximumSlices> order {};
    for (int slice = 0; slice < sliceCount; ++slice)
        order[static_cast<std::size_t>(slice)] = slice;
    for (int slice = sliceCount - 1; slice > 0; --slice)
    {
        const auto other = static_cast<int>(random.bounded(
            static_cast<uint32_t>(slice + 1)));
        std::swap(order[static_cast<std::size_t>(slice)],
                  order[static_cast<std::size_t>(other)]);
    }

    lastManipulatedSlices = std::clamp(
        static_cast<int>(std::ceil(amount * static_cast<float>(sliceCount))),
        1, sliceCount);
    lastPitchedSlices = 0;
    for (int index = 0; index < lastManipulatedSlices; ++index)
    {
        const auto slice = order[static_cast<std::size_t>(index)];
        manipulated[static_cast<std::size_t>(slice)] = true;
        configureSlice(slice, amount);
    }

    // Carry a resolved gesture into an adjacent selected slice sometimes. This
    // creates phrase-like repeats instead of a bag of independent random switches.
    if (amount > 0.28f && random.unit() < 0.18 + 0.42 * amount)
    {
        for (int slice = 1; slice < sliceCount; ++slice)
        {
            if (!manipulated[static_cast<std::size_t>(slice - 1)]
                || !manipulated[static_cast<std::size_t>(slice)])
                continue;
            readOrigin[static_cast<std::size_t>(slice)]
                = readOrigin[static_cast<std::size_t>(slice - 1)];
            readOffset[static_cast<std::size_t>(slice)]
                = readOffset[static_cast<std::size_t>(slice - 1)];
            readIncrement[static_cast<std::size_t>(slice)]
                = readIncrement[static_cast<std::size_t>(slice - 1)];
            loopFrames[static_cast<std::size_t>(slice)]
                = loopFrames[static_cast<std::size_t>(slice - 1)];
            break;
        }
    }

    eventFrame = 0;
    lastCaptureFrames = captureFrames;
    ++activationCount;
    active = true;
}

float ScrambleProcessor::readCaptured(int channel, double logicalFrame) const noexcept
{
    if (captureFrames < 2 || history.getNumSamples() < 2)
        return 0.0f;
    logicalFrame = wrap(logicalFrame, static_cast<double>(captureFrames));
    const auto first = static_cast<int>(logicalFrame);
    const auto second = (first + 1) % captureFrames;
    const auto fraction = static_cast<float>(logicalFrame - first);
    const auto capacity = history.getNumSamples();
    const auto firstPhysical = (captureStart + first) % capacity;
    const auto secondPhysical = (captureStart + second) % capacity;
    const auto* values = history.getReadPointer(std::clamp(channel, 0, 1));
    return values[firstPhysical]
        + fraction * (values[secondPhysical] - values[firstPhysical]);
}

void ScrambleProcessor::process(juce::AudioBuffer<float>& buffer,
                                const GridBoundaries& boundaries, int gridChoice,
                                ScrambleSettings settings) noexcept
{
    if (history.getNumSamples() < 2 || buffer.getNumChannels() < 1)
        return;
    if (boundaries.transportDiscontinuity)
        invalidateHistory();

    const auto amount = std::clamp(std::isfinite(settings.amountPercent)
        ? settings.amountPercent * 0.01f : 0.0f, 0.0f, 1.0f);
    if (amount <= 0.0f && active)
        endEvent();

    int boundaryIndex = 0;
    const auto capacity = history.getNumSamples();
    const auto channels = std::min(2, buffer.getNumChannels());
    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        while (boundaryIndex < boundaries.count
               && boundaries.sampleOffsets[static_cast<std::size_t>(boundaryIndex)] == frame)
        {
            beginEvent(boundaries, gridChoice, amount);
            ++boundaryIndex;
        }

        const float dry[2] {
            sanitise(buffer.getSample(0, frame)),
            sanitise(buffer.getSample(std::min(1, buffer.getNumChannels() - 1), frame))
        };

        if (active)
        {
            const auto slice = std::min(sliceCount - 1, eventFrame / sliceFrames);
            const auto localFrame = eventFrame - slice * sliceFrames;
            const auto activeFrames = std::min(sliceFrames, eventFrames - slice * sliceFrames);
            auto wet = 0.0f;
            double sourceFrame = 0.0;
            if (manipulated[static_cast<std::size_t>(slice)])
            {
                const auto loop = static_cast<double>(
                    loopFrames[static_cast<std::size_t>(slice)]);
                const auto localRead = wrap(readOffset[static_cast<std::size_t>(slice)]
                    + static_cast<double>(localFrame)
                        * readIncrement[static_cast<std::size_t>(slice)], loop);
                sourceFrame = readOrigin[static_cast<std::size_t>(slice)] + localRead;
                wet = sliceWet[static_cast<std::size_t>(slice)]
                    * edgeFade(localFrame, activeFrames, fadeFrames);
            }
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto scrambled = sanitise(readCaptured(channel, sourceFrame));
                buffer.setSample(channel, frame, sanitise(
                    dry[channel] + wet * (scrambled - dry[channel])));
            }
            ++eventFrame;
            if (eventFrame >= eventFrames)
                endEvent();
        }
        else
        {
            for (int channel = 0; channel < channels; ++channel)
                buffer.setSample(channel, frame, dry[channel]);
        }

        if (!active || recordDuringEvent)
        {
            history.setSample(0, writeFrame, dry[0]);
            history.setSample(1, writeFrame, dry[1]);
            writeFrame = (writeFrame + 1) % capacity;
            validFrames = std::min(validFrames + 1, capacity);
        }
    }
}
}
