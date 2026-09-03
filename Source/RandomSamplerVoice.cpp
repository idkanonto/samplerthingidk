#include "RandomSamplerVoice.h"
#include <cmath>

void RandomSamplerVoice::start(SampleManager::SamplePtr newSample, int note, float velocity,
                               double startFrame, randomchop::FrameRegion sourceRegion,
                               bool reverse, float voiceGain, float attackSeconds,
                               float releaseSeconds, uint64_t newAge) noexcept
{
    if (isActive())
    {
        stealTail[0] = lastOutput[0];
        stealTail[1] = lastOutput[1];
        stealTailLength = juce::jmax(1, static_cast<int>(hostRate * 0.003));
        stealTailRemaining = stealTailLength;
    }
    else
    {
        stealTailRemaining = 0;
    }
    sample = std::move(newSample);
    midiNote = note;
    age = newAge;
    region = sourceRegion;
    playingInReverse = reverse;
    sourcePosition = juce::jlimit(static_cast<double>(region.firstFrame),
                                  randomchop::lastInterpolationPosition(region), startFrame);
    increment = sample->sampleRate / juce::jmax(1.0, hostRate);
    if (playingInReverse)
        increment = -increment;
    targetLevel = juce::jmax(0.0f, velocity * voiceGain);
    level = 0.0f;
    attackStep = attackSeconds <= 0.00001f ? targetLevel
        : targetLevel / juce::jmax(1.0f, attackSeconds * static_cast<float>(hostRate));
    releaseStep = targetLevel / juce::jmax(1.0f, releaseSeconds * static_cast<float>(hostRate));
    stage = Stage::attack;
}

void RandomSamplerVoice::release(float releaseSeconds) noexcept
{
    if (!isActive()) return;
    releaseStep = level / juce::jmax(1.0f, releaseSeconds * static_cast<float>(hostRate));
    stage = Stage::release;
}

void RandomSamplerVoice::render(juce::AudioBuffer<float>& output, int startSample, int numSamples) noexcept
{
    if (!sample) return;
    const auto sourceChannels = sample->audio->getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        if (!randomchop::isInterpolationPositionLegal(region, sourcePosition))
        {
            sample.reset();
            break;
        }
        const int index = static_cast<int>(sourcePosition);
        const float fraction = static_cast<float>(sourcePosition - index);

        if (stage == Stage::attack)
        {
            level += attackStep;
            if (level >= targetLevel) { level = targetLevel; stage = Stage::sustain; }
        }
        else if (stage == Stage::release)
        {
            level -= releaseStep;
            if (level <= 0.0f) { sample.reset(); break; }
        }

        // The fixed 3 ms boundary fade follows the active source region, not
        // the physical file, and works in either playback direction.
        const auto framesLeft = playingInReverse
            ? sourcePosition - static_cast<double>(region.firstFrame)
            : static_cast<double>(region.lastFrame) - sourcePosition;
        const float endGain = static_cast<float>(juce::jlimit(0.0, 1.0, framesLeft / (0.003 * sample->sampleRate)));
        const float tailGain = stealTailRemaining > 0
            ? static_cast<float>(stealTailRemaining) / static_cast<float>(stealTailLength) : 0.0f;
        for (int channel = 0; channel < output.getNumChannels(); ++channel)
        {
            const int sourceChannel = sourceChannels == 1 ? 0 : juce::jmin(channel, sourceChannels - 1);
            const float* data = sample->audio->getReadPointer(sourceChannel);
            const float value = data[index] + fraction * (data[index + 1] - data[index]);
            const float voiceOutput = value * level * endGain;
            const int stereoChannel = juce::jmin(channel, 1);
            output.addSample(channel, startSample + i, voiceOutput + stealTail[stereoChannel] * tailGain);
            lastOutput[stereoChannel] = voiceOutput;
        }
        if (stealTailRemaining > 0) --stealTailRemaining;
        sourcePosition += increment;
    }
}
