#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace IDs
{
constexpr auto randomStart = "randomStart";
constexpr auto attack = "attack";
constexpr auto release = "release";
constexpr auto output = "output";
constexpr auto seed = "seed";
constexpr auto targetKey = "targetKey";
constexpr auto midiPitch = "midiPitch";
constexpr auto rootNote = "rootNote";
constexpr auto finalLength = "finalLength";
constexpr auto voiceMode = "voiceMode";
constexpr auto globalGrid = "globalGrid";
constexpr auto freezeChance = "freezeChance";
constexpr auto freezeSize = "freezeSize";
constexpr auto freezeHold = "freezeHold";
constexpr auto freezeOctaveChance = "freezeOctaveChance";
constexpr auto scrambleChance = "scrambleChance";
constexpr auto scrambleAmount = "scrambleAmount";
constexpr auto fractureDrive = "fractureDrive";
constexpr auto fractureCharacter = "fractureCharacter";
constexpr auto fractureFilterMorph = "fractureFilterMorph";
constexpr auto fractureFrequency = "fractureFrequency";
constexpr auto fractureResonance = "fractureResonance";
constexpr auto fractureMix = "fractureMix";
constexpr auto smearAmount = "smearAmount";
constexpr auto codecAmount = "codecAmount";
constexpr auto codecQuality = "codecQuality";
constexpr auto rateReduction = "rateReduction";
}

RandomChopSamplerAudioProcessor::RandomChopSamplerAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
RandomChopSamplerAudioProcessor::createParameterLayout()
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
    juce::StringArray tonicChoices;
    for (const auto* name : randomchop::tonicNames)
        tonicChoices.add(name);
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::targetKey, "Target Key", tonicChoices, randomchop::noTonic));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::midiPitch, "MIDI Pitch", false));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::rootNote, "Root MIDI Note", 0, 127, 72));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::finalLength, "Final Length",
        juce::NormalisableRange<float>(0.0f, 5000.0f, 10.0f), 0.0f, "ms"));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::voiceMode, "Voice Mode", juce::StringArray { "POLY", "MONO" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::globalGrid, "Global Grid", juce::StringArray { "1/8", "1/16", "1/32" }, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::freezeChance, "Freeze Chance",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterChoice>(IDs::freezeSize, "Freeze Size",
        juce::StringArray { "1/4 grid", "1/2 grid", "1 grid" }, 1));
    layout.add(std::make_unique<juce::AudioParameterChoice>(IDs::freezeHold, "Freeze Hold",
        juce::StringArray { "1 grid", "2 grids", "4 grids", "8 grids" }, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::freezeOctaveChance, "Freeze Octave Chance",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::scrambleChance, "Scramble Chance",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::scrambleAmount, "Scramble Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::fractureDrive, "Fracture Drive",
        juce::NormalisableRange<float>(0.0f, 36.0f, 0.1f), 0.0f, "dB"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::fractureCharacter, "Fracture Character",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::fractureFilterMorph, "Fracture Filter Morph",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::fractureFrequency, "Fracture Frequency",
        juce::NormalisableRange<float>(80.0f, 12000.0f, 1.0f, 0.35f), 1000.0f, "Hz"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::fractureResonance, "Fracture Resonance",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::fractureMix, "Fracture Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::smearAmount, "Smear Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::codecAmount, "Codec Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterChoice>(IDs::codecQuality, "Codec Quality",
        juce::StringArray { "HIGH", "MEDIUM", "LOW", "SHREDDED" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::rateReduction, "Codec Rate Reduction",
        juce::StringArray { "1x (OFF)", "2x", "4x", "8x", "16x", "32x", "64x" }, 0));
    return layout;
}

