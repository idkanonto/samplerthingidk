#pragma once

#include "SampleManager.h"
#include <cstdint>

class RandomSamplerVoice final
{
public:
    void prepare(double outputRate) noexcept { hostRate = outputRate; }
    bool isActive() const noexcept { return sample != nullptr; }
    int getNote() const noexcept { return midiNote; }
    uint64_t getAge() const noexcept { return age; }
    void start(PreparedSamplePtr newSample, int note, float velocity, double startFrame,
               randomchop::FrameRegion sourceRegion, double playbackPitchRatio,
               bool reverse, float voiceGain,
               float attackSeconds, float releaseSeconds,
               uint64_t newAge, float finalLengthMilliseconds = 0.0f) noexcept;
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
    PreparedSamplePtr sample;
    double sourcePosition = 0.0, increment = 1.0, hostRate = 44100.0;
    randomchop::FrameRegion region;
    bool playingInReverse = false;
    float level = 0.0f, targetLevel = 1.0f, attackStep = 1.0f, releaseStep = 1.0f;
    int midiNote = -1;
    uint64_t age = 0;
    float lastOutput[2] { 0.0f, 0.0f };
    float stealTail[2] { 0.0f, 0.0f };
    int stealTailRemaining = 0, stealTailLength = 1;
    std::int64_t renderedFrames = 0, finalLengthFrames = 0;
    int finalBoundaryReleaseFrames = 1;
    enum class Stage { attack, sustain, release } stage = Stage::attack;
};
