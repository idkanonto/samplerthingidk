#include <JuceHeader.h>
#include "HarmonicPitch.h"
#include "MasterDigitalProcessor.h"
#include "RandomSamplerVoice.h"
#include "SampleManager.h"
#include "SourceSelection.h"
#include "StepMask.h"
#include "TakeHistory.h"
#include "VoicePool.h"
#include <atomic>
#include <iostream>
#include <limits>

namespace
{
int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

SampleManager::SamplePtr makeSource(float weight, bool enabled = true, bool missing = false)
{
    auto buffer = std::make_shared<juce::AudioBuffer<float>>(1, 8);
    buffer->clear();
    auto source = std::make_shared<SampleData>();
    source->settings.id = juce::Uuid().toString();
    source->settings.enabled = enabled;
    source->settings.missing = missing;
    source->settings.selectionWeight = weight;
    source->audio = std::move(buffer);
    source->prepared = randomchop::prepareStretch(source->audio, source->sampleRate,
                                                  1.0f, 0);
    return source;
}

juce::File makeTinyWaveFile()
{
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto file = directory.getNonexistentChildFile("random-chop-phase1", ".wav", false);
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    juce::WavAudioFormat format;
    const auto options = juce::AudioFormatWriterOptions {}
        .withSampleRate(44100.0)
        .withNumChannels(1)
        .withBitsPerSample(16);
    auto writer = format.createWriterFor(stream, options);
    if (writer == nullptr)
        return {};

    juce::AudioBuffer<float> audio(1, 32);
    for (int i = 0; i < audio.getNumSamples(); ++i)
        audio.setSample(0, i, 0.25f * std::sin(juce::MathConstants<float>::twoPi
                                               * static_cast<float>(i) / 16.0f));
    if (!writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()))
        return {};
    writer.reset();
    return file;
}

juce::File makeConstantWaveFile(float value)
{
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto stem = value >= 0.0f ? "random-chop-positive" : "random-chop-negative";
    const auto file = directory.getNonexistentChildFile(stem, ".wav", false);
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    juce::WavAudioFormat format;
    const auto options = juce::AudioFormatWriterOptions {}
        .withSampleRate(44100.0)
        .withNumChannels(1)
        .withBitsPerSample(16);
    auto writer = format.createWriterFor(stream, options);
    if (writer == nullptr)
        return {};

    juce::AudioBuffer<float> audio(1, 32);
    audio.clear();
    for (int frame = 0; frame < audio.getNumSamples(); ++frame)
        audio.setSample(0, frame, value);
    if (!writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()))
        return {};
    writer.reset();
    return file;
}

void testWeightedSelection()
{
    SampleManager::Pool pool;
    pool.push_back(makeSource(1.0f));
    pool.push_back(makeSource(9.0f));
    pool.push_back(makeSource(100.0f, false));
    pool.push_back(makeSource(100.0f, true, true));

    RandomizationEngine random;
    random.setSeed(12345);
    int counts[4] {};
    for (int i = 0; i < 10000; ++i)
    {
        const auto selected = randomchop::chooseWeightedSource(pool, random);
        check(selected >= 0 && selected < 4, "weighted selection returned an invalid index");
        if (selected >= 0 && selected < 4)
            ++counts[selected];
    }

    check(counts[1] > counts[0] * 6, "higher weight did not substantially increase selection");
    check(counts[2] == 0, "disabled source was selected");
    check(counts[3] == 0, "missing source was selected");

    pool[0] = makeSource(1.0f, false);
    pool[1] = makeSource(1.0f, false);
    check(randomchop::chooseWeightedSource(pool, random) == -1,
          "an unplayable pool should return no source");
}

void testStepMaskSequencingAndState()
{
    for (const auto length : randomchop::StepMask::allowedLengths)
    {
        randomchop::StepMask mask;
        mask.setMask(0xaaaa);
        for (int event = 0; event < length; ++event)
        {
            const auto step = mask.consume(length);
            check(step.index == event && step.length == length,
                  "Step Mask did not advance exactly once per Note On");
            check(step.chanceAllowed == ((event & 1) != 0),
                  "Step Mask NORMAL/FX decision did not match its stored bit");
            check(step.cycleCompleted == (event == length - 1),
                  "Step Mask cycle boundary was reported on the wrong event");
        }
        check(mask.consume(length).index == 0,
              "Step Mask did not restart at the beginning of a completed cycle");
    }

    randomchop::StepMask chordMask;
    const auto chordFirst = chordMask.consume(8);
    const auto chordSecond = chordMask.consume(8);
    const auto chordThird = chordMask.consume(8);
    check(chordFirst.index == 0 && chordSecond.index == 1 && chordThird.index == 2,
          "simultaneous Note On events did not consume deterministic consecutive steps");

    chordMask.consume(8);
    const auto changedLength = chordMask.consume(4);
    check(changedLength.index == 0 && changedLength.sequenceReset,
          "changing Step Mask length did not reset the event cursor");
    chordMask.consume(4);
    chordMask.requestSequenceReset();
    const auto explicitlyReset = chordMask.consume(4);
    check(explicitlyReset.index == 0 && explicitlyReset.sequenceReset,
          "a Step Mask length edit returning to the same value did not reset the cursor");
    check(randomchop::StepMask::lengthFromChoice(0) == 2
              && randomchop::StepMask::lengthFromChoice(1) == 4
              && randomchop::StepMask::lengthFromChoice(2) == 8
              && randomchop::StepMask::lengthFromChoice(3) == 16
              && randomchop::StepMask::lengthFromChoice(99) == 8,
          "Step Mask length choices or default length changed");

    randomchop::StepMask editable;
    check(editable.getMask() == 0xffff,
          "Step Mask did not default every step to FX");
    editable.setAll(false);
    editable.setStep(3, true);
    check(editable.getMask() == 0x0008 && editable.isFxStep(3)
              && !editable.isFxStep(2),
          "Step Mask individual or All NORMAL controls were incorrect");
    editable.setAll(true);
    check(editable.getMask() == 0xffff,
          "Step Mask All FX control did not enable every step");

    editable.setMask(0x5aa5);
    const auto saved = editable.createState();
    randomchop::StepMask restored;
    restored.setAll(false);
    restored.restoreState(saved);
    check(restored.getMask() == 0x5aa5,
          "Step Mask bits were not persisted and restored exactly");
    restored.restoreState({});
    check(restored.getMask() == 0xffff,
          "a legacy state without Step Mask data did not use the all-FX default");

    randomchop::StepMask normalMask;
    normalMask.setAll(false);
    RandomizationEngine normalRandom;
    RandomizationEngine untouchedRandom;
    normalRandom.setSeed(7301);
    untouchedRandom.setSeed(7301);
    randomchop::ChanceSettings certainChance;
    certainChance.reverseChance = 100.0f;
    randomchop::EventDecision normalDecision;
    const auto normalStep = normalMask.consume(8);
    if (normalStep.chanceAllowed)
        normalDecision = randomchop::resolveChanceEvent(
            certainChance, { 0, 999 }, 200.0, 1000.0, 1000.0, normalRandom);
    check(normalDecision == randomchop::EventDecision {}
              && normalRandom.next() == untouchedRandom.next(),
          "a NORMAL step did not bypass Chance and preserve the random sequence");

    normalMask.setStep(1, true);
    const auto fxStep = normalMask.consume(8);
    const auto fxDecision = fxStep.chanceAllowed
        ? randomchop::resolveChanceEvent(
            certainChance, { 0, 999 }, 200.0, 1000.0, 1000.0, normalRandom)
        : randomchop::EventDecision {};
    check(fxStep.index == 1 && fxDecision.reverseEnabled,
          "an FX step did not allow Chance processing");
}

