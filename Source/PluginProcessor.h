#pragma once

#include <JuceHeader.h>
#include "CreativeEffects.h"
#include "HostGrid.h"
#include "RandomizationEngine.h"
#include "SourceSelection.h"
#include "StateMigration.h"
#include "SpectralDraw.h"
#include "TemporalEffects.h"
#include "VoicePool.h"

class RandomChopSamplerAudioProcessor final : public juce::AudioProcessor
{
public:
    RandomChopSamplerAudioProcessor();
    ~RandomChopSamplerAudioProcessor() override = default;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    randomchop::SpectralMaskStore::Canvas getSpectralCanvas() const;
    void setSpectralCanvas(const randomchop::SpectralMaskStore::Canvas& canvas);
    void clearSpectralCanvas();
    uint64_t getSpectralCanvasGeneration() const noexcept;
    float getSpectralScanPosition() const noexcept;
    uint32_t getScrambleVisualFlags() const noexcept
    {
        return scrambleVisualFlags.load(std::memory_order_relaxed);
    }
    float getScrambleVisualPhase() const noexcept
    {
        return scrambleVisualPhase.load(std::memory_order_relaxed);
    }
    float getFractureVisualMorph() const noexcept
    {
        return fractureVisualMorph.load(std::memory_order_relaxed);
    }
    float getFractureVisualMotion() const noexcept
    {
        return fractureVisualMotion.load(std::memory_order_relaxed);
    }
    float getSmearVisualActivity() const noexcept
    {
        return smearVisualActivity.load(std::memory_order_relaxed);
    }
    float getSmearVisualGain() const noexcept
    {
        return smearVisualGain.load(std::memory_order_relaxed);
    }
    juce::AudioProcessorValueTreeState parameters;
    SampleManager samples;
    std::atomic<uint64_t> lastTriggeredRuntimeId { 0 };
    std::atomic<bool> triggeredWhileEmpty { false };

private:
    void noteOn(int note, float velocity) noexcept;
    void noteOff(int note) noexcept;
    randomchop::HostTiming readHostTiming() const noexcept;

    randomchop::VoicePool voices;
    randomchop::FractureProcessor fractureProcessor;
    randomchop::SpectralMaskStore spectralMaskStore;
    randomchop::SpectralDrawProcessor spectralDrawProcessor;
    randomchop::SmearProcessor smearProcessor;
    randomchop::HostGrid hostGrid;
    randomchop::GridBoundaries lastGridBoundaries;
    randomchop::ScrambleProcessor scrambleProcessor;
    RandomizationEngine random;
    std::atomic<uint64_t> internalSeed { 1 };
    std::atomic<uint32_t> scrambleVisualFlags { 0 };
    std::atomic<float> scrambleVisualPhase { 0.0f };
    std::atomic<float> fractureVisualMorph { 0.0f };
    std::atomic<float> fractureVisualMotion { 0.0f };
    std::atomic<float> smearVisualActivity { 0.0f };
    std::atomic<float> smearVisualGain { 0.0f };
    double currentRate = 44100.0;
    uint64_t voiceCounter = 0;
    uint64_t lastSeed = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain { 1.0f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomChopSamplerAudioProcessor)
};

