#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <atomic>
#include <optional>
#include <vector>

class RandomChopSamplerWebViewEditor final : public juce::AudioProcessorEditor,
    public juce::FileDragAndDropTarget,
    private juce::AudioProcessorParameter::Listener,
    private juce::Timer
{
public:
    explicit RandomChopSamplerWebViewEditor(RandomChopSamplerAudioProcessor&);
    ~RandomChopSamplerWebViewEditor() override;

    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;

private:
    struct ParameterBinding
    {
        juce::String id;
        juce::RangedAudioParameter* parameter = nullptr;
        int processorIndex = -1;
    };

    static constexpr size_t maximumTrackedParameters = 64;

    juce::WebBrowserComponent::Options createBrowserOptions();
    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String&) const;
    juce::var createParameterState() const;
    juce::var createBackendState();
    juce::var createVisualisationState() const;
    void handleParameterValue(const juce::var&);
    void handleParameterGesture(const juce::var&);
    void handleCommand(const juce::var&);
    void emitBackendState();
    void openFileChooser();
    void addFiles(const juce::StringArray&);
    void ensureValidSelection(const std::shared_ptr<const SampleManager::Pool>&);
    ParameterBinding* findParameter(const juce::String&) noexcept;

    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int, bool) override {}
    void timerCallback() override;

    RandomChopSamplerAudioProcessor& processor;
    std::vector<ParameterBinding> parameterBindings;
    std::array<std::atomic<bool>, maximumTrackedParameters> dirtyParameters {};
    std::shared_ptr<const SampleManager::Pool> lastPool;
    juce::String selectedSampleId;
    std::unique_ptr<juce::FileChooser> fileChooser;
    uint64_t lastSpectralGeneration = 0;
    bool backendStateDirty = true;
    juce::WebBrowserComponent browser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomChopSamplerWebViewEditor)
};
