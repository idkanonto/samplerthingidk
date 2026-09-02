#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace IDs { constexpr auto randomStart="randomStart", attack="attack", release="release", output="output", seed="seed"; }

RandomChopSamplerAudioProcessor::RandomChopSamplerAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout RandomChopSamplerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::randomStart, "Random Start",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::attack, "Attack",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.4f), 0.005f, "s"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::release, "Release",
        juce::NormalisableRange<float>(0.005f, 3.0f, 0.001f, 0.35f), 0.08f, "s"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::output, "Output",
        juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f), 0.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterInt>(IDs::seed, "Seed", 1, 999999, 1));
    return layout;
}

void RandomChopSamplerAudioProcessor::prepareToPlay(double rate, int)
{
    currentRate = rate;
    for (auto& voice : voices) { voice.forceStop(); voice.prepare(rate); }
    lastSeed = -1;
}

bool RandomChopSamplerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void RandomChopSamplerAudioProcessor::noteOn(int note, float velocity) noexcept
{
    const auto pool = samples.getSnapshot();
    if (pool->empty()) { triggeredWhileEmpty.store(true, std::memory_order_relaxed); return; }
    const auto selected = random.bounded(static_cast<uint32_t>(pool->size()));
    const auto& sample = (*pool)[selected];
    const double amount = parameters.getRawParameterValue(IDs::randomStart)->load() * 0.01;
    const double margin = juce::jmax(2.0, sample->sampleRate * 0.003);
    const double lastLegal = juce::jmax(0.0, static_cast<double>(sample->audio.getNumSamples() - 2) - margin);
    const double start = random.unit() * lastLegal * amount;

    auto* voice = &voices[0];
    for (auto& candidate : voices)
        if (!candidate.isActive()) { voice = &candidate; break; }
        else if (candidate.getAge() < voice->getAge()) voice = &candidate;

    voice->start(sample, note, velocity, start,
                 juce::jmax(0.001f, parameters.getRawParameterValue(IDs::attack)->load()),
                 parameters.getRawParameterValue(IDs::release)->load(), ++voiceCounter);
    lastTriggeredIndex.store(static_cast<int>(selected), std::memory_order_relaxed);
    triggeredWhileEmpty.store(false, std::memory_order_relaxed);
}

void RandomChopSamplerAudioProcessor::noteOff(int note) noexcept
{
    const float release = parameters.getRawParameterValue(IDs::release)->load();
    for (auto& voice : voices) if (voice.isActive() && voice.getNote() == note) voice.release(release);
}

void RandomChopSamplerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const int seed = static_cast<int>(parameters.getRawParameterValue(IDs::seed)->load());
    if (seed != lastSeed) { random.setSeed(static_cast<uint64_t>(seed)); lastSeed = seed; }

    int rendered = 0;
    for (const auto metadata : midi)
    {
        const int eventPosition = juce::jlimit(0, buffer.getNumSamples(), metadata.samplePosition);
        const int span = eventPosition - rendered;
        if (span > 0) for (auto& voice : voices) voice.render(buffer, rendered, span);
        const auto message = metadata.getMessage();
        if (message.isNoteOn()) noteOn(message.getNoteNumber(), message.getFloatVelocity());
        else if (message.isNoteOff()) noteOff(message.getNoteNumber());
        rendered = eventPosition;
    }
    if (rendered < buffer.getNumSamples())
        for (auto& voice : voices) voice.render(buffer, rendered, buffer.getNumSamples() - rendered);

    buffer.applyGain(juce::Decibels::decibelsToGain(parameters.getRawParameterValue(IDs::output)->load()));
}

void RandomChopSamplerAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    auto paths = std::make_unique<juce::XmlElement>("SAMPLES");
    for (const auto& path : samples.getPaths()) paths->createNewChildElement("FILE")->setAttribute("path", path);
    state.appendChild(juce::ValueTree::fromXml(*paths), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void RandomChopSamplerAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid())
        {
            juce::StringArray paths;
            auto files = state.getChildWithName("SAMPLES");
            for (int i = 0; i < files.getNumChildren(); ++i) paths.add(files.getChild(i)["path"].toString());
            if (files.isValid()) state.removeChild(files, nullptr);
            parameters.replaceState(state);
            samples.clear();
            samples.addFiles(paths); // Hosts call state restore away from the real-time callback.
        }
    }
}

juce::AudioProcessorEditor* RandomChopSamplerAudioProcessor::createEditor() { return new RandomChopSamplerAudioProcessorEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RandomChopSamplerAudioProcessor(); }
