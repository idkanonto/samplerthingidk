#include "PluginEditor.h"
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
    return getLocalBounds().reduced(10).withTrimmedTop(28).withTrimmedBottom(42);
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

    g.setFont(14.0f);
    g.setColour(juce::Colour(0xffc8cad1));
    juce::String heading("SOURCE REGION");
    if (source != nullptr)
    {
        heading += " — ";
        heading += source->settings.displayName;
    }
    g.drawText(heading, getLocalBounds().reduced(10).removeFromTop(24),
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

    auto footer = getLocalBounds().reduced(10).removeFromBottom(36);
    auto startText = footer.removeFromLeft(footer.getWidth() / 2);
    g.setFont(12.0f);
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

namespace
{
class SourceRowControls final : public juce::Component
{
public:
    SourceRowControls()
    {
        addAndMakeVisible(enabled);
        addAndMakeVisible(remove);
        remove.setButtonText("Remove");
    }
    void resized() override
    {
        auto area = getLocalBounds();
        remove.setBounds(area.removeFromRight(68).reduced(2));
        enabled.setBounds(area.removeFromRight(70).reduced(2));
    }
    juce::ToggleButton enabled { "On" };
    juce::TextButton remove;
    int row = -1;
};
}

RandomChopSamplerAudioProcessorEditor::RandomChopSamplerAudioProcessorEditor(RandomChopSamplerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(1050, 1060);
    title.setText("RANDOM CHOP SAMPLER V2", juce::dontSendNotification);
    title.setFont(juce::Font(24.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff2f2f5));
    status.setColour(juce::Label::textColourId, juce::Colour(0xffa9acb7));

    juce::Component* components[] = { &title, &status, &addButton, &clearButton,
        &enableAllButton, &disableAllButton, &list, &waveform, &sourceKey,
        &sourceTranspose, &sourceFineTune, &sourceGain, &sourceWeight, &sourceStretch,
        &sourceKeyLabel,
        &sourceTransposeLabel, &sourceFineTuneLabel, &sourceGainLabel, &sourceWeightLabel,
        &sourceStretchLabel, &targetKey, &targetKeyLabel, &midiPitch,
        &voiceMode, &voiceModeLabel, &stepLength, &stepLengthLabel,
        &allNormalButton, &allFxButton, &randomiseStepsButton };
    for (auto* component : components) addAndMakeVisible(component);
    for (auto& button : stepButtons)
        addAndMakeVisible(button);
    list.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff191b21));
    list.setRowHeight(34);

    for (int index = 0; index < static_cast<int>(randomchop::tonicNames.size()); ++index)
    {
        sourceKey.addItem(randomchop::tonicNames[static_cast<size_t>(index)], index + 1);
        targetKey.addItem(randomchop::tonicNames[static_cast<size_t>(index)], index + 1);
    }
    voiceMode.addItem("POLY", 1);
    voiceMode.addItem("MONO", 2);
    stepLength.addItem("2", 1);
    stepLength.addItem("4", 2);
    stepLength.addItem("8", 3);
    stepLength.addItem("16", 4);
    sourceTranspose.setRange(-24.0, 24.0, 1.0);
    sourceTranspose.setTextValueSuffix(" st");
    sourceFineTune.setRange(-100.0, 100.0, 1.0);
    sourceFineTune.setTextValueSuffix(" cents");
    sourceGain.setRange(-60.0, 12.0, 0.1);
    sourceGain.setTextValueSuffix(" dB");
    sourceWeight.setRange(0.01, 10.0, 0.01);
    sourceStretch.setRange(0.5, 2.0, 0.01);
    sourceStretch.setTextValueSuffix(" x");
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
    stepLengthLabel.setText("STEP MASK", juce::dontSendNotification);
    voiceModeLabel.setJustificationType(juce::Justification::centredLeft);
    for (auto* label : { &sourceKeyLabel, &sourceTransposeLabel, &sourceFineTuneLabel,
                         &sourceGainLabel, &sourceWeightLabel, &sourceStretchLabel,
                         &targetKeyLabel, &voiceModeLabel, &stepLengthLabel })
        label->setColour(juce::Label::textColourId, juce::Colour(0xffc8cad1));
    for (int index = 0; index < static_cast<int>(stepButtons.size()); ++index)
    {
        auto& button = stepButtons[static_cast<size_t>(index)];
        button.onClick = [this, index]
        {
            processor.stepMask.setStep(index,
                stepButtons[static_cast<size_t>(index)].getToggleState());
            refreshStepMaskControls();
        };
    }
    allNormalButton.onClick = [this]
    {
        processor.stepMask.setAll(false);
        refreshStepMaskControls();
    };
    allFxButton.onClick = [this]
    {
        processor.stepMask.setAll(true);
        refreshStepMaskControls();
    };
    randomiseStepsButton.onClick = [this]
    {
        processor.stepMask.setMask(static_cast<std::uint16_t>(
            juce::Random::getSystemRandom().nextInt(1 << randomchop::StepMask::maximumSteps)));
        refreshStepMaskControls();
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
        displayPool = processor.samples.getSnapshot();
        list.repaint();
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

    configureKnob(randomStart, randomStartLabel, "RANDOM START");
    configureKnob(finalLength, finalLengthLabel, "FINAL LENGTH");
    configureKnob(attack, attackLabel, "ATTACK");
    configureKnob(release, releaseLabel, "RELEASE");
    configureKnob(output, outputLabel, "OUTPUT");
    configureKnob(seed, seedLabel, "SEED");
    configureKnob(rootNote, rootNoteLabel, "ROOT MIDI NOTE");
    configureLinearControl(reverseChance, reverseChanceLabel, "REVERSE CHANCE");
    configureLinearControl(retriggerChance, retriggerChanceLabel, "RETRIGGER CHANCE");
    configureLinearControl(skipChance, skipChanceLabel, "SKIP CHANCE");
    configureLinearControl(reorderChance, reorderChanceLabel, "REORDER CHANCE");
    configureLinearControl(bendChance, bendChanceLabel, "BEND CHANCE");
    configureLinearControl(dropChance, dropChanceLabel, "DROP CHANCE");
    configureLinearControl(retriggerSize, retriggerSizeLabel, "REPEAT SIZE");
    configureLinearControl(retriggerCount, retriggerCountLabel, "REPEAT COUNT");
    finalLength.setNumDecimalPlacesToDisplay(0);
    finalLength.textFromValueFunction = [](double value)
    {
        return value < 5.0 ? juce::String("FULL")
                           : juce::String(juce::roundToInt(value)) + " ms";
    };
    finalLength.valueFromTextFunction = [](const juce::String& text)
    {
        return text.trim().equalsIgnoreCase("FULL") ? 0.0 : text.getDoubleValue();
    };
    rootNote.setSliderStyle(juce::Slider::LinearHorizontal);
    rootNote.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 22);
    randomStartAttachment = std::make_unique<SliderAttachment>(p.parameters, "randomStart", randomStart);
    finalLengthAttachment = std::make_unique<SliderAttachment>(p.parameters, "finalLength", finalLength);
    attackAttachment = std::make_unique<SliderAttachment>(p.parameters, "attack", attack);
    releaseAttachment = std::make_unique<SliderAttachment>(p.parameters, "release", release);
    outputAttachment = std::make_unique<SliderAttachment>(p.parameters, "output", output);
    seedAttachment = std::make_unique<SliderAttachment>(p.parameters, "seed", seed);
    rootNoteAttachment = std::make_unique<SliderAttachment>(p.parameters, "rootNote", rootNote);
    targetKeyAttachment = std::make_unique<ComboBoxAttachment>(p.parameters, "targetKey", targetKey);
    voiceModeAttachment = std::make_unique<ComboBoxAttachment>(p.parameters, "voiceMode", voiceMode);
    stepLengthAttachment = std::make_unique<ComboBoxAttachment>(
        p.parameters, "stepLength", stepLength);
    midiPitchAttachment = std::make_unique<ButtonAttachment>(p.parameters, "midiPitch", midiPitch);
    reverseChanceAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "reverseChance", reverseChance);
    retriggerChanceAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "retriggerChance", retriggerChance);
    retriggerSizeAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "retriggerSize", retriggerSize);
    retriggerCountAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "retriggerCount", retriggerCount);
    skipChanceAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "skipChance", skipChance);
    reorderChanceAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "reorderChance", reorderChance);
    bendChanceAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "bendChance", bendChance);
    dropChanceAttachment = std::make_unique<SliderAttachment>(
        p.parameters, "dropChance", dropChance);

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
    refreshStepMaskControls();
    refresh();
    startTimerHz(8);
}

