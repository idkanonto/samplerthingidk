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
    combBuffer.clear();
    combWritePosition = 0;
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
    auto position = static_cast<float>(combWritePosition) - delayFrames;
    while (position < 0.0f)
        position += static_cast<float>(size);
    while (position >= static_cast<float>(size))
        position -= static_cast<float>(size);
    const auto first = static_cast<int>(position);
    const auto second = first + 1 < size ? first + 1 : 0;
    const auto fraction = position - static_cast<float>(first);
    return lerp(combBuffer.getSample(channel, first),
                combBuffer.getSample(channel, second), fraction);
}

void FractureProcessor::process(juce::AudioBuffer<float>& buffer,
                                FractureSettings settings) noexcept
{
    const auto mix = normalisePercent(settings.mix);
    if (mix <= 0.0f || combBuffer.getNumSamples() <= 1)
        return;

    const auto driveDb = std::clamp(finiteOr(settings.driveDb, 0.0f), 0.0f, 36.0f);
    const auto driveGain = std::pow(10.0f, driveDb / 20.0f);
    const auto character = normalisePercent(settings.character);
    const auto morphPosition = normalisePercent(settings.filterMorph) * 5.0f;
    const auto morphSegment = std::min(4, static_cast<int>(morphPosition));
    const auto morphFraction = morphPosition - static_cast<float>(morphSegment);
    const auto resonance = normalisePercent(settings.resonance);
    const auto damping = 2.0f - 1.91f * std::sqrt(resonance);
    const auto maximumFrequency = static_cast<float>(sampleRate * 0.45);
    const auto frequency = std::clamp(finiteOr(settings.frequencyHz, 1000.0f),
                                      30.0f, maximumFrequency);
    const auto formantFrequency = std::min(maximumFrequency,
        frequency * (1.65f + 1.25f * character));
    const auto g = std::clamp(std::tan(juce::MathConstants<float>::pi
                                      * frequency / static_cast<float>(sampleRate)),
                              0.00001f, 24.0f);
    const auto formantG = std::clamp(std::tan(juce::MathConstants<float>::pi
                                             * formantFrequency
                                             / static_cast<float>(sampleRate)),
                                     0.00001f, 24.0f);
    const auto combDelay = std::clamp(static_cast<float>(sampleRate) / frequency,
                                      2.0f,
                                      static_cast<float>(combBuffer.getNumSamples() - 2));
    const auto metallicDelay = std::clamp(combDelay * (0.503f + 0.11f * character),
        2.0f, static_cast<float>(combBuffer.getNumSamples() - 2));
    const auto combFeedback = 0.12f + 0.63f * resonance;
    const auto channels = std::min(2, buffer.getNumChannels());

    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto dry = sanitise(buffer.getSample(channel, frame));
            const auto shaped = waveshape(dry * driveGain, character);
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
            auto tonal = lerp(structures[static_cast<std::size_t>(morphSegment)],
                              structures[static_cast<std::size_t>(morphSegment + 1)],
                              morphFraction);
            const auto dcBlocked = tonal - dcInput[static_cast<std::size_t>(channel)]
                + 0.995f * dcOutput[static_cast<std::size_t>(channel)];
            dcInput[static_cast<std::size_t>(channel)] = sanitise(tonal);
            dcOutput[static_cast<std::size_t>(channel)] = sanitise(dcBlocked);
            const auto processed = 1.15f * std::tanh(sanitise(dcBlocked) * 1.15f);
            buffer.setSample(channel, frame,
                sanitise(lerp(dry, processed, mix)));
        }
        if (++combWritePosition >= combBuffer.getNumSamples())
            combWritePosition = 0;
    }
}

void SmearProcessor::prepare(double newSampleRate)
{
    sampleRate = std::clamp(std::isfinite(newSampleRate) ? newSampleRate : 44100.0,
                            1000.0, 768000.0);
    const auto frames = std::max(32, static_cast<int>(std::ceil(sampleRate * 0.5)) + 4);
    delayBuffer.setSize(2, frames, false, true, false);
    reset();
}

void SmearProcessor::reset() noexcept
{
    delayBuffer.clear();
    blurState.fill(0.0f);
    grainPhase = 0.0;
    writePosition = 0;
}

float SmearProcessor::sanitise(float value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, -64.0f, 64.0f) : 0.0f;
}

float SmearProcessor::readDelay(int channel, float delayFrames) const noexcept
{
    const auto size = delayBuffer.getNumSamples();
    if (size <= 1)
        return 0.0f;
    auto position = static_cast<float>(writePosition) - delayFrames;
    while (position < 0.0f)
        position += static_cast<float>(size);
    while (position >= static_cast<float>(size))
        position -= static_cast<float>(size);
    const auto first = static_cast<int>(position);
    const auto second = first + 1 < size ? first + 1 : 0;
    const auto fraction = position - static_cast<float>(first);
    return lerp(delayBuffer.getSample(channel, first),
                delayBuffer.getSample(channel, second), fraction);
}

