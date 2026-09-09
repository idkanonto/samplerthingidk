#pragma once

#include <JuceHeader.h>
#include "RandomizationEngine.h"
#include <array>
#include <cstdint>

namespace randomchop
{
struct FractureSettings
{
    float amount = 0.0f;
    float character = 0.0f;
};

class FractureProcessor final
{
public:
    void prepare(double newSampleRate);
    void reset() noexcept;
    void setSeed(uint64_t seed) noexcept;
    void process(juce::AudioBuffer<float>& buffer, FractureSettings settings) noexcept;

    float getLastMotionDepth() const noexcept { return lastMotionDepth; }
    float getLastMorphPosition() const noexcept { return lastMorphPosition; }
    float getLastDriveGain() const noexcept { return lastDriveGain; }

private:
    struct FilterOutputs
    {
        float low = 0.0f;
        float band = 0.0f;
        float high = 0.0f;
        float notch = 0.0f;
    };

    struct StateVariableFilter
    {
        FilterOutputs process(float input, float g, float damping) noexcept;
        void reset() noexcept { integrator1 = integrator2 = 0.0f; }
        float integrator1 = 0.0f;
        float integrator2 = 0.0f;
    };

    static float sanitise(float value) noexcept;
    static float waveshape(float input, float character) noexcept;
    static float morphFilter(const FilterOutputs&, float position) noexcept;

    std::array<StateVariableFilter, 2> mainFilters;
    std::array<float, 2> dcInput { 0.0f, 0.0f };
    std::array<float, 2> dcOutput { 0.0f, 0.0f };
    std::array<float, 2> previousInput { 0.0f, 0.0f };
    RandomizationEngine random;
    double sampleRate = 44100.0;
    double phaseA = 0.0;
    double phaseB = 0.37;
    float envelope = 0.0f;
    float smoothRandom = 0.0f;
    float randomTarget = 0.0f;
    float lastMotionDepth = 0.0f;
    float lastMorphPosition = 0.0f;
    float lastDriveGain = 1.0f;
    int randomCountdown = 0;
};

struct SmearSettings
{
    float amount = 0.0f;
};

class SmearProcessor final
{
public:
    void prepare(double newSampleRate);
    void reset() noexcept;
    void setSeed(uint64_t seed) noexcept;
    void process(juce::AudioBuffer<float>& buffer, SmearSettings settings) noexcept;

    int getActiveGrainCount() const noexcept;
    float getLastMotionAmount() const noexcept { return lastMotionAmount; }

private:
    static constexpr int maximumGrains = 6;
    struct Grain
    {
        double readPosition = 0.0;
        double increment = 1.0;
        float pan = 0.0f;
        float pitchPhase = 0.0f;
        float pitchRate = 0.0f;
        float panPhase = 0.0f;
        float panRate = 0.0f;
        float brightness = 1.0f;
        int age = 0;
        int length = 1;
        bool active = false;
    };

    static float sanitise(float value) noexcept;
    float readDelay(int channel, double position) const noexcept;
    void startGrain(float amount) noexcept;
    void resetRealtimeState() noexcept;

    juce::AudioBuffer<float> delayBuffer;
    std::array<Grain, maximumGrains> grains;
    std::array<float, 2> lowState { 0.0f, 0.0f };
    std::array<float, 2> feedbackState { 0.0f, 0.0f };
    RandomizationEngine random;
    double sampleRate = 44100.0;
    float fastEnvelope = 0.0f;
    float slowEnvelope = 0.0f;
    float motionPhase = 0.0f;
    float lastMotionAmount = 0.0f;
    int writePosition = 0;
    int validFrames = 0;
    int grainCountdown = 0;
};
}