void RandomChopSamplerAudioProcessorEditor::configureKnob(juce::Slider& slider, juce::Label& label,
                                                           const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 82, 20);
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
    auto area = getLocalBounds().reduced(20);
    auto header = area.removeFromTop(48);
    title.setBounds(header.removeFromLeft(360));
    status.setBounds(header);
    auto toolbar = area.removeFromTop(38);
    addButton.setBounds(toolbar.removeFromLeft(145).reduced(2));
    clearButton.setBounds(toolbar.removeFromLeft(90).reduced(2));
    enableAllButton.setBounds(toolbar.removeFromLeft(105).reduced(2));
    disableAllButton.setBounds(toolbar.removeFromLeft(105).reduced(2));

    auto globalControls = area.removeFromBottom(135);
    auto chanceControls2 = area.removeFromBottom(58);
    auto chanceControls1 = area.removeFromBottom(58);
    auto stepControls = area.removeFromBottom(82);
    auto globalPitchControls = area.removeFromBottom(58);
    auto sourcePitchControls = area.removeFromBottom(58);
    auto sourceControls = area.removeFromBottom(58);
    auto waveformArea = area.removeFromBottom(190);
    auto gainCell = sourceControls.removeFromLeft(350).reduced(3);
    sourceGainLabel.setBounds(gainCell.removeFromLeft(125));
    sourceGain.setBounds(gainCell);
    auto weightCell = sourceControls.removeFromLeft(390).reduced(3);
    sourceWeightLabel.setBounds(weightCell.removeFromLeft(145));
    sourceWeight.setBounds(weightCell);
    auto stretchCell = sourceControls.reduced(3);
    sourceStretchLabel.setBounds(stretchCell.removeFromLeft(95));
    sourceStretch.setBounds(stretchCell);

    auto sourceKeyCell = sourcePitchControls.removeFromLeft(260).reduced(3);
    sourceKeyLabel.setBounds(sourceKeyCell.removeFromLeft(105));
    sourceKey.setBounds(sourceKeyCell.reduced(2, 10));
    auto transposeCell = sourcePitchControls.removeFromLeft(340).reduced(3);
    sourceTransposeLabel.setBounds(transposeCell.removeFromLeft(105));
    sourceTranspose.setBounds(transposeCell);
    auto fineTuneCell = sourcePitchControls.removeFromLeft(340).reduced(3);
    sourceFineTuneLabel.setBounds(fineTuneCell.removeFromLeft(105));
    sourceFineTune.setBounds(fineTuneCell);

    auto targetCell = globalPitchControls.removeFromLeft(265).reduced(3);
    targetKeyLabel.setBounds(targetCell.removeFromLeft(105));
    targetKey.setBounds(targetCell.reduced(2, 10));
    midiPitch.setBounds(globalPitchControls.removeFromLeft(150).reduced(10));
    auto rootCell = globalPitchControls.removeFromLeft(250).reduced(3);
    rootNoteLabel.setBounds(rootCell.removeFromLeft(120));
    rootNote.setBounds(rootCell);
    auto voiceCell = globalPitchControls.removeFromLeft(250).reduced(3);
    voiceModeLabel.setBounds(voiceCell.removeFromLeft(105));
    voiceMode.setBounds(voiceCell.reduced(2, 10));

    const int knobWidth = globalControls.getWidth() / 6;
    juce::Slider* sliders[] = { &randomStart, &finalLength, &attack, &release, &output, &seed };
    juce::Label* labels[] = { &randomStartLabel, &finalLengthLabel, &attackLabel,
                             &releaseLabel, &outputLabel, &seedLabel };
    for (int i = 0; i < 6; ++i)
    {
        auto cell = globalControls.removeFromLeft(knobWidth);
        labels[i]->setBounds(cell.removeFromTop(22));
        sliders[i]->setBounds(cell.reduced(4));
    }
    juce::Slider* chanceSliders1[] = {
        &reverseChance, &retriggerChance, &skipChance, &reorderChance
    };
    juce::Label* chanceLabels1[] = {
        &reverseChanceLabel, &retriggerChanceLabel, &skipChanceLabel, &reorderChanceLabel
    };
    juce::Slider* chanceSliders2[] = {
        &bendChance, &dropChance, &retriggerSize, &retriggerCount
    };
    juce::Label* chanceLabels2[] = {
        &bendChanceLabel, &dropChanceLabel, &retriggerSizeLabel, &retriggerCountLabel
    };
    for (int index = 0; index < 4; ++index)
    {
        auto cell1 = chanceControls1.removeFromLeft(chanceControls1.getWidth() / (4 - index));
        chanceLabels1[index]->setBounds(cell1.removeFromTop(20));
        chanceSliders1[index]->setBounds(cell1.reduced(4, 2));
        auto cell2 = chanceControls2.removeFromLeft(chanceControls2.getWidth() / (4 - index));
        chanceLabels2[index]->setBounds(cell2.removeFromTop(20));
        chanceSliders2[index]->setBounds(cell2.reduced(4, 2));
    }
    auto stepToolbar = stepControls.removeFromTop(32);
    stepLengthLabel.setBounds(stepToolbar.removeFromLeft(100).reduced(3));
    stepLength.setBounds(stepToolbar.removeFromLeft(90).reduced(3));
    allNormalButton.setBounds(stepToolbar.removeFromLeft(115).reduced(3));
    allFxButton.setBounds(stepToolbar.removeFromLeft(90).reduced(3));
    randomiseStepsButton.setBounds(stepToolbar.removeFromLeft(105).reduced(3));
    for (int index = 0; index < static_cast<int>(stepButtons.size()); ++index)
    {
        auto cell = stepControls.removeFromLeft(
            stepControls.getWidth() / (static_cast<int>(stepButtons.size()) - index));
        stepButtons[static_cast<size_t>(index)].setBounds(cell.reduced(2));
    }
    waveform.setBounds(waveformArea.reduced(10));
    list.setBounds(area.reduced(10));
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

void RandomChopSamplerAudioProcessorEditor::refreshStepMaskControls()
{
    const auto length = randomchop::StepMask::lengthFromChoice(static_cast<int>(
        processor.parameters.getRawParameterValue("stepLength")->load()));
    for (int index = 0; index < static_cast<int>(stepButtons.size()); ++index)
    {
        auto& button = stepButtons[static_cast<size_t>(index)];
        const bool isFx = processor.stepMask.isFxStep(index);
        button.setToggleState(isFx, juce::dontSendNotification);
        button.setButtonText(juce::String(index + 1) + (isFx ? " FX" : " NORMAL"));
        button.setVisible(index < length);
    }
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
    if (displayPool && row >= 0 && row < static_cast<int>(displayPool->size()))
        controls->enabled.setToggleState((*displayPool)[static_cast<size_t>(row)]->settings.enabled,
                                         juce::dontSendNotification);
    controls->enabled.onClick = [this, controls]
    {
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
    refreshStepMaskControls();
    list.repaint();
}
