#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
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
    void configureLinearControl(juce::Slider&, juce::Label&, const juce::String&);
    void refreshStepMaskControls();

    RandomChopSamplerAudioProcessor& processor;
    juce::Label title, status;
    juce::TextButton addButton { "Add samples..." }, clearButton { "Clear All" };
    juce::TextButton enableAllButton { "Enable All" }, disableAllButton { "Disable All" };
    juce::ListBox list { "Samples", this };
    SourceWaveformComponent waveform;
    juce::ComboBox sourceKey;
    juce::Slider sourceTranspose, sourceFineTune, sourceGain, sourceWeight, sourceStretch;
    juce::Label sourceKeyLabel, sourceTransposeLabel, sourceFineTuneLabel,
        sourceGainLabel, sourceWeightLabel, sourceStretchLabel;
    juce::ComboBox targetKey, voiceMode;
    juce::ComboBox stepLength;
    juce::ToggleButton midiPitch { "MIDI Pitch" };
    std::array<juce::ToggleButton, randomchop::StepMask::maximumSteps> stepButtons;
    juce::TextButton allNormalButton { "All NORMAL" }, allFxButton { "All FX" },
        randomiseStepsButton { "Randomize" };
    juce::Slider rootNote, randomStart, finalLength, attack, release, output, seed;
    juce::Slider reverseChance, retriggerChance, retriggerSize, retriggerCount,
        skipChance, reorderChance, bendChance, dropChance;
    juce::Label targetKeyLabel, rootNoteLabel, voiceModeLabel, randomStartLabel,
        finalLengthLabel, attackLabel, releaseLabel, outputLabel, seedLabel;
    juce::Label reverseChanceLabel, retriggerChanceLabel, retriggerSizeLabel,
        retriggerCountLabel, skipChanceLabel, reorderChanceLabel, bendChanceLabel,
        dropChanceLabel;
    juce::Label stepLengthLabel;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> randomStartAttachment, finalLengthAttachment,
        attackAttachment, releaseAttachment, outputAttachment, seedAttachment, rootNoteAttachment;
    std::unique_ptr<SliderAttachment> reverseChanceAttachment, retriggerChanceAttachment,
        retriggerSizeAttachment, retriggerCountAttachment, skipChanceAttachment,
        reorderChanceAttachment, bendChanceAttachment, dropChanceAttachment;
    std::unique_ptr<ComboBoxAttachment> targetKeyAttachment, voiceModeAttachment,
        stepLengthAttachment;
    std::unique_ptr<ButtonAttachment> midiPitchAttachment;
    std::shared_ptr<const SampleManager::Pool> displayPool;
    juce::String selectedSourceId;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String transientMessage;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomChopSamplerAudioProcessorEditor)
};
