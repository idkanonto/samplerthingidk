#pragma once

#include <JuceHeader.h>
#include <array>

namespace randomchop
{
struct FractureSettings
{
    float driveDb = 0.0f;
    float character = 0.0f;
    float filterMorph = 0.0f;
    float frequencyHz = 1000.0f;
    float resonance = 0.0f;
    float mix = 0.0f;
};

struct FracturePreset
{
    const char* name;
    FractureSettings settings;
};

inline constexpr std::array<FracturePreset, 30> fracturePresets {{
    { "Glass Teeth",          { 25.0f, 78.0f, 32.0f, 6400.0f, 61.0f, 72.0f } },
    { "Hollow Plastic",       { 16.0f, 31.0f, 81.0f,  620.0f, 70.0f, 68.0f } },
    { "Cheap Speaker",        { 20.0f, 47.0f, 19.0f, 1800.0f, 38.0f, 82.0f } },
    { "Razor Mouth",          { 32.0f, 88.0f, 26.0f, 5200.0f, 83.0f, 76.0f } },
    { "Metallic Tube",        { 21.0f, 66.0f, 94.0f, 2400.0f, 77.0f, 73.0f } },
    { "Telephone Melt",       { 27.0f, 55.0f, 43.0f, 1300.0f, 58.0f, 86.0f } },
    { "Chrome Resonance",     { 18.0f, 72.0f, 97.0f, 3900.0f, 91.0f, 64.0f } },
    { "Broken Tweeter",       { 30.0f, 93.0f, 12.0f, 8800.0f, 54.0f, 78.0f } },
    { "Nasal Circuit",        { 19.0f, 39.0f, 68.0f, 1050.0f, 88.0f, 79.0f } },
    { "Plastic Comb",         { 23.0f, 62.0f, 84.0f,  840.0f, 64.0f, 74.0f } },
    { "Digital Scream",       { 36.0f,100.0f, 57.0f, 7600.0f, 86.0f, 83.0f } },
    { "Boxed In",             { 14.0f, 24.0f,  7.0f,  430.0f, 49.0f, 70.0f } },
    { "Thin Air",             { 11.0f, 81.0f, 24.0f, 9800.0f, 42.0f, 59.0f } },
    { "Hollow Bell",          { 17.0f, 53.0f, 79.0f, 2100.0f, 94.0f, 67.0f } },
    { "Radio Burn",           { 26.0f, 44.0f, 38.0f, 1550.0f, 72.0f, 84.0f } },
    { "Crushed Formant",      { 29.0f, 70.0f, 65.0f,  930.0f, 79.0f, 88.0f } },
    { "Acid Mouth",           { 31.0f, 86.0f, 51.0f, 3100.0f, 93.0f, 77.0f } },
    { "Resonant Bite",        { 22.0f, 58.0f, 47.0f, 2700.0f, 98.0f, 62.0f } },
    { "Tiny Speaker",         { 18.0f, 35.0f, 16.0f, 2300.0f, 31.0f, 89.0f } },
    { "Overloaded Converter", { 34.0f, 96.0f,  3.0f, 4100.0f, 46.0f, 91.0f } },
    { "Metallic Vocal",       { 24.0f, 73.0f, 71.0f, 1650.0f, 82.0f, 75.0f } },
    { "Deep Hollow",          { 15.0f, 29.0f, 86.0f,  260.0f, 76.0f, 80.0f } },
    { "Screech Filter",       { 28.0f, 90.0f, 59.0f, 6900.0f, 99.0f, 69.0f } },
    { "Soft Damage",          {  8.0f, 18.0f, 14.0f, 4200.0f, 22.0f, 42.0f } },
    { "Hard Damage",          { 33.0f, 76.0f, 48.0f, 3600.0f, 68.0f, 92.0f } },
    { "Destroyed",            { 36.0f,100.0f,100.0f,  180.0f,100.0f,100.0f } },
    { "Porcelain Nose",       { 13.0f, 42.0f, 62.0f, 1200.0f, 85.0f, 57.0f } },
    { "Aluminium Throat",     { 20.0f, 64.0f, 89.0f, 3300.0f, 73.0f, 71.0f } },
    { "Paper Megaphone",      { 17.0f, 27.0f, 35.0f, 2000.0f, 52.0f, 81.0f } },
    { "Cold Wire",            { 24.0f, 84.0f, 92.0f, 5700.0f, 66.0f, 65.0f } }
}};

const FracturePreset& getFracturePreset(int index) noexcept;

class FractureProcessor final
{
public:
    void prepare(double newSampleRate);
    void reset() noexcept;
    void process(juce::AudioBuffer<float>& buffer, FractureSettings settings) noexcept;

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
    juce::AudioBuffer<float> combBuffer;
    double sampleRate = 44100.0;
    int combWritePosition = 0;
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
    void process(juce::AudioBuffer<float>& buffer, SmearSettings settings) noexcept;

private:
    static float sanitise(float value) noexcept;
    float readDelay(int channel, float delayFrames) const noexcept;

    juce::AudioBuffer<float> delayBuffer;
    std::array<float, 2> blurState { 0.0f, 0.0f };
    double sampleRate = 44100.0;
    double grainPhase = 0.0;
    int writePosition = 0;
};

struct CodecSettings
{
    float amount = 0.0f;
    int qualityChoice = 0;
    int rateFactor = 1;
};

class CodecProcessor final
{
public:
    static constexpr int rateFactorFromChoice(int choice) noexcept
    {
        constexpr std::array<int, 7> factors { 1, 2, 4, 8, 16, 32, 64 };
        return choice >= 0 && choice < static_cast<int>(factors.size())
            ? factors[static_cast<std::size_t>(choice)] : 1;
    }

    void prepare(double newSampleRate) noexcept;
    void reset() noexcept;
    void process(juce::AudioBuffer<float>& buffer, CodecSettings settings) noexcept;

private:
    static float sanitise(float value) noexcept;

    std::array<float, 2> bandwidthState { 0.0f, 0.0f };
    std::array<float, 2> previousReconstruction { 0.0f, 0.0f };
    std::array<float, 2> heldResidual { 0.0f, 0.0f };
    std::array<float, 2> wateryState { 0.0f, 0.0f };
    std::array<float, 2> rateHeld { 0.0f, 0.0f };
    double sampleRate = 44100.0;
    int packetCountdown = 0;
    int rateCountdown = 0;
    int previousRateFactor = 1;
};
}