void testTakeHistoryCaptureReplayAndLifetime()
{
    const auto source = makeSource(1.0f);
    auto makeEvent = [&source](double marker)
    {
        randomchop::TakeEvent event;
        event.sourceId = randomchop::copyStableSourceId(source->settings.id);
        event.prepared = source->prepared;
        event.region = { 0, 7 };
        event.randomStart = marker;
        event.sourceRuntimeId = static_cast<std::uint64_t>(1000.0 + marker);
        event.sourceKey = 3;
        event.transposeSemitones = -5;
        event.fineTuneCents = 27.0f;
        event.sourceGain = 0.75f;
        event.chanceAllowed = true;
        event.decision.reverseEnabled = true;
        event.decision.skipEnabled = true;
        event.decision.skipJumpFrames = -1.25;
        event.decision.reorderEnabled = true;
        event.decision.reorderPermutation = { 2, 0, 3, 1 };
        event.decision.reorderPieceSpanFrames = 0.5;
        event.decision.bendEnabled = true;
        event.decision.bendDepthSemitones = -4.5f;
        event.decision.dropEnabled = true;
        event.decision.dropMask = 0x52;
        return event;
    };

    randomchop::TakeHistory boundaryHistory;
    randomchop::StepMask boundaryMask;
    for (int index = 0; index < 4; ++index)
    {
        const auto step = boundaryMask.consume(4);
        const auto completed = boundaryHistory.recordLiveEvent(
            makeEvent(index), step.length, step.cycleCompleted);
        check(completed == (index == 3),
              "Take completion did not follow the current Step Mask boundary");
        check(boundaryHistory.getTakeCount() == (index == 3 ? 1 : 0),
              "an incomplete Step Mask pass was exposed as a Take");
    }

    randomchop::TakeHistory resetHistory;
    randomchop::StepMask resetMask;
    for (int index = 0; index < 3; ++index)
    {
        const auto step = resetMask.consume(8);
        resetHistory.recordLiveEvent(makeEvent(index), step.length, step.cycleCompleted);
    }
    const auto resetStep = resetMask.consume(4);
    check(resetStep.sequenceReset && resetStep.index == 0,
          "Take reset fixture did not observe its Step Mask length change");
    resetHistory.discardIncompleteLiveTake();
    resetHistory.recordLiveEvent(makeEvent(40.0), 4, false);
    resetHistory.recordLiveEvent(makeEvent(41.0), 4, false);
    resetHistory.recordLiveEvent(makeEvent(42.0), 4, false);
    resetHistory.recordLiveEvent(makeEvent(43.0), 4, true);
    resetHistory.requestHistory(0);
    const auto resetReplay = resetHistory.beginTrigger();
    check(resetHistory.getTakeCount() == 1 && resetReplay.replayEvent != nullptr
              && resetReplay.replayEvent->randomStart == 40.0,
          "changing Step Mask length retained an incomplete LIVE pass");

    randomchop::TakeHistory replayHistory;
    const auto first = makeEvent(1.25);
    auto second = makeEvent(5.5);
    second.chanceAllowed = false;
    second.decision = {};
    check(!replayHistory.recordLiveEvent(first, 2, false)
              && replayHistory.recordLiveEvent(second, 2, true),
          "a complete two-event Take was not captured");
    replayHistory.requestHistory(0);
    const auto replay1 = replayHistory.beginTrigger();
    const auto replay2 = replayHistory.beginTrigger();
    const auto replay3 = replayHistory.beginTrigger();
    check(!replay1.isLive && !replay2.isLive && !replay3.isLive
              && replay1.replayEvent != nullptr && replay2.replayEvent != nullptr
              && replay3.replayEvent != nullptr,
          "HISTORY did not supply stored Take events");
    if (replay1.replayEvent != nullptr && replay2.replayEvent != nullptr
        && replay3.replayEvent != nullptr)
    {
        check(replay1.replayEvent->randomStart == first.randomStart
                  && replay2.replayEvent->randomStart == second.randomStart
                  && replay3.replayEvent->randomStart == first.randomStart,
              "HISTORY did not cycle its selected Take indefinitely");
        check(replay1.replayEvent->sourceId == first.sourceId
                  && replay1.replayEvent->prepared == first.prepared
                  && replay1.replayEvent->region.firstFrame == first.region.firstFrame
                  && replay1.replayEvent->region.lastFrame == first.region.lastFrame
                  && replay1.replayEvent->decision == first.decision
                  && replay2.replayEvent->decision == second.decision
                  && replay2.replayEvent->chanceAllowed == second.chanceAllowed,
              "HISTORY changed an explicit source, start, region, or Chance decision");
    }
    const auto countBeforeIgnoredRecord = replayHistory.getTakeCount();
    check(!replayHistory.recordLiveEvent(makeEvent(99.0), 1, true)
              && replayHistory.getTakeCount() == countBeforeIgnoredRecord,
          "HISTORY created a new Take");
    replayHistory.requestLive();
    const auto returnedLive = replayHistory.beginTrigger();
    check(returnedLive.isLive && returnedLive.resetLiveSequence
              && replayHistory.getSelectedTake() == -1,
          "returning to LIVE did not request a fresh Step Mask pass");

    randomchop::TakeHistory ringHistory;
    for (int take = 0; take < 9; ++take)
    {
        check(!ringHistory.recordLiveEvent(makeEvent(take * 10.0), 2, false),
              "a half-complete ring Take was committed");
        check(ringHistory.recordLiveEvent(makeEvent(take * 10.0 + 1.0), 2, true),
              "a complete ring Take was not committed");
    }
    check(ringHistory.getTakeCount() == randomchop::TakeHistory::maximumTakes,
          "Take History did not retain exactly its latest eight Takes");
    ringHistory.requestHistory(0);
    const auto oldestRemaining = ringHistory.beginTrigger();
    check(oldestRemaining.replayEvent != nullptr
              && oldestRemaining.replayEvent->randomStart == 10.0,
          "Take History did not evict its oldest Take");
    ringHistory.requestHistory(7);
    const auto newestFirst = ringHistory.beginTrigger();
    ringHistory.beginTrigger();
    ringHistory.beginTrigger();
    ringHistory.requestHistory(7);
    const auto newestReset = ringHistory.beginTrigger();
    check(newestFirst.replayEvent != nullptr && newestReset.replayEvent != nullptr
              && newestFirst.replayEvent->randomStart == 80.0
              && newestReset.replayEvent->randomStart == 80.0,
          "selecting a Take did not reset its replay cursor to event one");
    ringHistory.requestClear();
    const auto afterClear = ringHistory.beginTrigger();
    check(afterClear.isLive && afterClear.resetLiveSequence
              && ringHistory.getTakeCount() == 0,
          "session-only Take History was not cleared safely");

    const auto file = makeTinyWaveFile();
    check(file.existsAsFile(), "could not create the Take ownership fixture");
    if (!file.existsAsFile())
        return;
    {
        SampleManager manager;
        juce::StringArray paths;
        paths.add(file.getFullPathName());
        check(manager.addFiles(paths).empty(), "could not load the Take ownership fixture");
        auto snapshot = manager.getSnapshot();
        auto prepared = snapshot->front()->prepared;
        std::weak_ptr<const PreparedSampleData> preparedWeak = prepared;
        randomchop::TakeHistory ownershipHistory;
        randomchop::TakeEvent owned;
        owned.sourceId = randomchop::copyStableSourceId(snapshot->front()->settings.id);
        owned.prepared = prepared;
        owned.region = { 0, prepared->audio->getNumSamples() - 1 };
        ownershipHistory.recordLiveEvent(owned, 2, false);
        ownershipHistory.recordLiveEvent(owned, 2, true);
        owned.prepared.reset();
        prepared.reset();
        snapshot.reset();
        manager.clear();
        manager.collectGarbage();
        check(!preparedWeak.expired(),
              "clearing a source invalidated a PreparedSampleData version held by a Take");

        randomchop::TakeEvent silent;
        for (int take = 0; take < 8; ++take)
        {
            ownershipHistory.recordLiveEvent(silent, 2, false);
            ownershipHistory.recordLiveEvent(silent, 2, true);
        }
        check(!preparedWeak.expired(),
              "Take eviction destroyed retired prepared data on the capture thread");
        manager.collectGarbage();
        check(preparedWeak.expired(),
              "evicted Take prepared data was not reclaimed by non-realtime maintenance");
    }
    check(file.deleteFile(), "could not remove the Take ownership fixture");
}

