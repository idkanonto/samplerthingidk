#include "CreativeEffects.h"
#include <algorithm>
#include <cmath>

namespace randomchop
{
namespace
{
float finiteOr(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float normalisePercent(float value) noexcept
{
    return std::clamp(finiteOr(value, 0.0f) * 0.01f, 0.0f, 1.0f);
}

float lerp(float first, float second, float amount) noexcept
{
    return first + (second - first) * amount;
}

double wrap(double position, double length) noexcept
{
    if (length <= 0.0)
        return 0.0;
    position = std::fmod(position, length);
    return position < 0.0 ? position + length : position;
}
}

float MeltProcessor::sanitise(float value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, -64.0f, 64.0f) : 0.0f;
}

void MeltProcessor::prepare(double newSampleRate)
{
    sampleRate = std::clamp(std::isfinite(newSampleRate) ? newSampleRate : 44100.0,
                            1.0, 768000.0);
    history.setSize(2, std::max(2, static_cast<int>(
        std::ceil(sampleRate * 4.0))), false, true, false);
    history.clear();
    for (int index = 0; index < windowTableSize; ++index)
        hannWindow[static_cast<std::size_t>(index)] = static_cast<float>(
            0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi
                * static_cast<double>(index)
                / static_cast<double>(windowTableSize - 1)));
    edgeFadeFrames = std::max(1, static_cast<int>(std::llround(sampleRate * 0.006)));
    bypassGain.reset(sampleRate, 0.008);
    reset();
}

void MeltProcessor::reset() noexcept
{
    enabled = false;
    armed = false;
    invalidateHistory();
    activationCount = 0;
    lastReversedSlices = 0;
    lastStretchRatio = 1.0f;
    bypassGain.setCurrentAndTargetValue(0.0f);
}

void MeltProcessor::endEvent() noexcept
{
    captureStart = 0;
    captureFrames = 0;
    sliceFrames = 1;
    sliceCount = 1;
    eventFrame = 0;
    eventFrames = 0;
    eventBoundariesRemaining = 0;
    grainFrames = 64;
    synthesisHop = 16;
    eventWet = 0.0f;
    recordDuringEvent = false;
    active = false;
    armed = enabled;
}

void MeltProcessor::invalidateHistory() noexcept
{
    endEvent();
    writeFrame = 0;
    validFrames = 0;
}

void MeltProcessor::setSeed(uint64_t seed) noexcept
{
    random.setSeed(seed ^ 0x6d656c742d736c63ULL);
    enabled = false;
    armed = false;
    invalidateHistory();
    activationCount = 0;
    lastReversedSlices = 0;
    lastStretchRatio = 1.0f;
    bypassGain.setCurrentAndTargetValue(0.0f);
}

float MeltProcessor::getEventProgress() const noexcept
{
    return active && eventFrames > 0
        ? std::clamp(static_cast<float>(eventFrame) / static_cast<float>(eventFrames),
                     0.0f, 1.0f)
        : 0.0f;
}

uint32_t MeltProcessor::getActiveReverseMask() const noexcept
{
    uint32_t mask = 0;
    if (active)
        for (int slice = 0; slice < sliceCount; ++slice)
            if (slices[static_cast<std::size_t>(slice)].reversed)
                mask |= uint32_t { 1 } << static_cast<uint32_t>(slice);
    return mask;
}

