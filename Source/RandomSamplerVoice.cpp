#include "RandomSamplerVoice.h"
#include <cmath>

void RandomSamplerVoice::start(PreparedSamplePtr newSample, int note, float velocity,
                               double startFrame, randomchop::FrameRegion sourceRegion,
                               double playbackPitchRatio, bool reverse, float voiceGain,
                               float attackSeconds,
                               float releaseSeconds, uint64_t newAge,
                               float finalLengthMilliseconds,
                               randomchop::EventDecision eventDecision) noexcept
{
    if (newSample == nullptr || newSample->audio == nullptr)
    {
        forceStop();
        return;
    }
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
    event = eventDecision;
    playingInReverse = reverse || event.reverseEnabled;
    playbackStart = randomchop::resolvedEventStart(region, startFrame, event);
    reorderStart = playbackStart;
    const auto safePitchRatio = std::isfinite(playbackPitchRatio) && playbackPitchRatio > 0.0
        ? playbackPitchRatio : 1.0;
    baseIncrement = (sample->sampleRate / juce::jmax(1.0, hostRate)) * safePitchRatio;
    if (playingInReverse)
        baseIncrement = -baseIncrement;
    increment = baseIncrement;
    targetLevel = juce::jmax(0.0f, velocity * voiceGain);
    level = 0.0f;
    attackStep = attackSeconds <= 0.00001f ? targetLevel
        : targetLevel / juce::jmax(1.0f, attackSeconds * static_cast<float>(hostRate));
    releaseStep = targetLevel / juce::jmax(1.0f, releaseSeconds * static_cast<float>(hostRate));
    const auto safeFinalLength = !std::isfinite(finalLengthMilliseconds)
            || finalLengthMilliseconds <= 0.0f
        ? 0.0f : juce::jlimit(10.0f, 5000.0f, finalLengthMilliseconds);
    finalLengthFrames = safeFinalLength == 0.0f ? 0
        : juce::jmax<std::int64_t>(1, static_cast<std::int64_t>(std::llround(
            static_cast<double>(safeFinalLength) * hostRate / 1000.0)));
    finalBoundaryReleaseFrames = juce::jmax(1, static_cast<int>(std::llround(
        juce::jmax(0.0f, releaseSeconds) * static_cast<float>(hostRate))));
    if (finalLengthFrames > 0)
        finalBoundaryReleaseFrames = static_cast<int>(juce::jmin<std::int64_t>(
            finalBoundaryReleaseFrames, finalLengthFrames));
    renderedFrames = 0;
    retriggerFrames = 0;
    retriggersRemaining = event.retriggerEnabled ? event.retriggerCount : 0;
    bendRatio = 1.0;
    bendTargetRatio = event.bendEnabled
        ? std::exp2(static_cast<double>(juce::jlimit(
            -12.0f, 12.0f, event.bendDepthSemitones)) / 12.0) : 1.0;
    bendFramesRemaining = event.bendEnabled
        ? juce::jmax<std::int64_t>(1, static_cast<std::int64_t>(std::llround(hostRate))) : 0;
    bendMultiplier = bendFramesRemaining > 0
        ? std::pow(bendTargetRatio, 1.0 / static_cast<double>(bendFramesRemaining)) : 1.0;
    const auto availableSourceFrames = playingInReverse
        ? playbackStart - static_cast<double>(region.firstFrame)
        : randomchop::lastInterpolationPosition(region) - playbackStart;
    const auto naturalOutputFrames = std::max(1.0,
        availableSourceFrames / juce::jmax(0.000001, std::abs(baseIncrement)));
    const auto estimatedOutputFrames = naturalOutputFrames + (event.retriggerEnabled
        ? static_cast<double>(event.retriggerSizeFrames) * event.retriggerCount : 0.0);
    dropSlotFrames = juce::jmax<std::int64_t>(1, static_cast<std::int64_t>(
        std::ceil(estimatedOutputFrames / 8.0)));
    dropGain = 1.0f;
    dropSmoothingStep = 1.0f / juce::jmax(1.0f, static_cast<float>(hostRate * 0.001));
    resetPlaybackPass();
    stage = Stage::attack;
}

void RandomSamplerVoice::resetPlaybackPass() noexcept
{
    sourcePosition = playbackStart;
    reorderPieceIndex = 0;
    reorderPieceProgress = 0.0;
    if (event.reorderEnabled && event.reorderPieceSpanFrames > 0.0)
    {
        const auto direction = playingInReverse ? -1.0 : 1.0;
        sourcePosition = reorderStart + direction * event.reorderPieceSpanFrames
            * static_cast<double>(event.reorderPermutation[0]);
    }
}

