#include "SpectralDraw.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace randomchop
{
SpectralMaskStore::SpectralMaskStore() noexcept
{
    slots[0].snapshot.values = canonicalCanvas;
    slots[0].snapshot.generation = 0;
    slots[0].snapshot.hasContent = false;
    slots[0].state.store(published, std::memory_order_relaxed);
}

bool SpectralMaskStore::setCanvas(const Canvas& canvas)
{
    Canvas cleaned {};
    for (std::size_t index = 0; index < cleaned.size(); ++index)
    {
        const auto value = canvas[index];
        cleaned[index] = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
    }
    const std::lock_guard<std::mutex> lock(canonicalMutex);
    canonicalCanvas = cleaned;
    return publishLocked(canonicalCanvas);
}

void SpectralMaskStore::clear()
{
    setCanvas(Canvas {});
}

SpectralMaskStore::Canvas SpectralMaskStore::copyCanvas() const
{
    const std::lock_guard<std::mutex> lock(canonicalMutex);
    return canonicalCanvas;
}

juce::String SpectralMaskStore::encodeCanvas() const
{
    const auto canvas = copyCanvas();
    return juce::MemoryBlock(canvas.data(), canvas.size() * sizeof(float)).toBase64Encoding();
}

bool SpectralMaskStore::restoreEncodedCanvas(const juce::String& encoded)
{
    if (encoded.isEmpty())
    {
        clear();
        return true;
    }
    juce::MemoryBlock decoded;
    if (!decoded.fromBase64Encoding(encoded)
        || decoded.getSize() != cellCount * sizeof(float))
        return false;
    Canvas restored {};
    std::memcpy(restored.data(), decoded.getData(), decoded.getSize());
    return setCanvas(restored);
}

uint64_t SpectralMaskStore::getPublishedGeneration() const noexcept
{
    return generationCounter.load(std::memory_order_acquire);
}

bool SpectralMaskStore::publishLocked(const Canvas& canvas) noexcept
{
    int candidate = -1;
    for (int index = 0; index < slotCount; ++index)
    {
        int expected = free;
        if (slots[static_cast<std::size_t>(index)].state.compare_exchange_strong(
                expected, writing, std::memory_order_acq_rel))
        {
            candidate = index;
            break;
        }
    }
    if (candidate < 0)
        return false;

    auto& next = slots[static_cast<std::size_t>(candidate)].snapshot;
    next.values = canvas;
    next.hasContent = std::any_of(canvas.begin(), canvas.end(),
        [](float value) { return value > 0.000001f; });
    next.generation = generationCounter.fetch_add(1, std::memory_order_acq_rel) + 1;
    slots[static_cast<std::size_t>(candidate)].state.store(published,
                                                           std::memory_order_release);
    const auto previous = publishedSlot.exchange(candidate, std::memory_order_acq_rel);
    if (previous >= 0 && previous != candidate)
    {
        int expected = published;
        slots[static_cast<std::size_t>(previous)].state.compare_exchange_strong(
            expected, free, std::memory_order_acq_rel);
    }
    return true;
}

SpectralMaskStore::ReadHandle SpectralMaskStore::acquire() noexcept
{
    for (int attempt = 0; attempt < slotCount * 2; ++attempt)
    {
        const auto index = publishedSlot.load(std::memory_order_acquire);
        if (index < 0 || index >= slotCount)
            return {};
        int expected = published;
        if (slots[static_cast<std::size_t>(index)].state.compare_exchange_strong(
                expected, reading, std::memory_order_acq_rel))
            return { &slots[static_cast<std::size_t>(index)].snapshot, index };
    }
    return {};
}

void SpectralMaskStore::release(ReadHandle handle) noexcept
{
    if (handle.slot < 0 || handle.slot >= slotCount)
        return;
    auto& state = slots[static_cast<std::size_t>(handle.slot)].state;
    state.store(published, std::memory_order_release);
    if (publishedSlot.load(std::memory_order_acquire) != handle.slot)
    {
        int expected = published;
        state.compare_exchange_strong(expected, free, std::memory_order_acq_rel);
    }
}

void SpectralDrawProcessor::prepare(double newSampleRate)
{
    sampleRate = std::clamp(std::isfinite(newSampleRate) ? newSampleRate : 44100.0,
                            1000.0, 768000.0);
    fft.resize(fftSize);
    for (int index = 0; index < fftSize; ++index)
    {
        const auto hann = 0.5f - 0.5f * std::cos(
            juce::MathConstants<float>::twoPi * static_cast<float>(index)
            / static_cast<float>(fftSize));
        window[static_cast<std::size_t>(index)] = std::sqrt(std::max(0.0f, hann));
    }
    depthSmoother.reset(sampleRate, 0.020);
    bypassMix.reset(sampleRate, 0.020);
    reset();
}

void SpectralDrawProcessor::reset() noexcept
{
    for (auto& channel : inputRing)
        channel.fill(0.0f);
    for (auto& channel : outputRing)
        channel.fill(0.0f);
    for (auto& channel : dryDelay)
        channel.fill(0.0f);
    timeData.fill(Complex {});
    frequencyData.fill(Complex {});
    smoothedBinGains.fill(1.0f);
    frameBinGains.fill(1.0f);
    scannerPhase = 0.0;
    inputWritePosition = 0;
    outputReadPosition = 0;
    dryDelayPosition = 0;
    samplesUntilFrame = hopSize;
    depthSmoother.setCurrentAndTargetValue(0.0f);
    bypassMix.setCurrentAndTargetValue(0.0f);
    publishedScanPosition.store(0.0f, std::memory_order_relaxed);
}

float SpectralDrawProcessor::sanitise(float value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, -64.0f, 64.0f) : 0.0f;
}

