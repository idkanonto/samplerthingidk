#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WebViewEditor.h"
#include <algorithm>
#include <cmath>

namespace IDs
{
constexpr auto output = "output";
constexpr auto targetKey = "targetKey";
constexpr auto midiPitch = "midiPitch";
constexpr auto voiceMode = "voiceMode";
constexpr auto scrambleAmount = "scrambleAmount";
constexpr auto meltAmount = "meltAmount";
constexpr auto spectralDepth = "spectralDepth";
constexpr auto smearAmount = "smearAmount";
}

namespace
{
constexpr float internalAttackSeconds = 0.004f;
constexpr float internalReleaseSeconds = 0.075f;
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
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::output, "Output",
        juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f), 0.0f, "dB"));
    juce::StringArray tonicChoices;
    for (const auto* name : randomchop::tonicNames)
        tonicChoices.add(name);
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::targetKey, "Play In Key", tonicChoices, randomchop::noTonic));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::midiPitch, "Chords", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::voiceMode, "Voice Mode", juce::StringArray { "POLY", "MONO" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        IDs::scrambleAmount, "Scramble",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::meltAmount, "Melt",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::spectralDepth, "Spectral Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::smearAmount, "Smear",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    return layout;
}

void RandomChopSamplerAudioProcessor::prepareToPlay(double rate, int maximumBlockSize)
{
    juce::ignoreUnused(maximumBlockSize);
    currentRate = std::clamp(randomchop::finiteOr(rate, 44100.0), 1.0, 768000.0);
    voices.prepare(currentRate);
    activeVoiceCount.store(0, std::memory_order_relaxed);
    previewVoice.forceStop();
    previewVoice.prepare(currentRate);
    previewingRuntimeId.store(0, std::memory_order_relaxed);
    handledPreviewSerial = previewRequestSerial.load(std::memory_order_relaxed);
    hostGrid.reset();
    scrambleProcessor.prepare(currentRate);
    meltProcessor.prepare(currentRate);
    setLatencySamples(randomchop::SpectralDrawProcessor::latencySamples);
    spectralDrawProcessor.prepare(currentRate);
    smearProcessor.prepare(currentRate);
    outputGain.reset(currentRate, 0.010);
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue(IDs::output)->load()));
    muteGain.reset(currentRate, 0.010);
    muteGain.setCurrentAndTargetValue(outputMuted.load(std::memory_order_relaxed) ? 0.0f : 1.0f);
    lastGridBoundaries = {};
    const auto seed = internalSeed.load(std::memory_order_relaxed);
    random.setSeed(seed);
    scrambleProcessor.setSeed(seed);
    meltProcessor.setSeed(seed);
    smearProcessor.setSeed(seed);
    lastSeed = seed;
}

void RandomChopSamplerAudioProcessor::requestSourcePreview(uint64_t runtimeId) noexcept
{
    previewRequestId.store(runtimeId, std::memory_order_relaxed);
    previewRequestSerial.fetch_add(1, std::memory_order_release);
}

void RandomChopSamplerAudioProcessor::regenerateCreativeSeed()
{
    const auto generated = static_cast<uint64_t>(
        juce::Random::getSystemRandom().nextInt64()) & 0x7fffffffffffffffULL;
    internalSeed.store(generated != 0 ? generated : 1, std::memory_order_relaxed);
}

bool RandomChopSamplerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void RandomChopSamplerAudioProcessor::noteOn(int note, float velocity) noexcept
{
    const auto pool = samples.getSnapshot();
    const auto selected = randomchop::chooseSource(*pool, random);
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
    const auto start = randomchop::resolveRandomStart(
        region, prepared->sampleRate, 1.0, random.unit());
    const auto targetKey = static_cast<int>(parameters.getRawParameterValue(IDs::targetKey)->load());
    const auto chords = parameters.getRawParameterValue(IDs::midiPitch)->load() >= 0.5f;
    const auto rootNote = randomchop::chordRootMidiNote(targetKey);
    const auto pitchSemitones = randomchop::totalPitchSemitones(
        source->settings.sourceKey, targetKey, source->settings.transposeSemitones,
        source->settings.fineTuneCents, chords, note, rootNote);
    const auto pitchRatio = randomchop::pitchRatioForSemitones(pitchSemitones);
    const auto mode = parameters.getRawParameterValue(IDs::voiceMode)->load() >= 0.5f
        ? randomchop::VoiceMode::mono : randomchop::VoiceMode::poly;
    auto& voice = voices.acquire(mode);
    voice.start(prepared, note, velocity, start, region, pitchRatio,
                juce::Decibels::decibelsToGain(source->settings.gainDb),
                internalAttackSeconds, internalReleaseSeconds, ++voiceCounter, 0.0f);
    lastTriggeredRuntimeId.store(source->runtimeId, std::memory_order_relaxed);
    triggeredWhileEmpty.store(false, std::memory_order_relaxed);
}