void testMasterDigitalProcessing()
{
    randomchop::MasterDigitalProcessor processor;
    juce::AudioBuffer<float> off(2, 6);
    const std::array<float, 6> left { -0.75f, -0.2f, 0.0f, 0.125f, 0.5f, 0.9f };
    const std::array<float, 6> right { 0.75f, 0.2f, 0.0f, -0.125f, -0.5f, -0.9f };
    for (int frame = 0; frame < off.getNumSamples(); ++frame)
    {
        off.setSample(0, frame, left[static_cast<std::size_t>(frame)]);
        off.setSample(1, frame, right[static_cast<std::size_t>(frame)]);
    }
    processor.process(off, 0, 1);
    bool unchanged = true;
    for (int frame = 0; frame < off.getNumSamples(); ++frame)
        unchanged = unchanged
            && off.getSample(0, frame) == left[static_cast<std::size_t>(frame)]
            && off.getSample(1, frame) == right[static_cast<std::size_t>(frame)];
    check(unchanged, "OFF Bit Crush and 1x rate reduction changed finite audio");

    processor.reset();
    juce::AudioBuffer<float> crushed(2, 1);
    crushed.setSample(0, 0, 0.2f);
    crushed.setSample(1, 0, -0.2f);
    processor.process(crushed, 4, 1);
    check(std::abs(crushed.getSample(0, 0) - 0.25f) < 0.000001f
              && std::abs(crushed.getSample(1, 0) + 0.25f) < 0.000001f,
          "4-bit crush did not quantise stereo samples deterministically");

    processor.reset();
    juce::AudioBuffer<float> reduced(2, 6);
    for (int frame = 0; frame < reduced.getNumSamples(); ++frame)
    {
        reduced.setSample(0, frame, 0.1f * static_cast<float>(frame + 1));
        reduced.setSample(1, frame, -0.1f * static_cast<float>(frame + 1));
    }
    processor.process(reduced, 0, 4);
    bool heldInStereo = true;
    for (int frame = 0; frame < 4; ++frame)
        heldInStereo = heldInStereo
            && std::abs(reduced.getSample(0, frame) - 0.1f) < 0.000001f
            && std::abs(reduced.getSample(1, frame) + 0.1f) < 0.000001f;
    for (int frame = 4; frame < 6; ++frame)
        heldInStereo = heldInStereo
            && std::abs(reduced.getSample(0, frame) - 0.5f) < 0.000001f
            && std::abs(reduced.getSample(1, frame) + 0.5f) < 0.000001f;
    check(heldInStereo, "4x sample-rate reduction did not hold independent stereo samples");

    processor.reset();
    juce::AudioBuffer<float> ordered(2, 5);
    ordered.clear();
    ordered.setSample(0, 0, 0.2f);
    for (int frame = 1; frame < ordered.getNumSamples(); ++frame)
        ordered.setSample(0, frame, 0.9f);
    processor.process(ordered, 4, 4);
    check(std::abs(ordered.getSample(0, 0) - 0.25f) < 0.000001f
              && std::abs(ordered.getSample(0, 3) - 0.25f) < 0.000001f
              && std::abs(ordered.getSample(0, 4) - 0.875f) < 0.000001f,
          "master processing did not apply Bit Crush before rate reduction");

    processor.reset();
    juce::AudioBuffer<float> firstBlock(2, 3);
    firstBlock.clear();
    firstBlock.setSample(0, 0, 0.1f);
    firstBlock.setSample(0, 1, 0.2f);
    firstBlock.setSample(0, 2, 0.3f);
    processor.process(firstBlock, 0, 4);
    juce::AudioBuffer<float> secondBlock(2, 2);
    secondBlock.clear();
    secondBlock.setSample(0, 0, 0.4f);
    secondBlock.setSample(0, 1, 0.5f);
    processor.process(secondBlock, 0, 4);
    check(std::abs(secondBlock.getSample(0, 0) - 0.1f) < 0.000001f
              && std::abs(secondBlock.getSample(0, 1) - 0.5f) < 0.000001f,
          "sample-rate reduction did not preserve its phase across host blocks");

    check(randomchop::MasterDigitalProcessor::bitDepthFromChoice(0) == 0
              && randomchop::MasterDigitalProcessor::bitDepthFromChoice(1) == 4
              && randomchop::MasterDigitalProcessor::bitDepthFromChoice(21) == 24
              && randomchop::MasterDigitalProcessor::rateFactorFromChoice(0) == 1
              && randomchop::MasterDigitalProcessor::rateFactorFromChoice(6) == 64,
          "master parameter choice mappings changed");

    processor.reset();
    juce::AudioBuffer<float> unsafe(2, 4);
    unsafe.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
    unsafe.setSample(0, 1, std::numeric_limits<float>::infinity());
    unsafe.setSample(0, 2, 1.0e30f);
    unsafe.setSample(0, 3, -1.0e30f);
    unsafe.copyFrom(1, 0, unsafe, 0, 0, unsafe.getNumSamples());
    processor.process(unsafe, 24, 1);
    unsafe.applyGain(2.0f);
    bool finiteAndBounded = true;
    for (int channel = 0; channel < unsafe.getNumChannels(); ++channel)
        for (int frame = 0; frame < unsafe.getNumSamples(); ++frame)
            finiteAndBounded = finiteAndBounded
                && std::isfinite(unsafe.getSample(channel, frame))
                && std::abs(unsafe.getSample(channel, frame)) <= 128.0f;
    check(finiteAndBounded,
          "master processing propagated non-finite or unbounded output");

    randomchop::MasterDigitalProcessor deterministicA;
    randomchop::MasterDigitalProcessor deterministicB;
    juce::AudioBuffer<float> deterministic1(2, 17);
    juce::AudioBuffer<float> deterministic2(2, 17);
    for (int channel = 0; channel < 2; ++channel)
        for (int frame = 0; frame < 17; ++frame)
        {
            const auto value = std::sin(static_cast<float>(frame + channel * 3));
            deterministic1.setSample(channel, frame, value);
            deterministic2.setSample(channel, frame, value);
        }
    deterministicA.process(deterministic1, 9, 8);
    deterministicB.process(deterministic2, 9, 8);
    bool exactlyEqual = true;
    for (int channel = 0; channel < 2; ++channel)
        for (int frame = 0; frame < 17; ++frame)
            exactlyEqual = exactlyEqual
                && deterministic1.getSample(channel, frame)
                    == deterministic2.getSample(channel, frame);
    check(exactlyEqual, "master digital processing was not deterministic");
}

void testPoolLimitAndPersistentSettings()
{
    const auto file = makeTinyWaveFile();
    check(file.existsAsFile(), "could not create the temporary WAV fixture");
    if (!file.existsAsFile())
        return;

    {
        SampleManager manager;
        juce::StringArray paths;
        for (int i = 0; i < SampleManager::maximumSamples + 1; ++i)
            paths.add(file.getFullPathName());

        const auto errors = manager.addFiles(paths);
        check(manager.size() == SampleManager::maximumSamples, "pool did not stop at 20 sources");
        check(errors.size() == 1, "source 21 was not reported as rejected");

        const auto first = manager.getSnapshot()->front();
        const auto id = first->settings.id;
        manager.updateSettings(id, [] (SampleSettings& settings)
        {
            settings.enabled = false;
            settings.startNormalised = 0.2;
            settings.endNormalised = 0.8;
            settings.sourceKey = 10;
            settings.gainDb = -7.5f;
            settings.transposeSemitones = -13;
            settings.fineTuneCents = 37.5f;
            settings.selectionWeight = 3.25f;
        });

        const auto saved = manager.createState();
        manager.clear();
        const auto restoreErrors = manager.restoreState(saved);
        const auto restored = manager.getSnapshot();
        check(restoreErrors.empty(), "valid saved sources failed to restore");
        check(restored->size() == SampleManager::maximumSamples, "restored pool size changed");
        check(restored->front()->settings.id == id, "stable source UUID was not restored");
        check(!restored->front()->settings.enabled, "enabled state was not restored");
        check(std::abs(restored->front()->settings.startNormalised - 0.2) < 0.000001,
              "source Start marker was not restored");
        check(std::abs(restored->front()->settings.endNormalised - 0.8) < 0.000001,
              "source End marker was not restored");
        check(restored->front()->settings.sourceKey == 10,
              "source tonic was not restored");
        check(std::abs(restored->front()->settings.gainDb + 7.5f) < 0.001f,
              "source gain was not restored");
        check(restored->front()->settings.transposeSemitones == -13,
              "source transpose was not restored");
        check(std::abs(restored->front()->settings.fineTuneCents - 37.5f) < 0.001f,
              "source Fine Tune was not restored");
        check(std::abs(restored->front()->settings.selectionWeight - 3.25f) < 0.001f,
              "source weight was not restored");
        check(restored->front()->waveformPeaks != nullptr
                  && !restored->front()->waveformPeaks->empty()
                  && restored->front()->waveformPeaks->size() <= 1024,
              "waveform peaks were not prepared outside rendering");
    }

    check(file.deleteFile(), "could not remove the temporary WAV fixture");
}

