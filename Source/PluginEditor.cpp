#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

void SourceWaveformComponent::setSource(SampleManager::SamplePtr newSource)
{
    source = std::move(newSource);
    region = source != nullptr
        ? randomchop::clampNormalisedRegion(source->settings.startNormalised,
                                            source->settings.endNormalised)
        : randomchop::NormalisedRegion {};
    repaint();
}

juce::Rectangle<int> SourceWaveformComponent::getWaveformBounds() const
{
    return getLocalBounds().reduced(4).withTrimmedTop(16).withTrimmedBottom(18);
}

double SourceWaveformComponent::positionToNormalised(float x) const noexcept
{
    const auto bounds = getWaveformBounds();
    if (bounds.getWidth() <= 0)
        return 0.0;
    return juce::jlimit(0.0, 1.0,
        static_cast<double>(x - static_cast<float>(bounds.getX()))
            / static_cast<double>(bounds.getWidth()));
}

juce::String SourceWaveformComponent::markerDescription(const juce::String& name,
                                                          double position) const
{
    if (source == nullptr || source->audio == nullptr)
        return name;

    const auto lastFrame = juce::jmax(0, source->audio->getNumSamples() - 1);
    const auto frame = juce::jlimit(0, lastFrame,
        static_cast<int>(std::llround(position * static_cast<double>(lastFrame))));
    const auto seconds = static_cast<double>(frame) / juce::jmax(1.0, source->sampleRate);
    return name + "  " + juce::String(position * 100.0, 1) + "%  |  "
        + juce::String(seconds, 3) + " s  |  sample " + juce::String(frame);
}

void SourceWaveformComponent::paint(juce::Graphics& g)
{
    const auto outer = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff171920));
    g.fillRoundedRectangle(outer, 7.0f);
    g.setColour(juce::Colour(0xff494c5a));
    g.drawRoundedRectangle(outer.reduced(0.5f), 7.0f, 1.0f);

    g.setFont(12.0f);
    g.setColour(juce::Colour(0xffc8cad1));
    juce::String heading("SOURCE REGION");
    if (source != nullptr)
    {
        heading += " — ";
        heading += source->settings.displayName;
    }
    g.drawText(heading, getLocalBounds().reduced(4).removeFromTop(14),
               juce::Justification::centredLeft, true);

    const auto waveBounds = getWaveformBounds();
    g.setColour(juce::Colour(0xff20232c));
    g.fillRect(waveBounds);
    g.setColour(juce::Colour(0xff343846));
    g.drawHorizontalLine(waveBounds.getCentreY(), static_cast<float>(waveBounds.getX()),
                         static_cast<float>(waveBounds.getRight()));

    if (source == nullptr)
    {
        g.setColour(juce::Colour(0xff8f93a3));
        g.drawText("Select a source to edit its region", waveBounds,
                   juce::Justification::centred);
        return;
    }

    if (source->settings.missing || source->audio == nullptr
        || source->waveformPeaks == nullptr || source->waveformPeaks->empty())
    {
        g.setColour(juce::Colour(0xffff8a8a));
        g.drawText("Waveform unavailable for missing source", waveBounds,
                   juce::Justification::centred);
    }
    else
    {
        const auto& peaks = *source->waveformPeaks;
        const auto peakCount = peaks.size();
        const auto halfHeight = static_cast<float>(waveBounds.getHeight()) * 0.46f;
        const auto centreY = static_cast<float>(waveBounds.getCentreY());
        g.setColour(juce::Colour(0xff9a7cff));
        for (int x = 0; x < waveBounds.getWidth(); ++x)
        {
            const auto first = static_cast<size_t>(x) * peakCount
                / static_cast<size_t>(waveBounds.getWidth());
            const auto last = juce::jmax(first + 1,
                static_cast<size_t>(x + 1) * peakCount
                    / static_cast<size_t>(waveBounds.getWidth()));
            float minimum = 0.0f;
            float maximum = 0.0f;
            for (auto peak = first; peak < juce::jmin(last, peakCount); ++peak)
            {
                minimum = juce::jmin(minimum, peaks[peak].minimum);
                maximum = juce::jmax(maximum, peaks[peak].maximum);
            }
            g.drawVerticalLine(waveBounds.getX() + x,
                               centreY - maximum * halfHeight,
                               centreY - minimum * halfHeight);
        }
    }

    const auto startX = static_cast<float>(waveBounds.getX())
        + static_cast<float>(region.start) * static_cast<float>(waveBounds.getWidth());
    const auto endX = static_cast<float>(waveBounds.getX())
        + static_cast<float>(region.end) * static_cast<float>(waveBounds.getWidth());
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRect(juce::Rectangle<float>(static_cast<float>(waveBounds.getX()),
                                     static_cast<float>(waveBounds.getY()),
                                     juce::jmax(0.0f, startX - waveBounds.getX()),
                                     static_cast<float>(waveBounds.getHeight())));
    g.fillRect(juce::Rectangle<float>(endX, static_cast<float>(waveBounds.getY()),
                                     juce::jmax(0.0f, waveBounds.getRight() - endX),
                                     static_cast<float>(waveBounds.getHeight())));

    g.setColour(juce::Colour(0xff66e3a4));
    g.drawLine(startX, static_cast<float>(waveBounds.getY()), startX,
               static_cast<float>(waveBounds.getBottom()), 2.0f);
    g.setColour(juce::Colour(0xffffa65c));
    g.drawLine(endX, static_cast<float>(waveBounds.getY()), endX,
               static_cast<float>(waveBounds.getBottom()), 2.0f);

    auto footer = getLocalBounds().reduced(4).removeFromBottom(16);
    auto startText = footer.removeFromLeft(footer.getWidth() / 2);
    g.setFont(10.0f);
    g.setColour(juce::Colour(0xff66e3a4));
    g.drawText(markerDescription("START", region.start), startText,
               juce::Justification::centredLeft, true);
    g.setColour(juce::Colour(0xffffa65c));
    g.drawText(markerDescription("END", region.end), footer,
               juce::Justification::centredRight, true);
}

