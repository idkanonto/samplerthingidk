#pragma once

#include "SampleManager.h"

class RandomSamplerVoice final
{
public:
    void prepare(double outputRate) noexcept { hostRate = outputRate; }
    bool isActive() const noexcept { return sample != nullptr; }
    int getNote() const noexcept { return midiNote; }
    uint64_t getAge() const noexcept { return age; }
    void start(SampleManager::SamplePtr newSample, int note, float velocity, double startFrame,
               float attackSeconds, float releaseSeconds, uint64_t newAge) noexcept;
    void release(float releaseSeconds) noexcept;
    void forceStop() noexcept { sample.reset(); stealTailRemaining = 0; lastOutput[0] = lastOutput[1] = 0.0f; }
    void render(juce::AudioBuffer<float>& output, int startSample, int numSamples) noexcept;

private:
    SampleManager::SamplePtr sample;
    double sourcePosition = 0.0, increment = 1.0, hostRate = 44100.0;
    float level = 0.0f, targetLevel = 1.0f, attackStep = 1.0f, releaseStep = 1.0f;
    int midiNote = -1;
    uint64_t age = 0;
    float lastOutput[2] { 0.0f, 0.0f };
    float stealTail[2] { 0.0f, 0.0f };
    int stealTailRemaining = 0, stealTailLength = 1;
    enum class Stage { attack, sustain, release } stage = Stage::attack;
};