void testParameterOnlyStateClearsSamples()
{
    const auto file = makeTinyWaveFile();
    check(file.existsAsFile(), "could not create the parameter-only state fixture");
    if (!file.existsAsFile())
        return;

    {
        SampleManager manager;
        juce::StringArray paths;
        paths.add(file.getFullPathName());
        check(manager.addFiles(paths).empty(), "could not load the parameter-only state fixture");
        check(manager.size() == 1, "parameter-only state fixture was not added");

        const juce::ValueTree parameterOnlyState("PARAMETERS");
        const auto restoreErrors = manager.restoreState(
            parameterOnlyState.getChildWithName("SAMPLES"));
        check(restoreErrors.empty(), "parameter-only state produced sample restore errors");
        check(manager.size() == 0, "parameter-only state retained the previous sample pool");
    }

    check(file.deleteFile(), "could not remove the parameter-only state fixture");
}

void testRegionClampingAndRestore()
{
    const auto extremes = randomchop::clampNormalisedRegion(-2.0, 3.0);
    check(extremes.start == 0.0 && extremes.end == 1.0,
          "marker extremes were not clamped to zero and one");

    const auto reversed = randomchop::clampNormalisedRegion(0.8, 0.2);
    check(reversed.start == 0.8 && reversed.end == 0.8,
          "invalid marker ordering was not collapsed safely");

    const auto nonFinite = randomchop::clampNormalisedRegion(
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity());
    check(nonFinite.start == 0.0 && nonFinite.end == 1.0,
          "non-finite markers were not replaced with safe defaults");

    juce::ValueTree state("SAMPLES");
    juce::ValueTree source("SAMPLE");
    source.setProperty("id", "invalid-region", nullptr);
    source.setProperty("path", "missing-region-fixture.wav", nullptr);
    source.setProperty("start", 0.9, nullptr);
    source.setProperty("end", 0.1, nullptr);
    source.setProperty("sourceKey", 99, nullptr);
    source.setProperty("transpose", -99, nullptr);
    source.setProperty("fineTune", std::numeric_limits<double>::infinity(), nullptr);
    source.setProperty("stretch", 1.75, nullptr);
    state.appendChild(source, nullptr);

    SampleManager manager;
    const auto errors = manager.restoreState(state);
    const auto restored = manager.getSnapshot();
    check(errors.size() == 1 && restored->size() == 1,
          "missing invalid-region source was not represented safely");
    check(std::abs(restored->front()->settings.startNormalised - 0.9) < 0.000001
              && std::abs(restored->front()->settings.endNormalised - 0.9) < 0.000001,
          "invalid restored marker order was not clamped");
    check(restored->front()->settings.sourceKey == 12
              && restored->front()->settings.transposeSemitones == -24
              && restored->front()->settings.fineTuneCents == 0.0f,
          "invalid restored pitch settings were not clamped safely");
    check(std::abs(restored->front()->settings.stretchRatio - 1.75f) < 0.000001f,
          "restored Stretch duration multiplier changed");
    const auto saved = manager.createState();
    check(saved.getNumChildren() == 1
              && std::abs(static_cast<float>(saved.getChild(0).getProperty("stretch"))
                          - 1.75f) < 0.000001f,
          "Stretch duration multiplier was not persisted");
    check(!restored->front()->isPlayable(),
          "missing invalid-region source was considered playable");
}

void testRandomStartAndReadBounds()
{
    const auto region = randomchop::makeFrameRegion(1001, 0.2, 0.8);
    check(region.firstFrame == 200 && region.lastFrame == 800,
          "normalised region did not map to expected frames");
    check(randomchop::resolveRandomStart(region, 1000.0, 0.0, 0.999) == 200.0,
          "Random Start at zero did not begin exactly at Start");

    const auto maximum = randomchop::maximumRandomStart(region, 1000.0);
    check(std::abs(maximum - 797.0) < 0.000001,
          "random-start maximum did not retain the boundary fade margin");
    const auto middle = randomchop::resolveRandomStart(region, 1000.0, 0.5, 1.0);
    check(std::abs(middle - 498.5) < 0.000001,
          "intermediate Random Start did not expand proportionally from Start");
    const auto full = randomchop::resolveRandomStart(region, 1000.0, 1.0, 1.0);
    check(full >= region.firstFrame && full <= maximum,
          "Random Start at 100 percent escaped its legal range");

    check(randomchop::isInterpolationPositionLegal(region, 200.0),
          "first region frame was not interpolation-safe");
    check(randomchop::isInterpolationPositionLegal(region, 799.999),
          "last fractional read inside End was rejected");
    check(!randomchop::isInterpolationPositionLegal(region, 199.999)
              && !randomchop::isInterpolationPositionLegal(region, 800.0),
          "read bounds allowed interpolation outside Start-End");

    const auto twoFrames = randomchop::makeFrameRegion(2, 0.0, 1.0);
    check(twoFrames.canInterpolate()
              && randomchop::isInterpolationPositionLegal(twoFrames, 0.0),
          "two-frame extreme region should have one legal interpolation position");
    check(!randomchop::makeFrameRegion(1, 0.0, 1.0).canInterpolate(),
          "single-frame source should not produce a playable region");
    check(!randomchop::makeFrameRegion(100, 1.0, 0.0).canInterpolate(),
          "collapsed extreme markers should not produce an invalid read");
}

void testVoiceRegionBoundaries()
{
    auto audio = std::make_shared<juce::AudioBuffer<float>>(1, 12);
    for (int frame = 0; frame < audio->getNumSamples(); ++frame)
        audio->setSample(0, frame, frame >= 3 && frame <= 7 ? 0.5f : 100.0f);
    auto mutableSource = std::make_shared<SampleData>();
    mutableSource->audio = audio;
    mutableSource->sampleRate = 1000.0;
    mutableSource->settings.startNormalised = 3.0 / 11.0;
    mutableSource->settings.endNormalised = 7.0 / 11.0;
    mutableSource->prepared = randomchop::prepareStretch(
        mutableSource->audio, mutableSource->sampleRate, 1.0f, 0);
    SampleManager::SamplePtr source = mutableSource;
    const randomchop::FrameRegion region { 3, 7 };

    auto renderDirection = [&](bool reverse)
    {
        RandomSamplerVoice voice;
        voice.prepare(1000.0);
        juce::AudioBuffer<float> output(2, 16);
        output.clear();
        const auto start = reverse ? randomchop::lastInterpolationPosition(region) : 3.0;
        voice.start(source->prepared, 60, 1.0f, start, region, 1.0, reverse,
                    1.0f, 0.0f, 0.1f, 1);
        voice.render(output, 0, output.getNumSamples());

        float maximumMagnitude = 0.0f;
        for (int channel = 0; channel < output.getNumChannels(); ++channel)
            for (int frame = 0; frame < output.getNumSamples(); ++frame)
            {
                const auto value = output.getSample(channel, frame);
                check(std::isfinite(value), "voice produced a non-finite region sample");
                maximumMagnitude = juce::jmax(maximumMagnitude, std::abs(value));
            }
        check(maximumMagnitude > 0.0f && maximumMagnitude < 1.0f,
              "voice read a sentinel sample outside its source region");
        check(std::abs(output.getSample(0, 3)) < 0.000001f,
              "final region sample was not faded to zero before leaving bounds");
        check(!voice.isActive(), "voice did not end naturally at its source-region boundary");
    };

    renderDirection(false);
    renderDirection(true);
}