void RandomChopSamplerAudioProcessor::prepareToPlay(double rate, int)
{
    currentRate = std::clamp(randomchop::finiteOr(rate, 44100.0), 1.0, 768000.0);
    voices.prepare(currentRate);
    hostGrid.reset();
    freezeProcessor.prepare(currentRate);
    scrambleProcessor.prepare(currentRate);
    fractureProcessor.prepare(currentRate);
    smearProcessor.prepare(currentRate);
    codecProcessor.prepare(currentRate);
    lastGridBoundaries = {};
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
    const auto selected = randomchop::chooseWeightedSource(*pool, random);
    if (selected < 0)
    {
        triggeredWhileEmpty.store(true, std::memory_order_relaxed);
        return;
    }

    const auto& source = (*pool)[static_cast<size_t>(selected)];
    const auto prepared = source->prepared;
    const auto region = randomchop::makeFrameRegion(
        prepared->audio->getNumSamples(), source->settings.startNormalised,
        source->settings.endNormalised);
    const auto startAmount = parameters.getRawParameterValue(IDs::randomStart)->load() * 0.01;
    const auto start = randomchop::resolveRandomStart(
        region, prepared->sampleRate, startAmount, random.unit());
    const auto targetKey = static_cast<int>(parameters.getRawParameterValue(IDs::targetKey)->load());
    const auto midiPitch = parameters.getRawParameterValue(IDs::midiPitch)->load() >= 0.5f;
    const auto rootNote = static_cast<int>(parameters.getRawParameterValue(IDs::rootNote)->load());
    const auto pitchSemitones = randomchop::totalPitchSemitones(
        source->settings.sourceKey, targetKey, source->settings.transposeSemitones,
        source->settings.fineTuneCents, midiPitch, note, rootNote);
    const auto pitchRatio = randomchop::pitchRatioForSemitones(pitchSemitones);
    const auto mode = parameters.getRawParameterValue(IDs::voiceMode)->load() >= 0.5f
        ? randomchop::VoiceMode::mono : randomchop::VoiceMode::poly;
    auto& voice = voices.acquire(mode);
    voice.start(prepared, note, velocity, start, region, pitchRatio,
                juce::Decibels::decibelsToGain(source->settings.gainDb),
                juce::jmax(0.001f, parameters.getRawParameterValue(IDs::attack)->load()),
                parameters.getRawParameterValue(IDs::release)->load(), ++voiceCounter,
                parameters.getRawParameterValue(IDs::finalLength)->load());
    lastTriggeredRuntimeId.store(source->runtimeId, std::memory_order_relaxed);
    triggeredWhileEmpty.store(false, std::memory_order_relaxed);
}

void RandomChopSamplerAudioProcessor::noteOff(int note) noexcept
{
    voices.noteOff(note, parameters.getRawParameterValue(IDs::release)->load());
}

randomchop::HostTiming RandomChopSamplerAudioProcessor::readHostTiming() const noexcept
{
    randomchop::HostTiming timing;
    if (const auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
            {
                timing.bpm = *bpm;
                timing.hasBpm = std::isfinite(*bpm) && *bpm > 0.0;
            }
            if (const auto ppq = position->getPpqPosition())
            {
                timing.ppq = *ppq;
                timing.hasPpq = std::isfinite(*ppq);
            }
            timing.isPlaying = position->getIsPlaying();
        }
    }
    return timing;
}

void RandomChopSamplerAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    const auto seed = static_cast<int>(parameters.getRawParameterValue(IDs::seed)->load());
    if (seed != lastSeed)
    {
        random.setSeed(static_cast<uint64_t>(seed));
        freezeProcessor.setSeed(static_cast<uint64_t>(seed));
        scrambleProcessor.setSeed(static_cast<uint64_t>(seed));
        lastSeed = seed;
    }

    const auto gridChoice = static_cast<int>(
        parameters.getRawParameterValue(IDs::globalGrid)->load());
    lastGridBoundaries = hostGrid.process(
        currentRate, buffer.getNumSamples(), gridChoice, readHostTiming());

    int rendered = 0;
    for (const auto metadata : midi)
    {
        const auto eventPosition = juce::jlimit(0, buffer.getNumSamples(), metadata.samplePosition);
        const auto span = eventPosition - rendered;
        if (span > 0)
            voices.render(buffer, rendered, span);
        const auto message = metadata.getMessage();
        if (message.isNoteOn())
            noteOn(message.getNoteNumber(), message.getFloatVelocity());
        else if (message.isNoteOff())
            noteOff(message.getNoteNumber());
        rendered = eventPosition;
    }
    if (rendered < buffer.getNumSamples())
        voices.render(buffer, rendered, buffer.getNumSamples() - rendered);

    freezeProcessor.process(buffer, lastGridBoundaries, gridChoice,
        { parameters.getRawParameterValue(IDs::freezeChance)->load(),
          static_cast<int>(parameters.getRawParameterValue(IDs::freezeSize)->load()),
          static_cast<int>(parameters.getRawParameterValue(IDs::freezeHold)->load()),
          parameters.getRawParameterValue(IDs::freezeOctaveChance)->load() });
    scrambleProcessor.process(buffer, lastGridBoundaries, gridChoice,
        { parameters.getRawParameterValue(IDs::scrambleChance)->load(),
          parameters.getRawParameterValue(IDs::scrambleAmount)->load() });
    fractureProcessor.process(buffer,
        { parameters.getRawParameterValue(IDs::fractureDrive)->load(),
          parameters.getRawParameterValue(IDs::fractureCharacter)->load(),
          parameters.getRawParameterValue(IDs::fractureFilterMorph)->load(),
          parameters.getRawParameterValue(IDs::fractureFrequency)->load(),
          parameters.getRawParameterValue(IDs::fractureResonance)->load(),
          parameters.getRawParameterValue(IDs::fractureMix)->load() });
    smearProcessor.process(buffer,
        { parameters.getRawParameterValue(IDs::smearAmount)->load() });
    codecProcessor.process(buffer,
        { parameters.getRawParameterValue(IDs::codecAmount)->load(),
          static_cast<int>(parameters.getRawParameterValue(IDs::codecQuality)->load()),
          randomchop::CodecProcessor::rateFactorFromChoice(static_cast<int>(
              parameters.getRawParameterValue(IDs::rateReduction)->load())) });
    buffer.applyGain(juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue(IDs::output)->load()));
}

void RandomChopSamplerAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.setProperty("stateVersion", randomchop::currentStateVersion, nullptr);
    state.appendChild(samples.createState(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destination);
}

void RandomChopSamplerAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (!state.isValid())
            return;
        const auto files = state.getChildWithName("SAMPLES");
        if (files.isValid())
            state.removeChild(files, nullptr);
        randomchop::removeLegacyState(state);
        const auto ensureParameter = [&state](const char* id, float value)
        {
            if (!state.hasProperty(id))
                state.setProperty(id, value, nullptr);
        };
        ensureParameter(IDs::globalGrid, 1.0f);
        ensureParameter(IDs::freezeChance, 0.0f);
        ensureParameter(IDs::freezeSize, 1.0f);
        ensureParameter(IDs::freezeHold, 1.0f);
        ensureParameter(IDs::freezeOctaveChance, 0.0f);
        ensureParameter(IDs::scrambleChance, 0.0f);
        ensureParameter(IDs::scrambleAmount, 50.0f);
        ensureParameter(IDs::fractureDrive, 0.0f);
        ensureParameter(IDs::fractureCharacter, 0.0f);
        ensureParameter(IDs::fractureFilterMorph, 0.0f);
        ensureParameter(IDs::fractureFrequency, 1000.0f);
        ensureParameter(IDs::fractureResonance, 0.0f);
        ensureParameter(IDs::fractureMix, 0.0f);
        ensureParameter(IDs::smearAmount, 0.0f);
        ensureParameter(IDs::codecAmount, 0.0f);
        ensureParameter(IDs::codecQuality, 0.0f);
        parameters.replaceState(state);
        samples.restoreState(files);
    }
}

void RandomChopSamplerAudioProcessor::applyFracturePreset(int index)
{
    const auto& settings = randomchop::getFracturePreset(index).settings;
    const auto setParameter = [this](const char* id, float value)
    {
        if (auto* parameter = parameters.getParameter(id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    };
    setParameter(IDs::fractureDrive, settings.driveDb);
    setParameter(IDs::fractureCharacter, settings.character);
    setParameter(IDs::fractureFilterMorph, settings.filterMorph);
    setParameter(IDs::fractureFrequency, settings.frequencyHz);
    setParameter(IDs::fractureResonance, settings.resonance);
    setParameter(IDs::fractureMix, settings.mix);
}

juce::AudioProcessorEditor* RandomChopSamplerAudioProcessor::createEditor()
{
    return new RandomChopSamplerAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RandomChopSamplerAudioProcessor();
}