void SourceWaveformComponent::mouseDown(const juce::MouseEvent& event)
{
    if (source == nullptr || source->audio == nullptr)
        return;

    const auto bounds = getWaveformBounds();
    const auto startX = static_cast<float>(bounds.getX())
        + static_cast<float>(region.start) * static_cast<float>(bounds.getWidth());
    const auto endX = static_cast<float>(bounds.getX())
        + static_cast<float>(region.end) * static_cast<float>(bounds.getWidth());
    if (std::abs(startX - endX) < 0.5f)
    {
        const auto position = positionToNormalised(event.position.x);
        dragMarker = position < region.start ? DragMarker::start
            : (position > region.end ? DragMarker::end : DragMarker::coincident);
    }
    else
    {
        dragMarker = std::abs(event.position.x - startX) <= std::abs(event.position.x - endX)
            ? DragMarker::start : DragMarker::end;
    }

    if (dragMarker != DragMarker::coincident)
        mouseDrag(event);
}

void SourceWaveformComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (dragMarker == DragMarker::none || source == nullptr)
        return;

    const auto position = positionToNormalised(event.position.x);
    if (dragMarker == DragMarker::coincident)
    {
        if (position < region.start)
            dragMarker = DragMarker::start;
        else if (position > region.end)
            dragMarker = DragMarker::end;
        else
            return;
    }

    if (dragMarker == DragMarker::start)
        region.start = juce::jmin(position, region.end);
    else
        region.end = juce::jmax(position, region.start);

    if (onRegionChanged)
        onRegionChanged(region.start, region.end);
    repaint();
}

void SourceWaveformComponent::mouseUp(const juce::MouseEvent&)
{
    dragMarker = DragMarker::none;
}

void SpectralCanvasComponent::setCanvas(const Canvas& newCanvas)
{
    canvas = newCanvas;
    repaint();
}

void SpectralCanvasComponent::clearCanvas()
{
    canvas.fill(0.0f);
    if (onCanvasChanged)
        onCanvasChanged(canvas);
    repaint();
}

void SpectralCanvasComponent::setScanPosition(float position)
{
    const auto next = std::clamp(std::isfinite(position) ? position : 0.0f, 0.0f, 1.0f);
    if (std::abs(next - scanPosition) > 0.0001f)
    {
        scanPosition = next;
        repaint();
    }
}

juce::Point<int> SpectralCanvasComponent::eventToCell(
    const juce::MouseEvent& event) const noexcept
{
    const auto bounds = getLocalBounds().reduced(2);
    if (bounds.isEmpty())
        return {};
    const auto x = juce::jlimit(0, randomchop::SpectralMaskStore::canvasWidth - 1,
        static_cast<int>((event.position.x - static_cast<float>(bounds.getX()))
            * randomchop::SpectralMaskStore::canvasWidth
            / static_cast<float>(bounds.getWidth())));
    const auto y = juce::jlimit(0, randomchop::SpectralMaskStore::canvasHeight - 1,
        static_cast<int>((event.position.y - static_cast<float>(bounds.getY()))
            * randomchop::SpectralMaskStore::canvasHeight
            / static_cast<float>(bounds.getHeight())));
    return { x, y };
}

void SpectralCanvasComponent::applyBrush(juce::Point<int> cell) noexcept
{
    for (int row = std::max(0, cell.y - 1);
         row <= std::min(randomchop::SpectralMaskStore::canvasHeight - 1, cell.y + 1); ++row)
        for (int column = std::max(0, cell.x - 1);
             column <= std::min(randomchop::SpectralMaskStore::canvasWidth - 1, cell.x + 1);
             ++column)
            canvas[static_cast<std::size_t>(
                row * randomchop::SpectralMaskStore::canvasWidth + column)]
                = eraseMode ? 0.0f : 1.0f;
}

void SpectralCanvasComponent::applyLine(juce::Point<int> from, juce::Point<int> to)
{
    const auto steps = std::max(std::abs(to.x - from.x), std::abs(to.y - from.y));
    for (int step = 0; step <= steps; ++step)
    {
        const auto amount = steps > 0
            ? static_cast<float>(step) / static_cast<float>(steps) : 0.0f;
        applyBrush({ juce::roundToInt(static_cast<float>(from.x)
                                      + static_cast<float>(to.x - from.x) * amount),
                     juce::roundToInt(static_cast<float>(from.y)
                                      + static_cast<float>(to.y - from.y) * amount) });
    }
    if (onCanvasChanged)
        onCanvasChanged(canvas);
    repaint();
}

void SpectralCanvasComponent::mouseDown(const juce::MouseEvent& event)
{
    lastCell = eventToCell(event);
    applyLine(lastCell, lastCell);
}

void SpectralCanvasComponent::mouseDrag(const juce::MouseEvent& event)
{
    const auto next = eventToCell(event);
    applyLine(lastCell, next);
    lastCell = next;
}

void SpectralCanvasComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().reduced(2);
    g.fillAll(juce::Colour(0xff111218));
    g.setColour(juce::Colour(0xff343846));
    g.fillRect(bounds);
    const auto cellWidth = static_cast<float>(bounds.getWidth())
        / randomchop::SpectralMaskStore::canvasWidth;
    const auto cellHeight = static_cast<float>(bounds.getHeight())
        / randomchop::SpectralMaskStore::canvasHeight;
    g.setColour(juce::Colour(0xff9a7cff));
    for (int row = 0; row < randomchop::SpectralMaskStore::canvasHeight; ++row)
        for (int column = 0; column < randomchop::SpectralMaskStore::canvasWidth; ++column)
        {
            const auto value = canvas[static_cast<std::size_t>(
                row * randomchop::SpectralMaskStore::canvasWidth + column)];
            if (value > 0.0001f)
                g.fillRect(static_cast<float>(bounds.getX()) + column * cellWidth,
                           static_cast<float>(bounds.getY()) + row * cellHeight,
                           cellWidth + 0.5f, cellHeight + 0.5f);
        }
    g.setColour(juce::Colour(0xffffcf5a));
    const auto scannerX = static_cast<float>(bounds.getX())
        + scanPosition * static_cast<float>(bounds.getWidth());
    g.drawVerticalLine(juce::roundToInt(scannerX), static_cast<float>(bounds.getY()),
                       static_cast<float>(bounds.getBottom()));
    g.setColour(juce::Colour(0xff696d7c));
    g.drawRect(bounds, 1);
}

void CreativeVisualizer::setState(float primaryPercent, float secondaryPercent) noexcept
{
    const auto nextPrimary = std::clamp(std::isfinite(primaryPercent)
        ? primaryPercent * 0.01f : 0.0f, 0.0f, 1.0f);
    const auto nextSecondary = std::clamp(std::isfinite(secondaryPercent)
        ? secondaryPercent * 0.01f : 0.0f, 0.0f, 1.0f);
    if (std::abs(nextPrimary - primary) > 0.0001f
        || std::abs(nextSecondary - secondary) > 0.0001f)
    {
        primary = nextPrimary;
        secondary = nextSecondary;
        repaint();
    }
}

void CreativeVisualizer::advance() noexcept
{
    phase = std::fmod(phase + 0.012f + 0.030f * primary, 1.0f);
    if (primary > 0.0001f)
        repaint();
}

void CreativeVisualizer::paint(juce::Graphics& g)
{
    const auto outer = getLocalBounds().toFloat();
    const auto inner = outer.reduced(7.0f, 8.0f);
    g.setColour(juce::Colour(0xff171922));
    g.fillRoundedRectangle(outer, 7.0f);
    g.setColour(juce::Colour(0xff414657));
    g.drawRoundedRectangle(outer.reduced(0.5f), 7.0f, 1.0f);

    if (kind == Kind::scramble)
    {
        constexpr int slices = 8;
        const auto gap = 2.0f;
        const auto cellWidth = (inner.getWidth() - gap * (slices - 1)) / slices;
        for (int index = 0; index < slices; ++index)
        {
            const auto activeSlice = static_cast<float>(index + 1) / slices
                <= 0.08f + 0.92f * std::pow(primary, 0.78f);
            const auto displacement = activeSlice
                ? std::sin((phase * 2.0f + index * 0.31f)
                           * juce::MathConstants<float>::twoPi)
                    * primary * inner.getHeight() * 0.16f
                : 0.0f;
            auto cell = juce::Rectangle<float>(
                inner.getX() + index * (cellWidth + gap), inner.getY() + displacement,
                cellWidth, inner.getHeight() - std::abs(displacement));
            g.setColour(activeSlice ? juce::Colour(0xffa78bfa).withAlpha(0.42f + 0.48f * primary)
                                    : juce::Colour(0xff2d3140));
            g.fillRoundedRectangle(cell, 2.0f);
        }
        const auto scanner = inner.getX() + phase * inner.getWidth();
        g.setColour(juce::Colour(0xffffcf5a).withAlpha(0.70f + 0.30f * primary));
        g.drawLine(scanner, inner.getY() - 2.0f, scanner, inner.getBottom() + 2.0f, 1.5f);
        return;
    }

    if (kind == Kind::fracture)
    {
        juce::Path distorted;
        juce::Path filter;
        constexpr int points = 48;
        for (int point = 0; point <= points; ++point)
        {
            const auto unit = static_cast<float>(point) / points;
            const auto input = std::sin((unit * 2.0f + phase * 0.35f)
                                        * juce::MathConstants<float>::twoPi);
            const auto shaped = std::tanh(input * (1.0f + 5.0f * primary));
            const auto x = inner.getX() + unit * inner.getWidth();
            const auto y = inner.getCentreY() - shaped * inner.getHeight() * 0.32f;
            const auto filterWave = std::sin((unit + phase) * juce::MathConstants<float>::pi)
                * (0.12f + 0.30f * secondary);
            const auto filterY = inner.getBottom() - inner.getHeight()
                * (0.18f + secondary * 0.56f + filterWave * primary);
            if (point == 0)
            {
                distorted.startNewSubPath(x, y);
                filter.startNewSubPath(x, filterY);
            }
            else
            {
                distorted.lineTo(x, y);
                filter.lineTo(x, filterY);
            }
        }
        g.setColour(juce::Colour(0xffff6b6b).withAlpha(0.35f + 0.65f * primary));
        g.strokePath(distorted, juce::PathStrokeType(2.0f));
        g.setColour(juce::Colour(0xff7dd3fc).withAlpha(0.48f + 0.45f * primary));
        g.strokePath(filter, juce::PathStrokeType(1.6f));
        return;
    }

    for (int index = 0; index < 6; ++index)
    {
        const auto orbit = std::fmod(phase * (0.55f + index * 0.07f)
                                     + index * 0.173f, 1.0f);
        const auto x = inner.getX() + orbit * inner.getWidth();
        const auto y = inner.getCentreY() + std::sin((orbit + index * 0.21f)
            * juce::MathConstants<float>::twoPi) * inner.getHeight() * 0.30f * primary;
        const auto length = 5.0f + primary * (8.0f + index * 1.4f);
        g.setColour(juce::Colour(0xff67e8f9).withAlpha(0.18f + 0.62f * primary));
        g.drawLine(x - length, y + length * 0.35f, x + length, y - length * 0.35f,
                   1.0f + 1.2f * primary);
        g.setColour(juce::Colour(0xfff0f9ff).withAlpha(0.28f + 0.66f * primary));
        g.fillEllipse(x - 1.5f - primary, y - 1.5f - primary,
                      3.0f + 2.0f * primary, 3.0f + 2.0f * primary);
    }
}