void testWaveformPeakExtrema()
{
    const float values[] { 0.25f, -0.25f };
    for (const auto expected : values)
    {
        const auto file = makeConstantWaveFile(expected);
        check(file.existsAsFile(), "could not create constant waveform fixture");
        if (!file.existsAsFile())
            continue;

        {
            SampleManager manager;
            juce::StringArray paths;
            paths.add(file.getFullPathName());
            check(manager.addFiles(paths).empty(), "constant waveform fixture did not load");
            const auto snapshot = manager.getSnapshot();
            check(snapshot->size() == 1 && snapshot->front()->waveformPeaks != nullptr,
                  "constant waveform peaks were not prepared");
            if (snapshot->size() == 1 && snapshot->front()->waveformPeaks != nullptr)
                for (const auto& peak : *snapshot->front()->waveformPeaks)
                    check(std::abs(peak.minimum - expected) < 0.01f
                              && std::abs(peak.maximum - expected) < 0.01f,
                          "waveform peak bounds introduced a false zero crossing");
        }

        check(file.deleteFile(), "could not remove constant waveform fixture");
    }
}

void testHarmonicPitch()
{
    constexpr int none = 0;
    constexpr int c = 1;
    constexpr int cSharp = 2;
    constexpr int fSharp = 7;
    constexpr int b = 12;

    check(randomchop::shortestTonicCorrection(c, b) == -1,
          "C to B did not choose the shortest downward correction");
    check(randomchop::shortestTonicCorrection(c, cSharp) == 1,
          "C to C sharp did not choose the shortest upward correction");
    check(randomchop::shortestTonicCorrection(c, fSharp) == 6
              && randomchop::shortestTonicCorrection(fSharp, c) == 6,
          "tritone ties did not resolve consistently upward");
    check(randomchop::shortestTonicCorrection(none, b) == 0
              && randomchop::shortestTonicCorrection(c, none) == 0,
          "NONE tonic did not disable automatic correction");

    const auto midiOff = randomchop::totalPitchSemitones(
        c, b, 12, 50.0f, false, 84, 72);
    check(std::abs(midiOff - 11.5) < 0.000001,
          "MIDI Pitch OFF did not ignore the incoming note");
    const auto midiOn = randomchop::totalPitchSemitones(
        c, b, 12, 50.0f, true, 84, 72);
    check(std::abs(midiOn - 23.5) < 0.000001,
          "combined tonic, transpose, Fine Tune, and MIDI pitch math was incorrect");
    const auto atRoot = randomchop::totalPitchSemitones(
        none, none, 0, 0.0f, true, 72, 72);
    check(atRoot == 0.0, "default C5 root note introduced a pitch offset");

    check(std::abs(randomchop::pitchRatioForSemitones(12.0) - 2.0) < 0.000001
              && std::abs(randomchop::pitchRatioForSemitones(-12.0) - 0.5) < 0.000001,
          "semitone-to-playback-ratio conversion was inaccurate");
    check(randomchop::clampTranspose(99) == 24
              && randomchop::clampFineTune(-150.0f) == -100.0f
              && randomchop::clampFineTune(std::numeric_limits<float>::quiet_NaN()) == 0.0f,
          "pitch setting bounds did not sanitize invalid values");
    check(std::isfinite(randomchop::pitchRatioForSemitones(1.0e300))
              && randomchop::pitchRatioForSemitones(
                     std::numeric_limits<double>::infinity()) == 1.0,
          "invalid pitch input produced a non-finite playback ratio");
}

void testVoicePitchRatio()
{
    auto audio = std::make_shared<juce::AudioBuffer<float>>(1, 128);
    for (int frame = 0; frame < audio->getNumSamples(); ++frame)
        audio->setSample(0, frame, static_cast<float>(frame) / 100.0f);
    auto mutableSource = std::make_shared<SampleData>();
    mutableSource->audio = audio;
    mutableSource->sampleRate = 1000.0;
    SampleManager::SamplePtr source = mutableSource;

    RandomSamplerVoice voice;
    voice.prepare(1000.0);
    juce::AudioBuffer<float> output(2, 4);
    output.clear();
    mutableSource->prepared = randomchop::prepareStretch(
        mutableSource->audio, mutableSource->sampleRate, 1.0f, 0);
    voice.start(source->prepared, 60, 1.0f, 0.0, { 0, 127 }, 2.0, false,
                1.0f, 0.0f, 0.1f, 1);
    voice.render(output, 0, output.getNumSamples());
    check(std::abs(output.getSample(0, 1) - 0.02f) < 0.0001f,
          "voice playback increment did not apply the resolved pitch ratio");
}

PreparedSamplePtr makeVoiceSample(float value, int frames = 1024)
{
    auto audio = std::make_shared<juce::AudioBuffer<float>>(1, frames);
    for (int frame = 0; frame < frames; ++frame)
        audio->setSample(0, frame, value);
    return randomchop::prepareStretch(audio, 1000.0, 1.0f, 0);
}

PreparedSamplePtr makeRampVoiceSample(int frames = 2048)
{
    auto audio = std::make_shared<juce::AudioBuffer<float>>(1, frames);
    for (int frame = 0; frame < frames; ++frame)
        audio->setSample(0, frame, static_cast<float>(frame) / static_cast<float>(frames));
    return randomchop::prepareStretch(audio, 1000.0, 1.0f, 0);
}

void testFinalLengthAndEnvelope()
{
    const auto prepared = makeVoiceSample(1.0f);
    const randomchop::FrameRegion region { 0, 1023 };

    RandomSamplerVoice fullVoice;
    fullVoice.prepare(1000.0);
    fullVoice.start(prepared, 60, 1.0f, 0.0, region, 1.0, false,
                    1.0f, 0.0f, 0.005f, 1, 0.0f);
    juce::AudioBuffer<float> fullOutput(2, 32);
    fullOutput.clear();
    fullVoice.render(fullOutput, 0, fullOutput.getNumSamples());
    check(fullVoice.isActive() && fullOutput.getSample(0, 31) > 0.99f,
          "FULL final length did not preserve natural playback");

    RandomSamplerVoice limitedVoice;
    limitedVoice.prepare(1000.0);
    limitedVoice.start(prepared, 60, 1.0f, 0.0, region, 1.0, false,
                       1.0f, 0.0f, 0.005f, 2, 20.0f);
    juce::AudioBuffer<float> limitedOutput(2, 40);
    limitedOutput.clear();
    limitedVoice.render(limitedOutput, 0, 20);
    check(!limitedVoice.isActive(),
          "forced Final Length did not stop exactly at its sample boundary");
    limitedVoice.render(limitedOutput, 20, 20);
    check(limitedOutput.getSample(0, 14) > 0.99f
              && std::abs(limitedOutput.getSample(0, 16) - 0.75f) < 0.0001f
              && std::abs(limitedOutput.getSample(0, 19)) < 0.000001f
              && std::abs(limitedOutput.getSample(0, 20)) < 0.000001f,
          "Final Length did not place the Release envelope inside its boundary");

    RandomSamplerVoice shapedVoice;
    shapedVoice.prepare(1000.0);
    shapedVoice.start(prepared, 60, 1.0f, 0.0, region, 1.0, false,
                      1.0f, 0.010f, 0.010f, 3, 20.0f);
    juce::AudioBuffer<float> shapedOutput(2, 24);
    shapedOutput.clear();
    shapedVoice.render(shapedOutput, 0, shapedOutput.getNumSamples());
    check(std::abs(shapedOutput.getSample(0, 0) - 0.1f) < 0.0001f
              && std::abs(shapedOutput.getSample(0, 9) - 1.0f) < 0.0001f
              && shapedOutput.getSample(0, 15) < shapedOutput.getSample(0, 11)
              && std::abs(shapedOutput.getSample(0, 19)) < 0.000001f,
          "Attack and Release did not shape the forced-length event cleanly");
}