void MeltProcessor::configureSlice(int slice, int outputFrames, float amount) noexcept
{
    auto& configured = slices[static_cast<std::size_t>(slice)];
    const auto curve = std::pow(amount, 0.72f);
    const auto minimumRatio = 1.08 + 0.42 * static_cast<double>(curve);
    const auto maximumRatio = 1.28 + 2.72 * static_cast<double>(curve);
    configured.stretchRatio = lerp(static_cast<float>(minimumRatio),
                                   static_cast<float>(maximumRatio),
                                   static_cast<float>(random.unit()));
    if ((currentFeatures & MeltFeatures::stretch) == 0)
        configured.stretchRatio = 1.0;
    const auto desiredSourceFrames = static_cast<int>(std::ceil(
        static_cast<double>(outputFrames) / configured.stretchRatio)) + grainFrames + 2;
    configured.sourceFrames = std::clamp(desiredSourceFrames, grainFrames + 2,
                                         std::max(grainFrames + 2, captureFrames));
    configured.sourceFrames = std::min(configured.sourceFrames, captureFrames);
    const auto availableOrigins = std::max(1, captureFrames - configured.sourceFrames + 1);
    configured.sourceOrigin = static_cast<double>(random.bounded(
        static_cast<uint32_t>(availableOrigins)));
    const auto reverseChance = 0.08f + 0.52f * std::pow(amount, 0.90f);
    configured.reversed = (currentFeatures & MeltFeatures::reverse) != 0
        && random.unit() < static_cast<double>(reverseChance);
    constexpr int energyProbes = 24;
    double captureEnergy = 0.0;
    double sourceEnergy = 0.0;
    for (int probe = 0; probe < energyProbes; ++probe)
    {
        const auto unit = static_cast<double>(probe)
            / static_cast<double>(energyProbes - 1);
        const auto captureFrame = unit * static_cast<double>(captureFrames - 1);
        const auto sourceFrame = configured.sourceOrigin
            + unit * static_cast<double>(configured.sourceFrames - 1);
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto captureSample = static_cast<double>(
                sanitise(readCaptured(channel, captureFrame)));
            const auto sourceSample = static_cast<double>(
                sanitise(readCaptured(channel, sourceFrame)));
            captureEnergy += captureSample * captureSample;
            sourceEnergy += sourceSample * sourceSample;
        }
    }
    configured.levelGain = std::clamp(static_cast<float>(std::sqrt(
        (captureEnergy + 1.0e-9) / (sourceEnergy + 1.0e-9))), 0.80f, 1.25f);
    if (configured.reversed)
        ++lastReversedSlices;
    lastStretchRatio = std::max(lastStretchRatio,
        static_cast<float>(configured.stretchRatio));
}

void MeltProcessor::beginEvent(const GridBoundaries& boundaries, int gridChoice,
                               float amount) noexcept
{
    if (active || !armed || validFrames < 2 || amount <= 0.0f)
        return;

    const auto bpm = std::clamp(std::isfinite(boundaries.bpm) ? boundaries.bpm : 120.0,
                                20.0, 400.0);
    const auto exactStep = sampleRate * 60.0
        * HostGrid::quarterNotesPerStep(gridChoice) / bpm;
    const auto step = std::max(1, static_cast<int>(std::llround(exactStep)));
    captureFrames = std::min({ step, validFrames, history.getNumSamples() });
    if (captureFrames < 34)
        return;
    captureStart = writeFrame - captureFrames;
    if (captureStart < 0)
        captureStart += history.getNumSamples();

    eventFrames = step;
    eventBoundariesRemaining = 1;
    recordDuringEvent = eventFrames <= history.getNumSamples() - captureFrames;
    sliceCount = (currentFeatures & MeltFeatures::sliceVariation) != 0
        ? std::clamp(2 + static_cast<int>(std::floor(amount * 2.99f)),
                     2, maximumSlices) : 2;
    sliceFrames = std::max(1, (eventFrames + sliceCount - 1) / sliceCount);
    grainFrames = std::clamp(std::min(
        static_cast<int>(std::llround(sampleRate * (0.024 + 0.018 * amount))),
        std::max(32, sliceFrames / 2)), 32, 4096);
    synthesisHop = std::max(8, grainFrames / 4);
    eventWet = std::clamp(0.18f + 0.82f * std::pow(amount, 0.66f), 0.0f, 1.0f);
    lastReversedSlices = 0;
    lastStretchRatio = 1.0f;
    for (int slice = 0; slice < sliceCount; ++slice)
    {
        const auto outputFrames = std::max(1,
            std::min(sliceFrames, eventFrames - slice * sliceFrames));
        configureSlice(slice, outputFrames, amount);
    }

    eventFrame = 0;
    ++activationCount;
    active = true;
    armed = false;
}

