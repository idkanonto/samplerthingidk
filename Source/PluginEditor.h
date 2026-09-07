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

class SpectralCanvasComponent final : public juce::Component
{
public:
    using Canvas = randomchop::SpectralMaskStore::Canvas;

    void setCanvas(const Canvas& newCanvas);
    void clearCanvas();
    void setEraseMode(bool shouldErase) noexcept { eraseMode = shouldErase; }
    void setScanPosition(float position);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

    std::function<void(const Canvas&)> onCanvasChanged;

private:
    juce::Point<int> eventToCell(const juce::MouseEvent&) const noexcept;
    void applyLine(juce::Point<int> from, juce::Point<int> to);
    void applyBrush(juce::Point<int> cell) noexcept;

    Canvas canvas {};
    juce::Point<int> lastCell { -1, -1 };
    float scanPosition = 0.0f;
    bool eraseMode = false;
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
    void selectFracturePreset(int index);

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
    juce::ComboBox targetKey, voiceMode, globalGrid, rateReduction, codecQuality;
    juce::ComboBox freezeSize, freezeHold;
    juce::ComboBox fracturePreset;
    juce::TextButton previousFracturePreset { "<" }, nextFracturePreset { ">" };
    juce::TextButton spectralDrawButton { "Draw" }, spectralEraseButton { "Erase" },
        spectralClearButton { "Clear" };
    juce::ComboBox spectralScanRate;
    SpectralCanvasComponent spectralCanvas;
    juce::ToggleButton midiPitch { "MIDI Pitch" };
    juce::Slider rootNote, randomStart, finalLength, attack, release, output, seed;
    juce::Slider freezeChance, freezeOctaveChance, scrambleChance, scrambleAmount;
    juce::Slider fractureDrive, fractureCharacter, fractureFilterMorph, fractureFrequency,
        fractureResonance, fractureMix, spectralDepth, smearAmount, codecAmount;
    juce::Label targetKeyLabel, rootNoteLabel, voiceModeLabel, globalGridLabel,
        rateReductionLabel, randomStartLabel, finalLengthLabel, attackLabel,
        releaseLabel, outputLabel, seedLabel, freezeChanceLabel, freezeSizeLabel,
        freezeHoldLabel, freezeOctaveChanceLabel, scrambleChanceLabel,
        scrambleAmountLabel, fracturePresetLabel, fractureDriveLabel,
        fractureCharacterLabel, fractureFilterMorphLabel, fractureFrequencyLabel,
        fractureResonanceLabel, fractureMixLabel, spectralDrawLabel, spectralScanRateLabel,
        spectralDepthLabel, smearAmountLabel, codecAmountLabel, codecQualityLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> randomStartAttachment, finalLengthAttachment,
        attackAttachment, releaseAttachment, outputAttachment, seedAttachment, rootNoteAttachment,
        freezeChanceAttachment, freezeOctaveChanceAttachment, scrambleChanceAttachment,
        scrambleAmountAttachment, fractureDriveAttachment, fractureCharacterAttachment,
        fractureFilterMorphAttachment, fractureFrequencyAttachment,
        fractureResonanceAttachment, fractureMixAttachment, spectralDepthAttachment,
        smearAmountAttachment, codecAmountAttachment;
    std::unique_ptr<ComboBoxAttachment> targetKeyAttachment, voiceModeAttachment,
        globalGridAttachment, rateReductionAttachment, freezeSizeAttachment,
        freezeHoldAttachment, codecQualityAttachment, spectralScanRateAttachment;
    std::unique_ptr<ButtonAttachment> midiPitchAttachment;
    std::shared_ptr<const SampleManager::Pool> displayPool;
    juce::String selectedSourceId;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String transientMessage;
    int selectedFracturePreset = -1;
    uint64_t lastSpectralCanvasGeneration = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomChopSamplerAudioProcessorEditor)
};