void SmearProcessor::process(juce::AudioBuffer<float>& buffer,
                             SmearSettings settings) noexcept
{
    if (delayBuffer.getNumSamples() <= 1)
        return;
    const auto amount = normalisePercent(settings.amount);
    const auto grainFrames = std::clamp(
        static_cast<float>(sampleRate) * (0.012f + 0.060f * amount),
        8.0f, static_cast<float>(delayBuffer.getNumSamples() - 4));
    const auto baseDelay = std::clamp(
        static_cast<float>(sampleRate) * (0.008f + 0.180f * amount),
        2.0f, static_cast<float>(delayBuffer.getNumSamples() - 4));
    const auto sweepFrames = grainFrames * (0.4f + 1.6f * amount);
    const auto blurCoefficient = 0.35f - 0.32f * amount;
    const auto wet = 0.85f * amount;
    const auto channels = std::min(2, buffer.getNumChannels());

    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        const auto phaseA = static_cast<float>(grainPhase);
        const auto phaseB = std::fmod(phaseA + 0.5f, 1.0f);
        const auto weightA = 0.5f - 0.5f * std::cos(
            juce::MathConstants<float>::twoPi * phaseA);
        const auto weightB = 0.5f - 0.5f * std::cos(
            juce::MathConstants<float>::twoPi * phaseB);
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto dry = sanitise(buffer.getSample(channel, frame));
            delayBuffer.setSample(channel, writePosition, dry);
            if (amount > 0.0f)
            {
                const auto first = readDelay(channel, baseDelay + phaseA * sweepFrames);
                const auto second = readDelay(channel, baseDelay + phaseB * sweepFrames);
                const auto grain = sanitise(first * weightA + second * weightB);
                auto& blur = blurState[static_cast<std::size_t>(channel)];
                blur = sanitise(blur + blurCoefficient * (grain - blur));
                const auto texture = sanitise(grain * (0.76f - 0.22f * amount)
                                              + blur * (0.24f + 0.22f * amount));
                buffer.setSample(channel, frame,
                    sanitise(lerp(dry, texture, wet)));
            }
        }
        if (++writePosition >= delayBuffer.getNumSamples())
            writePosition = 0;
        grainPhase += (0.55 + 0.25 * (1.0 - amount))
            / static_cast<double>(grainFrames);
        if (grainPhase >= 1.0)
            grainPhase -= std::floor(grainPhase);
    }
}

void CodecProcessor::prepare(double newSampleRate) noexcept
{
    sampleRate = std::clamp(std::isfinite(newSampleRate) ? newSampleRate : 44100.0,
                            1000.0, 768000.0);
    reset();
}

void CodecProcessor::reset() noexcept
{
    bandwidthState.fill(0.0f);
    previousReconstruction.fill(0.0f);
    heldResidual.fill(0.0f);
    wateryState.fill(0.0f);
    rateHeld.fill(0.0f);
    packetCountdown = 0;
    rateCountdown = 0;
    previousRateFactor = 1;
}

float CodecProcessor::sanitise(float value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, -64.0f, 64.0f) : 0.0f;
}

void CodecProcessor::process(juce::AudioBuffer<float>& buffer,
                             CodecSettings settings) noexcept
{
    const auto amount = normalisePercent(settings.amount);
    const auto quality = std::clamp(settings.qualityChoice, 0, 3);
    const auto rateFactor = std::clamp(settings.rateFactor, 1, 64);
    if (rateFactor != previousRateFactor)
    {
        rateCountdown = 0;
        previousRateFactor = rateFactor;
    }
    if (amount <= 0.0f && rateFactor == 1)
        return;

    const auto qualityLoss = static_cast<float>(quality) / 3.0f;
    const auto loss = std::clamp(amount * (0.25f + 0.75f * qualityLoss), 0.0f, 1.0f);
    const auto cutoff = std::clamp(
        18000.0f * std::pow(2800.0f / 18000.0f, loss),
        80.0f, static_cast<float>(sampleRate * 0.45));
    const auto bandwidthCoefficient = 1.0f - std::exp(
        -juce::MathConstants<float>::twoPi * cutoff / static_cast<float>(sampleRate));
    const auto wateryCoefficient = 0.018f + 0.10f * (1.0f - loss);
    const auto packetFrames = 1 + static_cast<int>(std::lround(loss * 14.0f));
    const auto residualDrive = 1.0f + 5.0f * loss;
    const auto channels = std::min(2, buffer.getNumChannels());

    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        const bool packetCapture = packetCountdown == 0;
        const bool rateCapture = rateCountdown == 0;
        for (int channel = 0; channel < channels; ++channel)
        {
            const auto dry = sanitise(buffer.getSample(channel, frame));
            auto damaged = dry;
            if (amount > 0.0f)
            {
                auto& bandwidth = bandwidthState[static_cast<std::size_t>(channel)];
                bandwidth = sanitise(bandwidth
                    + bandwidthCoefficient * (dry - bandwidth));
                const auto previous = previousReconstruction[static_cast<std::size_t>(channel)];
                const auto predictor = previous + (bandwidth - previous) * (0.18f + 0.34f * loss);
                const auto residual = bandwidth - predictor;
                if (packetCapture)
                    heldResidual[static_cast<std::size_t>(channel)]
                        = std::tanh(residual * residualDrive) / residualDrive;
                const auto reconstruction = sanitise(predictor
                    + heldResidual[static_cast<std::size_t>(channel)] * (1.0f - 0.38f * loss));
                auto& watery = wateryState[static_cast<std::size_t>(channel)];
                watery = sanitise(watery
                    + wateryCoefficient * (reconstruction - watery));
                const auto metallic = sanitise(reconstruction
                    + (reconstruction - previous) * (0.16f * loss)
                    + (watery - reconstruction) * (0.28f * loss));
                previousReconstruction[static_cast<std::size_t>(channel)] = reconstruction;
                damaged = sanitise(lerp(dry, std::tanh(metallic * 1.1f) / 1.1f, amount));
            }
            if (rateCapture)
                rateHeld[static_cast<std::size_t>(channel)] = damaged;
            buffer.setSample(channel, frame,
                sanitise(rateHeld[static_cast<std::size_t>(channel)]));
        }

        if (packetCapture)
            packetCountdown = packetFrames - 1;
        else
            --packetCountdown;
        if (rateCapture)
            rateCountdown = rateFactor - 1;
        else
            --rateCountdown;
    }
}
}
