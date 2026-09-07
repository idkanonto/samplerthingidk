#pragma once

#include <JuceHeader.h>
#include "HostGrid.h"
#include "MasterDigitalProcessor.h"
#include "RandomizationEngine.h"
#include "SourceSelection.h"
#include "StateMigration.h"
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
    juce::AudioProcessorValueTreeState parameters;
    SampleManager samples;
    std::atomic<uint64_t> lastTriggeredRuntimeId { 0 };
    std::atomic<bool> triggeredWhileEmpty { false };

private:
    void noteOn(int note, float velocity) noexcept;
    void noteOff(int note) noexcept;
    randomchop::HostTiming readHostTiming() const noexcept;

    randomchop::VoicePool voices;
    randomchop::MasterDigitalProcessor masterDigitalProcessor;
    randomchop::HostGrid hostGrid;
    randomchop::GridBoundaries lastGridBoundaries;
    RandomizationEngine random;
    double currentRate = 44100.0;
    uint64_t voiceCounter = 0;
    int lastSeed = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomChopSamplerAudioProcessor)
};
