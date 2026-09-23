#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <cstdint>
#include <functional>

class XpLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    XpLookAndFeel();
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float,
                          juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int,
                      juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int, int, int, int,
                       bool, int, int, bool, bool) override;
    void drawTooltip(juce::Graphics&, const juce::String&, int, int) override;
};

class SourceWaveformComponent final : public juce::Component,
    public juce::SettableTooltipClient
{
public:
    void setSource(SampleManager::SamplePtr);
    void zoomIn();
    void zoomOut();
    void focusRegion();
    void fitAll();
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
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
    DragMarker hoverMarker = DragMarker::none;
    double viewStart = 0.0;
    double viewSpan = 1.0;
};

class SpectralCanvasComponent final : public juce::Component,
    public juce::SettableTooltipClient
{
public:
    using Canvas = randomchop::SpectralMaskStore::Canvas;

    void setCanvas(const Canvas& newCanvas);
    void setSpectrum(const std::array<float, randomchop::SpectralDrawProcessor::displayBins>&);
    void clearCanvas();
    void setScanPosition(float position);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    std::function<void(const Canvas&)> onCanvasChanged;

private:
    juce::Point<int> eventToCell(const juce::MouseEvent&) const noexcept;
    void applyLine(juce::Point<int> from, juce::Point<int> to);
    void applyBrush(juce::Point<int> cell) noexcept;

    Canvas canvas {};
    std::array<float, randomchop::SpectralDrawProcessor::displayBins> spectrum {};
    juce::Point<int> lastCell { -1, -1 };
    juce::Point<float> hoverPosition { -1.0f, -1.0f };
    float scanPosition = 0.0f;
};

class CreativeVisualizer final : public juce::Component,
    public juce::SettableTooltipClient
{
public:
    enum class Kind { scramble, melt, smear };

    explicit CreativeVisualizer(Kind visualKind) : kind(visualKind) {}
    void setState(float primaryPercent) noexcept;
    void setTelemetry(float first, float second = 0.0f,
                      uint32_t flags = 0) noexcept;
    void advance() noexcept;
    void paint(juce::Graphics&) override;

private:
    Kind kind;
    float primary = 0.0f;
    float phase = 0.0f;
    float telemetryFirst = 0.0f;
    float telemetrySecond = 0.0f;
    uint32_t telemetryFlags = 0;
};

class OutputMeterComponent final : public juce::Component
{
public:
    void setPeak(float newPeak) noexcept;
    void paint(juce::Graphics&) override;
private:
    float peak = 0.0f;
};

class XpInfoButton final : public juce::Button
{
public:
    XpInfoButton() : juce::Button("Information") {}
    void paintButton(juce::Graphics&, bool isMouseOverButton,
                     bool isButtonDown) override;
};

class XpWindowCloseButton final : public juce::Button
{
public:
    XpWindowCloseButton() : juce::Button("Close window") {}
    void paintButton(juce::Graphics&, bool, bool) override;
};

class XpModalOverlay final : public juce::Component
{
public:
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    std::function<void()> onDismiss;
};

class XpInfoPanel final : public juce::Component
{
public:
    XpInfoPanel();
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;

    std::function<void()> onClose;

private:
    juce::TextButton closeButton { "Close" };
    XpWindowCloseButton titleCloseButton;
};

class RandomChopSamplerAudioProcessorEditor final : public juce::AudioProcessorEditor,
    public juce::FileDragAndDropTarget, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit RandomChopSamplerAudioProcessorEditor(RandomChopSamplerAudioProcessor&);
    ~RandomChopSamplerAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;