namespace
{
class SourceRowControls final : public juce::Component
{
public:
    SourceRowControls()
    {
        setInterceptsMouseClicks(true, true);
        addAndMakeVisible(enabled);
        addAndMakeVisible(remove);
        remove.setButtonText("Remove");
    }
    void mouseDown(const juce::MouseEvent&) override
    {
        if (onSelect)
            onSelect(row);
    }
    void resized() override
    {
        auto area = getLocalBounds();
        remove.setBounds(area.removeFromRight(68).reduced(2));
        enabled.setBounds(area.removeFromRight(70).reduced(2));
    }
    juce::ToggleButton enabled { "On" };
    juce::TextButton remove;
    std::function<void(int)> onSelect;
    int row = -1;
};
}

RandomChopSamplerAudioProcessorEditor::RandomChopSamplerAudioProcessorEditor(RandomChopSamplerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setResizable(true, true);
    setResizeLimits(840, 640, 1280, 960);
    setSize(940, 680);
    title.setText("recompiler.dll", juce::dontSendNotification);
    title.setFont(juce::Font(24.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff2f2f5));
    status.setColour(juce::Label::textColourId, juce::Colour(0xffa9acb7));

    juce::Component* components[] = { &title, &status, &addButton, &clearButton,
        &enableAllButton, &disableAllButton, &list, &waveform, &sourceKey,
        &sourceTranspose, &sourceFineTune, &sourceGain, &sourceWeight, &sourceStretch,
        &sourceKeyLabel,
        &sourceTransposeLabel, &sourceFineTuneLabel, &sourceGainLabel, &sourceWeightLabel,
        &sourceStretchLabel, &targetKey, &targetKeyLabel, &midiPitch,
        &voiceMode, &voiceModeLabel, &globalGrid, &globalGridLabel,
        &spectralDrawLabel, &spectralDrawButton, &spectralEraseButton,
        &spectralClearButton, &spectralScanRateLabel, &spectralScanRate, &spectralCanvas,
        &scrambleVisual, &fractureVisual, &smearVisual };
    for (auto* component : components) addAndMakeVisible(component);
    list.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff191b21));
    list.setRowHeight(28);

    for (int index = 0; index < static_cast<int>(randomchop::tonicNames.size()); ++index)
    {
        sourceKey.addItem(randomchop::tonicNames[static_cast<size_t>(index)], index + 1);
        targetKey.addItem(randomchop::tonicNames[static_cast<size_t>(index)], index + 1);
    }
    voiceMode.addItem("POLY", 1);
    voiceMode.addItem("MONO", 2);
    globalGrid.addItem("1/8", 1);
    globalGrid.addItem("1/16", 2);
    globalGrid.addItem("1/32", 3);
    for (const auto& name : juce::StringArray { "2 beats", "1 bar", "2 bars", "4 bars" })
        spectralScanRate.addItem(name, spectralScanRate.getNumItems() + 1);
    sourceTranspose.setRange(-24.0, 24.0, 1.0);
    sourceTranspose.setTextValueSuffix(" st");
    sourceFineTune.setRange(-100.0, 100.0, 1.0);
    sourceFineTune.setTextValueSuffix(" cents");
    sourceGain.setRange(-60.0, 12.0, 0.1);
    sourceGain.setTextValueSuffix(" dB");
    sourceWeight.setRange(0.01, 10.0, 0.01);
    sourceStretch.setRange(0.0, 4.0, 0.01);
    sourceStretch.textFromValueFunction = [](double value)
    {
        return value < 1.0 ? juce::String("OFF") : juce::String(value, 2) + " x";
    };
    sourceStretch.valueFromTextFunction = [](const juce::String& text)
    {
        return text.trim().equalsIgnoreCase("OFF") ? 0.0 : text.getDoubleValue();
    };
    for (auto* slider : { &sourceTranspose, &sourceFineTune, &sourceGain, &sourceWeight,
                          &sourceStretch })
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 22);
    }
    sourceKeyLabel.setText("SOURCE KEY", juce::dontSendNotification);
    sourceTransposeLabel.setText("TRANSPOSE", juce::dontSendNotification);
    sourceFineTuneLabel.setText("FINE TUNE", juce::dontSendNotification);
    sourceGainLabel.setText("SELECTED GAIN", juce::dontSendNotification);
    sourceWeightLabel.setText("SELECTION WEIGHT", juce::dontSendNotification);
    sourceStretchLabel.setText("STRETCH", juce::dontSendNotification);
    targetKeyLabel.setText("TARGET KEY", juce::dontSendNotification);
    voiceModeLabel.setText("VOICE MODE", juce::dontSendNotification);
    globalGridLabel.setText("GLOBAL GRID", juce::dontSendNotification);
    spectralDrawLabel.setText("SPECTRAL DRAW", juce::dontSendNotification);
    spectralScanRateLabel.setText("SCAN RATE", juce::dontSendNotification);
    voiceModeLabel.setJustificationType(juce::Justification::centredLeft);
    for (auto* label : { &sourceKeyLabel, &sourceTransposeLabel, &sourceFineTuneLabel,
                         &sourceGainLabel, &sourceWeightLabel, &sourceStretchLabel,
                         &targetKeyLabel, &voiceModeLabel, &globalGridLabel, &spectralDrawLabel,
                         &spectralScanRateLabel })
        label->setColour(juce::Label::textColourId, juce::Colour(0xffc8cad1));
    spectralDrawButton.setClickingTogglesState(true);
    spectralEraseButton.setClickingTogglesState(true);
    spectralDrawButton.setRadioGroupId(3001);
    spectralEraseButton.setRadioGroupId(3001);
    spectralDrawButton.setToggleState(true, juce::dontSendNotification);
    spectralDrawButton.onClick = [this] { spectralCanvas.setEraseMode(false); };
    spectralEraseButton.onClick = [this] { spectralCanvas.setEraseMode(true); };
    spectralClearButton.onClick = [this] { spectralCanvas.clearCanvas(); };
    spectralCanvas.setCanvas(processor.getSpectralCanvas());
    lastSpectralCanvasGeneration = processor.getSpectralCanvasGeneration();
    spectralCanvas.onCanvasChanged = [this](const SpectralCanvasComponent::Canvas& canvas)
    {
        processor.setSpectralCanvas(canvas);
        lastSpectralCanvasGeneration = processor.getSpectralCanvasGeneration();
    };
    sourceKey.onChange = [this]
    {
        if (selectedSourceId.isNotEmpty()) processor.samples.updateSettings(selectedSourceId,
            [this](SampleSettings& s) { s.sourceKey = sourceKey.getSelectedItemIndex(); });
    };
    sourceTranspose.onValueChange = [this]
    {
        if (selectedSourceId.isNotEmpty()) processor.samples.updateSettings(selectedSourceId,
            [this](SampleSettings& s)
            {
                s.transposeSemitones = static_cast<int>(sourceTranspose.getValue());
            });
    };
    sourceFineTune.onValueChange = [this]
    {
        if (selectedSourceId.isNotEmpty()) processor.samples.updateSettings(selectedSourceId,
            [this](SampleSettings& s)
            {
                s.fineTuneCents = static_cast<float>(sourceFineTune.getValue());
            });
    };
    sourceGain.onValueChange = [this]
    {
        if (selectedSourceId.isNotEmpty()) processor.samples.updateSettings(selectedSourceId,
            [this](SampleSettings& s) { s.gainDb = static_cast<float>(sourceGain.getValue()); });
    };
    sourceWeight.onValueChange = [this]
    {
        if (selectedSourceId.isNotEmpty()) processor.samples.updateSettings(selectedSourceId,
            [this](SampleSettings& s) { s.selectionWeight = static_cast<float>(sourceWeight.getValue()); });
    };
    sourceStretch.onValueChange = [this]
    {
        if (selectedSourceId.isNotEmpty()) processor.samples.updateSettings(selectedSourceId,
            [this](SampleSettings& s)
            {
                s.stretchRatio = static_cast<float>(sourceStretch.getValue());
            });
        refresh();
    };
    waveform.onRegionChanged = [this](double start, double end)
    {
        if (selectedSourceId.isEmpty())
            return;
        processor.samples.updateSettings(selectedSourceId, [start, end](SampleSettings& settings)
        {
            settings.startNormalised = start;
            settings.endNormalised = end;
        });
        displayPool = processor.samples.getSnapshot();
        list.repaint();
    };

    configureKnob(output, outputLabel, "OUTPUT");
    configureKnob(rootNote, rootNoteLabel, "ROOT NOTE");
    configureKnob(scrambleAmount, scrambleAmountLabel, "SCRAMBLE");
    configureKnob(fractureCharacter, fractureCharacterLabel, "FILTER MORPH");
    configureKnob(fractureMix, fractureMixLabel, "FRACTURE");
    configureLinearControl(spectralDepth, spectralDepthLabel, "SPECTRAL DEPTH");
    configureKnob(smearAmount, smearAmountLabel, "SMEAR");
    rootNote.setSliderStyle(juce::Slider::LinearHorizontal);
    rootNote.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 22);
    rootNote.textFromValueFunction = [](double value)
    {
        return juce::String(randomchop::midiNoteName(juce::roundToInt(value)));
    };
    rootNote.valueFromTextFunction = [](const juce::String& text)
    {
        return static_cast<double>(randomchop::midiNoteFromName(text.toStdString(), 72));
    };
    outputAttachment = std::make_unique<SliderAttachment>(p.parameters, "output", output);
    rootNoteAttachment = std::make_unique<SliderAttachment>(p.parameters, "rootNote", rootNote);
    targetKeyAttachment = std::make_unique<ComboBoxAttachment>(p.parameters, "targetKey", targetKey);
    voiceModeAttachment = std::make_unique<ComboBoxAttachment>(p.parameters, "voiceMode", voiceMode);
    globalGridAttachment = std::make_unique<ComboBoxAttachment>(
        p.parameters, "globalGrid", globalGrid);
    spectralScanRateAttachment = std::make_unique<ComboBoxAttachment>(
        p.parameters, "spectralScanRate", spectralScanRate);
    midiPitchAttachment = std::make_unique<ButtonAttachment>(p.parameters, "midiPitch", midiPitch);
    scrambleAmountAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "scrambleAmount", scrambleAmount);
    fractureCharacterAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "fractureCharacter", fractureCharacter);
    fractureMixAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "fractureMix", fractureMix);
    spectralDepthAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "spectralDepth", spectralDepth);
    smearAmountAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "smearAmount", smearAmount);

    addButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>("Choose audio files", juce::File(),
                                                       "*.wav;*.aif;*.aiff;*.mp3;*.flac");
        chooser->launchAsync(juce::FileBrowserComponent::openMode
                           | juce::FileBrowserComponent::canSelectFiles
                           | juce::FileBrowserComponent::canSelectMultipleItems,
            [this](const juce::FileChooser& fc)
            {
                juce::StringArray paths;
                for (const auto& file : fc.getResults()) paths.add(file.getFullPathName());
                addFiles(paths);
            });
    };
    clearButton.onClick = [this] { processor.samples.clear(); refresh(); };
    enableAllButton.onClick = [this] { processor.samples.setAllEnabled(true); refresh(); };
    disableAllButton.onClick = [this] { processor.samples.setAllEnabled(false); refresh(); };
    refresh();
    startTimerHz(20);
}

