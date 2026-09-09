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

class CreativeVisualizer final : public juce::Component
{
public:
    enum class Kind { scramble, fracture, smear };

    explicit CreativeVisualizer(Kind visualKind) : kind(visualKind) {}
    void setState(float primaryPercent, float secondaryPercent = 0.0f) noexcept;
    void advance() noexcept;
    void paint(juce::Graphics&) override;

private:
    Kind kind;
    float primary = 0.0f;
    float secondary = 0.0f;
    float phase = 0.0f;
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
    juce::ComboBox targetKey, voiceMode, globalGrid;
    juce::TextButton spectralDrawButton { "Draw" }, spectralEraseButton { "Erase" },
        spectralClearButton { "Clear" };
    juce::ComboBox spectralScanRate;
    SpectralCanvasComponent spectralCanvas;
    CreativeVisualizer scrambleVisual { CreativeVisualizer::Kind::scramble };
    CreativeVisualizer fractureVisual { CreativeVisualizer::Kind::fracture };
    CreativeVisualizer smearVisual { CreativeVisualizer::Kind::smear };
    juce::ToggleButton midiPitch { "MIDI Pitch" };
    juce::Slider rootNote, output;
    juce::Slider scrambleAmount, fractureCharacter, fractureMix, spectralDepth, smearAmount;
    juce::Label targetKeyLabel, rootNoteLabel, voiceModeLabel, globalGridLabel,
        outputLabel, scrambleAmountLabel,
        fractureCharacterLabel, fractureMixLabel, spectralDrawLabel, spectralScanRateLabel,
        spectralDepthLabel, smearAmountLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment, rootNoteAttachment,
        scrambleAmountAttachment, fractureCharacterAttachment, fractureMixAttachment,
        spectralDepthAttachment, smearAmountAttachment;
    std::unique_ptr<ComboBoxAttachment> targetKeyAttachment, voiceModeAttachment,
        globalGridAttachment, spectralScanRateAttachment;
    std::unique_ptr<ButtonAttachment> midiPitchAttachment;
    std::shared_ptr<const SampleManager::Pool> displayPool;
    juce::String selectedSourceId;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String transientMessage;
    uint64_t lastSpectralCanvasGeneration = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomChopSamplerAudioProcessorEditor)
};

