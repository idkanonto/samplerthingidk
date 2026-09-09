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
    reset();
}

void FractureProcessor::reset() noexcept
{
    for (auto& filter : mainFilters)
        filter.reset();
    dcInput.fill(0.0f);
    dcOutput.fill(0.0f);
    previousInput.fill(0.0f);
    phaseA = 0.0;
    phaseB = 0.37;
    envelope = 0.0f;
    smoothRandom = 0.0f;
    randomTarget = 0.0f;
    lastMotionDepth = 0.0f;
    lastMorphPosition = 0.0f;
    lastDriveGain = 1.0f;
    randomCountdown = 0;
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
    const auto position = std::clamp(character, 0.0f, 1.0f) * 3.0f;
    const auto segment = std::min(2, static_cast<int>(position));
    const auto fraction = position - static_cast<float>(segment);
    const auto soft = std::tanh(input);
    constexpr auto bias = 0.18f;
    const auto asymmetric = std::tanh(input + bias) - std::tanh(bias);
    const auto folded = 0.6366197724f * std::asin(std::sin(input * 1.35f));
    const auto clipped = std::clamp(input, -1.0f, 1.0f);
    const std::array<float, 4> shapes { soft, asymmetric, folded, clipped };
    return sanitise(lerp(shapes[static_cast<std::size_t>(segment)],
                         shapes[static_cast<std::size_t>(segment + 1)], fraction));
}

float FractureProcessor::morphFilter(const FilterOutputs& outputs,
                                     float position) noexcept
{
    position = std::clamp(position, 0.0f, 3.0f);
    const auto segment = std::min(2, static_cast<int>(position));
    const auto fraction = position - static_cast<float>(segment);
    const std::array<float, 4> responses {
        outputs.low,
        outputs.band * 1.38f,
        outputs.notch * 0.94f,
        outputs.high * 0.88f
    };
    return sanitise(lerp(responses[static_cast<std::size_t>(segment)],
                         responses[static_cast<std::size_t>(segment + 1)], fraction));
}

