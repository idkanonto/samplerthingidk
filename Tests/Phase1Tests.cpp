#include <JuceHeader.h>
#include "CreativeEffects.h"
#include "HarmonicPitch.h"
#include "HostGrid.h"
#include "RandomSamplerVoice.h"
#include "SampleManager.h"
#include "SourceSelection.h"
#include "StateMigration.h"
#include "SpectralDraw.h"
#include "TemporalEffects.h"
#include "VoicePool.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

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

juce::File makeAudioFixture(juce::AudioFormat& format, const juce::String& suffix,
                            float level = 0.25f)
{
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("recompiler-test", suffix, false);
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    const auto options = juce::AudioFormatWriterOptions {}
        .withSampleRate(48000.0).withNumChannels(2).withBitsPerSample(16);
    auto writer = format.createWriterFor(stream, options);
    if (writer == nullptr)
        return {};
    juce::AudioBuffer<float> audio(2, 256);
    for (int channel = 0; channel < 2; ++channel)
        for (int frame = 0; frame < audio.getNumSamples(); ++frame)
            audio.setSample(channel, frame, level * std::sin(
                juce::MathConstants<float>::twoPi * static_cast<float>(frame) / 32.0f));
    if (!writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()))
        return {};
    writer.reset();
    return file;
}

PreparedSamplePtr makePrepared(
    const std::shared_ptr<const juce::AudioBuffer<float>>& audio,
    double sampleRate)
{
    auto prepared = std::make_shared<PreparedSampleData>();
    prepared->audio = audio;
    prepared->sampleRate = sampleRate;
    return prepared;
}

SampleManager::SamplePtr makeSource(bool enabled = true, bool missing = false)
{
    auto audio = std::make_shared<juce::AudioBuffer<float>>(1, 64);
    audio->clear();
    auto source = std::make_shared<SampleData>();
    source->settings.id = juce::Uuid().toString();
    source->settings.enabled = enabled;
    source->settings.missing = missing;
    source->audio = audio;
    source->prepared = makePrepared(audio, 44100.0);
    return source;
}

PreparedSamplePtr makeVoiceSample(float value, int frames = 1024)
{
    auto audio = std::make_shared<juce::AudioBuffer<float>>(1, frames);
    for (int frame = 0; frame < frames; ++frame)
        audio->setSample(0, frame, value);
    return makePrepared(audio, 1000.0);
}

bool bufferFiniteAndBounded(const juce::AudioBuffer<float>& buffer) noexcept;
juce::AudioBuffer<float> makeTemporalInput(int frames, int offset = 0);
juce::AudioBuffer<float> copyBuffer(const juce::AudioBuffer<float>& source);
bool buffersEqual(const juce::AudioBuffer<float>&, const juce::AudioBuffer<float>&) noexcept;

void testSupportedFormatsAndPoolState()
{
    check(SampleManager::isSupported(juce::File("source.wav"))
              && SampleManager::isSupported(juce::File("source.aiff"))
              && SampleManager::isSupported(juce::File("source.aif"))
              && SampleManager::isSupported(juce::File("source.mp3"))
              && SampleManager::isSupported(juce::File("source.flac"))
              && !SampleManager::isSupported(juce::File("source.ogg")),
          "supported source extension matrix changed");

    juce::WavAudioFormat formatWav;
    juce::AiffAudioFormat formatAiff;
    juce::FlacAudioFormat formatFlac;
    std::vector<juce::File> formatFiles {
        makeAudioFixture(formatWav, ".wav"),
        makeAudioFixture(formatAiff, ".aiff"),
        makeAudioFixture(formatAiff, ".aif"),
        makeAudioFixture(formatFlac, ".flac")
    };
    juce::StringArray formatPaths;
    for (const auto& fixture : formatFiles)
    {
        check(fixture.existsAsFile() && fixture.getSize() > 0,
              "could not create a writable format fixture");
        if (fixture.existsAsFile())
            formatPaths.add(fixture.getFullPathName());
    }
    const auto mp3Path = juce::SystemStats::getEnvironmentVariable(
        "RANDOM_CHOP_TEST_MP3", {});
    if (mp3Path.isNotEmpty())
        formatPaths.add(mp3Path);
    {
        SampleManager decoder;
        const auto errors = decoder.addFiles(formatPaths);
        check(errors.empty() && decoder.size() == formatPaths.size(),
              "JUCE did not decode the supported format fixture matrix");
        for (const auto& source : *decoder.getSnapshot())
            check(source->isPlayable() && source->audio->getNumChannels() == 2,
                  "decoded format fixture was not a playable stereo source");
    }
    for (const auto& fixture : formatFiles)
        if (fixture.existsAsFile())
            check(fixture.deleteFile(), "could not remove format fixture");

    juce::WavAudioFormat wav;
    const auto file = makeAudioFixture(wav, ".wav");
    check(file.existsAsFile(), "could not create WAV fixture");
    if (!file.existsAsFile())
        return;

    {
        SampleManager manager;
        juce::StringArray paths;
        for (int index = 0; index <= SampleManager::maximumSamples; ++index)
            paths.add(file.getFullPathName());
        const auto errors = manager.addFiles(paths);
        check(manager.size() == SampleManager::maximumSamples && errors.size() == 1,
              "20-source pool limit changed");

        const auto first = manager.getSnapshot()->front();
        const auto id = first->settings.id;
        const auto runtimeId = first->runtimeId;
        manager.updateSettings(id, [](SampleSettings& settings)
        {
            settings.enabled = false;
            settings.startNormalised = 0.2;
            settings.endNormalised = 0.8;
            settings.sourceKey = 8;
            settings.gainDb = -7.5f;
            settings.transposeSemitones = -12;
            settings.fineTuneCents = 37.0f;
        });
        const auto changed = manager.getSnapshot()->front();
        check(changed->settings.id == id && changed->runtimeId == runtimeId,
              "source identity changed during an immutable settings update");
        check(!changed->settings.enabled && changed->settings.startNormalised == 0.2
                  && changed->settings.endNormalised == 0.8
                  && changed->settings.sourceKey == 8
                  && changed->settings.gainDb == -7.5f
                  && changed->settings.transposeSemitones == -12
                   && changed->settings.fineTuneCents == 37.0f,
              "source controls were not preserved in the snapshot");

        manager.updateSettings(id, [](SampleSettings& settings)
        {
            settings.gainDb = std::numeric_limits<float>::quiet_NaN();
        });
        const auto sanitised = manager.getSnapshot()->front();
        check(sanitised->settings.gainDb == 0.0f,
              "hostile per-source Gain escaped finite state bounds");

        const auto state = manager.createState();
        SampleManager restored;
        const auto restoreErrors = restored.restoreState(state);
        check(restoreErrors.empty() && restored.size() == SampleManager::maximumSamples,
              "sample pool state did not restore");
        check(restored.getSnapshot()->front()->settings.id == id,
              "stable source ID was not persisted");
        juce::ValueTree hostileState("SAMPLES");
        juce::ValueTree hostileSource("SAMPLE");
        hostileSource.setProperty("id", "hostile-source", nullptr);
        hostileSource.setProperty("path", file.getFullPathName(), nullptr);
        hostileSource.setProperty("gain", std::numeric_limits<float>::quiet_NaN(), nullptr);
        hostileSource.setProperty("weight", std::numeric_limits<float>::infinity(), nullptr);
        hostileSource.setProperty("stretch", 4.0f, nullptr);
        hostileState.appendChild(hostileSource, nullptr);
        SampleManager hostileRestore;
        const auto hostileErrors = hostileRestore.restoreState(hostileState);
        const auto hostileSnapshot = hostileRestore.getSnapshot();
        check(hostileErrors.empty() && !hostileSnapshot->empty()
                   && hostileSnapshot->front()->settings.gainDb == 0.0f,
              "hostile persisted Gain escaped finite restore bounds");
        const auto rewritten = hostileRestore.createState().getChild(0);
        check(!rewritten.hasProperty("weight") && !rewritten.hasProperty("stretch"),
              "retired Weight or Stretch state was written back into a session");
        const auto restoredId = restored.getSnapshot()->front()->settings.id;
        const auto heldPrepared = restored.getSnapshot()->front()->prepared;
        const auto heldFrames = heldPrepared != nullptr && heldPrepared->audio != nullptr
            ? heldPrepared->audio->getNumSamples() : 0;
        restored.remove(restoredId);
        restored.collectGarbage();
        check(restored.size() == SampleManager::maximumSamples - 1,
              "individual source removal changed pool semantics");
        check(heldPrepared != nullptr && heldPrepared->audio != nullptr
                  && heldPrepared->audio->getNumSamples() == heldFrames && heldFrames > 0,
              "removing a source reclaimed immutable audio still held by an active consumer");
    }
    check(file.deleteFile(), "could not remove WAV fixture");
}