float MeltProcessor::readCaptured(int channel, double logicalFrame) const noexcept
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
    return lerp(values[firstPhysical], values[secondPhysical], fraction);
}

float MeltProcessor::renderSliceSample(int channel, int slice,
                                       int localFrame) const noexcept
{
    const auto& configured = slices[static_cast<std::size_t>(slice)];
    const auto analysisHop = static_cast<double>(synthesisHop)
        / configured.stretchRatio;
    const auto latestGrain = localFrame / synthesisHop;
    float sample = 0.0f;
    float weightSum = 0.0f;
    float weightSquareSum = 0.0f;
    for (int grainOffset = 0; grainOffset <= 4; ++grainOffset)
    {
        const auto grain = latestGrain - grainOffset;
        if (grain < 0)
            continue;
        const auto grainFrame = localFrame - grain * synthesisHop;
        if (grainFrame < 0 || grainFrame >= grainFrames)
            continue;
        const auto phase = static_cast<double>(grain) * analysisHop
            + static_cast<double>(grainFrame);
        const auto wrapped = wrap(phase, static_cast<double>(configured.sourceFrames));
        const auto oriented = configured.reversed
            ? static_cast<double>(configured.sourceFrames - 1) - wrapped : wrapped;
        const auto windowIndex = std::clamp(static_cast<int>(
            static_cast<std::int64_t>(grainFrame) * (windowTableSize - 1)
                / std::max(1, grainFrames - 1)), 0, windowTableSize - 1);
        const auto window = hannWindow[static_cast<std::size_t>(windowIndex)];
        sample += window * sanitise(readCaptured(channel,
            configured.sourceOrigin + oriented));
        weightSum += window;
        weightSquareSum += window * window;
    }
    if (weightSum <= 0.00001f || weightSquareSum <= 0.0000001f)
        return 0.0f;
    // Nearby grains are phase-coherent near 1x but increasingly decorrelated
    // as the analysis hop slows. Blend amplitude and energy normalization so
    // deep stretches do not collapse in level without over-boosting mild ones.
    const auto decorrelation = std::clamp(static_cast<float>(
        (configured.stretchRatio - 1.0) / 1.5), 0.0f, 1.0f);
    const auto normaliser = lerp(weightSum, std::sqrt(weightSquareSum),
                                 decorrelation);
    return sanitise(configured.levelGain * sample
                    / std::max(0.00001f, normaliser));
}

