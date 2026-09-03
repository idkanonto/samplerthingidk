#include "PluginEditor.h"

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
    setSize(940, 680);
    title.setText("RANDOM CHOP SAMPLER V2", juce::dontSendNotification);
    title.setFont(juce::Font(24.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff2f2f5));
    status.setColour(juce::Label::textColourId, juce::Colour(0xffa9acb7));

    juce::Component* components[] = { &title, &status, &addButton, &clearButton,
        &enableAllButton, &disableAllButton, &list, &sourceGain, &sourceWeight,
        &sourceGainLabel, &sourceWeightLabel };
    for (auto* component : components) addAndMakeVisible(component);
    list.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff191b21));
    list.setRowHeight(34);

    sourceGain.setRange(-60.0, 12.0, 0.1);
    sourceGain.setTextValueSuffix(" dB");
    sourceWeight.setRange(0.01, 10.0, 0.01);
    sourceGainLabel.setText("SELECTED GAIN", juce::dontSendNotification);
    sourceWeightLabel.setText("SELECTION WEIGHT", juce::dontSendNotification);
    sourceGain.onValueChange = [this]
    {
        const auto row = processor.selectedSourceIndex.load();
        if (row >= 0) processor.samples.updateSettings(static_cast<size_t>(row),
            [this](SampleSettings& s) { s.gainDb = static_cast<float>(sourceGain.getValue()); });
    };
    sourceWeight.onValueChange = [this]
    {
        const auto row = processor.selectedSourceIndex.load();
        if (row >= 0) processor.samples.updateSettings(static_cast<size_t>(row),
            [this](SampleSettings& s) { s.selectionWeight = static_cast<float>(sourceWeight.getValue()); });
    };

    configureKnob(randomStart, randomStartLabel, "RANDOM START");
    configureKnob(attack, attackLabel, "ATTACK");
    configureKnob(release, releaseLabel, "RELEASE");
    configureKnob(output, outputLabel, "OUTPUT");
    configureKnob(seed, seedLabel, "SEED");
    randomStartAttachment = std::make_unique<SliderAttachment>(p.parameters, "randomStart", randomStart);
    attackAttachment = std::make_unique<SliderAttachment>(p.parameters, "attack", attack);
    releaseAttachment = std::make_unique<SliderAttachment>(p.parameters, "release", release);
    outputAttachment = std::make_unique<SliderAttachment>(p.parameters, "output", output);
    seedAttachment = std::make_unique<SliderAttachment>(p.parameters, "seed", seed);

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

void RandomChopSamplerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111218));
    auto poolArea = getLocalBounds().reduced(20).withTrimmedTop(90).withTrimmedBottom(205);
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
    auto sourceControls = area.removeFromBottom(62);
    auto gainCell = sourceControls.removeFromLeft(350).reduced(3);
    sourceGainLabel.setBounds(gainCell.removeFromLeft(125));
    sourceGain.setBounds(gainCell);
    auto weightCell = sourceControls.removeFromLeft(390).reduced(3);
    sourceWeightLabel.setBounds(weightCell.removeFromLeft(145));
    sourceWeight.setBounds(weightCell);

    const int knobWidth = globalControls.getWidth() / 5;
    juce::Slider* sliders[] = { &randomStart, &attack, &release, &output, &seed };
    juce::Label* labels[] = { &randomStartLabel, &attackLabel, &releaseLabel, &outputLabel, &seedLabel };
    for (int i = 0; i < 5; ++i)
    {
        auto cell = globalControls.removeFromLeft(knobWidth);
        labels[i]->setBounds(cell.removeFromTop(22));
        sliders[i]->setBounds(cell.reduced(4));
    }
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
    const int oldSelection = processor.selectedSourceIndex.load();
    displayPool = processor.samples.getSnapshot();
    const int selected = displayPool->empty() ? -1
        : juce::jlimit(0, static_cast<int>(displayPool->size()) - 1, juce::jmax(0, oldSelection));
    processor.selectedSourceIndex.store(selected);
    list.updateContent();
    list.selectRow(selected);
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
    const bool recent = row == processor.lastTriggeredIndex.load(std::memory_order_relaxed);
    g.fillAll(selected ? juce::Colour(0xff343746)
                       : (recent ? juce::Colour(0xff29233a) : juce::Colour(0xff191b21)));
    const auto& source = (*displayPool)[static_cast<size_t>(row)];
    g.setColour(source->settings.missing ? juce::Colour(0xffff8a8a) : juce::Colour(0xffe3e4e8));
    const auto suffix = source->settings.missing ? "  [MISSING]" : juce::String();
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
        processor.samples.setEnabled(static_cast<size_t>(controls->row),
                                     controls->enabled.getToggleState());
        refresh();
    };
    controls->remove.onClick = [this, controls]
    {
        processor.samples.remove(static_cast<size_t>(controls->row));
        refresh();
    };
    return controls;
}

void RandomChopSamplerAudioProcessorEditor::selectedRowsChanged(int row)
{
    processor.selectedSourceIndex.store(row);
    if (displayPool && row >= 0 && row < static_cast<int>(displayPool->size()))
    {
        const auto& settings = (*displayPool)[static_cast<size_t>(row)]->settings;
        sourceGain.setValue(settings.gainDb, juce::dontSendNotification);
        sourceWeight.setValue(settings.selectionWeight, juce::dontSendNotification);
    }
}

void RandomChopSamplerAudioProcessorEditor::timerCallback()
{
    const int count = processor.samples.size();
    auto message = juce::String(count).paddedLeft('0', 2) + " / 20 sources";
    if (processor.triggeredWhileEmpty.load(std::memory_order_relaxed))
        message = "No enabled playable sources";
    else if (transientMessage.isNotEmpty())
        message += " — " + transientMessage;
    status.setText(message, juce::dontSendNotification);
    list.repaint();
}