void testEqualSelectionAndPitch()
{
    SampleManager::Pool pool { makeSource(), makeSource(),
                               makeSource(false), makeSource(true, true) };
    RandomizationEngine random;
    random.setSeed(123456);
    int first = 0;
    int second = 0;
    for (int iteration = 0; iteration < 10000; ++iteration)
    {
        const auto selected = randomchop::chooseSource(pool, random);
        first += selected == 0 ? 1 : 0;
        second += selected == 1 ? 1 : 0;
        check(selected == 0 || selected == 1,
              "equal selection chose disabled or missing source");
    }
    check(first > 4500 && first < 5500 && second > 4500 && second < 5500,
          "enabled sources no longer receive approximately equal selection probability");
    SampleManager::Pool empty { makeSource(false) };
    check(randomchop::chooseSource(empty, random) == -1,
          "empty playable pool did not return the silent sentinel");

    check(randomchop::shortestTonicCorrection(1, 12) == -1,
          "tonic correction no longer uses shortest direction");
    check(randomchop::chordRootMidiNote(1) == 72
              && randomchop::chordRootMidiNote(12) == 71
              && randomchop::chordRootMidiNote(0) == 72,
          "automatic Chords root no longer follows Play In Key near the central octave");
    check(std::abs(randomchop::totalPitchSemitones(
        1, 12, 12, 50.0f, true, 84, 72) - 23.5) < 0.000001,
        "combined source, key, tuning, and MIDI pitch calculation changed");
    check(std::abs(randomchop::pitchRatioForSemitones(12.0) - 2.0) < 0.000001,
          "pitch ratio conversion changed");
    check(randomchop::midiNoteName(0) == "C-1"
              && randomchop::midiNoteName(60) == "C4"
              && randomchop::midiNoteName(127) == "G9"
              && randomchop::midiNoteFromName("C4") == 60
              && randomchop::midiNoteFromName("F#4") == 66
              && randomchop::midiNoteFromName("Db3") == 49
              && randomchop::midiNoteFromName("72") == 72,
          "root-note musical display or parsing changed");
}

void testRegionsAndVoices()
{
    const auto clamped = randomchop::clampNormalisedRegion(-1.0, 2.0);
    check(clamped.start == 0.0 && clamped.end == 1.0,
          "source region bounds were not clamped");
    const randomchop::FrameRegion region { 3, 20 };
    const auto start = randomchop::resolveRandomStart(region, 1000.0, 1.0, 0.999999);
    check(randomchop::isInterpolationPositionLegal(region, start)
              && start <= randomchop::maximumRandomStart(region, 1000.0),
          "random start escaped the selected region");

    auto audio = std::make_shared<juce::AudioBuffer<float>>(1, 24);
    for (int frame = 0; frame < 24; ++frame)
        audio->setSample(0, frame, frame >= 3 && frame <= 20 ? 0.5f : 100.0f);
    const auto prepared = makePrepared(audio, 1000.0);
    RandomSamplerVoice voice;
    voice.prepare(1000.0);
    voice.start(prepared, 60, 1.0f, 3.0, region, 1.0,
                1.0f, 0.0f, 0.01f, 1);
    juce::AudioBuffer<float> output(2, 64);
    output.clear();
    voice.render(output, 0, output.getNumSamples());
    float maximum = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
        for (int frame = 0; frame < output.getNumSamples(); ++frame)
            maximum = std::max(maximum, std::abs(output.getSample(channel, frame)));
    check(maximum > 0.0f && maximum < 1.0f && !voice.isActive(),
          "forward voice crossed its source region or failed to finish");

    const auto constant = makeVoiceSample(1.0f);
    RandomSamplerVoice limited;
    limited.prepare(1000.0);
    limited.start(constant, 60, 1.0f, 0.0, { 0, 1023 }, 1.0,
                  1.0f, 0.010f, 0.005f, 2, 20.0f);
    juce::AudioBuffer<float> shaped(2, 24);
    shaped.clear();
    limited.render(shaped, 0, shaped.getNumSamples());
    check(!limited.isActive() && shaped.getSample(0, 0) > 0.0f
              && shaped.getSample(0, 9) > shaped.getSample(0, 0)
              && std::abs(shaped.getSample(0, 19)) < 0.000001f,
          "attack/release or final-length boundary changed");

    randomchop::VoicePool pool;
    pool.prepare(1000.0);
    for (size_t index = 0; index < randomchop::VoicePool::capacity; ++index)
    {
        auto& slot = pool.acquire(randomchop::VoiceMode::poly);
        slot.start(constant, 40 + static_cast<int>(index), 1.0f, 0.0,
                   { 0, 1023 }, 1.0, 1.0f, 0.0f, 0.01f, index + 1);
    }
    check(pool.activeCount() == 16 && pool.acquire(randomchop::VoiceMode::poly).getNote() == 40,
          "16-voice pool or oldest-voice selection changed");

    auto hostileAudio = std::make_shared<juce::AudioBuffer<float>>(1, 16);
    std::fill_n(hostileAudio->getWritePointer(0), hostileAudio->getNumSamples(), 0.5f);
    hostileAudio->setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
    hostileAudio->setSample(0, 1, std::numeric_limits<float>::infinity());
    hostileAudio->setSample(0, 2, 1.0e30f);
    auto hostilePrepared = std::make_shared<PreparedSampleData>();
    hostilePrepared->audio = hostileAudio;
    hostilePrepared->sampleRate = 1000.0;
    RandomSamplerVoice hostileVoice;
    hostileVoice.prepare(1000.0);
    hostileVoice.start(hostilePrepared, 60, 1.0f, 0.0, { 0, 15 }, 1.0,
                       1.0f, 0.0f, std::numeric_limits<float>::quiet_NaN(), 100);
    juce::AudioBuffer<float> hostileOutput(2, 16);
    hostileOutput.clear();
    hostileVoice.render(hostileOutput, 0, 4);
    hostileVoice.release(std::numeric_limits<float>::quiet_NaN());
    hostileVoice.render(hostileOutput, 4, 12);
    check(bufferFiniteAndBounded(hostileOutput) && !hostileVoice.isActive(),
          "voice rendering propagated hostile audio/envelope state or failed to release");
}