void MeltProcessor::process(juce::AudioBuffer<float>& buffer,
                            const GridBoundaries& boundaries, int gridChoice,
                            MeltSettings settings) noexcept
{
    if (history.getNumSamples() < 2 || buffer.getNumChannels() < 1)
        return;
    if (boundaries.transportDiscontinuity || boundaries.gridChanged)
        invalidateHistory();

    currentFeatures = settings.features & MeltFeatures::all;
    const auto amount = currentFeatures != 0
        ? normalisePercent(settings.amountPercent) : 0.0f;
    const auto wantsEnabled = amount > 0.0f;
    if (wantsEnabled && !enabled)
    {
        enabled = true;
        armed = true;
        bypassGain.setTargetValue(1.0f);
    }
    else if (!wantsEnabled && enabled)
    {
        enabled = false;
        armed = false;
        bypassGain.setTargetValue(0.0f);
    }

    int boundaryIndex = 0;
    const auto capacity = history.getNumSamples();
    const auto channels = std::min(2, buffer.getNumChannels());
    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        const auto effectGain = bypassGain.getNextValue();
        while (boundaryIndex < boundaries.count
               && boundaries.sampleOffsets[static_cast<std::size_t>(boundaryIndex)] == frame)
        {
            if (active && eventBoundariesRemaining > 0
                && --eventBoundariesRemaining == 0)
                endEvent();
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
            const auto activeFrames = std::max(1,
                std::min(sliceFrames, eventFrames - slice * sliceFrames));
            const auto localFade = std::min(edgeFadeFrames,
                                            std::max(1, activeFrames / 3));
            const auto fadeIn = std::clamp(static_cast<float>(localFrame)
                                           / static_cast<float>(localFade), 0.0f, 1.0f);
            const auto fadeOut = std::clamp(static_cast<float>(activeFrames - 1 - localFrame)
                                            / static_cast<float>(localFade), 0.0f, 1.0f);
            const auto blend = std::clamp(
                effectGain * eventWet * std::min(fadeIn, fadeOut), 0.0f, 1.0f);
            const auto dryGain = std::sqrt(1.0f - blend);
            const auto wetGain = std::sqrt(blend);
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto stretched = renderSliceSample(channel, slice, localFrame);
                buffer.setSample(channel, frame, sanitise(
                    dryGain * dry[channel] + wetGain * stretched));
            }
            ++eventFrame;
            if ((!enabled && !bypassGain.isSmoothing() && effectGain <= 0.0f)
                || eventFrame >= eventFrames)
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

void SmearProcessor::prepare(double newSampleRate)
{
    sampleRate = std::clamp(std::isfinite(newSampleRate) ? newSampleRate : 44100.0,
                            1000.0, 768000.0);
    const auto frames = std::max(32, static_cast<int>(std::ceil(sampleRate * 1.0)) + 4);
    delayBuffer.setSize(2, frames, false, true, false);
    for (int index = 0; index < modulationTableSize; ++index)
    {
        const auto phase = static_cast<double>(index)
            / static_cast<double>(modulationTableSize);
        sineTable[static_cast<std::size_t>(index)] = static_cast<float>(
            std::sin(juce::MathConstants<double>::twoPi * phase));
        const auto hann = static_cast<float>(
            0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * phase));
        hannTable[static_cast<std::size_t>(index)] = hann;
        softWindowTable[static_cast<std::size_t>(index)] = std::pow(hann, 1.25f);
        constexpr auto sharpEdge = 0.22;
        const auto edgePhase = phase < sharpEdge
            ? phase / sharpEdge
            : phase > 1.0 - sharpEdge ? (1.0 - phase) / sharpEdge : 1.0;
        sharpWindowTable[static_cast<std::size_t>(index)] = edgePhase < 1.0
            ? static_cast<float>(0.5 - 0.5 * std::cos(
                juce::MathConstants<double>::pi * edgePhase))
            : 1.0f;
    }
    pressureSmoother.reset(sampleRate, 0.010);
    enabledSmoother.reset(sampleRate, 0.010);
    mixSmoother.reset(sampleRate, 0.020);
    shapeSmoother.reset(sampleRate, 0.040);
    reset();
}

void SmearProcessor::reset() noexcept
{
    delayBuffer.clear();
    resetRealtimeState();
}

void SmearProcessor::resetRealtimeState() noexcept
{
    for (auto& grain : grains)
        grain = {};
    lowState.fill(0.0f);
    fastEnvelope = 0.0f;
    slowEnvelope = 0.0f;
    motionPhase = 0.0f;
    lastMotionAmount = 0.0f;
    overlapEnergy = 0.0f;
    lastOverlapGain = 0.0f;
    pressureSmoother.setCurrentAndTargetValue(0.0f);
    enabledSmoother.setCurrentAndTargetValue(0.0f);
    mixSmoother.setCurrentAndTargetValue(0.0f);
    shapeSmoother.setCurrentAndTargetValue(0.0f);
    writePosition = 0;
    validFrames = 0;
    grainCountdown = 0;
    activeGrainCount = 0;
    peakActiveGrainCount = 0;
    lastGrainLengthFrames = 0;
    grainStartCount = 0;
}