void FractureProcessor::process(juce::AudioBuffer<float>& buffer,
                                FractureSettings settings) noexcept
{
    const auto amount = normalisePercent(settings.amount);
    if (buffer.getNumChannels() < 1)
        return;
    if (amount <= 0.0f)
    {
        for (auto& filter : mainFilters)
            filter.reset();
        dcInput.fill(0.0f);
        dcOutput.fill(0.0f);
        previousInput.fill(0.0f);
        envelope = 0.0f;
        lastMotionDepth = 0.0f;
        lastDriveGain = 1.0f;
        return;
    }

    const auto character = normalisePercent(settings.character);
    const auto macroCurve = std::pow(amount, 0.82f);
    const auto motionDepth = std::pow(amount, 1.15f);
    const auto driveGain = 1.0f + 24.0f * std::pow(amount, 1.35f);
    const auto driveCompensation = 1.0f / std::sqrt(1.0f + 0.16f * (driveGain - 1.0f));
    const auto wet = std::pow(amount, 0.78f);
    const auto baseFrequency = 260.0f * std::pow(20.0f, character);
    const auto maximumFrequency = static_cast<float>(sampleRate * 0.44);
    const auto phaseIncrementA = (0.07 + 0.55 * amount * amount) / sampleRate;
    const auto phaseIncrementB = (0.11 + 0.83 * amount) / sampleRate;
    const auto attackCoefficient = 1.0f - std::exp(-1.0f /
        static_cast<float>(sampleRate * 0.003));
    const auto releaseCoefficient = 1.0f - std::exp(-1.0f /
        static_cast<float>(sampleRate * 0.075));
    const auto randomCoefficient = 1.0f - std::exp(-1.0f /
        static_cast<float>(sampleRate * 0.18));
    const auto dcCoefficient = std::exp(-juce::MathConstants<float>::twoPi
        * 15.0f / static_cast<float>(sampleRate));
    const auto channels = std::min(2, buffer.getNumChannels());
    lastMotionDepth = motionDepth;
    lastDriveGain = driveGain;

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
                * (0.72 - 0.30 * amount)
                * (0.82 + 0.36 * random.unit()))));
        }
        smoothRandom += randomCoefficient * (randomTarget - smoothRandom);

        const auto angleA = juce::MathConstants<double>::twoPi * phaseA;
        const auto angleB = juce::MathConstants<double>::twoPi * phaseB;
        const auto oscillatorA = static_cast<float>(std::sin(angleA));
        const auto oscillatorB = static_cast<float>(std::sin(angleB));
        const auto quadrature = static_cast<float>(std::cos(angleA));
        const auto slowMotion = 0.56f * oscillatorA + 0.30f * oscillatorB
            + 0.14f * smoothRandom;
        const auto crossMotion = 0.54f * quadrature - 0.30f * oscillatorB
            + 0.16f * (envelopeMotion * 2.0f - 1.0f);
        const auto frequencyOctaves = motionDepth
            * ((0.28f + 0.58f * amount) * slowMotion
               + 0.34f * (envelopeMotion - 0.25f));
        const auto frequency = std::clamp(baseFrequency * std::pow(2.0f, frequencyOctaves),
                                          35.0f, maximumFrequency);
        const auto g = std::clamp(std::tan(juce::MathConstants<float>::pi
                                          * frequency / static_cast<float>(sampleRate * 2.0)),
                                  0.00001f, 24.0f);
        const auto resonance = std::clamp(0.08f + 0.54f * macroCurve
            + motionDepth * (0.07f * slowMotion + 0.06f * envelopeMotion), 0.0f, 0.82f);
        const auto damping = std::max(0.28f, 2.0f - 1.72f * std::sqrt(resonance));
        const auto morphPosition = std::clamp(character * 3.0f
            + motionDepth * (0.16f * crossMotion
                             + 0.08f * (envelopeMotion - 0.5f)), 0.0f, 3.0f);
        lastMorphPosition = morphPosition;
        const auto animatedCharacter = std::clamp(character
            + 0.08f * motionDepth * crossMotion, 0.0f, 1.0f);

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto previous = previousInput[static_cast<std::size_t>(channel)];
            auto tonal = 0.0f;
            for (int oversample = 0; oversample < 2; ++oversample)
            {
                const auto interpolation = 0.5f + 0.5f * static_cast<float>(oversample);
                const auto input = lerp(previous, dry[channel], interpolation);
                const auto animatedDrive = driveGain
                    * (1.0f + 0.10f * motionDepth * crossMotion);
                const auto shaped = waveshape(input * animatedDrive, animatedCharacter)
                    * driveCompensation;
                tonal += 0.5f * morphFilter(
                    mainFilters[static_cast<std::size_t>(channel)].process(shaped, g, damping),
                    morphPosition);
            }
            previousInput[static_cast<std::size_t>(channel)] = dry[channel];
            const auto dcBlocked = tonal - dcInput[static_cast<std::size_t>(channel)]
                + dcCoefficient * dcOutput[static_cast<std::size_t>(channel)];
            dcInput[static_cast<std::size_t>(channel)] = sanitise(tonal);
            dcOutput[static_cast<std::size_t>(channel)] = sanitise(dcBlocked);
            const auto processed = 1.04f * std::tanh(sanitise(dcBlocked) * 1.08f);
            buffer.setSample(channel, frame,
                sanitise(lerp(dry[channel], processed, wet)));
        }

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
    resetRealtimeState();
}

void SmearProcessor::resetRealtimeState() noexcept
{
    for (auto& grain : grains)
        grain = {};
    lowState.fill(0.0f);
    feedbackState.fill(0.0f);
    fastEnvelope = 0.0f;
    slowEnvelope = 0.0f;
    motionPhase = 0.0f;
    lastMotionAmount = 0.0f;
    writePosition = 0;
    validFrames = 0;
    grainCountdown = 0;
}