void testMeltSingleMacroProgression()
{
    randomchop::GridBoundaries noBoundary;
    noBoundary.bpm = 120.0;
    randomchop::GridBoundaries trigger;
    trigger.bpm = 120.0;
    trigger.count = 1;
    trigger.sampleOffsets[0] = 0;

    randomchop::MeltProcessor low;
    randomchop::MeltProcessor highA;
    randomchop::MeltProcessor highB;
    low.prepare(48000.0);
    highA.prepare(48000.0);
    highB.prepare(48000.0);
    low.setSeed(77);
    highA.setSeed(77);
    highB.setSeed(77);
    auto lowHistory = makeTemporalInput(6000);
    auto highHistoryA = copyBuffer(lowHistory);
    auto highHistoryB = copyBuffer(lowHistory);
    low.process(lowHistory, noBoundary, 1, { 5.0f });
    highA.process(highHistoryA, noBoundary, 1, { 72.0f });
    highB.process(highHistoryB, noBoundary, 1, { 72.0f });
    auto lowOutput = makeTemporalInput(6000, 6000);
    auto highOutputA = copyBuffer(lowOutput);
    auto highOutputB = copyBuffer(lowOutput);
    low.process(lowOutput, trigger, 1, { 5.0f });
    highA.process(highOutputA, trigger, 1, { 72.0f });
    highB.process(highOutputB, trigger, 1, { 72.0f });
    check(low.getLastStretchRatio() > 1.0f
              && highA.getLastStretchRatio() > low.getLastStretchRatio()
              && low.getLastReversedSlices() == 0
              && highA.getLastReversedSlices() > 0
              && buffersEqual(highOutputA, highOutputB)
              && !buffersEqual(lowOutput, highOutputA)
              && bufferFiniteAndBounded(lowOutput)
              && bufferFiniteAndBounded(highOutputA),
          "Melt's single macro did not progress stretch/reversal deterministically");
}

void testHostGrid()
{
    check(randomchop::HostGrid::quarterNotesPerStep(0) == 0.5
              && randomchop::HostGrid::quarterNotesPerStep(1) == 0.25
              && randomchop::HostGrid::quarterNotesPerStep(2) == 0.125
              && randomchop::HostGrid::quarterNotesPerStep(99) == 0.25,
          "host-grid division mapping changed");
    check(randomchop::HostGrid::automaticDivisionChoice(60.0) == 2
              && randomchop::HostGrid::automaticDivisionChoice(120.0) == 1
              && randomchop::HostGrid::automaticDivisionChoice(240.0) == 0
              && randomchop::HostGrid::automaticDivisionChoice(
                     std::numeric_limits<double>::quiet_NaN()) == 1,
          "automatic host-grid choice no longer keeps creative slices near 125 ms");

    randomchop::HostGrid grid;
    randomchop::HostTiming host { 120.0, 0.0, true, true, true };
    auto boundaries = grid.process(1000.0, 500, 1, host);
    check(boundaries.usedHostClock && boundaries.transportDiscontinuity
              && boundaries.count == 4
              && boundaries.sampleOffsets[0] == 0
              && boundaries.sampleOffsets[1] == 125
              && boundaries.sampleOffsets[2] == 250
              && boundaries.sampleOffsets[3] == 375,
          "1/16 host boundaries were not sample accurate");
    host.ppq = 1.0;
    boundaries = grid.process(1000.0, 500, 1, host);
    check(!boundaries.transportDiscontinuity && boundaries.count == 4,
          "continuous host blocks were mistaken for a seek");
    host.ppq = 2.0;
    host.bpm = 90.0;
    boundaries = grid.process(1000.0, 500, 1, host);
    check(!boundaries.transportDiscontinuity && boundaries.count == 3
              && boundaries.sampleOffsets[1] == 167,
          "continuous tempo change broke grid phase");
    host.ppq = 0.25;
    boundaries = grid.process(1000.0, 64, 1, host);
    check(boundaries.transportDiscontinuity,
          "host seek/loop discontinuity was not detected");

    grid.reset();
    host = { 120.0, 0.1, true, true, true };
    boundaries = grid.process(1000.0, 100, 1, host);
    check(boundaries.count == 1 && boundaries.sampleOffsets[0] == 75,
          "non-zero PPQ offset did not resolve the next boundary");
    grid.reset();
    host.ppq = 0.0;
    boundaries = grid.process(1000.0, 500, 0, host);
    check(boundaries.count == 2 && boundaries.sampleOffsets[1] == 250,
          "1/8 host boundary spacing changed");
    host.isPlaying = false;
    auto stopped = grid.process(1000.0, 64, 0, host);
    check(!stopped.usedHostClock && stopped.transportDiscontinuity
              && stopped.count == 1 && stopped.sampleOffsets[0] == 0,
          "stopped transport did not enter the safe fallback clock");

    grid.reset();
    randomchop::HostTiming missing;
    auto fallbackA = grid.process(1000.0, 60, 1, missing);
    auto fallbackB = grid.process(1000.0, 80, 1, missing);
    check(!fallbackA.usedHostClock && fallbackA.count == 1
              && fallbackA.sampleOffsets[0] == 0
              && fallbackB.count == 1 && fallbackB.sampleOffsets[0] == 65,
          "120 BPM fallback grid did not span arbitrary block sizes");

    grid.reset();
    auto fastest = grid.process(1000.0, 500, 2, host);
    check(fastest.count == 8 && fastest.sampleOffsets[1] == 63
              && fastest.sampleOffsets[7] == 438,
          "1/32 grid rounding changed");

    const auto collectBoundaries = [](const std::vector<int>& blockSizes)
    {
        randomchop::HostGrid partitioned;
        randomchop::HostTiming timing { 123.0, 0.0, true, true, true };
        std::vector<int> absolute;
        int start = 0;
        for (const auto size : blockSizes)
        {
            timing.ppq = static_cast<double>(start) * timing.bpm / (60.0 * 48000.0);
            const auto block = partitioned.process(48000.0, size, 1, timing);
            for (int index = 0; index < block.count; ++index)
                absolute.push_back(start + block.sampleOffsets[static_cast<std::size_t>(index)]);
            start += size;
        }
        return absolute;
    };
    check(collectBoundaries({ 24000 }) == collectBoundaries({ 5854, 18146 })
              && collectBoundaries({ 24000 })
                    == collectBoundaries({ 1, 17, 64, 511, 5854, 17553 }),
          "fractional-tempo grid boundaries changed across host block partitions");

    randomchop::HostGrid denseGrid;
    randomchop::HostTiming denseTiming { 400.0, 0.0, true, true, true };
    const auto dense = denseGrid.process(1000.0, 2000, 2, denseTiming);
    check(dense.count > 64 && !dense.truncated,
          "long offline block still lost boundaries at the old 64-event limit");

    randomchop::HostGrid editGrid;
    randomchop::HostTiming editTiming { 120.0, 0.0, true, true, true };
    editGrid.process(1000.0, 500, 1, editTiming);
    editTiming.ppq = 1.0;
    const auto edited = editGrid.process(1000.0, 500, 2, editTiming);
    check(edited.gridChanged && !edited.transportDiscontinuity,
          "a grid-division edit was still reported as an audio discontinuity");
}

juce::AudioBuffer<float> makeTemporalInput(int frames, int offset)
{
    juce::AudioBuffer<float> buffer(2, frames);
    for (int channel = 0; channel < 2; ++channel)
        for (int frame = 0; frame < frames; ++frame)
            buffer.setSample(channel, frame,
                0.7f * std::sin(static_cast<float>(frame + offset + channel * 7) * 0.11f));
    return buffer;
}

juce::AudioBuffer<float> copyBuffer(const juce::AudioBuffer<float>& source)
{
    juce::AudioBuffer<float> copy;
    copy.makeCopyOf(source);
    return copy;
}

bool buffersEqual(const juce::AudioBuffer<float>& a,
                  const juce::AudioBuffer<float>& b) noexcept
{
    if (a.getNumChannels() != b.getNumChannels()
        || a.getNumSamples() != b.getNumSamples())
        return false;
    for (int channel = 0; channel < a.getNumChannels(); ++channel)
        for (int frame = 0; frame < a.getNumSamples(); ++frame)
            if (a.getSample(channel, frame) != b.getSample(channel, frame))
                return false;
    return true;
}