float SpectralDrawProcessor::maskValue(const SpectralMaskStore::Snapshot* snapshot,
                                       float scanPosition,
                                       float frequencyNormalised) noexcept
{
    if (snapshot == nullptr || !snapshot->hasContent)
        return 0.0f;
    const auto x = std::clamp(scanPosition, 0.0f, 1.0f)
        * static_cast<float>(SpectralMaskStore::canvasWidth);
    const auto xBase = static_cast<int>(std::floor(x)) % SpectralMaskStore::canvasWidth;
    const auto xNext = (xBase + 1) % SpectralMaskStore::canvasWidth;
    const auto xFraction = x - std::floor(x);
    const auto curvedFrequency = std::sqrt(std::clamp(frequencyNormalised, 0.0f, 1.0f));
    const auto y = (1.0f - curvedFrequency)
        * static_cast<float>(SpectralMaskStore::canvasHeight - 1);
    const auto yBase = std::clamp(static_cast<int>(std::floor(y)),
                                  0, SpectralMaskStore::canvasHeight - 1);
    const auto yNext = std::min(yBase + 1, SpectralMaskStore::canvasHeight - 1);
    const auto yFraction = y - static_cast<float>(yBase);
    const auto at = [snapshot](int column, int row)
    {
        const auto index = static_cast<std::size_t>(
            row * SpectralMaskStore::canvasWidth + column);
        return snapshot->values[index];
    };
    const auto upper = at(xBase, yBase) + (at(xNext, yBase) - at(xBase, yBase)) * xFraction;
    const auto lower = at(xBase, yNext) + (at(xNext, yNext) - at(xBase, yNext)) * xFraction;
    return std::clamp(upper + (lower - upper) * yFraction, 0.0f, 1.0f);
}

