#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WebViewEditor.h"
#include "OutputGain.h"
#include <algorithm>
#include <cmath>

namespace IDs
{
constexpr auto output = "output";
constexpr auto midiPitch = "midiPitch";
constexpr auto globalPitch = "globalPitch";
constexpr auto voiceMode = "voiceMode";
constexpr auto faultPressure = "faultPressure";
constexpr auto spectralDepth = "spectralDepth";
constexpr auto smearAmount = "smearAmount";
constexpr auto bleedMix = "bleedMix";
constexpr auto bleedGrainSize = "bleedGrainSize";
constexpr auto bleedShape = "bleedShape";
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
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::output, "Vol",
        juce::NormalisableRange<float>(0.0f, 125.0f, 0.1f), 100.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterBool>(IDs::midiPitch, "Stack", false));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        IDs::globalPitch, "Global Pitch", -12, 12, 0, "st"));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        IDs::voiceMode, "Voice Mode", juce::StringArray { "POLY", "MONO" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::faultPressure, "Fault Pressure",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::spectralDepth, "Etch Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::smearAmount, "Bleed Pressure",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::bleedMix, "Bleed Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f, "%"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::bleedGrainSize,
        "Legacy Bleed Grain Size (ignored)",
        juce::NormalisableRange<float>(8.0f, 120.0f, 0.1f), 40.0f, "ms"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(IDs::bleedShape, "Bleed Shape",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f, "%"));
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
    faultGrid.reset();
    faultProcessor.prepare(currentRate);
    setLatencySamples(randomchop::SpectralDrawProcessor::latencySamples);
    spectralDrawProcessor.prepare(currentRate);
    smearProcessor.prepare(currentRate);
    outputGain.reset(currentRate, 0.010);
    outputGain.setCurrentAndTargetValue(randomchop::outputPercentToGain(
        parameters.getRawParameterValue(IDs::output)->load()));
    lastGridBoundaries = {};
    const auto seed = internalSeed.load(std::memory_order_relaxed);
    random.setSeed(seed);
    faultProcessor.setSeed(seed);
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
    const auto chords = parameters.getRawParameterValue(IDs::midiPitch)->load() >= 0.5f;
    const auto globalPitch = static_cast<int>(
        parameters.getRawParameterValue(IDs::globalPitch)->load());
    const auto pitchSemitones = randomchop::playbackPitchSemitones(
        source->settings.transposeSemitones, source->settings.fineTuneCents,
        globalPitch, chords, note);
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
        faultProcessor.setSeed(seed);
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
    const auto faultBoundaries = faultGrid.process(
        currentRate, buffer.getNumSamples(), 1, hostTiming);

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

    faultProcessor.process(buffer, faultBoundaries,
        { isEffectEnabled(0) ? parameters.getRawParameterValue(IDs::faultPressure)->load() : 0.0f,
          faultMutations.load(std::memory_order_relaxed) });
    faultMutation.store(static_cast<int>(faultProcessor.getCurrentMutation()),
                        std::memory_order_relaxed);
    faultDivision.store(faultProcessor.getCurrentDivisionDenominator(),
                        std::memory_order_relaxed);
    faultProgress.store(faultProcessor.getSegmentProgress(), std::memory_order_relaxed);
    faultResampleSemitones.store(faultProcessor.getLastResampleSemitones(),
                                 std::memory_order_relaxed);
    spectralDrawProcessor.process(buffer, spectralMaskStore,
        { isEffectEnabled(2) ? parameters.getRawParameterValue(IDs::spectralDepth)->load() : 0.0f,
          randomchop::SpectralDrawProcessor::automaticCycleChoice(lastGridBoundaries.bpm),
          lastGridBoundaries.bpm,
          hostTiming.ppq,
          lastGridBoundaries.usedHostClock && hostTiming.hasPpq,
          lastGridBoundaries.transportDiscontinuity });
    smearProcessor.process(buffer,
        { isEffectEnabled(1) ? parameters.getRawParameterValue(IDs::smearAmount)->load() : 0.0f,
          smearFeatures.load(std::memory_order_relaxed),
          parameters.getRawParameterValue(IDs::bleedMix)->load(),
          parameters.getRawParameterValue(IDs::bleedShape)->load() });
    smearVisualActivity.store(static_cast<float>(smearProcessor.getActiveGrainCount())
                                  / static_cast<float>(
                                      randomchop::SmearProcessor::maximumGrains),
                              std::memory_order_relaxed);
    smearVisualGain.store(smearProcessor.getLastOverlapGain() / 1.10f,
                          std::memory_order_relaxed);
    outputGain.setTargetValue(randomchop::outputPercentToGain(
        parameters.getRawParameterValue(IDs::output)->load()));
    float leftPeak = 0.0f;
    float rightPeak = 0.0f;
    double outputSquaredSum = 0.0;
    uint64_t outputSampleCount = 0;
    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
    {
        const auto gain = outputGain.getNextValue();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto sample = buffer.getSample(channel, frame) * gain;
            buffer.setSample(channel, frame, sample);
            outputSquaredSum += static_cast<double>(sample) * static_cast<double>(sample);
            ++outputSampleCount;
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
    const auto outputRms = outputSampleCount > 0
        ? static_cast<float>(std::sqrt(outputSquaredSum
            / static_cast<double>(outputSampleCount))) : 0.0f;
    visualAudioLevel.store(std::clamp(std::isfinite(outputRms) ? outputRms * 5.0f : 0.0f,
                                      0.0f, 1.0f),
                           std::memory_order_relaxed);
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
    state.setProperty("uiScale", uiScaleIndex.load(std::memory_order_relaxed), nullptr);
    state.setProperty("selectedSampleId", getSelectedSampleId(), nullptr);
    state.setProperty("faultMutations",
        static_cast<int>(faultMutations.load(std::memory_order_relaxed)), nullptr);
    state.setProperty("smearFeatures",
        static_cast<int>(smearFeatures.load(std::memory_order_relaxed)), nullptr);
    for (int effect = 0; effect < 3; ++effect)
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
        const auto restoredVersion = static_cast<int>(state.getProperty("stateVersion", 0));
        if (restoredVersion < 14 && files.isValid())
            for (auto source : files)
                source.removeProperty("stretch", nullptr);
        setUiScaleIndex(static_cast<int>(state.getProperty("uiScale", 1)));
        setSelectedSampleId(state.getProperty("selectedSampleId").toString());
        setFaultMutations(static_cast<uint32_t>(static_cast<int>(
            state.getProperty("faultMutations", static_cast<int>(randomchop::FaultMutations::all)))));
        setSmearFeatures(static_cast<uint32_t>(static_cast<int>(
            state.getProperty("smearFeatures", static_cast<int>(randomchop::SmearFeatures::all)))));
        if (restoredVersion < 13)
        {
            setEffectEnabled(0, true);
            setEffectEnabled(1, static_cast<bool>(state.getProperty("effectEnabled2", true)));
            setEffectEnabled(2, static_cast<bool>(state.getProperty("effectEnabled3", true)));
        }
        else
        {
            for (int effect = 0; effect < 3; ++effect)
                setEffectEnabled(effect, static_cast<bool>(state.getProperty(
                    juce::Identifier("effectEnabled" + juce::String(effect)), true)));
        }
        randomchop::migrateOutputToPercent(state, restoredVersion);
        const auto restoredSeedText = state.getProperty("creativeSeed").toString();
        const auto legacySeed = static_cast<int64_t>(state.getProperty("seed", 0));
        auto restoredSeed = static_cast<uint64_t>(restoredSeedText.getLargeIntValue());
        if (restoredSeed == 0 && legacySeed > 0)
            restoredSeed = static_cast<uint64_t>(legacySeed);
        if (restoredSeed != 0)
            internalSeed.store(restoredSeed, std::memory_order_relaxed);
        if (files.isValid())
            state.removeChild(files, nullptr);
        state.removeProperty("spectralCanvas", nullptr);
        state.removeProperty("outputMuted", nullptr);
        state.removeProperty("uiScale", nullptr);
        state.removeProperty("selectedSampleId", nullptr);
        state.removeProperty("faultMutations", nullptr);
        state.removeProperty("scrambleFeatures", nullptr);
        state.removeProperty("meltFeatures", nullptr);
        state.removeProperty("smearFeatures", nullptr);
        for (int effect = 0; effect < 4; ++effect)
            state.removeProperty(juce::Identifier("effectEnabled" + juce::String(effect)), nullptr);
        state.removeProperty("creativeSeed", nullptr);
        randomchop::removeLegacyState(state);
        const auto ensureParameter = [&state](const char* id, float value)
        {
            if (state.hasProperty(id)) return;
            for (auto child : state)
                if (child.getProperty("id").toString() == id) return;
            juce::ValueTree parameter("PARAM");
            parameter.setProperty("id", id, nullptr);
            parameter.setProperty("value", value, nullptr);
            state.addChild(parameter, -1, nullptr);
        };
        ensureParameter(IDs::faultPressure, 0.0f);
        ensureParameter(IDs::spectralDepth, 0.0f);
        ensureParameter(IDs::smearAmount, 0.0f);
        ensureParameter(IDs::bleedMix, 50.0f);
        ensureParameter(IDs::bleedGrainSize, 40.0f);
        ensureParameter(IDs::bleedShape, 0.0f);
        ensureParameter(IDs::globalPitch, 0.0f);
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