bool bufferFiniteAndBounded(const juce::AudioBuffer<float>& buffer) noexcept
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
            if (!std::isfinite(buffer.getSample(channel, frame))
                || std::abs(buffer.getSample(channel, frame)) > 64.0f)
                return false;
    return true;
}

float differenceRms(const juce::AudioBuffer<float>& dry,
                    const juce::AudioBuffer<float>& wet) noexcept
{
    double energy = 0.0;
    std::int64_t samples = 0;
    for (int channel = 0; channel < std::min(dry.getNumChannels(), wet.getNumChannels()); ++channel)
        for (int frame = 0; frame < std::min(dry.getNumSamples(), wet.getNumSamples()); ++frame)
        {
            const auto difference = static_cast<double>(wet.getSample(channel, frame))
                - static_cast<double>(dry.getSample(channel, frame));
            energy += difference * difference;
            ++samples;
        }
    return samples > 0 ? static_cast<float>(std::sqrt(energy / static_cast<double>(samples)))
                       : 0.0f;
}

float firstDifferenceRms(const juce::AudioBuffer<float>& buffer) noexcept
{
    double energy = 0.0;
    std::int64_t samples = 0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int frame = 1; frame < buffer.getNumSamples(); ++frame)
        {
            const auto difference = static_cast<double>(buffer.getSample(channel, frame))
                - static_cast<double>(buffer.getSample(channel, frame - 1));
            energy += difference * difference;
            ++samples;
        }
    return samples > 0 ? static_cast<float>(std::sqrt(energy / static_cast<double>(samples)))
                       : 0.0f;
}

juce::AudioBuffer<float> makeListeningInput(int frames, double sampleRate)
{
    juce::AudioBuffer<float> buffer(2, frames);
    constexpr std::array<float, 4> roots { 110.0f, 146.832f, 164.814f, 130.813f };
    for (int frame = 0; frame < frames; ++frame)
    {
        const auto time = static_cast<double>(frame) / sampleRate;
        const auto beat = std::fmod(time, 0.5);
        const auto barPosition = std::fmod(time, 2.0);
        const auto chord = std::min(3, static_cast<int>(barPosition / 0.5));
        const auto root = roots[static_cast<std::size_t>(chord)];
        const auto kickEnvelope = static_cast<float>(std::exp(-beat * 18.0));
        const auto kick = kickEnvelope * std::sin(juce::MathConstants<double>::twoPi
            * (52.0 + 54.0 * std::exp(-beat * 28.0)) * beat);
        const auto noteEnvelope = static_cast<float>(0.45 + 0.55
            * std::exp(-std::fmod(time, 0.25) * 7.0));
        const auto tonal = noteEnvelope * (0.34 * std::sin(
            juce::MathConstants<double>::twoPi * root * time)
            + 0.20 * std::sin(juce::MathConstants<double>::twoPi * root * 1.5 * time)
            + 0.13 * std::sin(juce::MathConstants<double>::twoPi * root * 2.0 * time));
        const auto hatEnvelope = std::exp(-std::fmod(time + 0.125, 0.25) * 55.0);
        const auto hat = 0.10 * hatEnvelope
            * (std::sin(juce::MathConstants<double>::twoPi * 6127.0 * time)
               + 0.5 * std::sin(juce::MathConstants<double>::twoPi * 9173.0 * time));
        const auto mono = static_cast<float>(0.36 * kick + tonal + hat);
        buffer.setSample(0, frame, mono);
        buffer.setSample(1, frame, static_cast<float>(0.36 * kick + noteEnvelope
            * (0.34 * std::sin(juce::MathConstants<double>::twoPi * root * time + 0.08)
               + 0.20 * std::sin(juce::MathConstants<double>::twoPi * root * 1.5 * time + 0.13)
               + 0.13 * std::sin(juce::MathConstants<double>::twoPi * root * 2.0 * time + 0.19))
            + hat));
    }
    return buffer;
}

juce::AudioBuffer<float> renderScramble(float amount,
                                        const juce::AudioBuffer<float>& input)
{
    auto output = copyBuffer(input);
    randomchop::ScrambleProcessor processor;
    processor.prepare(48000.0);
    processor.setSeed(0x51a7);
    randomchop::GridBoundaries boundaries;
    boundaries.bpm = 120.0;
    constexpr int gridFrames = 6000;
    for (int frame = gridFrames;
         frame < output.getNumSamples() && boundaries.count < 64;
         frame += gridFrames)
        boundaries.sampleOffsets[static_cast<std::size_t>(boundaries.count++)] = frame;
    processor.process(output, boundaries, 1, { amount });
    return output;
}

juce::AudioBuffer<float> renderMelt(float amount,
                                   const juce::AudioBuffer<float>& input)
{
    auto output = copyBuffer(input);
    randomchop::MeltProcessor processor;
    processor.prepare(48000.0);
    processor.setSeed(0x6d31a);
    randomchop::GridBoundaries boundaries;
    boundaries.bpm = 120.0;
    constexpr int gridFrames = 6000;
    for (int frame = gridFrames;
         frame < output.getNumSamples() && boundaries.count < 64;
         frame += gridFrames)
        boundaries.sampleOffsets[static_cast<std::size_t>(boundaries.count++)] = frame;
    processor.process(output, boundaries, 1, { amount });
    return output;
}

juce::AudioBuffer<float> renderSmear(float amount,
                                     const juce::AudioBuffer<float>& input)
{
    auto output = copyBuffer(input);
    randomchop::SmearProcessor processor;
    processor.prepare(48000.0);
    processor.setSeed(0x5ea2);
    processor.process(output, { amount });
    return output;
}

bool writeListeningWave(const juce::File& file,
                        const juce::AudioBuffer<float>& buffer)
{
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
        return false;
    juce::WavAudioFormat format;
    const auto options = juce::AudioFormatWriterOptions {}
        .withSampleRate(48000.0).withNumChannels(2).withBitsPerSample(24);
    auto writer = format.createWriterFor(stream, options);
    return writer != nullptr
        && writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
}

void testCreativeMacroProgressionAndRender()
{
    constexpr std::array<float, 5> levels { 0.0f, 25.0f, 50.0f, 75.0f, 100.0f };
    constexpr int renderFrames = 48000 * 4;
    const auto dry = makeListeningInput(renderFrames, 48000.0);
    std::array<float, levels.size()> scrambleDistance {};
    std::array<float, levels.size()> meltDistance {};
    std::array<float, levels.size()> smearDistance {};
    const auto renderPath = juce::SystemStats::getEnvironmentVariable(
        "RANDOM_CHOP_RENDER_DIR", {});
    const auto renderDirectory = juce::File(renderPath);
    if (renderPath.isNotEmpty())
    {
        check(renderDirectory.createDirectory().wasOk(),
              "could not create listening-render directory");
        check(writeListeningWave(renderDirectory.getChildFile("source_dry.wav"), dry),
              "could not write dry listening fixture");
    }

    for (std::size_t index = 0; index < levels.size(); ++index)
    {
        const auto level = levels[index];
        const auto scramble = renderScramble(level, dry);
        const auto melt = renderMelt(level, dry);
        const auto smear = renderSmear(level, dry);
        scrambleDistance[index] = differenceRms(dry, scramble);
        meltDistance[index] = differenceRms(dry, melt);
        smearDistance[index] = differenceRms(dry, smear);
        check(bufferFiniteAndBounded(scramble) && bufferFiniteAndBounded(melt)
                  && bufferFiniteAndBounded(smear),
              "a creative macro listening render was invalid or unbounded");
        if (renderPath.isNotEmpty())
        {
            const auto suffix = juce::String(static_cast<int>(level)).paddedLeft('0', 3) + ".wav";
            check(writeListeningWave(renderDirectory.getChildFile("scramble_" + suffix), scramble)
                      && writeListeningWave(renderDirectory.getChildFile("melt_" + suffix), melt)
                      && writeListeningWave(renderDirectory.getChildFile("smear_" + suffix), smear),
                  "could not write a creative macro listening render");
        }
    }

    check(scrambleDistance[0] == 0.0f && meltDistance[0] == 0.0f
              && smearDistance[0] == 0.0f,
          "a flagship macro was not sample-identical at zero");
    check(scrambleDistance[2] > 0.01f && scrambleDistance[4] > scrambleDistance[1],
          "Scramble did not create consistent medium or stronger maximum transformation");
    check(meltDistance[2] > 0.01f && meltDistance[4] > meltDistance[1],
          "Melt did not create a meaningful macro intensity progression");
    check(smearDistance[2] > 0.002f && smearDistance[4] > smearDistance[1]
              && firstDifferenceRms(renderSmear(75.0f, dry))
                    > firstDifferenceRms(dry),
          "Smear did not retain meaningful crystalline/high-frequency activity");

    std::cout << "Macro render difference RMS"
              << " | Scramble " << scrambleDistance[0] << ',' << scrambleDistance[1]
              << ',' << scrambleDistance[2] << ',' << scrambleDistance[3]
              << ',' << scrambleDistance[4]
              << " | Melt " << meltDistance[0] << ',' << meltDistance[1]
              << ',' << meltDistance[2] << ',' << meltDistance[3]
              << ',' << meltDistance[4]
              << " | Smear " << smearDistance[0] << ',' << smearDistance[1]
              << ',' << smearDistance[2] << ',' << smearDistance[3]
              << ',' << smearDistance[4] << '\n';
}