void testNoteOffRelease()
{
    const auto prepared = makeVoiceSample(1.0f);
    RandomSamplerVoice voice;
    voice.prepare(1000.0);
    voice.start(prepared, 60, 1.0f, 0.0, { 0, 1023 }, 1.0, false,
                1.0f, 0.0f, 0.005f, 1);
    juce::AudioBuffer<float> output(2, 16);
    output.clear();
    voice.render(output, 0, 4);
    voice.release(0.005f);
    voice.render(output, 4, 8);
    check(!voice.isActive(), "Note Off release did not finish the voice");
    check(output.getSample(0, 4) > output.getSample(0, 7)
              && output.getSample(0, 7) > 0.0f
              && std::abs(output.getSample(0, 10)) < 0.000001f,
          "Note Off did not produce a bounded descending Release envelope");
}

void testPolyMonoAndVoiceStealing()
{
    const auto positive = makeVoiceSample(1.0f);
    const auto negative = makeVoiceSample(-1.0f);
    const randomchop::FrameRegion region { 0, 1023 };
    randomchop::VoicePool poly;
    poly.prepare(1000.0);

    for (size_t index = 0; index < randomchop::VoicePool::capacity; ++index)
    {
        auto& voice = poly.acquire(randomchop::VoiceMode::poly);
        voice.start(positive, 60 + static_cast<int>(index), 1.0f, 0.0, region,
                    1.0, false, 1.0f, 0.0f, 0.01f,
                    static_cast<uint64_t>(index + 1));
    }
    check(poly.activeCount() == randomchop::VoicePool::capacity,
          "POLY did not retain a full 16-note chord");

    auto& stolen = poly.acquire(randomchop::VoiceMode::poly);
    check(stolen.getNote() == 60, "voice stealing did not select the oldest voice");
    stolen.start(positive, 100, 1.0f, 0.0, region, 1.0, false,
                 1.0f, 0.0f, 0.01f, 17);
    size_t replacementCount = 0;
    size_t oldestCount = 0;
    for (size_t index = 0; index < randomchop::VoicePool::capacity; ++index)
    {
        replacementCount += poly[index].getNote() == 100 ? 1u : 0u;
        oldestCount += poly[index].getNote() == 60 ? 1u : 0u;
    }
    check(poly.activeCount() == randomchop::VoicePool::capacity
              && replacementCount == 1 && oldestCount == 0,
          "oldest-voice stealing changed the fixed POLY voice count");

    randomchop::VoicePool mono;
    mono.prepare(1000.0);
    auto& first = mono.acquire(randomchop::VoiceMode::mono);
    first.start(positive, 60, 1.0f, 0.0, region, 1.0, false,
                1.0f, 0.0f, 0.01f, 1);
    juce::AudioBuffer<float> monoOutput(2, 16);
    monoOutput.clear();
    mono.render(monoOutput, 0, 5);
    auto& replacement = mono.acquire(randomchop::VoiceMode::mono);
    replacement.start(negative, 62, 1.0f, 0.0, region, 1.0, false,
                      1.0f, 0.0f, 0.01f, 2);
    mono.render(monoOutput, 5, 1);
    check(mono.activeCount() == 1 && mono[0].getNote() == 62,
          "MONO did not replace the previous event");
    check(std::abs(monoOutput.getSample(0, 5)) < 0.05f,
          "MONO replacement bypassed the short voice-steal crossfade");

    randomchop::VoicePool switched;
    switched.prepare(1000.0);
    for (int note : { 60, 64 })
    {
        auto& voice = switched.acquire(randomchop::VoiceMode::poly);
        voice.start(positive, note, 1.0f, 0.0, region, 1.0, false,
                    1.0f, 0.0f, 0.01f, static_cast<uint64_t>(note));
    }
    auto& monoVoice = switched.acquire(randomchop::VoiceMode::mono);
    monoVoice.start(positive, 67, 1.0f, 0.0, region, 1.0, false,
                    1.0f, 0.0f, 0.01f, 100);
    juce::AudioBuffer<float> switchedOutput(2, 8);
    switchedOutput.clear();
    switched.render(switchedOutput, 0, switchedOutput.getNumSamples());
    check(switched.activeCount() == 1 && switched[0].getNote() == 67,
          "entering MONO did not release prior POLY voices cleanly");
}

void testChanceDecisionResolution()
{
    const randomchop::FrameRegion region { 100, 2099 };
    RandomizationEngine noChanceRandom;
    RandomizationEngine untouchedRandom;
    noChanceRandom.setSeed(12345);
    untouchedRandom.setSeed(12345);
    const auto none = randomchop::resolveChanceEvent(
        {}, region, 400.0, 1000.0, 1000.0, noChanceRandom);
    check(none == randomchop::EventDecision {}, "0% Chance unexpectedly enabled an effect");
    check(noChanceRandom.next() == untouchedRandom.next(),
          "0% Chance consumed random values and changed the existing trigger sequence");

    randomchop::ChanceSettings settings;
    settings.reverseChance = 100.0f;
    settings.retriggerChance = 100.0f;
    settings.skipChance = 100.0f;
    settings.reorderChance = 100.0f;
    settings.bendChance = 100.0f;
    settings.dropChance = 100.0f;
    settings.retriggerSizeMilliseconds = 37.0f;
    settings.retriggerCount = 6;
    RandomizationEngine firstRandom;
    RandomizationEngine secondRandom;
    firstRandom.setSeed(987654321);
    secondRandom.setSeed(987654321);
    const auto first = randomchop::resolveChanceEvent(
        settings, region, 400.0, 1000.0, 2000.0, firstRandom);
    const auto second = randomchop::resolveChanceEvent(
        settings, region, 400.0, 1000.0, 2000.0, secondRandom);
    check(first == second, "stored Chance decisions were not deterministic for a fixed seed");
    check(first.reverseEnabled && first.retriggerEnabled && first.skipEnabled
              && first.reorderEnabled && first.bendEnabled && first.dropEnabled,
          "a 100% Chance effect was not enabled");
    check(first.retriggerSizeFrames == 74 && first.retriggerCount == 6,
          "Retrigger did not store the resolved size and count");
    check(randomchop::isInterpolationPositionLegal(
              region, randomchop::resolvedEventStart(region, 400.0, first)),
          "Skip resolved outside the legal source region");
    bool permutationSlots[4] { false, false, false, false };
    for (const auto piece : first.reorderPermutation)
        if (piece < 4)
            permutationSlots[piece] = true;
    check(permutationSlots[0] && permutationSlots[1]
              && permutationSlots[2] && permutationSlots[3]
              && first.reorderPermutation != std::array<std::uint8_t, 4> { 0, 1, 2, 3 },
          "Reorder did not store a non-identity four-piece permutation");
    check(first.reorderPieceSpanFrames > 0.0
              && first.reorderPieceSpanFrames * 4.0 <= 1000.000001,
          "Reorder fragment was empty or exceeded the one-second cap");
    check(std::isfinite(first.bendDepthSemitones)
              && std::abs(first.bendDepthSemitones) >= 0.05f
              && std::abs(first.bendDepthSemitones) <= 12.0f,
          "Bend depth was not explicit and bounded");
    check(first.dropMask != 0, "Drop did not store an eight-slot mute mask");
}

