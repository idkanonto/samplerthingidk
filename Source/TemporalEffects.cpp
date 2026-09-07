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

float eventFade(std::int64_t frame, std::int64_t total, int fadeFrames) noexcept
{
    const auto fade = static_cast<float>(std::max(1, fadeFrames));
    const auto attack = std::clamp(static_cast<float>(frame) / fade, 0.0f, 1.0f);
    const auto remaining = std::max<std::int64_t>(0, total - frame);
    const auto release = std::clamp(static_cast<float>(remaining) / fade, 0.0f, 1.0f);
    return std::min(attack, release);
}
}

void FreezeProcessor::prepare(double newSampleRate)
{
    sampleRate = safeSampleRate(newSampleRate);
    history.setSize(2, std::max(2, static_cast<int>(
        std::ceil(sampleRate * maximumHistorySeconds))), false, true, false);
    history.clear();
    fadeFrames = std::max(1, static_cast<int>(std::llround(sampleRate * 0.003)));
    reset();
}

void FreezeProcessor::reset() noexcept
{
    resetRealtimeState();
    activationCount = 0;
    lastOctaveSemitones = 0;
    lastCaptureFrames = 0;
}

void FreezeProcessor::resetRealtimeState() noexcept
{
    writeFrame = 0;
    validFrames = 0;
    captureStart = 0;
    captureFrames = 0;
    readFrame = 0.0;
    eventFrame = 0;
    eventFrames = 0;
    active = false;
}

void FreezeProcessor::setSeed(uint64_t seed) noexcept
{
    random.setSeed(seed ^ 0x667265657a652d31ULL);
    resetRealtimeState();
}

bool FreezeProcessor::roll(float percent) noexcept
{
    if (!std::isfinite(percent) || percent <= 0.0f)
        return false;
    if (percent >= 100.0f)
        return true;
    return random.unit() < static_cast<double>(percent) * 0.01;
}

void FreezeProcessor::beginEvent(const GridBoundaries& boundaries, int gridChoice,
                                 FreezeSettings settings) noexcept
{
    if (active || validFrames < 2 || !roll(settings.chancePercent))
        return;

    constexpr std::array<double, 3> sizeMultipliers { 0.25, 0.5, 1.0 };
    constexpr std::array<int, 4> holdMultipliers { 1, 2, 4, 8 };
    const auto sizeIndex = std::clamp(settings.sizeChoice, 0, 2);
    const auto holdIndex = std::clamp(settings.holdChoice, 0, 3);
    const auto step = gridFrames(sampleRate, boundaries.bpm, gridChoice);
    const auto wantedCapture = std::max(2, static_cast<int>(std::llround(
        static_cast<double>(step) * sizeMultipliers[static_cast<size_t>(sizeIndex)])));
    captureFrames = std::min({ wantedCapture, validFrames, history.getNumSamples() });
    if (captureFrames < 2)
        return;
    captureStart = writeFrame - captureFrames;
    if (captureStart < 0)
        captureStart += history.getNumSamples();

    lastOctaveSemitones = 0;
    readIncrement = 1.0;
    if (roll(settings.octaveChancePercent))
    {
        lastOctaveSemitones = random.bounded(2) == 0 ? -12 : 12;
        readIncrement = lastOctaveSemitones < 0 ? 0.5 : 2.0;
    }
    readFrame = 0.0;
    eventFrame = 0;
    eventFrames = static_cast<std::int64_t>(step)
        * holdMultipliers[static_cast<size_t>(holdIndex)];
    lastCaptureFrames = captureFrames;
    ++activationCount;
    active = true;
}