void testFullCreativeChainSafety()
{
    constexpr double sampleRate = 48000.0;
    randomchop::ScrambleProcessor scramble;
    randomchop::MeltProcessor melt;
    randomchop::SpectralDrawProcessor spectral;
    randomchop::SmearProcessor smear;
    scramble.prepare(sampleRate);
    melt.prepare(sampleRate);
    spectral.prepare(sampleRate);
    smear.prepare(sampleRate);
    scramble.setSeed(0x5678);
    melt.setSeed(0x9abc);
    smear.setSeed(0xdef0);

    randomchop::SpectralMaskStore mask;
    randomchop::SpectralMaskStore::Canvas canvas {};
    for (int row = 12; row < 52; ++row)
        for (int column = 0; column < randomchop::SpectralMaskStore::canvasWidth; ++column)
            if ((row + column) % 3 == 0)
                canvas[static_cast<std::size_t>(
                    row * randomchop::SpectralMaskStore::canvasWidth + column)] = 0.75f;
    check(mask.setCanvas(canvas), "full-chain Spectral mask did not publish");

    constexpr std::array<int, 8> sizes { 1, 17, 63, 128, 255, 511, 7, 89 };
    bool safe = true;
    bool sawSignal = false;
    int sourceOffset = 0;
    for (int iteration = 0; iteration < 48; ++iteration)
    {
        const auto frames = sizes[static_cast<std::size_t>(
            iteration % static_cast<int>(sizes.size()))];
        auto block = makeTemporalInput(frames, sourceOffset);
        sourceOffset += frames;
        if (iteration == 5)
        {
            block.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
            block.setSample(1, 0, std::numeric_limits<float>::infinity());
        }

        randomchop::GridBoundaries boundaries;
        boundaries.bpm = iteration % 2 == 0 ? 90.0 : 173.0;
        boundaries.transportDiscontinuity = iteration == 24;
        if (iteration % 7 == 1)
        {
            boundaries.count = 1;
            boundaries.sampleOffsets[0] = frames / 2;
        }
        scramble.process(block, boundaries, 2, { 100.0f });
        melt.process(block, boundaries, 2, { 100.0f });
        spectral.process(block, mask,
            { 100.0f, 3, boundaries.bpm, 0.0, false,
              boundaries.transportDiscontinuity });
        smear.process(block, { 100.0f });
        block.applyGain(0.5f);
        safe = safe && bufferFiniteAndBounded(block);
        if (iteration > 10)
            sawSignal = sawSignal || block.getMagnitude(0, block.getNumSamples()) > 0.000001f;
    }
    check(safe && sawSignal,
          "the complete global chain became invalid, unbounded, or permanently silent");
}

void testMeltProcessor()
{
    randomchop::GridBoundaries noBoundary;
    noBoundary.bpm = 120.0;
    randomchop::MeltProcessor bypass;
    bypass.prepare(48000.0);
    auto dry = makeTemporalInput(512);
    auto output = copyBuffer(dry);
    bypass.process(output, noBoundary, 1, { 0.0f });
    check(buffersEqual(dry, output),
          "Melt Amount 0 was not sample-identical bypass");

    randomchop::MeltProcessor first;
    randomchop::MeltProcessor second;
    first.prepare(48000.0);
    second.prepare(48000.0);
    first.setSeed(991);
    second.setSeed(991);
    auto animatedA = makeTemporalInput(24000);
    auto animatedB = copyBuffer(animatedA);
    randomchop::GridBoundaries boundaries;
    boundaries.bpm = 120.0;
    boundaries.count = 3;
    boundaries.sampleOffsets[0] = 6000;
    boundaries.sampleOffsets[1] = 12000;
    boundaries.sampleOffsets[2] = 18000;
    first.process(animatedA, boundaries, 1, { 72.0f });
    second.process(animatedB, boundaries, 1, { 72.0f });
    check(buffersEqual(animatedA, animatedB)
              && first.getActivationCount() == 3
              && first.getLastStretchRatio() > 1.0f
              && first.getLastReversedSlices() > 0,
          "Melt slice stretching was not deterministic or active");

    randomchop::MeltProcessor extreme;
    extreme.prepare(192000.0);
    juce::AudioBuffer<float> unsafe(2, 2048);
    for (int frame = 0; frame < unsafe.getNumSamples(); ++frame)
    {
        const auto value = frame % 4 == 0 ? std::numeric_limits<float>::quiet_NaN()
            : (frame % 4 == 1 ? std::numeric_limits<float>::infinity()
                              : (frame % 4 == 2 ? 1.0e30f : -1.0e30f));
        unsafe.setSample(0, frame, value);
        unsafe.setSample(1, frame, value);
    }
    extreme.setSeed(123);
    extreme.process(unsafe, noBoundary, 1, { 1000.0f });
    check(bufferFiniteAndBounded(unsafe),
          "Melt extreme macro state propagated NaN, Inf, or runaway gain");

    randomchop::MeltProcessor silenceProcessor;
    silenceProcessor.prepare(48000.0);
    silenceProcessor.setSeed(321);
    juce::AudioBuffer<float> silence(2, 48000);
    silence.clear();
    randomchop::GridBoundaries silenceBoundaries;
    silenceBoundaries.bpm = 120.0;
    silenceBoundaries.count = 3;
    silenceBoundaries.sampleOffsets[0] = 6000;
    silenceBoundaries.sampleOffsets[1] = 12000;
    silenceBoundaries.sampleOffsets[2] = 18000;
    silenceProcessor.process(silence, silenceBoundaries, 1, { 100.0f });
    check(silence.getMagnitude(0, silence.getNumSamples()) == 0.0f
              && silence.getMagnitude(1, silence.getNumSamples()) == 0.0f,
          "Melt generated self-noise from silence");
}

