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

const FracturePreset& getFracturePreset(int index) noexcept
{
    return fracturePresets[static_cast<std::size_t>(
        std::clamp(index, 0, static_cast<int>(fracturePresets.size()) - 1))];
}

FractureProcessor::FilterOutputs FractureProcessor::StateVariableFilter::process(
    float input, float g, float damping) noexcept
{
    const auto denominator = 1.0f + g * (g + damping);
    const auto a1 = 1.0f / denominator;
    const auto a2 = g * a1;
    const auto a3 = g * a2;
    const auto v3 = input - integrator2;
    const auto band = a1 * integrator1 + a2 * v3;
    const auto low = integrator2 + a2 * integrator1 + a3 * v3;
    integrator1 = sanitise(2.0f * band - integrator1);
    integrator2 = sanitise(2.0f * low - integrator2);
    const auto high = input - damping * band - low;
    return { sanitise(low), sanitise(band), sanitise(high), sanitise(low + high) };
}

void FractureProcessor::prepare(double newSampleRate)
{
    sampleRate = std::clamp(std::isfinite(newSampleRate) ? newSampleRate : 44100.0,
                            1000.0, 768000.0);
    const auto frames = std::max(16, static_cast<int>(std::ceil(sampleRate * 0.06)) + 4);
    combBuffer.setSize(2, frames, false, true, false);
    reset();
}

void FractureProcessor::reset() noexcept
{
    for (auto& filter : mainFilters)
        filter.reset();
    for (auto& filter : formantFilters)
        filter.reset();
    dcInput.fill(0.0f);
    dcOutput.fill(0.0f);
    bandwidthState.fill(0.0f);
    previousReconstruction.fill(0.0f);
    heldResidual.fill(0.0f);
    rateHeld.fill(0.0f);
    combBuffer.clear();
    phaseA = 0.0;
    phaseB = 0.25;
    envelope = 0.0f;
    smoothRandom = 0.0f;
    randomTarget = 0.0f;
    lastMotionDepth = 0.0f;
    combWritePosition = 0;
    randomCountdown = 0;
    packetCountdown = 0;
    rateCountdown = 0;
    previousRateFactor = 1;
    lastEffectiveRateFactor = 1;
}

void FractureProcessor::setSeed(uint64_t seed) noexcept
{
    random.setSeed(seed ^ 0x6672616374757265ULL);
    reset();
}

float FractureProcessor::sanitise(float value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, -64.0f, 64.0f) : 0.0f;
}

float FractureProcessor::waveshape(float input, float character) noexcept
{
    const auto position = std::clamp(character, 0.0f, 1.0f) * 4.0f;
    const auto segment = std::min(3, static_cast<int>(position));
    const auto fraction = position - static_cast<float>(segment);
    const auto soft = std::tanh(input);
    const auto asymmetric = input >= 0.0f
        ? std::tanh(input * 1.45f)
        : 0.58f * std::tanh(input * 0.82f);
    const auto folded = 0.6366197724f * std::asin(std::sin(input * 1.7f));
    const auto clipped = std::clamp(input, -0.78f, 0.78f) / 0.78f;
    const auto digital = std::sin(std::clamp(input, -8.0f, 8.0f) * 2.35f);
    const std::array<float, 5> shapes { soft, asymmetric, folded, clipped, digital };
    return sanitise(lerp(shapes[static_cast<std::size_t>(segment)],
                         shapes[static_cast<std::size_t>(segment + 1)], fraction));
}

float FractureProcessor::readComb(int channel, float delayFrames) const noexcept
{
    const auto size = combBuffer.getNumSamples();
    if (size <= 1)
        return 0.0f;
    const auto position = wrap(static_cast<double>(combWritePosition) - delayFrames,
                               static_cast<double>(size));
    const auto first = static_cast<int>(position);
    const auto second = first + 1 < size ? first + 1 : 0;
    const auto fraction = static_cast<float>(position - first);
    return lerp(combBuffer.getSample(channel, first),
                combBuffer.getSample(channel, second), fraction);
}

