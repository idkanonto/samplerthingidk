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
    int rateFactor = 1;
};

struct FracturePreset
{
    const char* name;
    FractureSettings settings;
};

inline constexpr std::array<FracturePreset, 30> fracturePresets {{
    { "Glass Teeth",          { 64.0f, 78.0f,  2 } },
    { "Hollow Plastic",       { 52.0f, 31.0f,  1 } },
    { "Cheap Speaker",        { 61.0f, 47.0f,  4 } },
    { "Razor Mouth",          { 78.0f, 88.0f,  2 } },
    { "Metallic Tube",        { 67.0f, 66.0f,  1 } },
    { "Telephone Melt",       { 72.0f, 55.0f,  8 } },
    { "Chrome Resonance",     { 58.0f, 72.0f,  1 } },
    { "Broken Tweeter",       { 76.0f, 93.0f,  8 } },
    { "Nasal Circuit",        { 63.0f, 39.0f,  2 } },
    { "Plastic Comb",         { 60.0f, 62.0f,  1 } },
    { "Digital Scream",       { 86.0f,100.0f, 16 } },
    { "Boxed In",             { 47.0f, 24.0f,  2 } },
    { "Thin Air",             { 42.0f, 81.0f,  1 } },
    { "Hollow Bell",          { 55.0f, 53.0f,  1 } },
    { "Radio Burn",           { 70.0f, 44.0f,  8 } },
    { "Crushed Formant",      { 80.0f, 70.0f, 16 } },
    { "Acid Mouth",           { 74.0f, 86.0f,  4 } },
    { "Resonant Bite",        { 57.0f, 58.0f,  1 } },
    { "Tiny Speaker",         { 69.0f, 35.0f,  8 } },
    { "Overloaded Converter", { 88.0f, 96.0f, 32 } },
    { "Metallic Vocal",       { 66.0f, 73.0f,  2 } },
    { "Deep Hollow",          { 60.0f, 29.0f,  1 } },
    { "Screech Filter",       { 72.0f, 90.0f,  2 } },
    { "Soft Damage",          { 28.0f, 18.0f,  2 } },
    { "Hard Damage",          { 84.0f, 76.0f, 16 } },
    { "Destroyed",            {100.0f,100.0f, 64 } },
    { "Porcelain Nose",       { 45.0f, 42.0f,  1 } },
    { "Aluminium Throat",     { 62.0f, 64.0f,  2 } },
    { "Paper Megaphone",      { 65.0f, 27.0f,  4 } },
    { "Cold Wire",            { 59.0f, 84.0f,  4 } }
}};

const FracturePreset& getFracturePreset(int index) noexcept;

class FractureProcessor final
{
public:
    static constexpr int rateFactorFromChoice(int choice) noexcept
    {
        constexpr std::array<int, 7> factors { 1, 2, 4, 8, 16, 32, 64 };
        return choice >= 0 && choice < static_cast<int>(factors.size())
            ? factors[static_cast<std::size_t>(choice)] : 1;
    }

    static constexpr int choiceFromRateFactor(int factor) noexcept
    {
        constexpr std::array<int, 7> factors { 1, 2, 4, 8, 16, 32, 64 };
        for (int index = 0; index < static_cast<int>(factors.size()); ++index)
            if (factors[static_cast<std::size_t>(index)] == factor)
                return index;
        return 0;
    }

    void prepare(double newSampleRate);
    void reset() noexcept;
    void setSeed(uint64_t seed) noexcept;
    void process(juce::AudioBuffer<float>& buffer, FractureSettings settings) noexcept;

    int getLastEffectiveRateFactor() const noexcept { return lastEffectiveRateFactor; }
    float getLastMotionDepth() const noexcept { return lastMotionDepth; }

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
    float readComb(int channel, float delayFrames) const noexcept;

    std::array<StateVariableFilter, 2> mainFilters;
    std::array<StateVariableFilter, 2> formantFilters;
    std::array<float, 2> dcInput { 0.0f, 0.0f };
    std::array<float, 2> dcOutput { 0.0f, 0.0f };
    std::array<float, 2> bandwidthState { 0.0f, 0.0f };
    std::array<float, 2> previousReconstruction { 0.0f, 0.0f };
    std::array<float, 2> heldResidual { 0.0f, 0.0f };
    std::array<float, 2> rateHeld { 0.0f, 0.0f };
    juce::AudioBuffer<float> combBuffer;
    RandomizationEngine random;
    double sampleRate = 44100.0;
    double phaseA = 0.0;
    double phaseB = 0.25;
    float envelope = 0.0f;
    float smoothRandom = 0.0f;
    float randomTarget = 0.0f;
    float lastMotionDepth = 0.0f;
    int combWritePosition = 0;
    int randomCountdown = 0;
    int packetCountdown = 0;
    int rateCountdown = 0;
    int previousRateFactor = 1;
    int lastEffectiveRateFactor = 1;
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

private:
    static constexpr int maximumGrains = 6;
    struct Grain
    {
        double readPosition = 0.0;
        double increment = 1.0;
        float pan = 0.0f;
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
    RandomizationEngine random;
    double sampleRate = 44100.0;
    float fastEnvelope = 0.0f;
    float slowEnvelope = 0.0f;
    int writePosition = 0;
    int validFrames = 0;
    int grainCountdown = 0;
};
}
