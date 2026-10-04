#pragma once

#include "SampleManager.h"
#include <cstdint>

class RandomSamplerVoice final
{
public:
    void prepare(double outputRate) noexcept
    {
        hostRate = std::clamp(randomchop::finiteOr(outputRate, 44100.0), 1.0, 768000.0);
    }
    bool isActive() const noexcept { return sample != nullptr; }
    int getNote() const noexcept { return midiNote; }
    uint64_t getAge() const noexcept { return age; }
    uint64_t getSourceRuntimeId() const noexcept { return sourceRuntimeId; }
    float getNormalisedSourcePosition() const noexcept
    {
        if (!isActive() || sample == nullptr || sample->audio == nullptr)
            return 0.0f;
        const auto extent = juce::jmax(1, sample->audio->getNumSamples() - 1);
        return juce::jlimit(0.0f, 1.0f,
            static_cast<float>(sourcePosition / static_cast<double>(extent)));
    }
    uint32_t consumeLoopEvents() noexcept
    {
        const auto events = pendingLoopEvents;
        pendingLoopEvents = 0;
        return events;
    }
    void start(PreparedSamplePtr newSample, int note, float velocity, double startFrame,
               randomchop::FrameRegion sourceRegion, double playbackPitchRatio,
               float voiceGain, float attackSeconds, float releaseSeconds,
               uint64_t newAge, float finalLengthMilliseconds = 0.0f,
               bool loopFirstQuarter = false, uint64_t newSourceRuntimeId = 0) noexcept;
    void release(float releaseSeconds) noexcept;
    void forceStop() noexcept
    {
        sample.reset();
        stealTailRemaining = 0;
        renderedFrames = finalLengthFrames = 0;
        loopQuarter = false;
        sourceRuntimeId = 0;
        pendingLoopEvents = 0;
        lastOutput[0] = lastOutput[1] = 0.0f;
    }
    void render(juce::AudioBuffer<float>& output, int startSample, int numSamples) noexcept;

private:
    PreparedSamplePtr sample;
    double sourcePosition = 0.0, increment = 1.0;
    double hostRate = 44100.0;
    randomchop::FrameRegion region;
    float level = 0.0f, targetLevel = 1.0f, attackStep = 1.0f, releaseStep = 1.0f;
    int midiNote = -1;
    uint64_t age = 0;
    uint64_t sourceRuntimeId = 0;
    float lastOutput[2] { 0.0f, 0.0f };
    float stealTail[2] { 0.0f, 0.0f };
    int stealTailRemaining = 0, stealTailLength = 1;
    std::int64_t renderedFrames = 0, finalLengthFrames = 0;
    int finalBoundaryReleaseFrames = 1;
    bool loopQuarter = false;
    double loopStart = 0.0, loopEnd = 0.0;
    uint32_t pendingLoopEvents = 0;
    enum class Stage { attack, sustain, release } stage = Stage::attack;
};
