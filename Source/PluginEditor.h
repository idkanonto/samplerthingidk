#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class RandomChopSamplerAudioProcessorEditor final : public juce::AudioProcessorEditor,
    public juce::FileDragAndDropTarget, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit RandomChopSamplerAudioProcessorEditor(RandomChopSamplerAudioProcessor&);
    ~RandomChopSamplerAudioProcessorEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override;
    juce::Component* refreshComponentForRow(int, bool, juce::Component*) override;
    void selectedRowsChanged(int) override;
    void timerCallback() override;
    void refresh();
    void addFiles(const juce::StringArray&);
    void configureKnob(juce::Slider&, juce::Label&, const juce::String&);

    RandomChopSamplerAudioProcessor& processor;
    juce::Label title, status;
    juce::TextButton addButton { "Add samples..." }, clearButton { "Clear All" };
    juce::TextButton enableAllButton { "Enable All" }, disableAllButton { "Disable All" };
    juce::ListBox list { "Samples", this };
    juce::Slider sourceGain, sourceWeight;
    juce::Label sourceGainLabel, sourceWeightLabel;
    juce::Slider randomStart, attack, release, output, seed;
    juce::Label randomStartLabel, attackLabel, releaseLabel, outputLabel, seedLabel;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> randomStartAttachment, attackAttachment, releaseAttachment,
        outputAttachment, seedAttachment;
    std::shared_ptr<const SampleManager::Pool> displayPool;
    juce::String selectedSourceId;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String transientMessage;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomChopSamplerAudioProcessorEditor)
};