void SmearProcessor::setSeed(uint64_t seed) noexcept
{
    random.setSeed(seed ^ 0x736d6561722d6372ULL);
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
    destination->pitchPhase = static_cast<float>(random.unit());
    destination->pitchRate = static_cast<float>((0.18 + 0.72 * random.unit()) / sampleRate);
    destination->panPhase = static_cast<float>(random.unit());
    destination->panRate = static_cast<float>((0.10 + 0.44 * random.unit()) / sampleRate);
    destination->brightness = static_cast<float>(0.72 + 0.56 * random.unit());
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
        feedbackState.fill(0.0f);
        lastMotionAmount = 0.0f;
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
    const auto feedbackGain = 0.20f * std::pow(amount, 1.45f);
    const auto pitchOrbitCents = 3.0f + 11.0f * amount;
    const auto panOrbitDepth = 0.16f + 0.40f * amount;
    lastMotionAmount = amount;

    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        float dry[2] {
            sanitise(buffer.getSample(0, frame)),
            sanitise(buffer.getSample(std::min(1, channels - 1), frame))
        };
        delayBuffer.setSample(0, writePosition, sanitise(
            dry[0] + feedbackGain * feedbackState[0]));
        delayBuffer.setSample(1, writePosition, sanitise(
            dry[1] + feedbackGain * feedbackState[1]));
        validFrames = std::min(validFrames + 1, delayBuffer.getNumSamples());

        if (amount > 0.0f && grainCountdown-- <= 0)
        {
            startGrain(amount);
            const auto nominalLength = sampleRate * (0.070 - 0.040 * amount);
            const auto pulse = 0.90f + 0.10f * std::sin(
                juce::MathConstants<float>::twoPi * motionPhase);
            grainCountdown = std::max(1, static_cast<int>(std::llround(nominalLength
                * (0.52 - 0.27 * amount)
                * pulse * (0.86 + 0.28 * random.unit()))));
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
            const auto pitchOrbit = std::sin(juce::MathConstants<float>::twoPi
                                             * grain.pitchPhase);
            const auto panOrbit = std::sin(juce::MathConstants<float>::twoPi
                                           * grain.panPhase);
            const auto movingPan = std::clamp(grain.pan + panOrbitDepth * panOrbit,
                                              -1.0f, 1.0f);
            const auto increment = grain.increment * std::pow(2.0,
                static_cast<double>(pitchOrbitCents * pitchOrbit) / 1200.0);
            const auto left = sanitise(readDelay(0, grain.readPosition)) * grain.brightness;
            const auto right = sanitise(readDelay(1, grain.readPosition)) * grain.brightness;
            const auto leftPan = std::sqrt(0.5f * (1.0f - movingPan));
            const auto rightPan = std::sqrt(0.5f * (1.0f + movingPan));
            texture[0] += window * (left * leftPan + right * rightPan * 0.16f);
            texture[1] += window * (right * rightPan + left * leftPan * 0.16f);
            grain.readPosition = wrap(grain.readPosition + increment,
                                      static_cast<double>(delayBuffer.getNumSamples()));
            grain.pitchPhase = static_cast<float>(wrap(
                grain.pitchPhase + grain.pitchRate, 1.0));
            grain.panPhase = static_cast<float>(wrap(
                grain.panPhase + grain.panRate, 1.0));
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
            const auto crystal = sanitise(0.10f * texture[channel]
                + (1.34f + 0.72f * amount) * bright);
            feedbackState[static_cast<std::size_t>(channel)] = sanitise(
                0.82f * feedbackState[static_cast<std::size_t>(channel)]
                + 0.18f * std::tanh(bright * (1.0f + 0.65f * amount)));
            const auto output = dry[channel] * (1.0f - 0.08f * wet)
                + (1.26f + 0.44f * amount) * wet
                    * std::tanh(crystal * 1.25f);
            buffer.setSample(channel, frame, sanitise(output));
        }

        if (++writePosition >= delayBuffer.getNumSamples())
            writePosition = 0;
        motionPhase = static_cast<float>(wrap(
            motionPhase + (0.11 + 0.31 * amount) / sampleRate, 1.0));
    }
}
}