void testSmearProcessor()
{
    randomchop::SmearProcessor bypass;
    bypass.prepare(48000.0);
    auto dry = makeTemporalInput(512);
    auto output = copyBuffer(dry);
    bypass.process(output, { 0.0f });
    check(buffersEqual(dry, output), "Smear Amount 0 was not sample-identical bypass");

    randomchop::SmearProcessor continuous;
    continuous.prepare(48000.0);
    continuous.setSeed(411);
    auto first = makeTemporalInput(24000);
    continuous.process(first, { 100.0f });
    auto carried = makeTemporalInput(1024, 24000);
    continuous.process(carried, { 100.0f });
    randomchop::SmearProcessor cold;
    cold.prepare(48000.0);
    cold.setSeed(411);
    auto coldOutput = makeTemporalInput(1024, 24000);
    cold.process(coldOutput, { 100.0f });
    check(!buffersEqual(carried, coldOutput) && bufferFiniteAndBounded(carried)
              && continuous.getActiveGrainCount() > 0
              && continuous.getLastMotionAmount() > 0.99f
              && continuous.getLastOverlapGain() > 0.0f
              && continuous.getLastOverlapGain() <= 1.10f,
          "Smear did not retain its bounded moving crystal-grain cloud");

    randomchop::SmearProcessor deterministicA;
    randomchop::SmearProcessor deterministicB;
    deterministicA.prepare(44100.0);
    deterministicB.prepare(44100.0);
    deterministicA.setSeed(712);
    deterministicB.setSeed(712);
    auto sameA = makeTemporalInput(22000);
    auto sameB = copyBuffer(sameA);
    deterministicA.process(sameA, { 75.0f });
    deterministicB.process(sameB, { 75.0f });
    check(buffersEqual(sameA, sameB),
          "Smear crystal-grain scheduling was not deterministic for a restored seed");

    randomchop::SmearProcessor sparse;
    randomchop::SmearProcessor dense;
    sparse.prepare(48000.0);
    dense.prepare(48000.0);
    sparse.setSeed(913);
    dense.setSeed(913);
    auto sparseInput = makeTemporalInput(96000);
    auto denseInput = copyBuffer(sparseInput);
    sparse.process(sparseInput, { 20.0f });
    dense.process(denseInput, { 100.0f });
    check(dense.getPeakActiveGrainCount() > sparse.getPeakActiveGrainCount()
              && dense.getPeakActiveGrainCount() >= 12
              && dense.getLastGrainLengthFrames() > 0
              && dense.getLastGrainLengthFrames() < sparse.getLastGrainLengthFrames()
              && bufferFiniteAndBounded(sparseInput)
              && bufferFiniteAndBounded(denseInput),
          "Smear Amount did not introduce a denser cloud of smaller grains");

    juce::AudioBuffer<float> unsafe(2, 256);
    unsafe.clear();
    unsafe.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
    unsafe.setSample(1, 0, std::numeric_limits<float>::infinity());
    continuous.process(unsafe, { 1000.0f });
    check(bufferFiniteAndBounded(unsafe),
          "Smear extreme Amount propagated invalid or unbounded output");
}

void testSpectralMaskPublicationAndState()
{
    randomchop::SpectralMaskStore store;
    randomchop::SpectralMaskStore::Canvas canvas {};
    canvas[0] = -1.0f;
    canvas[1] = 0.5f;
    canvas[2] = 2.0f;
    canvas[3] = std::numeric_limits<float>::quiet_NaN();
    check(store.setCanvas(canvas), "Spectral canvas could not publish into a free slot");
    const auto generation = store.getPublishedGeneration();
    const auto cleaned = store.copyCanvas();
    check(generation > 0 && cleaned[0] == 0.0f && cleaned[1] == 0.5f
              && cleaned[2] == 1.0f && cleaned[3] == 0.0f,
          "Spectral canvas publication did not clamp hostile mask values");

    const auto held = store.acquire();
    check(held.snapshot != nullptr && held.snapshot->generation == generation,
          "audio-side Spectral canvas snapshot could not be acquired");
    auto replacement = cleaned;
    replacement[1] = 0.25f;
    check(store.setCanvas(replacement)
              && held.snapshot != nullptr && held.snapshot->values[1] == 0.5f,
          "Spectral canvas publication mutated a snapshot held by the audio side");
    store.release(held);
    const auto latest = store.acquire();
    check(latest.snapshot != nullptr && latest.snapshot->values[1] == 0.25f
              && latest.snapshot->generation > generation,
          "Spectral canvas did not advance to the latest immutable snapshot");
    store.release(latest);

    const auto encoded = store.encodeCanvas();
    randomchop::SpectralMaskStore restored;
    check(encoded.isNotEmpty() && restored.restoreEncodedCanvas(encoded)
              && restored.copyCanvas() == replacement,
          "Spectral canvas state did not round-trip through its bounded encoding");
    check(!restored.restoreEncodedCanvas("not-valid-canvas-state"),
          "invalid Spectral canvas state was accepted");
    restored.clear();
    const auto cleared = restored.copyCanvas();
    check(std::all_of(cleared.begin(), cleared.end(),
                      [](float value) { return value == 0.0f; }),
          "Spectral Clear did not publish an empty mask");
}