void RandomChopSamplerAudioProcessorEditor::configureKnob(juce::Slider& slider, juce::Label& label,
                                                           const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff8b5cf6));
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffc8cad1));
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void RandomChopSamplerAudioProcessorEditor::configureLinearControl(
    juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
    slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff8b5cf6));
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffc8cad1));
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
}

void RandomChopSamplerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111218));
    auto poolArea = list.getBounds().expanded(10);
    g.setColour(juce::Colour(0xff242630));
    g.fillRoundedRectangle(poolArea.toFloat(), 8.0f);
    g.setColour(juce::Colour(0xff494c5a));
    g.drawRoundedRectangle(poolArea.toFloat(), 8.0f, 1.0f);
    if (displayPool && displayPool->empty())
    {
        g.setColour(juce::Colour(0xff8f93a3));
        g.setFont(18.0f);
        g.drawText("Drop WAV, AIFF, MP3 or FLAC sources here", poolArea,
                   juce::Justification::centred);
    }
}

void RandomChopSamplerAudioProcessorEditor::resized()
{
    const auto verticalScale = juce::jlimit(0.9f, 1.15f,
        static_cast<float>(getHeight()) / 680.0f);
    const auto scaledHeight = [verticalScale](int height)
    {
        return juce::jmax(1, juce::roundToInt(static_cast<float>(height) * verticalScale));
    };
    const auto takeEqualCell = [](juce::Rectangle<int>& row, int cellsRemaining)
    {
        return row.removeFromLeft(row.getWidth() / juce::jmax(1, cellsRemaining)).reduced(2);
    };

    auto area = getLocalBounds().reduced(12);
    auto header = area.removeFromTop(scaledHeight(32));
    title.setBounds(header.removeFromLeft(260));
    status.setBounds(header);
    auto toolbar = area.removeFromTop(scaledHeight(30));
    addButton.setBounds(takeEqualCell(toolbar, 4));
    clearButton.setBounds(takeEqualCell(toolbar, 3));
    enableAllButton.setBounds(takeEqualCell(toolbar, 2));
    disableAllButton.setBounds(toolbar.reduced(2));

    auto creativeControls = area.removeFromBottom(scaledHeight(62));
    auto effectVisuals = area.removeFromBottom(scaledHeight(74));
    auto timingControls = area.removeFromBottom(scaledHeight(28));
    auto globalPitchControls = area.removeFromBottom(scaledHeight(34));
    auto sourcePitchControls = area.removeFromBottom(scaledHeight(34));
    auto sourceControls = area.removeFromBottom(scaledHeight(34));
    auto waveformArea = area.removeFromBottom(scaledHeight(76));

    auto gainCell = takeEqualCell(sourceControls, 3);
    sourceGainLabel.setBounds(gainCell.removeFromLeft(110));
    sourceGain.setBounds(gainCell);
    auto weightCell = takeEqualCell(sourceControls, 2);
    sourceWeightLabel.setBounds(weightCell.removeFromLeft(125));
    sourceWeight.setBounds(weightCell);
    auto stretchCell = sourceControls.reduced(2);
    sourceStretchLabel.setBounds(stretchCell.removeFromLeft(80));
    sourceStretch.setBounds(stretchCell);

    auto sourceKeyCell = takeEqualCell(sourcePitchControls, 3);
    sourceKeyLabel.setBounds(sourceKeyCell.removeFromLeft(92));
    sourceKey.setBounds(sourceKeyCell.reduced(2, 4));
    auto transposeCell = takeEqualCell(sourcePitchControls, 2);
    sourceTransposeLabel.setBounds(transposeCell.removeFromLeft(92));
    sourceTranspose.setBounds(transposeCell);
    auto fineTuneCell = sourcePitchControls.reduced(2);
    sourceFineTuneLabel.setBounds(fineTuneCell.removeFromLeft(92));
    sourceFineTune.setBounds(fineTuneCell);

    auto targetCell = takeEqualCell(globalPitchControls, 4);
    targetKeyLabel.setBounds(targetCell.removeFromLeft(90));
    targetKey.setBounds(targetCell.reduced(2, 4));
    midiPitch.setBounds(takeEqualCell(globalPitchControls, 3));
    auto rootCell = takeEqualCell(globalPitchControls, 2);
    rootNoteLabel.setBounds(rootCell.removeFromLeft(105));
    rootNote.setBounds(rootCell);
    auto voiceCell = globalPitchControls.reduced(2);
    voiceModeLabel.setBounds(voiceCell.removeFromLeft(88));
    voiceMode.setBounds(voiceCell.reduced(2, 4));

    auto gridCell = timingControls.removeFromLeft(300).reduced(2);
    globalGridLabel.setBounds(gridCell.removeFromLeft(100));
    globalGrid.setBounds(gridCell.reduced(2, 3));

    auto scrambleCell = takeEqualCell(creativeControls, 5);
    scrambleAmountLabel.setBounds(scrambleCell.removeFromTop(scaledHeight(16)));
    scrambleAmount.setBounds(scrambleCell.reduced(2));
    auto fractureCell = takeEqualCell(creativeControls, 4);
    fractureMixLabel.setBounds(fractureCell.removeFromTop(scaledHeight(16)));
    fractureMix.setBounds(fractureCell.reduced(2));
    auto characterCell = takeEqualCell(creativeControls, 3);
    fractureCharacterLabel.setBounds(characterCell.removeFromTop(scaledHeight(16)));
    fractureCharacter.setBounds(characterCell.reduced(2));
    auto smearCell = takeEqualCell(creativeControls, 2);
    smearAmountLabel.setBounds(smearCell.removeFromTop(scaledHeight(16)));
    smearAmount.setBounds(smearCell.reduced(2));
    auto outputCell = creativeControls.reduced(2);
    outputLabel.setBounds(outputCell.removeFromTop(scaledHeight(16)));
    output.setBounds(outputCell.reduced(2));

    auto scrambleVisualCell = takeEqualCell(effectVisuals, 3);
    auto fractureVisualCell = takeEqualCell(effectVisuals, 2);
    scrambleVisual.setBounds(scrambleVisualCell);
    fractureVisual.setBounds(fractureVisualCell);
    smearVisual.setBounds(effectVisuals.reduced(2));
    waveform.setBounds(waveformArea.reduced(4));
    auto listAndSpectral = area.reduced(6);
    auto listArea = listAndSpectral.removeFromLeft(listAndSpectral.getWidth() / 2).reduced(2);
    list.setBounds(listArea);
    auto spectralArea = listAndSpectral.reduced(2);
    auto spectralTools = spectralArea.removeFromTop(scaledHeight(26));
    spectralDrawLabel.setBounds(spectralTools.removeFromLeft(88));
    spectralDrawButton.setBounds(spectralTools.removeFromLeft(48).reduced(2));
    spectralEraseButton.setBounds(spectralTools.removeFromLeft(48).reduced(2));
    spectralClearButton.setBounds(spectralTools.removeFromLeft(48).reduced(2));
    spectralScanRateLabel.setBounds(spectralTools.removeFromLeft(58));
    spectralScanRate.setBounds(spectralTools.reduced(2, 3));
    auto depthArea = spectralArea.removeFromTop(scaledHeight(28)).reduced(2);
    spectralDepthLabel.setBounds(depthArea.removeFromLeft(105));
    spectralDepth.setBounds(depthArea);
    spectralCanvas.setBounds(spectralArea.reduced(2));
}