void RandomChopSamplerAudioProcessor::noteOff(int note) noexcept
{
    voices.noteOff(note, internalReleaseSeconds);
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
        meltProcessor.setSeed(seed);
        smearProcessor.setSeed(seed);
        lastSeed = seed;
    }

    const auto previewSerial = previewRequestSerial.load(std::memory_order_acquire);
    if (previewSerial != handledPreviewSerial)
    {
        handledPreviewSerial = previewSerial;
        previewVoice.release(0.005f);
        previewingRuntimeId.store(0, std::memory_order_relaxed);
        const auto requestedId = previewRequestId.load(std::memory_order_relaxed);
        if (requestedId != 0)
        {
            const auto pool = samples.getSnapshot();
            for (const auto& source : *pool)
            {
                if (source->runtimeId != requestedId || source->prepared == nullptr
                    || source->prepared->audio == nullptr)
                    continue;
                const auto region = randomchop::makeFrameRegion(
                    source->prepared->audio->getNumSamples(),
                    source->settings.startNormalised, source->settings.endNormalised);
                if (region.canInterpolate())
                {
                    previewVoice.start(source->prepared, 60, 1.0f,
                        static_cast<double>(region.firstFrame), region,
                        1.0, juce::Decibels::decibelsToGain(source->settings.gainDb),
                        internalAttackSeconds, internalReleaseSeconds, ++voiceCounter, 0.0f);
                    previewingRuntimeId.store(requestedId, std::memory_order_relaxed);
                }
                break;
            }
        }
    }

    const auto hostTiming = readHostTiming();
    const auto timingBpm = hostTiming.hasBpm ? hostTiming.bpm : lastGridBoundaries.bpm;
    const auto gridChoice = randomchop::HostGrid::automaticDivisionChoice(timingBpm);
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
    previewVoice.render(buffer, 0, buffer.getNumSamples());
    if (!previewVoice.isActive())
        previewingRuntimeId.store(0, std::memory_order_relaxed);

    scrambleProcessor.process(buffer, lastGridBoundaries, gridChoice,
        { isEffectEnabled(0) ? parameters.getRawParameterValue(IDs::scrambleAmount)->load() : 0.0f,
          scrambleFeatures.load(std::memory_order_relaxed) });
    const auto scrambleFlags = scrambleProcessor.getActiveSliceMask()
        | (scrambleProcessor.isActive() ? uint32_t { 1 } << 8 : 0)
        | (scrambleProcessor.isArmed() ? uint32_t { 1 } << 9 : 0);
    scrambleVisualFlags.store(scrambleFlags, std::memory_order_relaxed);
    scrambleVisualPhase.store(scrambleProcessor.getEventProgress(),
                              std::memory_order_relaxed);
    meltProcessor.process(buffer, lastGridBoundaries, gridChoice,
        { isEffectEnabled(1) ? parameters.getRawParameterValue(IDs::meltAmount)->load() : 0.0f,
          meltFeatures.load(std::memory_order_relaxed) });
    const auto meltStretch = std::clamp(
        (meltProcessor.getLastStretchRatio() - 1.0f) / 3.0f, 0.0f, 1.0f);
    const auto meltFlags = meltProcessor.getActiveReverseMask()
        | (meltProcessor.isActive() ? uint32_t { 1 } << 8 : 0)
        | (meltProcessor.isArmed() ? uint32_t { 1 } << 9 : 0);
    meltVisualStretch.store(meltStretch, std::memory_order_relaxed);
    meltVisualProgress.store(meltProcessor.getEventProgress(),
                             std::memory_order_relaxed);
    meltVisualFlags.store(meltFlags, std::memory_order_relaxed);
    spectralDrawProcessor.process(buffer, spectralMaskStore,
        { isEffectEnabled(3) ? parameters.getRawParameterValue(IDs::spectralDepth)->load() : 0.0f,
          randomchop::SpectralDrawProcessor::automaticCycleChoice(lastGridBoundaries.bpm),
          lastGridBoundaries.bpm,
          hostTiming.ppq,
          lastGridBoundaries.usedHostClock && hostTiming.hasPpq,
          lastGridBoundaries.transportDiscontinuity });
    smearProcessor.process(buffer,
        { isEffectEnabled(2) ? parameters.getRawParameterValue(IDs::smearAmount)->load() : 0.0f,
          smearFeatures.load(std::memory_order_relaxed) });
    smearVisualActivity.store(static_cast<float>(smearProcessor.getActiveGrainCount())
                                  / static_cast<float>(
                                      randomchop::SmearProcessor::maximumGrains),
                              std::memory_order_relaxed);
    smearVisualGain.store(smearProcessor.getLastOverlapGain() / 1.10f,
                          std::memory_order_relaxed);
    outputGain.setTargetValue(juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue(IDs::output)->load()));
    muteGain.setTargetValue(outputMuted.load(std::memory_order_relaxed) ? 0.0f : 1.0f);
    float leftPeak = 0.0f;
    float rightPeak = 0.0f;
    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        const auto gain = outputGain.getNextValue() * muteGain.getNextValue();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto sample = buffer.getSample(channel, frame) * gain;
            buffer.setSample(channel, frame, sample);
            if (channel == 0)
                leftPeak = juce::jmax(leftPeak, std::abs(sample));
            else if (channel == 1)
                rightPeak = juce::jmax(rightPeak, std::abs(sample));
        }
    }
    if (buffer.getNumChannels() < 2)
        rightPeak = leftPeak;
    const auto limitedLeft = juce::jlimit(0.0f, 1.0f, leftPeak);
    const auto limitedRight = juce::jlimit(0.0f, 1.0f, rightPeak);
    outputPeakLeft.store(limitedLeft, std::memory_order_relaxed);
    outputPeakRight.store(limitedRight, std::memory_order_relaxed);
    outputPeak.store(juce::jmax(limitedLeft, limitedRight), std::memory_order_relaxed);
    activeVoiceCount.store(static_cast<int>(voices.activeCount()),
                           std::memory_order_relaxed);
}

void RandomChopSamplerAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    auto state = parameters.copyState();
    state.setProperty("stateVersion", randomchop::currentStateVersion, nullptr);
    state.setProperty("creativeSeed",
        juce::String(static_cast<int64_t>(
            internalSeed.load(std::memory_order_relaxed))), nullptr);
    state.setProperty("spectralCanvas", spectralMaskStore.encodeCanvas(), nullptr);
    state.setProperty("outputMuted", outputMuted.load(std::memory_order_relaxed), nullptr);
    state.setProperty("selectedSampleId", getSelectedSampleId(), nullptr);
    state.setProperty("scrambleFeatures",
        static_cast<int>(scrambleFeatures.load(std::memory_order_relaxed)), nullptr);
    state.setProperty("meltFeatures",
        static_cast<int>(meltFeatures.load(std::memory_order_relaxed)), nullptr);
    state.setProperty("smearFeatures",
        static_cast<int>(smearFeatures.load(std::memory_order_relaxed)), nullptr);
    for (int effect = 0; effect < 4; ++effect)
        state.setProperty(juce::Identifier("effectEnabled" + juce::String(effect)),
                          isEffectEnabled(effect), nullptr);
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
        outputMuted.store(static_cast<bool>(state.getProperty("outputMuted", false)),
                          std::memory_order_relaxed);
        setSelectedSampleId(state.getProperty("selectedSampleId").toString());
        setScrambleFeatures(static_cast<uint32_t>(static_cast<int>(
            state.getProperty("scrambleFeatures", static_cast<int>(randomchop::ScrambleFeatures::all)))));
        setMeltFeatures(static_cast<uint32_t>(static_cast<int>(
            state.getProperty("meltFeatures", static_cast<int>(randomchop::MeltFeatures::all)))));
        setSmearFeatures(static_cast<uint32_t>(static_cast<int>(
            state.getProperty("smearFeatures", static_cast<int>(randomchop::SmearFeatures::all)))));
        for (int effect = 0; effect < 4; ++effect)
            setEffectEnabled(effect, static_cast<bool>(state.getProperty(
                juce::Identifier("effectEnabled" + juce::String(effect)), true)));
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

        }
        if (files.isValid())
            state.removeChild(files, nullptr);
        state.removeProperty("spectralCanvas", nullptr);
        state.removeProperty("outputMuted", nullptr);
        state.removeProperty("selectedSampleId", nullptr);
        state.removeProperty("scrambleFeatures", nullptr);
        state.removeProperty("meltFeatures", nullptr);
        state.removeProperty("smearFeatures", nullptr);
        for (int effect = 0; effect < 4; ++effect)
            state.removeProperty(juce::Identifier("effectEnabled" + juce::String(effect)), nullptr);
        state.removeProperty("creativeSeed", nullptr);
        randomchop::removeLegacyState(state);
        const auto ensureParameter = [&state](const char* id, float value)
        {
            if (!state.hasProperty(id))
                state.setProperty(id, value, nullptr);
        };
        ensureParameter(IDs::scrambleAmount, 0.0f);
        ensureParameter(IDs::meltAmount, 0.0f);
        ensureParameter(IDs::spectralDepth, 0.0f);
        ensureParameter(IDs::smearAmount, 0.0f);
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

juce::String RandomChopSamplerAudioProcessor::getSelectedSampleId() const
{
    const juce::ScopedLock lock(editorStateLock);
    return selectedSampleId;
}

void RandomChopSamplerAudioProcessor::setSelectedSampleId(const juce::String& id)
{
    const juce::ScopedLock lock(editorStateLock);
    selectedSampleId = id;
}

juce::AudioProcessorEditor* RandomChopSamplerAudioProcessor::createEditor()
{
#if RECOMPILER_USE_NATIVE_EDITOR
    return new RandomChopSamplerAudioProcessorEditor(*this);
#else
    return new RandomChopSamplerWebViewEditor(*this);
#endif
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RandomChopSamplerAudioProcessor();
}