void testChanceVoiceRendering()
{
    const auto ramp = makeRampVoiceSample();
    const auto constant = makeVoiceSample(1.0f, 2048);
    const randomchop::FrameRegion region { 0, 1999 };

    randomchop::EventDecision reverse;
    reverse.reverseEnabled = true;
    RandomSamplerVoice reverseVoice;
    reverseVoice.prepare(1000.0);
    reverseVoice.start(ramp, 60, 1.0f, 100.0, region, 1.0, false,
                       1.0f, 0.0f, 0.01f, 1, 0.0f, reverse);
    juce::AudioBuffer<float> reverseOutput(2, 2);
    reverseOutput.clear();
    reverseVoice.render(reverseOutput, 0, 2);
    check(reverseOutput.getSample(0, 0) > 0.8f
              && reverseOutput.getSample(0, 1) < reverseOutput.getSample(0, 0),
          "Reverse did not mirror the resolved start and read backward");

    randomchop::EventDecision skip;
    skip.skipEnabled = true;
    skip.skipJumpFrames = 400.0;
    RandomSamplerVoice skipVoice;
    skipVoice.prepare(1000.0);
    skipVoice.start(ramp, 60, 1.0f, 100.0, region, 1.0, false,
                    1.0f, 0.0f, 0.01f, 2, 0.0f, skip);
    juce::AudioBuffer<float> skipOutput(2, 1);
    skipOutput.clear();
    skipVoice.render(skipOutput, 0, 1);
    check(std::abs(skipOutput.getSample(0, 0) - 500.0f / 2048.0f) < 0.0001f,
          "Skip did not apply its exact stored read-head jump");

    randomchop::EventDecision retrigger;
    retrigger.retriggerEnabled = true;
    retrigger.retriggerSizeFrames = 2;
    retrigger.retriggerCount = 1;
    RandomSamplerVoice retriggerVoice;
    retriggerVoice.prepare(1000.0);
    retriggerVoice.start(ramp, 60, 1.0f, 0.0, region, 1.0, false,
                         1.0f, 0.0f, 0.01f, 3, 0.0f, retrigger);
    juce::AudioBuffer<float> retriggerOutput(2, 5);
    retriggerOutput.clear();
    retriggerVoice.render(retriggerOutput, 0, 5);
    check(std::abs(retriggerOutput.getSample(0, 0) - retriggerOutput.getSample(0, 2))
                  < 0.000001f
              && std::abs(retriggerOutput.getSample(0, 1) - retriggerOutput.getSample(0, 3))
                  < 0.000001f
              && retriggerOutput.getSample(0, 4) > retriggerOutput.getSample(0, 3),
          "Retrigger did not repeat the exact stored segment and continue");

    randomchop::EventDecision reorder;
    reorder.reorderEnabled = true;
    reorder.reorderPermutation = { 2, 0, 3, 1 };
    reorder.reorderPieceSpanFrames = 10.0;
    RandomSamplerVoice reorderVoice;
    reorderVoice.prepare(1000.0);
    reorderVoice.start(ramp, 60, 1.0f, 100.0, region, 1.0, false,
                       1.0f, 0.0f, 0.01f, 4, 0.0f, reorder);
    juce::AudioBuffer<float> reorderOutput(2, 12);
    reorderOutput.clear();
    reorderVoice.render(reorderOutput, 0, 12);
    check(std::abs(reorderOutput.getSample(0, 0) - 120.0f / 2048.0f) < 0.0001f
              && std::abs(reorderOutput.getSample(0, 10) - 100.0f / 2048.0f) < 0.0001f,
          "Reorder did not render stored pieces from the event's resolved start");

    randomchop::EventDecision bend;
    bend.bendEnabled = true;
    bend.bendDepthSemitones = 12.0f;
    RandomSamplerVoice bendVoice;
    bendVoice.prepare(1000.0);
    bendVoice.start(ramp, 60, 1.0f, 0.0, region, 1.0, false,
                    1.0f, 0.0f, 0.01f, 5, 0.0f, bend);
    juce::AudioBuffer<float> bendOutput(2, 600);
    bendOutput.clear();
    bendVoice.render(bendOutput, 0, bendOutput.getNumSamples());
    check(bendOutput.getSample(0, 500) > 0.27f,
          "Bend did not apply its stored playback-rate ramp");

    randomchop::EventDecision drop;
    drop.dropEnabled = true;
    drop.dropMask = 0x01;
    RandomSamplerVoice dropVoice;
    dropVoice.prepare(1000.0);
    dropVoice.start(constant, 60, 1.0f, 0.0, { 0, 79 }, 1.0, false,
                    1.0f, 0.0f, 0.005f, 6, 80.0f, drop);
    juce::AudioBuffer<float> dropOutput(2, 20);
    dropOutput.clear();
    dropVoice.render(dropOutput, 0, dropOutput.getNumSamples());
    check(std::abs(dropOutput.getSample(0, 0)) < 0.000001f
              && dropOutput.getSample(0, 10) > 0.99f,
          "Drop did not apply its stored slots with bounded edge smoothing");

    randomchop::EventDecision combined;
    combined.reverseEnabled = true;
    combined.retriggerEnabled = true;
    combined.retriggerSizeFrames = 12;
    combined.retriggerCount = 2;
    combined.skipEnabled = true;
    combined.skipJumpFrames = -100.0;
    combined.reorderEnabled = true;
    combined.reorderPermutation = { 3, 1, 0, 2 };
    combined.reorderPieceSpanFrames = 25.0;
    combined.bendEnabled = true;
    combined.bendDepthSemitones = -12.0f;
    combined.dropEnabled = true;
    combined.dropMask = 0x5a;
    RandomSamplerVoice combinedVoice;
    combinedVoice.prepare(1000.0);
    combinedVoice.start(ramp, 60, 1.0f, 200.0, region, 1.0, false,
                        1.0f, 0.0f, 0.005f, 7, 100.0f, combined);
    juce::AudioBuffer<float> combinedOutput(2, 120);
    combinedOutput.clear();
    combinedVoice.render(combinedOutput, 0, combinedOutput.getNumSamples());
    bool allFiniteAndBounded = true;
    float combinedMaximum = 0.0f;
    for (int channel = 0; channel < combinedOutput.getNumChannels(); ++channel)
        for (int frame = 0; frame < combinedOutput.getNumSamples(); ++frame)
        {
            const auto value = combinedOutput.getSample(channel, frame);
            allFiniteAndBounded = allFiniteAndBounded
                && std::isfinite(value) && std::abs(value) <= 2.0f;
            combinedMaximum = juce::jmax(combinedMaximum, std::abs(value));
        }
    check(allFiniteAndBounded && combinedMaximum > 0.01f,
          "combined Chance rendering was silent, non-finite, or unbounded");
}

void testPitchPreservingStretchPreparation()
{
    constexpr double sampleRate = 48000.0;
    constexpr double frequency = 440.0;
    constexpr int inputFrames = 32768;
    auto input = std::make_shared<juce::AudioBuffer<float>>(1, inputFrames);
    for (int frame = 0; frame < inputFrames; ++frame)
        input->setSample(0, frame, 0.5f * std::sin(
            juce::MathConstants<double>::twoPi * frequency * frame / sampleRate));

    const auto unchanged = randomchop::prepareStretch(input, sampleRate, 1.0f, 7);
    check(unchanged != nullptr && unchanged->audio == input
              && unchanged->revision == 7 && unchanged->stretchRatio == 1.0f,
          "1x stretch did not reuse the immutable decoded buffer");

    for (const auto ratio : { 0.5f, 2.0f })
    {
        const auto prepared = randomchop::prepareStretch(input, sampleRate, ratio, 11);
        const auto expectedFrames = static_cast<int>(std::llround(inputFrames * ratio));
        check(prepared != nullptr && prepared->audio != nullptr,
              "Signalsmith stretch preparation failed");
        if (prepared == nullptr || prepared->audio == nullptr)
            continue;

        check(prepared->audio->getNumSamples() == expectedFrames
                  && prepared->revision == 11
                  && std::abs(prepared->stretchRatio - ratio) < 0.000001f,
              "stretch duration or version metadata was incorrect");

        const auto* samples = prepared->audio->getReadPointer(0);
        float maximumMagnitude = 0.0f;
        bool allFinite = true;
        for (int frame = 0; frame < expectedFrames; ++frame)
        {
            allFinite = allFinite && std::isfinite(samples[frame]);
            maximumMagnitude = juce::jmax(maximumMagnitude, std::abs(samples[frame]));
        }
        check(allFinite && maximumMagnitude > 0.01f && maximumMagnitude <= 8.0f,
              "stretch output was silent, non-finite, or outside its safety bound");

        const int first = expectedFrames / 8;
        const int last = expectedFrames - first;
        int risingCrossings = 0;
        for (int frame = first + 1; frame < last; ++frame)
            if (samples[frame - 1] <= 0.0f && samples[frame] > 0.0f)
                ++risingCrossings;
        const auto measuredFrequency = risingCrossings * sampleRate
            / static_cast<double>(last - first);
        check(std::abs(measuredFrequency - frequency) < 70.0,
              "stretch changed pitch outside the allowed test tolerance");
    }

    check(randomchop::clampStretchRatio(0.1f) == 0.5f
              && randomchop::clampStretchRatio(3.0f) == 2.0f
              && randomchop::clampStretchRatio(
                     std::numeric_limits<float>::quiet_NaN()) == 1.0f,
          "stretch bounds did not sanitize invalid values");
}