void RandomSamplerVoice::advancePlayback(double step) noexcept
{
    sourcePosition += step;
    if (!event.reorderEnabled || event.reorderPieceSpanFrames <= 0.0
        || reorderPieceIndex >= 4)
        return;

    reorderPieceProgress += std::abs(step);
    const auto direction = playingInReverse ? -1.0 : 1.0;
    while (reorderPieceProgress >= event.reorderPieceSpanFrames && reorderPieceIndex < 4)
    {
        reorderPieceProgress -= event.reorderPieceSpanFrames;
        ++reorderPieceIndex;
        if (reorderPieceIndex < 4)
            sourcePosition = reorderStart + direction * (
                event.reorderPieceSpanFrames
                    * static_cast<double>(event.reorderPermutation[
                        static_cast<std::size_t>(reorderPieceIndex)])
                + reorderPieceProgress);
        else
            sourcePosition = reorderStart + direction * (
                4.0 * event.reorderPieceSpanFrames + reorderPieceProgress);
    }
}

float RandomSamplerVoice::updateDropGain() noexcept
{
    if (!event.dropEnabled)
        return 1.0f;
    const auto slot = static_cast<int>(juce::jmin<std::int64_t>(
        7, renderedFrames / dropSlotFrames));
    const auto muted = (event.dropMask & static_cast<std::uint8_t>(1u << slot)) != 0;
    const auto target = muted ? 0.0f : 1.0f;
    if (dropGain < target)
        dropGain = juce::jmin(target, dropGain + dropSmoothingStep);
    else if (dropGain > target)
        dropGain = juce::jmax(target, dropGain - dropSmoothingStep);
    return dropGain;
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
        if (event.retriggerEnabled && retriggersRemaining > 0
            && retriggerFrames >= event.retriggerSizeFrames)
        {
            resetPlaybackPass();
            retriggerFrames = 0;
            --retriggersRemaining;
        }
        if (finalLengthFrames > 0 && renderedFrames >= finalLengthFrames)
        {
            sample.reset();
            break;
        }
        if (!randomchop::isInterpolationPositionLegal(region, sourcePosition))
        {
            if (event.retriggerEnabled && retriggersRemaining > 0)
            {
                resetPlaybackPass();
                retriggerFrames = 0;
                --retriggersRemaining;
                if (!randomchop::isInterpolationPositionLegal(region, sourcePosition))
                {
                    sample.reset();
                    break;
                }
            }
            else
            {
                sample.reset();
                break;
            }
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
        auto endGain = static_cast<float>(juce::jlimit(
            0.0, 1.0, framesLeft / (0.003 * sample->sampleRate)));
        if (!randomchop::isInterpolationPositionLegal(region, sourcePosition + increment))
            endGain = 0.0f;
        float finalLengthGain = 1.0f;
        if (finalLengthFrames > 0)
        {
            const auto framesRemaining = finalLengthFrames - renderedFrames;
            if (framesRemaining <= finalBoundaryReleaseFrames)
            {
                finalLengthGain = finalBoundaryReleaseFrames <= 1 ? 0.0f
                    : juce::jlimit(0.0f, 1.0f,
                        static_cast<float>(framesRemaining - 1)
                            / static_cast<float>(finalBoundaryReleaseFrames - 1));
            }
        }
        const auto dropEnvelope = updateDropGain();
        const float tailGain = stealTailRemaining > 0
            ? static_cast<float>(stealTailRemaining) / static_cast<float>(stealTailLength) : 0.0f;
        for (int channel = 0; channel < output.getNumChannels(); ++channel)
        {
            const int sourceChannel = sourceChannels == 1 ? 0 : juce::jmin(channel, sourceChannels - 1);
            const float* data = sample->audio->getReadPointer(sourceChannel);
            const float value = data[index] + fraction * (data[index + 1] - data[index]);
            const float voiceOutput = value * level * endGain * finalLengthGain * dropEnvelope;
            const int stereoChannel = juce::jmin(channel, 1);
            output.addSample(channel, startSample + i, voiceOutput + stealTail[stereoChannel] * tailGain);
            lastOutput[stereoChannel] = voiceOutput;
        }
        if (stealTailRemaining > 0) --stealTailRemaining;
        advancePlayback(increment);
        ++retriggerFrames;
        ++renderedFrames;
        if (bendFramesRemaining > 0)
        {
            bendRatio *= bendMultiplier;
            --bendFramesRemaining;
            if (bendFramesRemaining == 0)
                bendRatio = bendTargetRatio;
            increment = baseIncrement * bendRatio;
        }
        if (finalLengthFrames > 0 && renderedFrames >= finalLengthFrames)
        {
            sample.reset();
            break;
        }
    }
}
