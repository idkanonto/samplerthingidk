#include "PluginProcessor.h"
#include "PluginEditor.h"

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
constexpr auto reverseChance = "reverseChance";
constexpr auto retriggerChance = "retriggerChance";
constexpr auto retriggerSize = "retriggerSize";
constexpr auto retriggerCount = "retriggerCount";
constexpr auto skipChance = "skipChance";
constexpr auto reorderChance = "reorderChance";
constexpr auto bendChance = "bendChance";
constexpr auto dropChance = "dropChance";
constexpr auto stepLength = "stepLength";
constexpr auto bitDepth = "bitDepth";
constexpr auto rateReduction = "rateReduction";
}

RandomChopSamplerAudioProcessor::RandomChopSamplerAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    parameters.addParameterListener(IDs::stepLength, this);
}

RandomChopSamplerAudioProcessor::~RandomChopSamplerAudioProcessor()
{
    parameters.removeParameterListener(IDs::stepLength, this);
}

void RandomChopSamplerAudioProcessor::parameterChanged(const juce::String& parameterId, float)
{
    if (parameterId == IDs::stepLength)
        stepMask.requestSequenceReset();
}

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
    juce::StringArray tonicChoices;
    for (const auto* name : randomchop::tonicNames)
        tonicChoices.add(name);
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::targetKey, "Target Key", tonicChoices, randomchop::noTonic));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        IDs::midiPitch, "MIDI Pitch", false));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::rootNote, "Root MIDI Note", 0, 127, 72));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::finalLength, "Final Length",
        juce::NormalisableRange<float>(0.0f, 5000.0f, 10.0f), 0.0f, "ms"));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::voiceMode, "Voice Mode", juce::StringArray { "POLY", "MONO" }, 0));
    for (const auto& parameter : std::initializer_list<std::pair<const char*, const char*>> {
             { IDs::reverseChance, "Reverse Chance" },
             { IDs::retriggerChance, "Retrigger Chance" },
             { IDs::skipChance, "Skip Chance" },
             { IDs::reorderChance, "Reorder Chance" },
             { IDs::bendChance, "Bend Chance" },
             { IDs::dropChance, "Drop Chance" } })
        layout.add(std::make_unique<juce::AudioParameterFloat>(parameter.first, parameter.second,
            juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::retriggerSize, "Repeat Size",
        juce::NormalisableRange<float>(10.0f, 500.0f, 1.0f), 100.0f, "ms"));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::retriggerCount, "Repeat Count", 1, 8, 2));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::stepLength, "Step Mask Length", juce::StringArray { "2", "4", "8", "16" }, 2));
    juce::StringArray bitDepthChoices { "OFF" };
    for (int bits = 4; bits <= 24; ++bits)
        bitDepthChoices.add(juce::String(bits) + " bit");
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::bitDepth, "Bit Crush", bitDepthChoices, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::rateReduction, "Sample Rate Reduction",
        juce::StringArray { "1x (OFF)", "2x", "4x", "8x", "16x", "32x", "64x" }, 0));
    return layout;
}

void RandomChopSamplerAudioProcessor::prepareToPlay(double rate, int)
{
    currentRate = rate;
    voices.prepare(rate);
    masterDigitalProcessor.reset();
    lastSeed = -1;
}

bool RandomChopSamplerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void RandomChopSamplerAudioProcessor::noteOn(int note, float velocity) noexcept
{
    const auto historyTrigger = takeHistory.beginTrigger();
    if (!historyTrigger.isLive)
    {
        startTakeEvent(*historyTrigger.replayEvent, note, velocity);
        return;
    }
    if (historyTrigger.resetLiveSequence)
        stepMask.requestSequenceReset();

    // In LIVE, the event-driven mask advances for every Note On, even when no source can play.
    const auto step = stepMask.consume(randomchop::StepMask::lengthFromChoice(
        static_cast<int>(parameters.getRawParameterValue(IDs::stepLength)->load())));
    if (step.sequenceReset)
        takeHistory.discardIncompleteLiveTake();

    randomchop::TakeEvent takeEvent;
    takeEvent.chanceAllowed = step.chanceAllowed;
    const auto pool = samples.getSnapshot();
    const int selected = randomchop::chooseWeightedSource(*pool, random);
    if (selected < 0)
    {
        takeHistory.recordLiveEvent(takeEvent, step.length, step.cycleCompleted);
        triggeredWhileEmpty.store(true, std::memory_order_relaxed);
        return;
    }
    const auto& sample = (*pool)[static_cast<size_t>(selected)];
    const auto prepared = sample->prepared;
    const double amount = parameters.getRawParameterValue(IDs::randomStart)->load() * 0.01;
    const auto region = randomchop::makeFrameRegion(prepared->audio->getNumSamples(),
                                                     sample->settings.startNormalised,
                                                     sample->settings.endNormalised);
    const double start = randomchop::resolveRandomStart(region, prepared->sampleRate,
                                                        amount, random.unit());
    randomchop::EventDecision eventDecision;
    if (step.chanceAllowed)
    {
        const randomchop::ChanceSettings chanceSettings {
            parameters.getRawParameterValue(IDs::reverseChance)->load(),
            parameters.getRawParameterValue(IDs::retriggerChance)->load(),
            parameters.getRawParameterValue(IDs::skipChance)->load(),
            parameters.getRawParameterValue(IDs::reorderChance)->load(),
            parameters.getRawParameterValue(IDs::bendChance)->load(),
            parameters.getRawParameterValue(IDs::dropChance)->load(),
            parameters.getRawParameterValue(IDs::retriggerSize)->load(),
            static_cast<int>(parameters.getRawParameterValue(IDs::retriggerCount)->load())
        };
        eventDecision = randomchop::resolveChanceEvent(
            chanceSettings, region, start, prepared->sampleRate, currentRate, random);
    }

    takeEvent.sourceId = randomchop::copyStableSourceId(sample->settings.id);
    takeEvent.prepared = prepared;
    takeEvent.region = region;
    takeEvent.randomStart = start;
    takeEvent.sourceRuntimeId = sample->runtimeId;
    takeEvent.sourceKey = sample->settings.sourceKey;
    takeEvent.transposeSemitones = sample->settings.transposeSemitones;
    takeEvent.fineTuneCents = sample->settings.fineTuneCents;
    takeEvent.sourceGain = juce::Decibels::decibelsToGain(sample->settings.gainDb);
    takeEvent.decision = eventDecision;
    startTakeEvent(takeEvent, note, velocity);
    takeHistory.recordLiveEvent(takeEvent, step.length, step.cycleCompleted);
}