void SmearProcessor::setSeed(uint64_t seed) noexcept
{
    random.setSeed(seed ^ 0x736d6561722d6372ULL);
    schedulerRandom.setSeed(seed ^ 0x626c6565642d646eULL);
    // Logical invalidation makes old ring contents unreachable while avoiding a
    // one-second buffer clear if a host restores state during playback.
    resetRealtimeState();
}

float SmearProcessor::sanitise(float value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, -64.0f, 64.0f) : 0.0f;
}

float SmearProcessor::readDelay(int channel, double position) const noexcept
{
    const auto size = delayBuffer.getNumSamples();
    if (size <= 1)
        return 0.0f;
    position = wrap(position, static_cast<double>(size));
    const auto first = static_cast<int>(position);
    const auto second = first + 1 < size ? first + 1 : 0;
    const auto fraction = static_cast<float>(position - first);
    return lerp(delayBuffer.getSample(channel, first),
                delayBuffer.getSample(channel, second), fraction);
}

float SmearProcessor::lookupSine(float phase) const noexcept
{
    const auto wrapped = static_cast<float>(wrap(static_cast<double>(phase), 1.0));
    const auto scaled = wrapped * static_cast<float>(modulationTableSize);
    const auto first = std::clamp(static_cast<int>(scaled), 0, modulationTableSize - 1);
    const auto second = (first + 1) % modulationTableSize;
    return lerp(sineTable[static_cast<std::size_t>(first)],
                sineTable[static_cast<std::size_t>(second)], scaled - first);
}

float SmearProcessor::lookupWindow(float phase, float shape) const noexcept
{
    const auto wrapped = static_cast<float>(wrap(static_cast<double>(phase), 1.0));
    const auto scaled = wrapped * static_cast<float>(modulationTableSize);
    const auto first = std::clamp(static_cast<int>(scaled), 0, modulationTableSize - 1);
    const auto second = (first + 1) % modulationTableSize;
    const auto lookup = [first, second, scaled](const auto& table)
    {
        return lerp(table[static_cast<std::size_t>(first)],
                    table[static_cast<std::size_t>(second)], scaled - first);
    };
    if (shape < 0.0f)
        return lerp(lookup(softWindowTable), lookup(hannTable), shape + 1.0f);
    return lerp(lookup(hannTable), lookup(sharpWindowTable), shape);
}

void SmearProcessor::startGrain(float shape, uint32_t features) noexcept
{
    Grain* destination = nullptr;
    for (auto& grain : grains)
    {
        if (!grain.active)
        {
            destination = &grain;
            break;
        }
    }
    if (destination == nullptr)
        return;

    shape = std::clamp(std::isfinite(shape) ? shape : 0.0f, -1.0f, 1.0f);
    const auto nominalLengthMs = shape < 0.0f
        ? 40.0f + 50.0f * -shape
        : 40.0f - 22.0f * shape;
    const auto maximumPossibleLength = static_cast<int>(std::ceil(
        sampleRate * static_cast<double>(nominalLengthMs) * 0.001 * 1.10));
    if (validFrames <= maximumPossibleLength + 4)
        return;
    const auto variedLengthMs = nominalLengthMs
        * static_cast<float>(0.90 + 0.20 * random.unit());
    constexpr float minimumGrainSeconds = 0.014f;
    const auto minimumLength = std::max(16, static_cast<int>(std::llround(
        sampleRate * minimumGrainSeconds)));
    const auto length = std::clamp(static_cast<int>(std::llround(
        sampleRate * static_cast<double>(variedLengthMs) * 0.001)),
        minimumLength, std::max(minimumLength, delayBuffer.getNumSamples() / 4));
    const auto minimumDelay = length + 4;
    if (validFrames <= minimumDelay)
        return;
    const auto scatterChoice = random.unit();
    const auto scatter = (features & SmearFeatures::scatter) != 0
        ? static_cast<int>(std::llround(sampleRate
            * (0.035 + 0.060 * scatterChoice))) : 0;
    const auto maximumDelay = std::max(minimumDelay,
        std::min(validFrames - 2, delayBuffer.getNumSamples() - 2));
    const auto delay = std::clamp(minimumDelay + scatter, minimumDelay, maximumDelay);

    destination->readPosition = wrap(static_cast<double>(writePosition - delay),
                                     static_cast<double>(delayBuffer.getNumSamples()));
    const auto panChoice = random.unit();
    destination->pan = (features & SmearFeatures::stereo) != 0
        ? static_cast<float>(panChoice * 2.0 - 1.0) : 0.0f;
    destination->panPhase = static_cast<float>(random.unit());
    destination->panRate = static_cast<float>((0.18 + 0.44 * random.unit())
                                               / static_cast<double>(length));
    destination->age = 0;
    destination->length = length;
    destination->active = true;
    lastGrainLengthFrames = length;
    ++grainStartCount;
    ++activeGrainCount;
}

