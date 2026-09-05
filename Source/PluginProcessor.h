#pragma once

#include <JuceHeader.h>
#include "VoicePool.h"
#include "RandomizationEngine.h"
#include "SourceSelection.h"
#include "StepMask.h"
#include "TakeHistory.h"

class RandomChopSamplerAudioProcessor final : public juce::AudioProcessor,
    private juce::AudioProcessorValueTreeState::Listener
{
public:
    RandomChopSamplerAudioProcessor();
    ~RandomChopSamplerAudioProcessor() override;
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
    randomchop::StepMask stepMask;
    randomchop::TakeHistory takeHistory;
    std::atomic<uint64_t> lastTriggeredRuntimeId { 0 };
    std::atomic<bool> triggeredWhileEmpty { false };

private:
    void noteOn(int note, float velocity) noexcept;
    void noteOff(int note) noexcept;
    void startTakeEvent(const randomchop::TakeEvent&, int note, float velocity) noexcept;
    void parameterChanged(const juce::String&, float) override;
    randomchop::VoicePool voices;
    RandomizationEngine random;
    double currentRate = 44100.0;
    uint64_t voiceCounter = 0;
    int lastSeed = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomChopSamplerAudioProcessor)
};