bool RandomChopSamplerAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files) if (SampleManager::isSupported(juce::File(path))) return true;
    return false;
}

void RandomChopSamplerAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    addFiles(files);
}

void RandomChopSamplerAudioProcessorEditor::addFiles(const juce::StringArray& files)
{
    const auto errors = processor.samples.addFiles(files);
    transientMessage = errors.empty() ? juce::String()
                                      : juce::String(errors.size()) + " file(s) rejected";
    refresh();
}

void RandomChopSamplerAudioProcessorEditor::refresh()
{
    displayPool = processor.samples.getSnapshot();
    int selected = -1;
    for (size_t i = 0; displayPool && i < displayPool->size(); ++i)
        if ((*displayPool)[i]->settings.id == selectedSourceId)
            selected = static_cast<int>(i);
    if (selected < 0 && displayPool && !displayPool->empty())
    {
        selected = 0;
        selectedSourceId = displayPool->front()->settings.id;
    }
    if (!displayPool || displayPool->empty())
        selectedSourceId.clear();
    list.updateContent();
    list.selectRow(selected);
    if (displayPool && selected >= 0 && selected < static_cast<int>(displayPool->size()))
    {
        const auto& selectedSource = (*displayPool)[static_cast<size_t>(selected)];
        sourceKey.setSelectedItemIndex(selectedSource->settings.sourceKey,
                                       juce::dontSendNotification);
        sourceTranspose.setValue(selectedSource->settings.transposeSemitones,
                                 juce::dontSendNotification);
        sourceFineTune.setValue(selectedSource->settings.fineTuneCents,
                                juce::dontSendNotification);
        sourceGain.setValue(selectedSource->settings.gainDb, juce::dontSendNotification);
        sourceWeight.setValue(selectedSource->settings.selectionWeight, juce::dontSendNotification);
        sourceStretch.setValue(selectedSource->settings.stretchRatio,
                               juce::dontSendNotification);
        waveform.setSource(selectedSource);
    }
    else
    {
        waveform.setSource({});
    }
    list.repaint();
    repaint();
}