void FractureProcessor::process(juce::AudioBuffer<float>& buffer,
                                FractureSettings settings) noexcept
{
    const auto amount = normalisePercent(settings.amount);
    if (amount <= 0.0f || combBuffer.getNumSamples() <= 1
        || buffer.getNumChannels() < 1)
        return;

    const auto character = normalisePercent(settings.character);
    const auto macroCurve = std::pow(amount, 0.72f);
    const auto motionDepth = std::pow(amount, 1.28f);
    const auto driveBaseDb = 2.0f + 28.0f * macroCurve;
    const auto wet = std::clamp(0.06f + 0.94f * macroCurve, 0.0f, 1.0f);
    const auto baseFrequency = 170.0f * std::pow(52.0f, character);
    const auto rateBase = std::clamp(settings.rateFactor, 1, 64);
    const auto maximumFrequency = static_cast<float>(sampleRate * 0.44);
    const auto phaseIncrementA = (0.08 + 1.34 * amount * amount) / sampleRate;
    const auto phaseIncrementB = (0.13 + 2.03 * amount) / sampleRate;
    const auto attackCoefficient = 1.0f - std::exp(-1.0f /
        static_cast<float>(sampleRate * 0.004));
    const auto releaseCoefficient = 1.0f - std::exp(-1.0f /
        static_cast<float>(sampleRate * 0.085));
    const auto randomCoefficient = 1.0f - std::exp(-1.0f /
        static_cast<float>(sampleRate * (0.12 - 0.075 * amount)));
    const auto dcCoefficient = std::exp(-juce::MathConstants<float>::twoPi
        * 15.0f / static_cast<float>(sampleRate));
    const auto channels = std::min(2, buffer.getNumChannels());
    lastMotionDepth = motionDepth;

    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        float dry[2] {
            sanitise(buffer.getSample(0, frame)),
            sanitise(buffer.getSample(std::min(1, channels - 1), frame))
        };
        const auto inputEnvelope = std::max(std::abs(dry[0]), std::abs(dry[1]));
        const auto envelopeCoefficient = inputEnvelope > envelope
            ? attackCoefficient : releaseCoefficient;
        envelope += envelopeCoefficient * (inputEnvelope - envelope);
        envelope = std::clamp(envelope, 0.0f, 4.0f);
        const auto envelopeMotion = std::clamp(envelope * 1.8f, 0.0f, 1.0f);

        if (randomCountdown-- <= 0)
        {
            randomTarget = static_cast<float>(random.unit() * 2.0 - 1.0);
            randomCountdown = std::max(1, static_cast<int>(std::llround(sampleRate
                * (0.62 - 0.48 * amount)
                * (0.78 + 0.44 * random.unit()))));
        }
        smoothRandom += randomCoefficient * (randomTarget - smoothRandom);

        const auto angleA = juce::MathConstants<double>::twoPi * phaseA;
        const auto angleB = juce::MathConstants<double>::twoPi * phaseB;
        const auto oscillatorA = static_cast<float>(std::sin(angleA));
        const auto oscillatorB = static_cast<float>(std::sin(angleB));
        const auto quadrature = static_cast<float>(std::cos(angleA));
        const auto slowMotion = 0.50f * oscillatorA + 0.31f * oscillatorB
            + 0.19f * smoothRandom;
        const auto crossMotion = 0.43f * quadrature - 0.34f * oscillatorB
            + 0.23f * (envelopeMotion * 2.0f - 1.0f);

        const auto frequencyOctaves = motionDepth * (0.48f + 1.42f * amount)
            * (0.78f * slowMotion + 0.22f * envelopeMotion);
        const auto frequency = std::clamp(baseFrequency * std::pow(2.0f, frequencyOctaves),
                                          35.0f, maximumFrequency);
        const auto formantFrequency = std::min(maximumFrequency,
            frequency * (1.48f + 1.58f * character
                + 0.16f * motionDepth * crossMotion));
        const auto g = std::clamp(std::tan(juce::MathConstants<float>::pi
                                          * frequency / static_cast<float>(sampleRate)),
                                  0.00001f, 24.0f);
        const auto formantG = std::clamp(std::tan(juce::MathConstants<float>::pi
            * formantFrequency / static_cast<float>(sampleRate)), 0.00001f, 24.0f);
        const auto resonance = std::clamp(0.10f + 0.66f * macroCurve
            + motionDepth * (0.13f * slowMotion + 0.09f * envelopeMotion), 0.0f, 0.96f);
        const auto damping = 2.0f - 1.88f * std::sqrt(resonance);
        const auto driveDb = std::clamp(driveBaseDb
            + 5.5f * motionDepth * crossMotion, 0.0f, 36.0f);
        const auto driveGain = std::pow(10.0f, driveDb / 20.0f);
        const auto morphPosition = std::clamp(character * 5.0f
            + motionDepth * 2.1f * crossMotion, 0.0f, 5.0f);
        const auto morphSegment = std::min(4, static_cast<int>(morphPosition));
        const auto morphFraction = morphPosition - static_cast<float>(morphSegment);
        const auto combDelay = std::clamp(static_cast<float>(sampleRate) / frequency,
            2.0f, static_cast<float>(combBuffer.getNumSamples() - 2));
        const auto metallicDelay = std::clamp(combDelay
            * (0.49f + 0.14f * character + 0.035f * slowMotion), 2.0f,
            static_cast<float>(combBuffer.getNumSamples() - 2));
        const auto combFeedback = std::clamp(0.08f + 0.68f * resonance, 0.0f, 0.76f);

        const auto rateMotion = std::clamp(0.84f + 0.16f * (slowMotion + 1.0f) * 0.5f,
                                           0.72f, 1.0f);
        const auto effectiveRate = std::clamp(1 + static_cast<int>(std::llround(
            static_cast<double>(rateBase - 1) * std::sqrt(amount) * rateMotion)), 1, 64);
        if (effectiveRate != previousRateFactor)
        {
            rateCountdown = 0;
            previousRateFactor = effectiveRate;
        }
        lastEffectiveRateFactor = effectiveRate;
        const auto rateCapture = rateCountdown <= 0;

        const auto digitalAmount = amount * (0.22f + 0.78f * character);
        const auto packetCapture = packetCountdown <= 0;
        const auto packetFrames = 1 + static_cast<int>(std::llround(
            digitalAmount * (2.0f + 11.0f * amount)
            * (0.82f + 0.18f * std::abs(smoothRandom))));
        const auto cutoff = std::clamp(18000.0f * std::pow(2400.0f / 18000.0f,
            digitalAmount * (0.72f + 0.28f * std::abs(crossMotion))), 100.0f,
            maximumFrequency);
        const auto bandwidthCoefficient = 1.0f - std::exp(
            -juce::MathConstants<float>::twoPi * cutoff / static_cast<float>(sampleRate));

        for (int channel = 0; channel < channels; ++channel)
        {
            auto& bandwidth = bandwidthState[static_cast<std::size_t>(channel)];
            bandwidth = sanitise(bandwidth + bandwidthCoefficient * (dry[channel] - bandwidth));
            const auto previous = previousReconstruction[static_cast<std::size_t>(channel)];
            const auto predictor = previous + (bandwidth - previous)
                * (0.22f + 0.31f * (1.0f - digitalAmount));
            const auto residual = bandwidth - predictor;
            if (packetCapture)
                heldResidual[static_cast<std::size_t>(channel)] = std::tanh(
                    residual * (1.0f + 4.5f * digitalAmount))
                    / (1.0f + 4.5f * digitalAmount);
            const auto reconstructed = sanitise(predictor
                + heldResidual[static_cast<std::size_t>(channel)]
                    * (1.0f - 0.52f * digitalAmount));
            previousReconstruction[static_cast<std::size_t>(channel)] = reconstructed;
            const auto damaged = sanitise(lerp(dry[channel], reconstructed,
                                               0.78f * digitalAmount));
            if (rateCapture)
                rateHeld[static_cast<std::size_t>(channel)] = damaged;

            const auto shaped = waveshape(rateHeld[static_cast<std::size_t>(channel)]
                                          * driveGain, character);
            const auto delayed = sanitise(readComb(channel, combDelay));
            const auto secondary = sanitise(readComb(channel, metallicDelay));
            combBuffer.setSample(channel, combWritePosition,
                sanitise(shaped + delayed * combFeedback));

            const auto main = mainFilters[static_cast<std::size_t>(channel)]
                .process(shaped, g, damping);
            const auto formant = formantFilters[static_cast<std::size_t>(channel)]
                .process(shaped, formantG, std::max(0.12f, damping * 0.72f));
            const std::array<float, 6> structures {
                main.low,
                main.band * 1.35f,
                main.notch,
                main.band * 0.72f + formant.band * 0.82f,
                shaped - delayed * 0.88f,
                main.high * 0.34f + (delayed - secondary) * 0.92f
            };
            const auto tonal = lerp(structures[static_cast<std::size_t>(morphSegment)],
                                    structures[static_cast<std::size_t>(morphSegment + 1)],
                                    morphFraction);
            const auto dcBlocked = tonal - dcInput[static_cast<std::size_t>(channel)]
                + dcCoefficient * dcOutput[static_cast<std::size_t>(channel)];
            dcInput[static_cast<std::size_t>(channel)] = sanitise(tonal);
            dcOutput[static_cast<std::size_t>(channel)] = sanitise(dcBlocked);
            const auto processed = 1.08f * std::tanh(sanitise(dcBlocked) * 1.08f);
            buffer.setSample(channel, frame,
                sanitise(lerp(dry[channel], processed, wet)));
        }

        packetCountdown = packetCapture ? packetFrames - 1 : packetCountdown - 1;
        rateCountdown = rateCapture ? effectiveRate - 1 : rateCountdown - 1;
        if (++combWritePosition >= combBuffer.getNumSamples())
            combWritePosition = 0;
        phaseA = wrap(phaseA + phaseIncrementA, 1.0);
        phaseB = wrap(phaseB + phaseIncrementB, 1.0);
    }
}