void RandomChopSamplerAudioProcessor::startTakeEvent(
    const randomchop::TakeEvent& event, int note, float velocity) noexcept
{
    if (!event.isPlayable())
    {
        triggeredWhileEmpty.store(true, std::memory_order_relaxed);
        return;
    }

    const auto targetKey = static_cast<int>(parameters.getRawParameterValue(IDs::targetKey)->load());
    const auto midiPitch = parameters.getRawParameterValue(IDs::midiPitch)->load() >= 0.5f;
    const auto rootNote = static_cast<int>(parameters.getRawParameterValue(IDs::rootNote)->load());
    const auto pitchSemitones = randomchop::totalPitchSemitones(
        event.sourceKey, targetKey, event.transposeSemitones,
        event.fineTuneCents, midiPitch, note, rootNote);
    const auto pitchRatio = randomchop::pitchRatioForSemitones(pitchSemitones);
    const auto mode = parameters.getRawParameterValue(IDs::voiceMode)->load() >= 0.5f
        ? randomchop::VoiceMode::mono : randomchop::VoiceMode::poly;
    auto& voice = voices.acquire(mode);

    voice.start(event.prepared, note, velocity, event.randomStart, event.region, pitchRatio, false,
                event.sourceGain,
                juce::jmax(0.001f, parameters.getRawParameterValue(IDs::attack)->load()),
                parameters.getRawParameterValue(IDs::release)->load(), ++voiceCounter,
                parameters.getRawParameterValue(IDs::finalLength)->load(), event.decision);
    lastTriggeredRuntimeId.store(event.sourceRuntimeId, std::memory_order_relaxed);
    triggeredWhileEmpty.store(false, std::memory_order_relaxed);
}

void RandomChopSamplerAudioProcessor::noteOff(int note) noexcept
{
    const float release = parameters.getRawParameterValue(IDs::release)->load();
    voices.noteOff(note, release);
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
        if (span > 0) voices.render(buffer, rendered, span);
        const auto message = metadata.getMessage();
        if (message.isNoteOn()) noteOn(message.getNoteNumber(), message.getFloatVelocity());
        else if (message.isNoteOff()) noteOff(message.getNoteNumber());
        rendered = eventPosition;
    }
    if (rendered < buffer.getNumSamples())
        voices.render(buffer, rendered, buffer.getNumSamples() - rendered);

    masterDigitalProcessor.process(buffer,
        randomchop::MasterDigitalProcessor::bitDepthFromChoice(static_cast<int>(
            parameters.getRawParameterValue(IDs::bitDepth)->load())),
        randomchop::MasterDigitalProcessor::rateFactorFromChoice(static_cast<int>(
            parameters.getRawParameterValue(IDs::rateReduction)->load())));
    buffer.applyGain(juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue(IDs::output)->load()));
}

void RandomChopSamplerAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.appendChild(samples.createState(), nullptr);
    state.appendChild(stepMask.createState(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void RandomChopSamplerAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid())
        {
            auto files = state.getChildWithName("SAMPLES");
            auto mask = state.getChildWithName("STEP_MASK");
            if (files.isValid()) state.removeChild(files, nullptr);
            if (mask.isValid()) state.removeChild(mask, nullptr);
            parameters.replaceState(state);
            // An absent SAMPLES node represents a parameter-only state and
            // must replace, rather than silently retain, the current pool.
            samples.restoreState(files);
            stepMask.restoreState(mask);
            // Take History is intentionally session-only and is never serialized.
            takeHistory.requestClear();
        }
    }
}

juce::AudioProcessorEditor* RandomChopSamplerAudioProcessor::createEditor() { return new RandomChopSamplerAudioProcessorEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RandomChopSamplerAudioProcessor(); }