private:
    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override;
    juce::Component* refreshComponentForRow(int, bool, juce::Component*) override;
    void selectedRowsChanged(int) override;
    void timerCallback() override;
    void refresh();
    void addFiles(const juce::StringArray&);
    void selectRelativeSource(int delta);
    void showSampleMenu();
    void openFileChooser();
    void selectTab(int tab);
    void showEffectModeMenu(int effect);
    void configureKnob(juce::Slider&, juce::Label&, const juce::String&);
    void configureLinearControl(juce::Slider&, juce::Label&, const juce::String&);

    RandomChopSamplerAudioProcessor& processor;
    XpLookAndFeel xpLookAndFeel;
    juce::Label title, subtitle, alert, status;
    juce::Label pageMessage;
    juce::TextButton pageActionButton { "BACK TO MAIN" };
    XpInfoButton infoButton;
    XpModalOverlay modalOverlay;
    XpInfoPanel infoPanel;
    juce::ListBox list { "Samples", this };
    juce::TextButton addButton { "+ ADD" }, sampleMenuButton { "=" };
    juce::TextButton previousSourceButton { "<" }, nextSourceButton { ">" };
    juce::TextButton closeEditorButton { "X" };
    juce::TextButton mainTab { "MAIN" }, fxTab { "FX" }, seqTab { "SEQ" }, settingsTab { "SETTINGS" };
    juce::TextButton zoomInButton { "+" }, zoomOutButton { "-" }, focusRegionButton { "[]" }, fitButton { "FIT" };
    juce::TextButton randomSourceButton { "DICE" }, regenerateButton { "R" };
    juce::TextButton muteButton { "MUTE" }, moreButton { "..." };
    juce::TextButton scrambleModeButton { "Random" }, meltModeButton { "Stretch" },
        smearModeButton { "Diffuse" };
    juce::TextButton scrambleFoldButton { ">" }, meltFoldButton { ">" },
        smearFoldButton { ">" }, spectralFoldButton { ">" }, outputFoldButton { ">" };
    juce::TextButton scramblePowerButton { "o" }, meltPowerButton { "o" },
        smearPowerButton { "o" }, spectralPowerButton { "o" }, outputPowerButton { "o" };
    juce::TextButton chordsOffButton { "OFF" }, chordsOnButton { "ON" };
    juce::TextButton polyButton { "POLY" }, monoButton { "MONO" };
    SourceWaveformComponent waveform;
    juce::ComboBox sourceKey;
    juce::Slider sourceTranspose, sourceFineTune, sourceGain;
    juce::Label sourceKeyLabel, sourceTransposeLabel, sourceFineTuneLabel,
        sourceGainLabel;
    juce::ComboBox targetKey;
    juce::ToggleButton voiceMode { "MONO" };
    juce::TextButton spectralResetButton { "Reset" };
    SpectralCanvasComponent spectralCanvas;
    CreativeVisualizer scrambleVisual { CreativeVisualizer::Kind::scramble };
    CreativeVisualizer meltVisual { CreativeVisualizer::Kind::melt };
    CreativeVisualizer smearVisual { CreativeVisualizer::Kind::smear };
    juce::ToggleButton midiPitch { "CHORDS" };
    juce::Slider output;
    OutputMeterComponent outputMeter;
    juce::Slider scrambleAmount, meltAmount, spectralDepth, smearAmount;
    juce::Label targetKeyLabel, voiceModeLabel,
        outputLabel, scrambleAmountLabel,
        meltAmountLabel, spectralDrawLabel,
        spectralDepthLabel, smearAmountLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment,
        scrambleAmountAttachment, meltAmountAttachment,
        spectralDepthAttachment, smearAmountAttachment;
    std::unique_ptr<ComboBoxAttachment> targetKeyAttachment;
    std::unique_ptr<ButtonAttachment> midiPitchAttachment, voiceModeAttachment;
    std::shared_ptr<const SampleManager::Pool> displayPool;
    juce::String selectedSourceId;
    juce::String transientMessage;
    int transientMessageTicks = 0;
    uint64_t lastSpectralCanvasGeneration = 0;
    bool dragActive = false;
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
    std::unique_ptr<juce::FileChooser> fileChooser;
    int selectedTab = 0;
    bool expandedCards[5] { true, true, true, true, true };
    juce::Rectangle<int> titleBarBounds, footerBounds, pagePaneBounds,
        samplePanelBounds, sampleDropBounds,
        sourcePanelBounds, globalPanelBounds, scramblePanelBounds,
        meltPanelBounds, smearPanelBounds, spectralPanelBounds, outputPanelBounds;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomChopSamplerAudioProcessorEditor)
};

