#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace IDs
{
constexpr auto randomStart = "randomStart";
constexpr auto attack = "attack";
constexpr auto release = "release";
constexpr auto output = "output";
constexpr auto targetKey = "targetKey";
constexpr auto midiPitch = "midiPitch";
constexpr auto rootNote = "rootNote";
constexpr auto finalLength = "finalLength";
constexpr auto voiceMode = "voiceMode";
constexpr auto globalGrid = "globalGrid";
constexpr auto scrambleAmount = "scrambleAmount";
constexpr auto fractureCharacter = "fractureCharacter";
constexpr auto fractureMix = "fractureMix";
constexpr auto spectralDepth = "spectralDepth";
constexpr auto spectralScanRate = "spectralScanRate";
constexpr auto smearAmount = "smearAmount";
constexpr auto rateReduction = "rateReduction";
}

RandomChopSamplerAudioProcessor::RandomChopSamplerAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    setLatencySamples(randomchop::SpectralDrawProcessor::latencySamples);
    const auto generated = static_cast<uint64_t>(
        juce::Random::getSystemRandom().nextInt64()) & 0x7fffffffffffffffULL;
    internalSeed.store(generated != 0 ? generated : 1, std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout
RandomChopSamplerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::randomStart, "Start Range",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::attack, "Attack",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.4f), 0.005f, "s"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::release, "Release",
        juce::NormalisableRange<float>(0.005f, 3.0f, 0.001f, 0.35f), 0.08f, "s"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::output, "Output",
        juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f), 0.0f, "dB"));
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
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::scrambleAmount, "Scramble",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::fractureCharacter, "Fracture Character",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 42.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::fractureMix, "Fracture",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::spectralDepth, "Spectral Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::spectralScanRate, "Spectral Scan Rate",
        juce::StringArray { "2 beats", "1 bar", "2 bars", "4 bars" }, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::smearAmount, "Smear",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::rateReduction, "Fracture Rate",
        juce::StringArray { "1x (OFF)", "2x", "4x", "8x", "16x", "32x", "64x" }, 0));
    return layout;
}

void RandomChopSamplerAudioProcessor::prepareToPlay(double rate, int)
{
    currentRate = std::clamp(randomchop::finiteOr(rate, 44100.0), 1.0, 768000.0);
    voices.prepare(currentRate);
    hostGrid.reset();
    scrambleProcessor.prepare(currentRate);
    fractureProcessor.prepare(currentRate);
    spectralDrawProcessor.prepare(currentRate);
    smearProcessor.prepare(currentRate);
    lastGridBoundaries = {};
    const auto seed = internalSeed.load(std::memory_order_relaxed);
    random.setSeed(seed);
    scrambleProcessor.setSeed(seed);
    fractureProcessor.setSeed(seed);
    smearProcessor.setSeed(seed);
    lastSeed = seed;
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
    const auto seed = internalSeed.load(std::memory_order_relaxed);
    if (seed != lastSeed)
    {
        random.setSeed(seed);
        scrambleProcessor.setSeed(seed);
        fractureProcessor.setSeed(seed);
        smearProcessor.setSeed(seed);
        lastSeed = seed;
    }

    const auto hostTiming = readHostTiming();
    const auto gridChoice = static_cast<int>(
        parameters.getRawParameterValue(IDs::globalGrid)->load());
    lastGridBoundaries = hostGrid.process(
        currentRate, buffer.getNumSamples(), gridChoice, hostTiming);

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

    scrambleProcessor.process(buffer, lastGridBoundaries, gridChoice,
        { parameters.getRawParameterValue(IDs::scrambleAmount)->load() });
    fractureProcessor.process(buffer,
        { parameters.getRawParameterValue(IDs::fractureMix)->load(),
          parameters.getRawParameterValue(IDs::fractureCharacter)->load(),
          randomchop::FractureProcessor::rateFactorFromChoice(static_cast<int>(
              parameters.getRawParameterValue(IDs::rateReduction)->load())) });
    spectralDrawProcessor.process(buffer, spectralMaskStore,
        { parameters.getRawParameterValue(IDs::spectralDepth)->load(),
          static_cast<int>(parameters.getRawParameterValue(IDs::spectralScanRate)->load()),
          lastGridBoundaries.bpm,
          hostTiming.ppq,
          lastGridBoundaries.usedHostClock && hostTiming.hasPpq,
          lastGridBoundaries.transportDiscontinuity });
    smearProcessor.process(buffer,
        { parameters.getRawParameterValue(IDs::smearAmount)->load() });
    buffer.applyGain(juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue(IDs::output)->load()));
}

void RandomChopSamplerAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.setProperty("stateVersion", randomchop::currentStateVersion, nullptr);
    state.setProperty("creativeSeed",
        juce::String(static_cast<int64_t>(
            internalSeed.load(std::memory_order_relaxed))), nullptr);
    state.setProperty("spectralCanvas", spectralMaskStore.encodeCanvas(), nullptr);
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
        const auto spectralCanvas = state.getProperty("spectralCanvas").toString();
        const auto restoredVersion = static_cast<int>(state.getProperty("stateVersion", 0));
        const auto restoredSeedText = state.getProperty("creativeSeed").toString();
        const auto legacySeed = static_cast<int64_t>(state.getProperty("seed", 0));
        auto restoredSeed = static_cast<uint64_t>(restoredSeedText.getLargeIntValue());
        if (restoredSeed == 0 && legacySeed > 0)
            restoredSeed = static_cast<uint64_t>(legacySeed);
        if (restoredSeed != 0)
            internalSeed.store(restoredSeed, std::memory_order_relaxed);
        if (restoredVersion < 7)
        {
            const auto oldScrambleChance = static_cast<float>(
                state.getProperty("scrambleChance", 0.0f));
            const auto oldScrambleAmount = static_cast<float>(
                state.getProperty(IDs::scrambleAmount, 50.0f));
            const auto oldFreezeChance = static_cast<float>(
                state.getProperty("freezeChance", 0.0f));
            const auto oldFreezeOctave = static_cast<float>(
                state.getProperty("freezeOctaveChance", 0.0f));
            const auto migratedScramble = std::clamp(std::max({
                oldScrambleAmount * oldScrambleChance * 0.01f,
                oldFreezeChance * 0.78f,
                oldFreezeOctave * oldFreezeChance * 0.006f }), 0.0f, 100.0f);
            state.setProperty(IDs::scrambleAmount, migratedScramble, nullptr);

            const auto oldFracture = static_cast<float>(
                state.getProperty(IDs::fractureMix, 0.0f));
            const auto oldCodec = static_cast<float>(
                state.getProperty("codecAmount", 0.0f));
            const auto oldRate = static_cast<int>(state.getProperty(IDs::rateReduction, 0));
            state.setProperty(IDs::fractureMix, std::clamp(std::max({ oldFracture,
                oldCodec * 0.78f, oldRate > 0 ? 28.0f : 0.0f }), 0.0f, 100.0f), nullptr);
        }
        if (files.isValid())
            state.removeChild(files, nullptr);
        state.removeProperty("spectralCanvas", nullptr);
        state.removeProperty("creativeSeed", nullptr);
        randomchop::removeLegacyState(state);
        const auto ensureParameter = [&state](const char* id, float value)
        {
            if (!state.hasProperty(id))
                state.setProperty(id, value, nullptr);
        };
        ensureParameter(IDs::globalGrid, 1.0f);
        ensureParameter(IDs::scrambleAmount, 0.0f);
        ensureParameter(IDs::fractureCharacter, 42.0f);
        ensureParameter(IDs::fractureMix, 0.0f);
        ensureParameter(IDs::spectralDepth, 0.0f);
        ensureParameter(IDs::spectralScanRate, 1.0f);
        ensureParameter(IDs::smearAmount, 0.0f);
        ensureParameter(IDs::rateReduction, 0.0f);
        parameters.replaceState(state);
        samples.restoreState(files);
        if (!spectralMaskStore.restoreEncodedCanvas(spectralCanvas))
            spectralMaskStore.clear();
    }
}

randomchop::SpectralMaskStore::Canvas
RandomChopSamplerAudioProcessor::getSpectralCanvas() const
{
    return spectralMaskStore.copyCanvas();
}

void RandomChopSamplerAudioProcessor::setSpectralCanvas(
    const randomchop::SpectralMaskStore::Canvas& canvas)
{
    spectralMaskStore.setCanvas(canvas);
}

void RandomChopSamplerAudioProcessor::clearSpectralCanvas()
{
    spectralMaskStore.clear();
}

uint64_t RandomChopSamplerAudioProcessor::getSpectralCanvasGeneration() const noexcept
{
    return spectralMaskStore.getPublishedGeneration();
}

float RandomChopSamplerAudioProcessor::getSpectralScanPosition() const noexcept
{
    return spectralDrawProcessor.getScanPosition();
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
    setParameter(IDs::fractureMix, settings.amount);
    setParameter(IDs::fractureCharacter, settings.character);
    setParameter(IDs::rateReduction, static_cast<float>(
        randomchop::FractureProcessor::choiceFromRateFactor(settings.rateFactor)));
}

juce::AudioProcessorEditor* RandomChopSamplerAudioProcessor::createEditor()
{
    return new RandomChopSamplerAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RandomChopSamplerAudioProcessor();
}