void SpectralDrawProcessor::processFrame(const SpectralMaskStore::Snapshot* snapshot,
                                         float depth, float scanPosition) noexcept
{
    constexpr auto inverseScale = 0.5f / static_cast<float>(fftSize);
    constexpr auto frameSmoothing = 0.35f;
    for (int foldedBin = 0; foldedBin <= fftSize / 2; ++foldedBin)
    {
        const auto frequency = static_cast<float>(foldedBin)
            / static_cast<float>(fftSize / 2);
        const auto target = snapshot != nullptr && snapshot->hasContent
            ? std::clamp(1.0f - depth
                * maskValue(snapshot, scanPosition, frequency), 0.0f, 1.0f)
            : 1.0f;
        auto& smoothed = smoothedBinGains[static_cast<std::size_t>(foldedBin)];
        smoothed += frameSmoothing * (target - smoothed);
    }
    for (int bin = 0; bin < fftSize; ++bin)
        frameBinGains[static_cast<std::size_t>(bin)]
            = smoothedBinGains[static_cast<std::size_t>(std::min(bin, fftSize - bin))];

    for (int channel = 0; channel < 2; ++channel)
    {
        for (int index = 0; index < fftSize; ++index)
        {
            const auto ringIndex = (inputWritePosition + index) % fftSize;
            timeData[static_cast<std::size_t>(index)] = {
                inputRing[static_cast<std::size_t>(channel)]
                         [static_cast<std::size_t>(ringIndex)]
                    * window[static_cast<std::size_t>(index)],
                0.0f
            };
        }
        fft.fft(timeData.data(), frequencyData.data());
        for (int bin = 0; bin < fftSize; ++bin)
            frequencyData[static_cast<std::size_t>(bin)]
                *= frameBinGains[static_cast<std::size_t>(bin)];
        fft.ifft(frequencyData.data(), timeData.data());
        for (int index = 0; index < fftSize; ++index)
        {
            const auto ringIndex = (outputReadPosition + index) % fftSize;
            auto& output = outputRing[static_cast<std::size_t>(channel)]
                                      [static_cast<std::size_t>(ringIndex)];
            output = sanitise(output
                + timeData[static_cast<std::size_t>(index)].real()
                    * window[static_cast<std::size_t>(index)] * inverseScale);
        }
    }
}

void SpectralDrawProcessor::process(juce::AudioBuffer<float>& buffer,
                                    SpectralMaskStore& maskStore,
                                    SpectralDrawSettings settings) noexcept
{
    if (settings.transportDiscontinuity)
        reset();
    const auto targetDepth = std::clamp(std::isfinite(settings.depth)
        ? settings.depth * 0.01f : 0.0f, 0.0f, 1.0f);
    depthSmoother.setTargetValue(targetDepth);
    bypassMix.setTargetValue(targetDepth > 0.0f ? 1.0f : 0.0f);
    const auto cycle = cycleQuarterNotes(settings.scanRateChoice);
    const auto bpm = std::clamp(std::isfinite(settings.bpm) ? settings.bpm : 120.0,
                                1.0, 1000.0);
    if (settings.useHostPpq && std::isfinite(settings.ppq))
    {
        scannerPhase = std::fmod(settings.ppq / cycle, 1.0);
        if (scannerPhase < 0.0)
            scannerPhase += 1.0;
    }
    const auto scannerIncrement = bpm / (60.0 * sampleRate * cycle);
    const auto handle = maskStore.acquire();
    const auto channels = std::min(2, buffer.getNumChannels());

    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        const auto depth = depthSmoother.getNextValue();
        const auto spectralMix = bypassMix.getNextValue();
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto dry = sanitise(buffer.getSample(channel, frame));
            const auto delayedDry = dryDelay[static_cast<std::size_t>(channel)]
                                            [static_cast<std::size_t>(dryDelayPosition)];
            dryDelay[static_cast<std::size_t>(channel)]
                    [static_cast<std::size_t>(dryDelayPosition)] = dry;
            inputRing[static_cast<std::size_t>(channel)]
                     [static_cast<std::size_t>(inputWritePosition)] = dry;
            const auto spectral = outputRing[static_cast<std::size_t>(channel)]
                                            [static_cast<std::size_t>(outputReadPosition)];
            outputRing[static_cast<std::size_t>(channel)]
                      [static_cast<std::size_t>(outputReadPosition)] = 0.0f;
            buffer.setSample(channel, frame,
                sanitise(delayedDry + spectralMix * (spectral - delayedDry)));
        }

        inputWritePosition = (inputWritePosition + 1) % fftSize;
        outputReadPosition = (outputReadPosition + 1) % fftSize;
        dryDelayPosition = (dryDelayPosition + 1) % fftSize;
        if (--samplesUntilFrame <= 0)
        {
            processFrame(handle.snapshot, depth, static_cast<float>(scannerPhase));
            samplesUntilFrame = hopSize;
        }
        scannerPhase += scannerIncrement;
        if (scannerPhase >= 1.0)
            scannerPhase -= std::floor(scannerPhase);
    }
    publishedScanPosition.store(static_cast<float>(scannerPhase),
                                std::memory_order_relaxed);
    maskStore.release(handle);
}
}