float FreezeProcessor::readCaptured(int channel, double logicalFrame) const noexcept
{
    if (captureFrames < 2 || history.getNumSamples() < 2)
        return 0.0f;
    logicalFrame = std::fmod(logicalFrame, static_cast<double>(captureFrames));
    if (logicalFrame < 0.0)
        logicalFrame += captureFrames;
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

void FreezeProcessor::process(juce::AudioBuffer<float>& buffer,
                              const GridBoundaries& boundaries, int gridChoice,
                              FreezeSettings settings) noexcept
{
    if (history.getNumSamples() < 2 || buffer.getNumChannels() < 1)
        return;
    if (boundaries.transportDiscontinuity)
        resetRealtimeState();

    int boundaryIndex = 0;
    const auto capacity = history.getNumSamples();
    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        while (boundaryIndex < boundaries.count
               && boundaries.sampleOffsets[static_cast<size_t>(boundaryIndex)] == frame)
        {
            beginEvent(boundaries, gridChoice, settings);
            ++boundaryIndex;
        }

        const float dry[2] {
            sanitise(buffer.getSample(0, frame)),
            sanitise(buffer.getSample(std::min(1, buffer.getNumChannels() - 1), frame))
        };
        if (!active)
        {
            history.setSample(0, writeFrame, dry[0]);
            history.setSample(1, writeFrame, dry[1]);
            writeFrame = (writeFrame + 1) % capacity;
            validFrames = std::min(validFrames + 1, capacity);
        }

        if (active)
        {
            const auto wet = eventFade(eventFrame, eventFrames, fadeFrames);
            for (int channel = 0; channel < std::min(2, buffer.getNumChannels()); ++channel)
            {
                const auto frozen = sanitise(readCaptured(channel, readFrame));
                buffer.setSample(channel, frame, sanitise(
                    dry[channel] + wet * (frozen - dry[channel])));
            }
            readFrame += readIncrement;
            while (readFrame >= static_cast<double>(captureFrames))
                readFrame -= static_cast<double>(captureFrames);
            ++eventFrame;
            if (eventFrame >= eventFrames)
                resetRealtimeState();
        }
        else
        {
            for (int channel = 0; channel < std::min(2, buffer.getNumChannels()); ++channel)
                buffer.setSample(channel, frame, dry[channel]);
        }
    }
}

void ScrambleProcessor::prepare(double newSampleRate)
{
    sampleRate = safeSampleRate(newSampleRate);
    history.setSize(2, std::max(2, static_cast<int>(
        std::ceil(sampleRate * maximumHistorySeconds))), false, true, false);
    history.clear();
    fadeFrames = std::max(1, static_cast<int>(std::llround(sampleRate * 0.002)));
    reset();
}

void ScrambleProcessor::reset() noexcept
{
    resetRealtimeState();
    activationCount = 0;
    lastCaptureFrames = 0;
}

void ScrambleProcessor::resetRealtimeState() noexcept
{
    writeFrame = 0;
    validFrames = 0;
    captureStart = 0;
    captureFrames = 0;
    chunkFrames = 1;
    chunkCount = 1;
    eventFrame = 0;
    active = false;
}

void ScrambleProcessor::setSeed(uint64_t seed) noexcept
{
    random.setSeed(seed ^ 0x736372616d626c65ULL);
    resetRealtimeState();
}

bool ScrambleProcessor::roll(float percent) noexcept
{
    if (!std::isfinite(percent) || percent <= 0.0f)
        return false;
    if (percent >= 100.0f)
        return true;
    return random.unit() < static_cast<double>(percent) * 0.01;
}

void ScrambleProcessor::beginEvent(const GridBoundaries& boundaries, int gridChoice,
                                   ScrambleSettings settings) noexcept
{
    const auto amount = std::clamp(
        std::isfinite(settings.amountPercent) ? settings.amountPercent * 0.01f : 0.0f,
        0.0f, 1.0f);
    if (active || validFrames < 2 || amount <= 0.0f || !roll(settings.chancePercent))
        return;

    const auto step = gridFrames(sampleRate, boundaries.bpm, gridChoice);
    captureFrames = std::min({ step, validFrames, history.getNumSamples() });
    if (captureFrames < 2)
        return;
    captureStart = writeFrame - captureFrames;
    if (captureStart < 0)
        captureStart += history.getNumSamples();
    chunkCount = std::clamp(captureFrames / 8, 2, maximumChunks);
    chunkFrames = std::max(1, (captureFrames + chunkCount - 1) / chunkCount);

    bool changed = false;
    for (int chunk = 0; chunk < chunkCount; ++chunk)
    {
        sourceChunk[static_cast<size_t>(chunk)] = chunk;
        reverseChunk[static_cast<size_t>(chunk)] = false;
        if (random.unit() < amount)
        {
            sourceChunk[static_cast<size_t>(chunk)] = static_cast<int>(
                random.bounded(static_cast<uint32_t>(chunkCount)));
            reverseChunk[static_cast<size_t>(chunk)] = random.bounded(2) != 0;
            changed = changed || sourceChunk[static_cast<size_t>(chunk)] != chunk
                || reverseChunk[static_cast<size_t>(chunk)];
        }
    }
    if (!changed)
    {
        sourceChunk[0] = chunkCount - 1;
        reverseChunk[0] = true;
    }

    eventFrame = 0;
    lastCaptureFrames = captureFrames;
    ++activationCount;
    active = true;
}