void SmearProcessor::process(juce::AudioBuffer<float>& buffer,
                             SmearSettings settings) noexcept
{
    if (delayBuffer.getNumSamples() <= 1 || buffer.getNumChannels() < 1)
        return;
    const auto features = settings.features & SmearFeatures::all;
    const auto targetPressure = normalisePercent(settings.pressure);
    pressureSmoother.setTargetValue(targetPressure);
    enabledSmoother.setTargetValue(targetPressure > 0.0f ? 1.0f : 0.0f);
    mixSmoother.setTargetValue(normalisePercent(settings.mix));
    shapeSmoother.setTargetValue(std::clamp(
        std::isfinite(settings.shape) ? settings.shape / 100.0f : 0.0f,
        -1.0f, 1.0f));
    const auto channels = std::min(2, buffer.getNumChannels());
    const auto fastCoefficient = 1.0f - std::exp(-1.0f
        / static_cast<float>(sampleRate * 0.004));
    const auto slowCoefficient = 1.0f - std::exp(-1.0f
        / static_cast<float>(sampleRate * 0.065));
    const auto overlapCoefficient = 1.0f - std::exp(-1.0f
        / static_cast<float>(sampleRate * 0.004));
    const auto lowCoefficient = 1.0f - std::exp(
        -juce::MathConstants<float>::twoPi * 700.0f
        / static_cast<float>(sampleRate));
    constexpr int maximumEventsPerDensityWindow = 16;
    const auto densityWindowFrames = sampleRate * 0.050;
    peakActiveGrainCount = activeGrainCount;

    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        const auto pressure = pressureSmoother.getNextValue();
        const auto mix = mixSmoother.getNextValue();
        const auto shape = shapeSmoother.getNextValue();
        lastMotionAmount = pressure;
        const auto panOrbitDepth = (features & SmearFeatures::orbit) != 0 ? 0.22f : 0.0f;
        float dry[2] {
            sanitise(buffer.getSample(0, frame)),
            sanitise(buffer.getSample(std::min(1, channels - 1), frame))
        };
        for (int channel = 0; channel < 2; ++channel)
            delayBuffer.setSample(channel, writePosition, dry[channel]);
        validFrames = std::min(validFrames + 1, delayBuffer.getNumSamples());

        if (pressure > 0.0f && grainCountdown-- <= 0)
        {
            const auto densityCurve = std::pow(pressure, 1.8f);
            const auto targetEvents = std::clamp(1 + static_cast<int>(std::llround(
                static_cast<double>(maximumEventsPerDensityWindow - 1) * densityCurve)),
                1, maximumEventsPerDensityWindow);
            if (activeGrainCount < maximumGrains)
                startGrain(shape, features);
            peakActiveGrainCount = std::max(peakActiveGrainCount, activeGrainCount);
            const auto pulse = 0.90f + 0.10f * std::sin(
                juce::MathConstants<float>::twoPi * motionPhase);
            grainCountdown = std::max(1, static_cast<int>(std::llround(
                densityWindowFrames / static_cast<double>(targetEvents)
                * pulse * (0.86 + 0.28 * schedulerRandom.unit()))));
        }

        float texture[2] { 0.0f, 0.0f };
        auto windowEnergy = 0.0f;
        for (auto& grain : grains)
        {
            if (!grain.active)
                continue;
            const auto phase = static_cast<float>(grain.age)
                / static_cast<float>(std::max(1, grain.length));
            const auto window = lookupWindow(phase, shape);
            const auto panOrbit = lookupSine(grain.panPhase);
            const auto movingPan = std::clamp(grain.pan + panOrbitDepth * panOrbit,
                                              -1.0f, 1.0f);
            const auto left = sanitise(readDelay(0, grain.readPosition));
            const auto right = sanitise(readDelay(1, grain.readPosition));
            const auto leftPan = std::sqrt(0.5f * (1.0f - movingPan));
            const auto rightPan = std::sqrt(0.5f * (1.0f + movingPan));
            texture[0] += window * (left * leftPan + right * rightPan * 0.16f);
            texture[1] += window * (right * rightPan + left * leftPan * 0.16f);
            windowEnergy += window * window;
            // BLEED is intentionally voiced one octave above the source. Shape
            // changes the grain envelope and duration, never this fixed pitch.
            grain.readPosition = wrap(grain.readPosition + 2.0,
                                      static_cast<double>(delayBuffer.getNumSamples()));
            grain.panPhase = static_cast<float>(wrap(
                grain.panPhase + grain.panRate, 1.0));
            if (++grain.age >= grain.length)
            {
                grain.active = false;
                activeGrainCount = std::max(0, activeGrainCount - 1);
            }
        }

        const auto inputEnvelope = std::max(std::abs(dry[0]), std::abs(dry[1]));
        fastEnvelope += fastCoefficient * (inputEnvelope - fastEnvelope);
        slowEnvelope += slowCoefficient * (inputEnvelope - slowEnvelope);
        const auto transient = std::clamp((fastEnvelope - slowEnvelope) * 5.0f,
                                          0.0f, 1.0f);
        const auto wet = enabledSmoother.getNextValue();
        overlapEnergy += overlapCoefficient * (windowEnergy - overlapEnergy);
        const auto grainNormalisation = windowEnergy > 0.0f || overlapEnergy > 0.0001f
            ? std::clamp(0.82f / std::sqrt(std::max(0.55f, overlapEnergy)),
                         0.35f, 1.10f)
            : 0.0f;
        lastOverlapGain = grainNormalisation;
        for (int channel = 0; channel < channels; ++channel)
        {
            texture[channel] *= grainNormalisation;
            auto& low = lowState[static_cast<std::size_t>(channel)];
            low = sanitise(low + lowCoefficient * (texture[channel] - low));
            const auto bright = sanitise(texture[channel] - low);
            const auto crystal = (features & SmearFeatures::brightness) != 0
                ? sanitise(0.22f * texture[channel] + 1.28f * bright)
                : texture[channel];
            const auto blend = mix * wet;
            const auto output = dry[channel] * (1.0f - blend)
                + blend * (1.0f - 0.35f * transient) * 1.5f * std::tanh(crystal * 1.25f);
            buffer.setSample(channel, frame, sanitise(output));
        }

        if (++writePosition >= delayBuffer.getNumSamples())
            writePosition = 0;
        motionPhase = static_cast<float>(wrap(
            motionPhase + 0.23 / sampleRate, 1.0));
    }
    if (targetPressure <= 0.0f && !pressureSmoother.isSmoothing())
    {
        for (auto& grain : grains)
            grain.active = false;
        grainCountdown = 0;
        activeGrainCount = 0;
        peakActiveGrainCount = 0;
        lastGrainLengthFrames = 0;
        overlapEnergy = 0.0f;
        lastOverlapGain = 0.0f;
        lastMotionAmount = 0.0f;
    }
}
}