void SmearProcessor::prepare(double newSampleRate)
{
    sampleRate = std::clamp(std::isfinite(newSampleRate) ? newSampleRate : 44100.0,
                            1000.0, 768000.0);
    const auto frames = std::max(32, static_cast<int>(std::ceil(sampleRate * 1.0)) + 4);
    delayBuffer.setSize(2, frames, false, true, false);
    reset();
}

void SmearProcessor::reset() noexcept
{
    delayBuffer.clear();
    for (auto& grain : grains)
        grain = {};
    lowState.fill(0.0f);
    fastEnvelope = 0.0f;
    slowEnvelope = 0.0f;
    writePosition = 0;
    validFrames = 0;
    grainCountdown = 0;
}

void SmearProcessor::setSeed(uint64_t seed) noexcept
{
    random.setSeed(seed ^ 0x736d6561722d6372ULL);
    reset();
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

void SmearProcessor::startGrain(float amount) noexcept
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

    const auto lengthSeconds = 0.070 - 0.040 * static_cast<double>(amount)
        + 0.012 * (random.unit() - 0.5);
    const auto length = std::clamp(static_cast<int>(std::llround(sampleRate * lengthSeconds)),
                                   16, std::max(16, delayBuffer.getNumSamples() / 4));
    const auto selector = random.unit();
    int semitones = 12;
    if (selector < 0.12 * (1.0 - amount))
        semitones = 0;
    else if (selector < 0.36)
        semitones = 7;
    else if (selector < 0.72)
        semitones = 12;
    else if (selector < 0.90)
        semitones = amount > 0.55f ? 19 : 7;
    else
        semitones = amount > 0.72f ? (random.bounded(2) == 0 ? -12 : 24) : 12;
    const auto increment = std::pow(2.0, static_cast<double>(semitones) / 12.0);
    const auto minimumDelay = static_cast<int>(std::ceil(length * std::max(1.0, increment))) + 4;
    if (validFrames <= minimumDelay)
        return;
    const auto scatter = static_cast<int>(std::llround(sampleRate
        * (0.035 + 0.24 * amount) * random.unit()));
    const auto maximumDelay = std::max(minimumDelay,
        std::min(validFrames - 2, delayBuffer.getNumSamples() - 2));
    const auto delay = std::clamp(minimumDelay + scatter, minimumDelay, maximumDelay);

    destination->readPosition = wrap(static_cast<double>(writePosition - delay),
                                     static_cast<double>(delayBuffer.getNumSamples()));
    destination->increment = increment;
    destination->pan = static_cast<float>(random.unit() * 2.0 - 1.0);
    destination->age = 0;
    destination->length = length;
    destination->active = true;
}