void testAsynchronousStretchPublication()
{
    const auto file = makeTinyWaveFile();
    check(file.existsAsFile(), "could not create the asynchronous stretch fixture");
    if (!file.existsAsFile())
        return;

    std::atomic<bool> firstJobStarted { false };
    std::atomic<bool> releaseFirstJob { false };
    std::atomic<bool> secondJobStarted { false };
    std::atomic<bool> releaseSecondJob { false };
    std::atomic<bool> thirdJobStarted { false };
    std::atomic<bool> releaseThirdJob { false };
    std::atomic<bool> thirdJobReturned { false };
    auto deterministicPrepare = [&] (
        const std::shared_ptr<const juce::AudioBuffer<float>>& decoded,
        double sampleRate, float ratio, uint64_t revision) -> PreparedSamplePtr
    {
        if (revision == 1)
        {
            firstJobStarted.store(true, std::memory_order_release);
            while (!releaseFirstJob.load(std::memory_order_acquire))
                juce::Thread::sleep(1);
        }
        else if (revision == 2)
        {
            secondJobStarted.store(true, std::memory_order_release);
            while (!releaseSecondJob.load(std::memory_order_acquire))
                juce::Thread::sleep(1);
        }
        else if (revision == 3)
        {
            thirdJobStarted.store(true, std::memory_order_release);
            while (!releaseThirdJob.load(std::memory_order_acquire))
                juce::Thread::sleep(1);
        }

        const auto outputFrames = juce::jmax(2, static_cast<int>(
            std::llround(decoded->getNumSamples() * static_cast<double>(ratio))));
        auto output = std::make_shared<juce::AudioBuffer<float>>(
            decoded->getNumChannels(), outputFrames);
        for (int channel = 0; channel < output->getNumChannels(); ++channel)
        {
            const auto* source = decoded->getReadPointer(channel);
            auto* destination = output->getWritePointer(channel);
            for (int frame = 0; frame < outputFrames; ++frame)
                destination[frame] = source[juce::jlimit(
                    0, decoded->getNumSamples() - 1,
                    static_cast<int>(frame / static_cast<double>(ratio)))];
        }

        auto prepared = std::make_shared<PreparedSampleData>();
        prepared->audio = std::move(output);
        prepared->sampleRate = sampleRate;
        prepared->revision = revision;
        prepared->stretchRatio = ratio;
        if (revision == 3)
            thirdJobReturned.store(true, std::memory_order_release);
        return prepared;
    };

    {
        SampleManager manager(deterministicPrepare);
        juce::StringArray paths;
        paths.add(file.getFullPathName());
        check(manager.addFiles(paths).empty(), "asynchronous stretch fixture did not load");

        auto initial = manager.getSnapshot();
        check(initial->size() == 1, "asynchronous stretch fixture was not added");
        if (initial->empty())
        {
            check(file.deleteFile(), "could not remove failed asynchronous stretch fixture");
            return;
        }
        const auto id = initial->front()->settings.id;
        auto oldPrepared = initial->front()->prepared;
        std::weak_ptr<const PreparedSampleData> oldPreparedWeak = oldPrepared;
        RandomSamplerVoice activeVoice;
        activeVoice.prepare(44100.0);
        activeVoice.start(oldPrepared, 60, 1.0f, 0.0,
                          { 0, oldPrepared->audio->getNumSamples() - 1 },
                          1.0, false, 1.0f, 0.0f, 0.1f, 1);
        oldPrepared.reset();
        initial.reset();

        manager.updateSettings(id, [](SampleSettings& settings)
        {
            settings.stretchRatio = 2.0f;
        });
        for (int attempt = 0; attempt < 2000
                && !firstJobStarted.load(std::memory_order_acquire); ++attempt)
            juce::Thread::sleep(1);
        check(firstJobStarted.load(std::memory_order_acquire),
              "background stretch worker did not start");

        manager.updateSettings(id, [](SampleSettings& settings)
        {
            settings.stretchRatio = 0.5f;
        });
        releaseFirstJob.store(true, std::memory_order_release);
        for (int attempt = 0; attempt < 2000
                && !secondJobStarted.load(std::memory_order_acquire); ++attempt)
            juce::Thread::sleep(1);
        check(secondJobStarted.load(std::memory_order_acquire),
              "replacement stretch revision did not start");
        auto beforeSecondPublish = manager.getSnapshot();
        check(!beforeSecondPublish->empty()
                  && beforeSecondPublish->front()->stretchPending
                  && beforeSecondPublish->front()->requestedStretchRevision == 2
                  && beforeSecondPublish->front()->prepared->revision == 0,
              "a stale stretch revision published before its replacement");
        beforeSecondPublish.reset();
        releaseSecondJob.store(true, std::memory_order_release);

        std::shared_ptr<const SampleManager::Pool> current;
        for (int attempt = 0; attempt < 300; ++attempt)
        {
            current = manager.getSnapshot();
            if (!current->empty() && !current->front()->stretchPending
                && current->front()->prepared->revision == 2)
                break;
            current.reset();
            juce::Thread::sleep(10);
        }

        check(current != nullptr && !current->empty()
                  && !current->front()->stretchPending
                  && !current->front()->stretchFailed
                  && current->front()->prepared->revision == 2
                  && std::abs(current->front()->prepared->stretchRatio - 0.5f) < 0.000001f
                  && current->front()->prepared->audio->getNumSamples() == 16,
              "latest stretch revision did not win over a stale result");
        manager.collectGarbage();
        check(!oldPreparedWeak.expired() && activeVoice.isActive(),
              "an active voice lost its superseded prepared sample");
        current.reset();

        manager.updateSettings(id, [](SampleSettings& settings)
        {
            settings.stretchRatio = 2.0f;
        });
        for (int attempt = 0; attempt < 2000
                && !thirdJobStarted.load(std::memory_order_acquire); ++attempt)
            juce::Thread::sleep(1);
        check(thirdJobStarted.load(std::memory_order_acquire),
              "removal test stretch revision did not start");
        manager.remove(id);
        releaseThirdJob.store(true, std::memory_order_release);
        for (int attempt = 0; attempt < 2000
                && !thirdJobReturned.load(std::memory_order_acquire); ++attempt)
            juce::Thread::sleep(1);
        juce::Thread::sleep(5);
        check(manager.getSnapshot()->empty(),
              "removing a source during stretch preparation republished it");

        activeVoice.forceStop();
        manager.collectGarbage();
        check(oldPreparedWeak.expired(),
              "superseded prepared data was not reclaimed after realtime owners released it");
    }

    check(file.deleteFile(), "could not remove the asynchronous stretch fixture");
}
}

int main()
{
    testWeightedSelection();
    testStepMaskSequencingAndState();
    testTakeHistoryCaptureReplayAndLifetime();
    testMasterDigitalProcessing();
    testPoolLimitAndPersistentSettings();
    testParameterOnlyStateClearsSamples();
    testRegionClampingAndRestore();
    testRandomStartAndReadBounds();
    testVoiceRegionBoundaries();
    testWaveformPeakExtrema();
    testHarmonicPitch();
    testVoicePitchRatio();
    testFinalLengthAndEnvelope();
    testNoteOffRelease();
    testPolyMonoAndVoiceStealing();
    testChanceDecisionResolution();
    testChanceVoiceRendering();
    testPitchPreservingStretchPreparation();
    testAsynchronousStretchPublication();
    if (failures == 0)
        std::cout << "All sampler tests passed.\n";
    return failures == 0 ? 0 : 1;
}
