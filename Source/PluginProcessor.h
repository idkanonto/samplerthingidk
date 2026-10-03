#pragma once

#include <JuceHeader.h>
#include "CreativeEffects.h"
#include "FaultProcessor.h"
#include "HostGrid.h"
#include "RandomizationEngine.h"
#include "SourceSelection.h"
#include "StateMigration.h"
#include "SpectralDraw.h"
#include "TemporalEffects.h"
#include "VoicePool.h"
#include <algorithm>
#include <array>

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
    std::array<float, randomchop::SpectralDrawProcessor::displayBins>
        getDisplaySpectrum() const noexcept
    {
        return spectralDrawProcessor.getDisplaySpectrum();
    }
    int getFaultMutation() const noexcept
    {
        return faultMutation.load(std::memory_order_relaxed);
    }
    uint64_t getFaultEventSerial() const noexcept
    {
        return faultEventSerial.load(std::memory_order_relaxed);
    }
    int getFaultDivision() const noexcept
    {
        return faultDivision.load(std::memory_order_relaxed);
    }
    float getFaultProgress() const noexcept
    {
        return faultProgress.load(std::memory_order_relaxed);
    }
    int getFaultResampleSemitones() const noexcept
    {
        return faultResampleSemitones.load(std::memory_order_relaxed);
    }
    // Native-editor compatibility shims. The retired engines are never called
    // from processBlock; these values only keep the optional fallback editor
    // source-compatible while its old panels remain hidden.
    uint32_t getScrambleVisualFlags() const noexcept { return 0; }
    float getScrambleVisualPhase() const noexcept { return 0.0f; }
    float getMeltVisualStretch() const noexcept { return 0.0f; }
    float getMeltVisualProgress() const noexcept { return 0.0f; }
    uint32_t getMeltVisualFlags() const noexcept { return 0; }
    float getSmearVisualActivity() const noexcept
    {
        return smearVisualActivity.load(std::memory_order_relaxed);
    }
    float getSmearVisualGain() const noexcept
    {
        return smearVisualGain.load(std::memory_order_relaxed);
    }
    float getVisualAudioLevel() const noexcept
    {
        return visualAudioLevel.load(std::memory_order_relaxed);
    }
    void requestSourcePreview(uint64_t runtimeId) noexcept;
    uint64_t getPreviewingSourceId() const noexcept
    {
        return previewingRuntimeId.load(std::memory_order_relaxed);
    }
    float getOutputPeak() const noexcept
    {
        return outputPeak.load(std::memory_order_relaxed);
    }
    float getOutputPeakLeft() const noexcept
    {
        return outputPeakLeft.load(std::memory_order_relaxed);
    }
    float getOutputPeakRight() const noexcept
    {
        return outputPeakRight.load(std::memory_order_relaxed);
    }
    int getActiveVoiceCount() const noexcept
    {
        return activeVoiceCount.load(std::memory_order_relaxed);
    }
    int getUiScaleIndex() const noexcept
    {
        return uiScaleIndex.load(std::memory_order_relaxed);
    }
    void setUiScaleIndex(int index) noexcept
    {
        uiScaleIndex.store(std::clamp(index, 0, 3), std::memory_order_relaxed);
    }
    juce::String getSelectedSampleId() const;
    void setSelectedSampleId(const juce::String&);
    void regenerateCreativeSeed();
    uint32_t getSmearFeatures() const noexcept
    {
        return smearFeatures.load(std::memory_order_relaxed);
    }
    uint32_t getScrambleFeatures() const noexcept
    {
        return scrambleFeatures.load(std::memory_order_relaxed);
    }
    uint32_t getMeltFeatures() const noexcept
    {
        return meltFeatures.load(std::memory_order_relaxed);
    }
    void setScrambleFeatures(uint32_t features) noexcept
    {
        scrambleFeatures.store(features & randomchop::ScrambleFeatures::all,
                               std::memory_order_relaxed);
    }
    void setMeltFeatures(uint32_t features) noexcept
    {
        meltFeatures.store(features & randomchop::MeltFeatures::all,
                           std::memory_order_relaxed);
    }
    uint32_t getFaultMutations() const noexcept
    {
        return faultMutations.load(std::memory_order_relaxed);
    }
    void setFaultMutations(uint32_t mutations) noexcept
    {
        faultMutations.store(mutations & randomchop::FaultMutations::all,
                             std::memory_order_relaxed);
    }
    void setSmearFeatures(uint32_t features) noexcept
    {
        smearFeatures.store(features & randomchop::SmearFeatures::all,
                            std::memory_order_relaxed);
    }
    bool isEffectEnabled(int effect) const noexcept
    {
        return effect >= 0 && effect < 3
            ? effectEnabled[static_cast<size_t>(effect)].load(std::memory_order_relaxed)
            : false;
    }
    void setEffectEnabled(int effect, bool enabled) noexcept
    {
        if (effect >= 0 && effect < 3)
            effectEnabled[static_cast<size_t>(effect)].store(enabled,
                std::memory_order_relaxed);
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
    RandomSamplerVoice previewVoice;
    randomchop::FaultProcessor faultProcessor;
    randomchop::SpectralMaskStore spectralMaskStore;
    randomchop::SpectralDrawProcessor spectralDrawProcessor;
    randomchop::SmearProcessor smearProcessor;
    randomchop::HostGrid hostGrid;
    randomchop::HostGrid faultGrid;
    randomchop::GridBoundaries lastGridBoundaries;
    RandomizationEngine random;
    std::atomic<uint64_t> internalSeed { 1 };
    std::atomic<int> faultMutation { 0 };
    std::atomic<uint64_t> faultEventSerial { 0 };
    std::atomic<int> faultDivision { 16 };
    std::atomic<float> faultProgress { 0.0f };
    std::atomic<int> faultResampleSemitones { 0 };
    std::atomic<float> smearVisualActivity { 0.0f };
    std::atomic<float> smearVisualGain { 0.0f };
    std::atomic<float> visualAudioLevel { 0.0f };
    std::atomic<uint64_t> previewRequestId { 0 };
    std::atomic<uint64_t> previewRequestSerial { 0 };
    std::atomic<uint64_t> previewingRuntimeId { 0 };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<float> outputPeakLeft { 0.0f };
    std::atomic<float> outputPeakRight { 0.0f };
    std::atomic<int> activeVoiceCount { 0 };
    std::atomic<int> uiScaleIndex { 1 };
    std::atomic<uint32_t> faultMutations { randomchop::FaultMutations::all };
    std::atomic<uint32_t> scrambleFeatures { randomchop::ScrambleFeatures::all };
    std::atomic<uint32_t> meltFeatures { randomchop::MeltFeatures::all };
    std::atomic<uint32_t> smearFeatures { randomchop::SmearFeatures::all };
    std::array<std::atomic<bool>, 3> effectEnabled {{ true, true, true }};
    mutable juce::CriticalSection editorStateLock;
    juce::String selectedSampleId;
    uint64_t handledPreviewSerial = 0;
    double currentRate = 44100.0;
    uint64_t voiceCounter = 0;
    uint64_t lastSeed = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain { 1.0f };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomChopSamplerAudioProcessor)
};