int RandomChopSamplerAudioProcessorEditor::getNumRows()
{
    return displayPool ? static_cast<int>(displayPool->size()) : 0;
}

void RandomChopSamplerAudioProcessorEditor::paintListBoxItem(int row, juce::Graphics& g, int width,
                                                              int height, bool selected)
{
    if (!displayPool || row < 0 || row >= static_cast<int>(displayPool->size())) return;
    const auto& source = (*displayPool)[static_cast<size_t>(row)];
    const bool recent = source->runtimeId
        == processor.lastTriggeredRuntimeId.load(std::memory_order_relaxed);
    g.fillAll(selected ? juce::Colour(0xff343746)
                       : (recent ? juce::Colour(0xff29233a) : juce::Colour(0xff191b21)));
    g.setColour(source->settings.missing ? juce::Colour(0xffff8a8a) : juce::Colour(0xffe3e4e8));
    const auto suffix = source->settings.missing ? juce::String("  [MISSING]")
        : (source->stretchPending ? juce::String("  [STRETCHING]")
                                  : (source->stretchFailed ? juce::String("  [STRETCH FAILED]")
                                                           : juce::String()));
    g.drawText(juce::String(row + 1).paddedLeft('0', 2) + ".  "
                   + source->settings.displayName + suffix,
               10, 0, width - 155, height, juce::Justification::centredLeft, true);
}

