#pragma once

#include "SampleManager.h"
#include "ChanceEvents.h"
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
    void start(PreparedSamplePtr newSample, int note, float velocity, double startFrame,
               randomchop::FrameRegion sourceRegion, double playbackPitchRatio,
               bool reverse, float voiceGain,
               float attackSeconds, float releaseSeconds,
               uint64_t newAge, float finalLengthMilliseconds = 0.0f,
               randomchop::EventDecision eventDecision = {}) noexcept;
    void release(float releaseSeconds) noexcept;
    void forceStop() noexcept
    {
        sample.reset();
        stealTailRemaining = 0;
        renderedFrames = finalLengthFrames = 0;
        lastOutput[0] = lastOutput[1] = 0.0f;
    }
    void render(juce::AudioBuffer<float>& output, int startSample, int numSamples) noexcept;

private:
    void resetPlaybackPass() noexcept;
    void advancePlayback(double step) noexcept;
    float updateDropGain() noexcept;

    PreparedSamplePtr sample;
    randomchop::EventDecision event;
    double sourcePosition = 0.0, increment = 1.0, baseIncrement = 1.0;
    double playbackStart = 0.0, reorderStart = 0.0, reorderPieceProgress = 0.0;
    double bendRatio = 1.0, bendMultiplier = 1.0, bendTargetRatio = 1.0;
    double hostRate = 44100.0;
    randomchop::FrameRegion region;
    bool playingInReverse = false;
    float level = 0.0f, targetLevel = 1.0f, attackStep = 1.0f, releaseStep = 1.0f;
    int midiNote = -1;
    uint64_t age = 0;
    float lastOutput[2] { 0.0f, 0.0f };
    float stealTail[2] { 0.0f, 0.0f };
    int stealTailRemaining = 0, stealTailLength = 1;
    std::int64_t renderedFrames = 0, finalLengthFrames = 0;
    std::int64_t retriggerFrames = 0, bendFramesRemaining = 0, dropSlotFrames = 1;
    int finalBoundaryReleaseFrames = 1;
    int retriggersRemaining = 0, reorderPieceIndex = 0;
    float dropGain = 1.0f, dropSmoothingStep = 1.0f;
    enum class Stage { attack, sustain, release } stage = Stage::attack;
};