void testSpectralDrawProcessor()
{
    check(randomchop::SpectralDrawProcessor::latencySamples == 1024
              && randomchop::SpectralDrawProcessor::cycleQuarterNotes(0) == 2.0
              && randomchop::SpectralDrawProcessor::cycleQuarterNotes(3) == 16.0
              && randomchop::SpectralDrawProcessor::cycleQuarterNotes(99) == 4.0,
          "Spectral latency or scan-rate mapping changed");
    check(randomchop::SpectralDrawProcessor::automaticCycleChoice(60.0) == 0
              && randomchop::SpectralDrawProcessor::automaticCycleChoice(120.0) == 1
              && randomchop::SpectralDrawProcessor::automaticCycleChoice(240.0) == 2
              && randomchop::SpectralDrawProcessor::automaticCycleChoice(
                     std::numeric_limits<double>::infinity()) == 1,
          "automatic Spectral cycle no longer stays near two seconds");

    randomchop::SpectralMaskStore emptyMask;
    randomchop::SpectralDrawProcessor bypass;
    bypass.prepare(48000.0);
    juce::AudioBuffer<float> impulse(2, 2048);
    impulse.clear();
    impulse.setSample(0, 0, 1.0f);
    impulse.setSample(1, 0, -1.0f);
    bypass.process(impulse, emptyMask, { 0.0f, 1, 120.0, 0.0, false, false });
    check(impulse.getSample(0, 1023) == 0.0f
              && impulse.getSample(0, 1024) == 1.0f
              && impulse.getSample(1, 1024) == -1.0f,
          "Spectral Depth 0 did not provide exact reported-latency bypass");

    randomchop::SpectralDrawProcessor reconstruction;
    reconstruction.prepare(48000.0);
    auto source = makeTemporalInput(8192);
    auto reconstructed = copyBuffer(source);
    reconstruction.process(reconstructed, emptyMask,
        { 100.0f, 1, 120.0, 0.0, false, false });
    float maximumError = 0.0f;
    for (int channel = 0; channel < 2; ++channel)
        for (int frame = randomchop::SpectralDrawProcessor::latencySamples;
             frame < reconstructed.getNumSamples(); ++frame)
            maximumError = std::max(maximumError, std::abs(
                reconstructed.getSample(channel, frame)
                - source.getSample(channel,
                    frame - randomchop::SpectralDrawProcessor::latencySamples)));
    check(maximumError < 0.0002f && bufferFiniteAndBounded(reconstructed),
          "empty Spectral canvas did not reconstruct through STFT overlap-add");

    randomchop::SpectralMaskStore fullMask;
    randomchop::SpectralMaskStore::Canvas fullCanvas;
    fullCanvas.fill(1.0f);
    check(fullMask.setCanvas(fullCanvas), "full Spectral canvas did not publish");
    randomchop::SpectralDrawProcessor muted;
    muted.prepare(44100.0);
    auto fullyDrawn = makeTemporalInput(32768);
    muted.process(fullyDrawn, fullMask,
        { 100.0f, 1, 120.0, 0.0, false, false });
    check(fullyDrawn.getMagnitude(fullyDrawn.getNumSamples() / 2,
                                  fullyDrawn.getNumSamples() / 2)
               < 0.00001f,
          "smoothed full Spectral canvas did not settle to maximum attenuation");

    randomchop::SpectralMaskStore partialMask;
    randomchop::SpectralMaskStore::Canvas partialCanvas {};
    for (int row = 8; row < 44; ++row)
        for (int column = 0; column < randomchop::SpectralMaskStore::canvasWidth; column += 3)
            partialCanvas[static_cast<std::size_t>(
                row * randomchop::SpectralMaskStore::canvasWidth + column)] = 0.8f;
    partialMask.setCanvas(partialCanvas);
    auto blockSource = makeTemporalInput(5000);
    auto singleBlock = copyBuffer(blockSource);
    randomchop::SpectralDrawProcessor whole;
    whole.prepare(96000.0);
    whole.process(singleBlock, partialMask,
        { 73.0f, 2, 137.0, 0.0, false, false });

    randomchop::SpectralDrawProcessor chunked;
    chunked.prepare(96000.0);
    juce::AudioBuffer<float> chunkedOutput(2, blockSource.getNumSamples());
    int offset = 0;
    constexpr std::array<int, 7> blockSizes { 17, 511, 64, 1000, 3, 257, 89 };
    int blockIndex = 0;
    while (offset < blockSource.getNumSamples())
    {
        const auto count = std::min(blockSizes[static_cast<std::size_t>(
                                        blockIndex % static_cast<int>(blockSizes.size()))],
                                    blockSource.getNumSamples() - offset);
        juce::AudioBuffer<float> block(2, count);
        for (int channel = 0; channel < 2; ++channel)
            block.copyFrom(channel, 0, blockSource, channel, offset, count);
        chunked.process(block, partialMask,
            { 73.0f, 2, 137.0, 0.0, false, false });
        for (int channel = 0; channel < 2; ++channel)
            chunkedOutput.copyFrom(channel, offset, block, channel, 0, count);
        offset += count;
        ++blockIndex;
    }
    check(buffersEqual(singleBlock, chunkedOutput) && bufferFiniteAndBounded(chunkedOutput),
          "Spectral STFT changed across arbitrary process block sizes");

    randomchop::SpectralDrawProcessor scanner;
    scanner.prepare(1000.0);
    juce::AudioBuffer<float> scannerAudio(2, 1250);
    scannerAudio.clear();
    scanner.process(scannerAudio, emptyMask,
        { 0.0f, 0, 120.0, 0.0, false, false });
    check(std::abs(scanner.getScanPosition() - 0.25f) < 0.0001f,
          "Spectral scanner did not wrap on its tempo-derived cycle");
    juce::AudioBuffer<float> hostAligned(2, 100);
    hostAligned.clear();
    scanner.process(hostAligned, emptyMask,
        { 0.0f, 1, 120.0, 3.0, true, true });
    check(std::abs(scanner.getScanPosition() - 0.80f) < 0.0001f,
          "Spectral scanner did not align to host PPQ after a discontinuity");

    randomchop::SpectralDrawProcessor changing;
    changing.prepare(48000.0);
    bool safe = true;
    for (int iteration = 0; iteration < 24; ++iteration)
    {
        partialCanvas[static_cast<std::size_t>(iteration)]
            = static_cast<float>(iteration) / 23.0f;
        safe = safe && partialMask.setCanvas(partialCanvas);
        auto block = makeTemporalInput(37, iteration * 37);
        changing.process(block, partialMask,
            { 1000.0f, -1, 120.0, 0.0, false, iteration == 12 });
        safe = safe && bufferFiniteAndBounded(block);
    }
    check(safe, "Spectral canvas changes during playback lost publication or produced invalid audio");
}

void testScrambleProcessor()
{
    randomchop::GridBoundaries noBoundary;
    noBoundary.bpm = 120.0;
    randomchop::GridBoundaries trigger;
    trigger.bpm = 120.0;
    trigger.count = 1;
    trigger.sampleOffsets[0] = 64;

    randomchop::ScrambleProcessor bypass;
    bypass.prepare(1000.0);
    bypass.setSeed(88);
    auto dry = makeTemporalInput(256);
    auto bypassed = copyBuffer(dry);
    bypass.process(bypassed, trigger, 1, { 0.0f });
    check(buffersEqual(dry, bypassed) && bypass.getActivationCount() == 0,
          "Scramble Amount 0 was not transparent");

    randomchop::ScrambleProcessor first;
    randomchop::ScrambleProcessor second;
    first.prepare(1000.0);
    second.prepare(1000.0);
    first.setSeed(999);
    second.setSeed(999);
    auto fillA = makeTemporalInput(256);
    auto fillB = copyBuffer(fillA);
    first.process(fillA, noBoundary, 1, {});
    second.process(fillB, noBoundary, 1, {});
    auto armDry = makeTemporalInput(32, 256);
    auto armA = copyBuffer(armDry);
    auto armB = copyBuffer(armDry);
    first.process(armA, noBoundary, 1, { 100.0f });
    second.process(armB, noBoundary, 1, { 100.0f });
    check(first.isArmed() && second.isArmed()
              && first.getActivationCount() == 0
              && buffersEqual(armA, armDry) && buffersEqual(armB, armDry),
          "Scramble did not arm silently while waiting for the next grid boundary");

    auto eventDry = makeTemporalInput(256, 288);
    auto eventA = copyBuffer(eventDry);
    auto eventB = copyBuffer(eventDry);
    first.process(eventA, trigger, 1, { 100.0f });
    second.process(eventB, trigger, 1, { 100.0f });
    check(first.getActivationCount() == 1 && first.getLastCaptureFrames() == 125,
          "armed Scramble did not begin at the next grid boundary with bounded capture");
    check(buffersEqual(eventA, eventB) && bufferFiniteAndBounded(eventA),
          "Scramble fixed-seed output was non-deterministic or invalid");
    bool unchangedBeforeBoundary = true;
    bool changedAfterBoundary = false;
    for (int frame = 0; frame < eventA.getNumSamples(); ++frame)
    {
        const auto different = eventA.getSample(0, frame) != eventDry.getSample(0, frame);
        if (frame < 64)
            unchangedBeforeBoundary = unchangedBeforeBoundary && !different;
        else
            changedAfterBoundary = changedAfterBoundary || different;
    }
    check(unchangedBeforeBoundary && changedAfterBoundary,
          "Scramble changed audio before its grid start or never rearranged a chunk");

    randomchop::ScrambleProcessor low;
    low.prepare(1000.0);
    low.setSeed(999);
    auto lowFill = makeTemporalInput(256);
    low.process(lowFill, noBoundary, 1, {});
    auto lowOutput = copyBuffer(eventDry);
    low.process(lowOutput, trigger, 1, { 10.0f });
    check(low.getActivationCount() == 1 && bufferFiniteAndBounded(lowOutput)
              && !buffersEqual(lowOutput, eventDry),
          "low Scramble Amount did not produce a bounded understandable rearrangement");

    auto finishEvent = makeTemporalInput(400, 512);
    first.process(finishEvent, noBoundary, 1, { 100.0f });
    randomchop::GridBoundaries immediateTrigger;
    immediateTrigger.bpm = 120.0;
    immediateTrigger.count = 1;
    immediateTrigger.sampleOffsets[0] = 0;
    const auto activationsBeforeRepeat = first.getActivationCount();
    auto repeated = makeTemporalInput(32, 912);
    first.process(repeated, immediateTrigger, 1, { 100.0f });
    check(first.getActivationCount() == activationsBeforeRepeat + 1 && first.isActive(),
          "Scramble discarded its history and left the next grid empty");

    constexpr std::array<float, 4> amounts { 25.0f, 50.0f, 75.0f, 100.0f };
    int previousBudget = 0;
    for (const auto amount : amounts)
    {
        randomchop::ScrambleProcessor progressive;
        progressive.prepare(1000.0);
        progressive.setSeed(4567);
        auto fill = makeTemporalInput(256);
        progressive.process(fill, noBoundary, 1, {});
        auto transformed = makeTemporalInput(192, 256);
        progressive.process(transformed, immediateTrigger, 1, { amount });
        const auto budget = progressive.getLastManipulatedSlices();
        check(progressive.getActivationCount() == 1 && budget >= previousBudget
                  && bufferFiniteAndBounded(transformed),
              "Scramble macro did not provide a monotonic per-grid event budget");
        previousBudget = budget;
    }

    bool sawIntegratedPitch = false;
    for (uint64_t seed = 1; seed <= 24; ++seed)
    {
        randomchop::ScrambleProcessor pitched;
        pitched.prepare(1000.0);
        pitched.setSeed(seed);
        auto fill = makeTemporalInput(256);
        pitched.process(fill, noBoundary, 1, {});
        auto output = makeTemporalInput(192, 256);
        pitched.process(output, immediateTrigger, 1, { 100.0f });
        sawIntegratedPitch = sawIntegratedPitch || pitched.getLastPitchedSlices() > 0;
    }
    check(sawIntegratedPitch,
          "Scramble no longer exposes integrated octave/pitched fragments at high intensity");

    auto disableBlock = makeTemporalInput(16, 944);
    first.process(disableBlock, noBoundary, 1, { 0.0f });
    check(!first.isActive() && !first.isArmed() && bufferFiniteAndBounded(disableBlock),
          "disabling Scramble did not finish its bounded release transition");
    auto settledBypass = makeTemporalInput(16, 960);
    const auto settledDry = copyBuffer(settledBypass);
    first.process(settledBypass, noBoundary, 1, { 0.0f });
    check(buffersEqual(settledBypass, settledDry),
          "settled Scramble bypass was not sample-identical");
    auto rearmBlock = makeTemporalInput(16, 960);
    const auto rearmDry = copyBuffer(rearmBlock);
    first.process(rearmBlock, noBoundary, 1, { 65.0f });
    check(first.isArmed() && !first.isActive() && buffersEqual(rearmBlock, rearmDry),
          "re-enabling Scramble did not wait for a fresh musical boundary");

    randomchop::ScrambleProcessor fractional;
    fractional.prepare(48000.0);
    fractional.setSeed(1223);
    auto fractionalFill = makeTemporalInput(12000);
    fractional.process(fractionalFill, noBoundary, 1, {});
    randomchop::GridBoundaries fractionalBoundaries;
    fractionalBoundaries.bpm = 123.0;
    fractionalBoundaries.count = 4;
    fractionalBoundaries.sampleOffsets[0] = 0;
    fractionalBoundaries.sampleOffsets[1] = 5854;
    fractionalBoundaries.sampleOffsets[2] = 11707;
    fractionalBoundaries.sampleOffsets[3] = 17561;
    auto fractionalAudio = makeTemporalInput(20000, 12000);
    fractional.process(fractionalAudio, fractionalBoundaries, 1, { 10.0f });
    check(fractional.getActivationCount() == 4,
          "one-grid Scramble events still skipped fractional-tempo boundaries");

    randomchop::GridBoundaries discontinuity;
    discontinuity.bpm = 120.0;
    discontinuity.transportDiscontinuity = true;
    auto seekOutput = makeTemporalInput(32, 512);
    const auto seekDry = copyBuffer(seekOutput);
    first.process(seekOutput, discontinuity, 1, { 100.0f });
    check(!first.isActive() && buffersEqual(seekOutput, seekDry),
          "Scramble retained stale chunks across a transport discontinuity");
}