float ScrambleProcessor::readCaptured(int channel, int logicalFrame) const noexcept
{
    if (captureFrames < 1 || history.getNumSamples() < 1)
        return 0.0f;
    logicalFrame = std::clamp(logicalFrame, 0, captureFrames - 1);
    const auto physical = (captureStart + logicalFrame) % history.getNumSamples();
    return history.getSample(std::clamp(channel, 0, 1), physical);
}

void ScrambleProcessor::process(juce::AudioBuffer<float>& buffer,
                                const GridBoundaries& boundaries, int gridChoice,
                                ScrambleSettings settings) noexcept
{
    if (history.getNumSamples() < 2 || buffer.getNumChannels() < 1)
        return;
    if (boundaries.transportDiscontinuity)
        resetRealtimeState();

    int boundaryIndex = 0;
    const auto capacity = history.getNumSamples();
    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        while (boundaryIndex < boundaries.count
               && boundaries.sampleOffsets[static_cast<size_t>(boundaryIndex)] == frame)
        {
            beginEvent(boundaries, gridChoice, settings);
            ++boundaryIndex;
        }

        const float dry[2] {
            sanitise(buffer.getSample(0, frame)),
            sanitise(buffer.getSample(std::min(1, buffer.getNumChannels() - 1), frame))
        };
        if (!active)
        {
            history.setSample(0, writeFrame, dry[0]);
            history.setSample(1, writeFrame, dry[1]);
            writeFrame = (writeFrame + 1) % capacity;
            validFrames = std::min(validFrames + 1, capacity);
        }

        if (active)
        {
            const auto outputChunk = std::min(chunkCount - 1, eventFrame / chunkFrames);
            const auto localFrame = eventFrame % chunkFrames;
            const auto selectedChunk = sourceChunk[static_cast<size_t>(outputChunk)];
            const auto sourceFirst = selectedChunk * chunkFrames;
            const auto sourceLength = std::max(1,
                std::min(chunkFrames, captureFrames - sourceFirst));
            const auto sourceLocal = std::min(localFrame, sourceLength - 1);
            const auto scrambledFrame = sourceFirst
                + (reverseChunk[static_cast<size_t>(outputChunk)]
                    ? sourceLength - 1 - sourceLocal : sourceLocal);
            const auto eventWet = eventFade(eventFrame, captureFrames, fadeFrames);
            const auto edgeDistance = std::min(sourceLocal, sourceLength - 1 - sourceLocal);
            const auto chunkWet = std::clamp(
                static_cast<float>(edgeDistance) / static_cast<float>(fadeFrames),
                0.0f, 1.0f);
            const auto wet = std::min(eventWet, chunkWet);
            for (int channel = 0; channel < std::min(2, buffer.getNumChannels()); ++channel)
            {
                const auto scrambled = sanitise(readCaptured(channel, scrambledFrame));
                buffer.setSample(channel, frame, sanitise(
                    dry[channel] + wet * (scrambled - dry[channel])));
            }
            ++eventFrame;
            if (eventFrame >= captureFrames)
                resetRealtimeState();
        }
        else
        {
            for (int channel = 0; channel < std::min(2, buffer.getNumChannels()); ++channel)
                buffer.setSample(channel, frame, dry[channel]);
        }
    }
}
}