juce::Component* RandomChopSamplerAudioProcessorEditor::refreshComponentForRow(int row, bool,
                                                                                juce::Component* existing)
{
    auto* controls = dynamic_cast<SourceRowControls*>(existing);
    if (controls == nullptr) { delete existing; controls = new SourceRowControls(); }
    controls->row = row;
    controls->onSelect = [this](int selectedRow)
    {
        if (selectedRow >= 0 && selectedRow < getNumRows())
            list.selectRow(selectedRow);
    };
    if (displayPool && row >= 0 && row < static_cast<int>(displayPool->size()))
        controls->enabled.setToggleState((*displayPool)[static_cast<size_t>(row)]->settings.enabled,
                                         juce::dontSendNotification);
    controls->enabled.onClick = [this, controls]
    {
        if (controls->onSelect)
            controls->onSelect(controls->row);
        if (displayPool && controls->row >= 0
            && controls->row < static_cast<int>(displayPool->size()))
            processor.samples.setEnabled((*displayPool)[static_cast<size_t>(controls->row)]->settings.id,
                                         controls->enabled.getToggleState());
        refresh();
    };
    controls->remove.onClick = [this, controls]
    {
        if (displayPool && controls->row >= 0
            && controls->row < static_cast<int>(displayPool->size()))
            processor.samples.remove((*displayPool)[static_cast<size_t>(controls->row)]->settings.id);
        refresh();
    };
    return controls;
}

void RandomChopSamplerAudioProcessorEditor::selectedRowsChanged(int row)
{
    if (displayPool && row >= 0 && row < static_cast<int>(displayPool->size()))
    {
        const auto& settings = (*displayPool)[static_cast<size_t>(row)]->settings;
        selectedSourceId = settings.id;
        sourceKey.setSelectedItemIndex(settings.sourceKey, juce::dontSendNotification);
        sourceTranspose.setValue(settings.transposeSemitones, juce::dontSendNotification);
        sourceFineTune.setValue(settings.fineTuneCents, juce::dontSendNotification);
        sourceGain.setValue(settings.gainDb, juce::dontSendNotification);
        sourceWeight.setValue(settings.selectionWeight, juce::dontSendNotification);
        sourceStretch.setValue(settings.stretchRatio, juce::dontSendNotification);
        waveform.setSource((*displayPool)[static_cast<size_t>(row)]);
    }
}

void RandomChopSamplerAudioProcessorEditor::timerCallback()
{
    processor.samples.collectGarbage();
    if (processor.samples.getSnapshot() != displayPool)
        refresh();
    const int count = processor.samples.size();
    auto message = juce::String(count).paddedLeft('0', 2) + " / 20 sources";
    if (processor.triggeredWhileEmpty.load(std::memory_order_relaxed))
        message = "No enabled playable sources";
    else if (transientMessage.isNotEmpty())
        message += " — " + transientMessage;
    status.setText(message, juce::dontSendNotification);
    const auto canvasGeneration = processor.getSpectralCanvasGeneration();
    if (canvasGeneration != lastSpectralCanvasGeneration)
    {
        spectralCanvas.setCanvas(processor.getSpectralCanvas());
        lastSpectralCanvasGeneration = canvasGeneration;
    }
    spectralCanvas.setScanPosition(processor.getSpectralScanPosition());
    const auto readParameter = [this](const char* id)
    {
        if (const auto* value = processor.parameters.getRawParameterValue(id))
            return value->load();
        return 0.0f;
    };
    scrambleVisual.setState(readParameter("scrambleAmount"));
    fractureVisual.setState(readParameter("fractureMix"),
                            readParameter("fractureCharacter"));
    smearVisual.setState(readParameter("smearAmount"));
    scrambleVisual.advance();
    fractureVisual.advance();
    smearVisual.advance();
    list.repaint();
}

