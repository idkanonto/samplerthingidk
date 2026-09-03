#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <functional>

class SourceWaveformComponent final : public juce::Component
{
public:
    void setSource(SampleManager::SamplePtr);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

    std::function<void(double, double)> onRegionChanged;

private:
    enum class DragMarker { none, start, end, coincident };
    juce::Rectangle<int> getWaveformBounds() const;
    double positionToNormalised(float x) const noexcept;
    juce::String markerDescription(const juce::String&, double) const;

    SampleManager::SamplePtr source;
    randomchop::NormalisedRegion region;
    DragMarker dragMarker = DragMarker::none;
};

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
    SourceWaveformComponent waveform;
    juce::ComboBox sourceKey;
    juce::Slider sourceTranspose, sourceFineTune, sourceGain, sourceWeight;
    juce::Label sourceKeyLabel, sourceTransposeLabel, sourceFineTuneLabel,
        sourceGainLabel, sourceWeightLabel;
    juce::ComboBox targetKey;
    juce::ToggleButton midiPitch { "MIDI Pitch" };
    juce::Slider rootNote, randomStart, attack, release, output, seed;
    juce::Label targetKeyLabel, rootNoteLabel, randomStartLabel, attackLabel,
        releaseLabel, outputLabel, seedLabel;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> randomStartAttachment, attackAttachment, releaseAttachment,
        outputAttachment, seedAttachment, rootNoteAttachment;
    std::unique_ptr<ComboBoxAttachment> targetKeyAttachment;
    std::unique_ptr<ButtonAttachment> midiPitchAttachment;
    std::shared_ptr<const SampleManager::Pool> displayPool;
    juce::String selectedSourceId;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String transientMessage;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomChopSamplerAudioProcessorEditor)
};