int SmearProcessor::getActiveGrainCount() const noexcept
{
    int count = 0;
    for (const auto& grain : grains)
        if (grain.active)
            ++count;
    return count;
}

void SmearProcessor::process(juce::AudioBuffer<float>& buffer,
                             SmearSettings settings) noexcept
{
    if (delayBuffer.getNumSamples() <= 1 || buffer.getNumChannels() < 1)
        return;
    const auto amount = normalisePercent(settings.amount);
    const auto channels = std::min(2, buffer.getNumChannels());
    if (amount <= 0.0f)
    {
        for (auto& grain : grains)
            grain.active = false;
        grainCountdown = 0;
    }

    const auto highPassCutoff = 1200.0f + 3600.0f * amount;
    const auto lowCoefficient = 1.0f - std::exp(
        -juce::MathConstants<float>::twoPi * highPassCutoff
        / static_cast<float>(sampleRate));
    const auto fastCoefficient = 1.0f - std::exp(-1.0f
        / static_cast<float>(sampleRate * 0.004));
    const auto slowCoefficient = 1.0f - std::exp(-1.0f
        / static_cast<float>(sampleRate * 0.065));
    const auto wetBase = std::pow(amount, 0.72f);

    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        float dry[2] {
            sanitise(buffer.getSample(0, frame)),
            sanitise(buffer.getSample(std::min(1, channels - 1), frame))
        };
        delayBuffer.setSample(0, writePosition, dry[0]);
        delayBuffer.setSample(1, writePosition, dry[1]);
        validFrames = std::min(validFrames + 1, delayBuffer.getNumSamples());

        if (amount > 0.0f && grainCountdown-- <= 0)
        {
            startGrain(amount);
            const auto nominalLength = sampleRate * (0.070 - 0.040 * amount);
            grainCountdown = std::max(1, static_cast<int>(std::llround(nominalLength
                * (0.52 - 0.27 * amount)
                * (0.86 + 0.28 * random.unit()))));
        }

        float texture[2] { 0.0f, 0.0f };
        int activeGrains = 0;
        for (auto& grain : grains)
        {
            if (!grain.active)
                continue;
            const auto phase = static_cast<float>(grain.age)
                / static_cast<float>(std::max(1, grain.length));
            const auto window = 0.5f - 0.5f * std::cos(
                juce::MathConstants<float>::twoPi * phase);
            const auto left = sanitise(readDelay(0, grain.readPosition));
            const auto right = sanitise(readDelay(1, grain.readPosition));
            const auto leftPan = std::sqrt(0.5f * (1.0f - grain.pan));
            const auto rightPan = std::sqrt(0.5f * (1.0f + grain.pan));
            texture[0] += window * (left * leftPan + right * rightPan * 0.12f);
            texture[1] += window * (right * rightPan + left * leftPan * 0.12f);
            grain.readPosition = wrap(grain.readPosition + grain.increment,
                                      static_cast<double>(delayBuffer.getNumSamples()));
            if (++grain.age >= grain.length)
                grain.active = false;
            ++activeGrains;
        }

        const auto inputEnvelope = std::max(std::abs(dry[0]), std::abs(dry[1]));
        fastEnvelope += fastCoefficient * (inputEnvelope - fastEnvelope);
        slowEnvelope += slowCoefficient * (inputEnvelope - slowEnvelope);
        const auto transient = std::clamp((fastEnvelope - slowEnvelope) * 5.0f,
                                          0.0f, 1.0f);
        const auto wet = wetBase * (1.0f - 0.68f * transient);
        const auto grainNormalisation = activeGrains > 0
            ? 0.96f / std::sqrt(static_cast<float>(activeGrains)) : 0.0f;
        for (int channel = 0; channel < channels; ++channel)
        {
            texture[channel] *= grainNormalisation;
            auto& low = lowState[static_cast<std::size_t>(channel)];
            low = sanitise(low + lowCoefficient * (texture[channel] - low));
            const auto bright = sanitise(texture[channel] - low);
            const auto crystal = sanitise(0.14f * texture[channel]
                + (1.30f + 0.62f * amount) * bright);
            const auto output = dry[channel] * (1.0f - 0.32f * wet)
                + (1.02f + 0.10f * amount) * wet
                    * std::tanh(crystal * 1.25f);
            buffer.setSample(channel, frame, sanitise(output));
        }

        if (++writePosition >= delayBuffer.getNumSamples())
            writePosition = 0;
    }
}
}