void testStateMigration()
{
    juce::ValueTree state("PARAMETERS");
    state.setProperty("output", -3.0f, nullptr);
    state.setProperty("randomStart", 75.0f, nullptr);
    state.setProperty("reverseChance", 100.0f, nullptr);
    state.setProperty("bitDepth", 12.0f, nullptr);
    state.setProperty("freezeChance", 80.0f, nullptr);
    state.setProperty("codecAmount", 70.0f, nullptr);
    state.setProperty("fractureCharacter", 42.0f, nullptr);
    state.setProperty("fractureMix", 80.0f, nullptr);
    state.setProperty("meltReverseChance", 88.0f, nullptr);
    state.setProperty("rootNote", 72, nullptr);
    state.setProperty("globalGrid", 2, nullptr);
    state.setProperty("spectralScanRate", 3, nullptr);
    for (const auto* id : { "output", "randomStart", "reverseChance", "retriggerChance",
                            "stepLength", "bitDepth", "takeSelection", "seed",
                            "freezeSize", "codecQuality", "fractureDrive",
                            "fractureCharacter", "fractureMix", "finalLength",
                            "attack", "release", "rateReduction", "meltReverseChance",
                            "rootNote", "globalGrid", "spectralScanRate" })
    {
        juce::ValueTree parameter("PARAM");
        parameter.setProperty("id", id, nullptr);
        parameter.setProperty("value", 0.5, nullptr);
        state.appendChild(parameter, nullptr);
    }
    state.appendChild(juce::ValueTree("STEP_MASK"), nullptr);
    state.appendChild(juce::ValueTree("TAKE_HISTORY"), nullptr);
    randomchop::removeLegacyState(state);
    check(state.hasProperty("output")
              && !state.hasProperty("randomStart")
              && !state.hasProperty("reverseChance")
              && !state.hasProperty("bitDepth")
              && !state.hasProperty("freezeChance")
              && !state.hasProperty("codecAmount")
              && !state.hasProperty("fractureCharacter")
               && !state.hasProperty("fractureMix")
               && !state.hasProperty("meltReverseChance")
               && !state.hasProperty("rootNote")
               && !state.hasProperty("globalGrid")
               && !state.hasProperty("spectralScanRate")
              && state.getNumChildren() == 1
              && state.getChild(0).getProperty("id").toString() == "output"
              && static_cast<int>(state.getProperty("stateVersion"))
                    == randomchop::currentStateVersion,
          "legacy state migration did not ignore only removed controls and nodes");
    check(randomchop::isRemovedParameterId("dropChance")
              && randomchop::isRemovedParameterId("freezeSize")
              && randomchop::isRemovedParameterId("codecQuality")
              && randomchop::isRemovedParameterId("fractureCharacter")
              && randomchop::isRemovedParameterId("fractureMix")
              && randomchop::isRemovedParameterId("randomStart")
              && randomchop::isRemovedParameterId("finalLength")
              && randomchop::isRemovedParameterId("attack")
              && randomchop::isRemovedParameterId("release")
               && randomchop::isRemovedParameterId("rateReduction")
               && randomchop::isRemovedParameterId("meltReverseChance")
               && randomchop::isRemovedParameterId("rootNote")
               && randomchop::isRemovedParameterId("globalGrid")
               && randomchop::isRemovedParameterId("spectralScanRate")
              && !randomchop::isRemovedParameterId("output"),
          "legacy parameter allow/deny boundary changed");
}

}

int main()
{
    testSupportedFormatsAndPoolState();
    testEqualSelectionAndPitch();
    testRegionsAndVoices();
    testMeltSingleMacroProgression();
    testHostGrid();
    testFullCreativeChainSafety();
    testCreativeMacroProgressionAndRender();
    testScrambleProcessor();
    testMeltProcessor();
    testSmearProcessor();
    testSpectralMaskPublicationAndState();
    testSpectralDrawProcessor();
    testStateMigration();
    if (failures == 0)
        std::cout << "All recompiler.dll foundation tests passed.\n";
    return failures == 0 ? 0 : 1;
}

