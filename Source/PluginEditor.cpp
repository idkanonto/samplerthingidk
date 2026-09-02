#include "PluginEditor.h"

namespace
{
class RemoveButton final : public juce::TextButton
{
public:
    RemoveButton() : TextButton("Remove") {}
    int row = -1;
};
}

RandomChopSamplerAudioProcessorEditor::RandomChopSamplerAudioProcessorEditor(RandomChopSamplerAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(760, 520);
    title.setText("RANDOM CHOP SAMPLER", juce::dontSendNotification);
    title.setFont(juce::Font(24.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff2f2f5));
    status.setColour(juce::Label::textColourId, juce::Colour(0xffa9acb7));
    for (auto* component : { static_cast<juce::Component*>(&title), &status, &addButton, &clearButton, &list }) addAndMakeVisible(component);
    list.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff191b21));
    list.setRowHeight(34);

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
        chooser = std::make_unique<juce::FileChooser>("Choose WAV or AIFF files", juce::File(), "*.wav;*.aif;*.aiff");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                           | juce::FileBrowserComponent::canSelectMultipleItems,
            [this](const juce::FileChooser& fc) { juce::StringArray paths; for (const auto& f : fc.getResults()) paths.add(f.getFullPathName()); addFiles(paths); });
    };
    clearButton.onClick = [this] { processor.samples.clear(); refresh(); };
    refresh();
    startTimerHz(8);
}

void RandomChopSamplerAudioProcessorEditor::configureKnob(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 82, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff8b5cf6));
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffc8cad1));
    addAndMakeVisible(slider); addAndMakeVisible(label);
}

void RandomChopSamplerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111218));
    auto drop = getLocalBounds().reduced(20).withTrimmedTop(70).withTrimmedBottom(150);
    g.setColour(juce::Colour(0xff242630)); g.fillRoundedRectangle(drop.toFloat(), 8.0f);
    g.setColour(juce::Colour(0xff494c5a)); g.drawRoundedRectangle(drop.toFloat(), 8.0f, 1.0f);
    if (displayPool && displayPool->empty())
    {
        g.setColour(juce::Colour(0xff8f93a3)); g.setFont(18.0f);
        g.drawText("Drop samples or loops here", drop, juce::Justification::centred);
    }
}

void RandomChopSamplerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto header = area.removeFromTop(55); title.setBounds(header.removeFromLeft(330));
    clearButton.setBounds(header.removeFromRight(90).reduced(2)); addButton.setBounds(header.removeFromRight(135).reduced(2));
    status.setBounds(header);
    auto controls = area.removeFromBottom(135);
    const int width = controls.getWidth() / 5;
    juce::Slider* sliders[] = { &randomStart, &attack, &release, &output, &seed };
    juce::Label* labels[] = { &randomStartLabel, &attackLabel, &releaseLabel, &outputLabel, &seedLabel };
    for (int i = 0; i < 5; ++i) { auto cell = controls.removeFromLeft(width); labels[i]->setBounds(cell.removeFromTop(22)); sliders[i]->setBounds(cell.reduced(4)); }
    list.setBounds(area.reduced(10));
}

bool RandomChopSamplerAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files) if (SampleManager::isSupported(juce::File(path))) return true;
    return false;
}

void RandomChopSamplerAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int) { addFiles(files); }

void RandomChopSamplerAudioProcessorEditor::addFiles(const juce::StringArray& files)
{
    const auto errors = processor.samples.addFiles(files);
    transientMessage = errors.empty() ? juce::String() : juce::String(errors.size()) + " file(s) could not be loaded";
    refresh();
}

void RandomChopSamplerAudioProcessorEditor::refresh()
{
    displayPool = processor.samples.getSnapshot();
    list.updateContent(); list.repaint(); repaint();
}

int RandomChopSamplerAudioProcessorEditor::getNumRows() { return displayPool ? static_cast<int>(displayPool->size()) : 0; }

void RandomChopSamplerAudioProcessorEditor::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (!displayPool || row >= static_cast<int>(displayPool->size())) return;
    const bool recent = row == processor.lastTriggeredIndex.load(std::memory_order_relaxed);
    g.fillAll(selected ? juce::Colour(0xff343746) : (recent ? juce::Colour(0xff29233a) : juce::Colour(0xff191b21)));
    g.setColour(juce::Colour(0xffe3e4e8));
    g.drawText(juce::String(row + 1) + ".  " + (*displayPool)[static_cast<size_t>(row)]->name, 10, 0, width - 95, height, juce::Justification::centredLeft, true);
}

std::unique_ptr<juce::Component> RandomChopSamplerAudioProcessorEditor::refreshComponentForRow(int row, bool, std::unique_ptr<juce::Component> existing)
{
    auto* button = dynamic_cast<RemoveButton*>(existing.get());
    if (!button) { existing = std::make_unique<RemoveButton>(); button = static_cast<RemoveButton*>(existing.get()); }
    button->row = row;
    button->onClick = [this, button] { processor.samples.remove(static_cast<size_t>(button->row)); refresh(); };
    return existing;
}

void RandomChopSamplerAudioProcessorEditor::timerCallback()
{
    auto message = juce::String(processor.samples.size()) + " sample" + (processor.samples.size() == 1 ? "" : "s") + " loaded";
    if (processor.triggeredWhileEmpty.load(std::memory_order_relaxed)) message = "No samples loaded — drop WAV or AIFF files first";
    else if (transientMessage.isNotEmpty()) message = transientMessage;
    status.setText(message, juce::dontSendNotification);
    list.repaint();
}

