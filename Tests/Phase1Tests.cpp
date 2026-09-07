#include <JuceHeader.h>
#include "HarmonicPitch.h"
#include "HostGrid.h"
#include "MasterDigitalProcessor.h"
#include "RandomSamplerVoice.h"
#include "SampleManager.h"
#include "SourceSelection.h"
#include "StateMigration.h"
#include "VoicePool.h"
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

SampleManager::SamplePtr makeSource(float weight, bool enabled = true, bool missing = false)
{
    auto audio = std::make_shared<juce::AudioBuffer<float>>(1, 64);
    audio->clear();
    auto source = std::make_shared<SampleData>();
    source->settings.id = juce::Uuid().toString();
    source->settings.enabled = enabled;
    source->settings.missing = missing;
    source->settings.selectionWeight = weight;
    source->audio = audio;
    source->prepared = randomchop::prepareStretch(audio, 44100.0, 0.0f, 0);
    return source;
}

PreparedSamplePtr makeVoiceSample(float value, int frames = 1024)
{
    auto audio = std::make_shared<juce::AudioBuffer<float>>(1, frames);
    for (int frame = 0; frame < frames; ++frame)
        audio->setSample(0, frame, value);
    return randomchop::prepareStretch(audio, 1000.0, 0.0f, 0);
}

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
            settings.selectionWeight = 3.25f;
            settings.stretchRatio = 1.0f;
        });
        const auto changed = manager.getSnapshot()->front();
        check(changed->settings.id == id && changed->runtimeId == runtimeId,
              "source identity changed during an immutable settings update");
        check(!changed->settings.enabled && changed->settings.startNormalised == 0.2
                  && changed->settings.endNormalised == 0.8
                  && changed->settings.sourceKey == 8
                  && changed->settings.gainDb == -7.5f
                  && changed->settings.transposeSemitones == -12
                  && changed->settings.fineTuneCents == 37.0f
                  && changed->settings.selectionWeight == 3.25f
                  && changed->settings.stretchRatio == 1.0f,
              "source controls were not preserved in the snapshot");

        const auto state = manager.createState();
        SampleManager restored;
        const auto restoreErrors = restored.restoreState(state);
        check(restoreErrors.empty() && restored.size() == SampleManager::maximumSamples,
              "sample pool state did not restore");
        check(restored.getSnapshot()->front()->settings.id == id,
              "stable source ID was not persisted");
        restored.setAllEnabled(true);
        restored.clear();
        check(restored.size() == 0, "enable-all or clear changed pool semantics");
    }
    check(file.deleteFile(), "could not remove WAV fixture");
}

void testWeightedSelectionAndPitch()
{
    SampleManager::Pool pool { makeSource(1.0f), makeSource(9.0f),
                               makeSource(100.0f, false), makeSource(100.0f, true, true) };
    RandomizationEngine random;
    random.setSeed(123456);
    int first = 0;
    int second = 0;
    for (int iteration = 0; iteration < 10000; ++iteration)
    {
        const auto selected = randomchop::chooseWeightedSource(pool, random);
        first += selected == 0 ? 1 : 0;
        second += selected == 1 ? 1 : 0;
        check(selected == 0 || selected == 1,
              "weighted selection chose disabled or missing source");
    }
    check(second > first * 7 && second < first * 11,
          "weighted selection no longer follows source weights");
    SampleManager::Pool empty { makeSource(1.0f, false) };
    check(randomchop::chooseWeightedSource(empty, random) == -1,
          "empty playable pool did not return the silent sentinel");

    check(randomchop::shortestTonicCorrection(1, 12) == -1,
          "tonic correction no longer uses shortest direction");
    check(std::abs(randomchop::totalPitchSemitones(
        1, 12, 12, 50.0f, true, 84, 72) - 23.5) < 0.000001,
        "combined source, key, tuning, and MIDI pitch calculation changed");
    check(std::abs(randomchop::pitchRatioForSemitones(12.0) - 2.0) < 0.000001,
          "pitch ratio conversion changed");
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
    const auto prepared = randomchop::prepareStretch(audio, 1000.0, 0.0f, 0);
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
}

void testRateReduction()
{
    randomchop::MasterDigitalProcessor processor;
    juce::AudioBuffer<float> buffer(2, 10);
    for (int frame = 0; frame < 10; ++frame)
    {
        buffer.setSample(0, frame, static_cast<float>(frame + 1) * 0.1f);
        buffer.setSample(1, frame, -static_cast<float>(frame + 1) * 0.1f);
    }
    processor.process(buffer, 4);
    check(buffer.getSample(0, 0) == buffer.getSample(0, 3)
              && buffer.getSample(0, 4) == buffer.getSample(0, 7)
              && buffer.getSample(1, 0) == buffer.getSample(1, 3),
          "rate reducer did not preserve stereo sample-and-hold behavior");
    check(randomchop::MasterDigitalProcessor::rateFactorFromChoice(0) == 1
              && randomchop::MasterDigitalProcessor::rateFactorFromChoice(6) == 64,
          "rate-reduction choice mapping changed");

    processor.reset();
    juce::AudioBuffer<float> unsafe(2, 4);
    unsafe.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
    unsafe.setSample(0, 1, std::numeric_limits<float>::infinity());
    unsafe.setSample(0, 2, 1.0e30f);
    unsafe.setSample(0, 3, -1.0e30f);
    unsafe.copyFrom(1, 0, unsafe, 0, 0, 4);
    processor.process(unsafe, 1);
    bool safe = true;
    for (int channel = 0; channel < 2; ++channel)
        for (int frame = 0; frame < 4; ++frame)
            safe = safe && std::isfinite(unsafe.getSample(channel, frame))
                && std::abs(unsafe.getSample(channel, frame)) <= 64.0f;
    check(safe, "rate reducer propagated non-finite or unbounded samples");
}

void testHostGrid()
{
    check(randomchop::HostGrid::quarterNotesPerStep(0) == 0.5
              && randomchop::HostGrid::quarterNotesPerStep(1) == 0.25
              && randomchop::HostGrid::quarterNotesPerStep(2) == 0.125
              && randomchop::HostGrid::quarterNotesPerStep(99) == 0.25,
          "host-grid division mapping changed");

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
    host.ppq = 0.25;
    boundaries = grid.process(1000.0, 64, 1, host);
    check(boundaries.transportDiscontinuity,
          "host seek/loop discontinuity was not detected");

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
}

void testStateMigration()
{
    juce::ValueTree state("PARAMETERS");
    state.setProperty("randomStart", 75.0f, nullptr);
    state.setProperty("reverseChance", 100.0f, nullptr);
    state.setProperty("bitDepth", 12.0f, nullptr);
    for (const auto* id : { "randomStart", "reverseChance", "retriggerChance",
                            "stepLength", "bitDepth", "takeSelection" })
    {
        juce::ValueTree parameter("PARAM");
        parameter.setProperty("id", id, nullptr);
        parameter.setProperty("value", 0.5, nullptr);
        state.appendChild(parameter, nullptr);
    }
    state.appendChild(juce::ValueTree("STEP_MASK"), nullptr);
    state.appendChild(juce::ValueTree("TAKE_HISTORY"), nullptr);
    randomchop::removeLegacyState(state);
    check(state.hasProperty("randomStart")
              && !state.hasProperty("reverseChance")
              && !state.hasProperty("bitDepth")
              && state.getNumChildren() == 1
              && state.getChild(0).getProperty("id").toString() == "randomStart"
              && static_cast<int>(state.getProperty("stateVersion"))
                    == randomchop::currentStateVersion,
          "legacy state migration did not ignore only removed controls and nodes");
    check(randomchop::isRemovedParameterId("dropChance")
              && !randomchop::isRemovedParameterId("rateReduction"),
          "legacy parameter allow/deny boundary changed");
}

void testStretchSemanticsAndPublication()
{
    check(randomchop::clampStretchRatio(0.0f) == 0.0f
              && randomchop::clampStretchRatio(0.5f) == 0.0f
              && randomchop::clampStretchRatio(1.0f) == 1.0f
              && randomchop::clampStretchRatio(5.0f) == 4.0f
              && randomchop::clampStretchRatio(
                     std::numeric_limits<float>::quiet_NaN()) == 0.0f,
          "stretch OFF/original/1x-4x bounds changed");

    auto audio = std::make_shared<juce::AudioBuffer<float>>(1, 2048);
    for (int frame = 0; frame < audio->getNumSamples(); ++frame)
        audio->setSample(0, frame, std::sin(static_cast<float>(frame) * 0.05f));
    const auto off = randomchop::prepareStretch(audio, 48000.0, 0.0f, 1);
    const auto unity = randomchop::prepareStretch(audio, 48000.0, 1.0f, 2);
    const auto four = randomchop::prepareStretch(audio, 48000.0, 4.0f, 3);
    check(off != nullptr && unity != nullptr && off->audio == audio && unity->audio == audio,
          "OFF and 1x stretch no longer reuse original audio");
    check(four != nullptr && four->audio->getNumSamples() == 8192
              && four->stretchRatio == 4.0f,
          "4x background stretch preparation changed duration");

    juce::WavAudioFormat wav;
    const auto file = makeAudioFixture(wav, ".wav");
    if (!file.existsAsFile())
    {
        check(false, "could not create stretch publication fixture");
        return;
    }
    {
        SampleManager manager([](const auto& decoded, double sampleRate,
                                 float ratio, uint64_t revision)
        {
            auto result = std::make_shared<PreparedSampleData>();
            result->sampleRate = sampleRate;
            result->revision = revision;
            result->stretchRatio = ratio;
            auto output = std::make_shared<juce::AudioBuffer<float>>(
                decoded->getNumChannels(), static_cast<int>(decoded->getNumSamples() * ratio));
            output->clear();
            result->audio = output;
            return PreparedSamplePtr(result);
        });
        juce::StringArray paths { file.getFullPathName() };
        check(manager.addFiles(paths).empty(), "stretch publication fixture did not load");
        const auto initial = manager.getSnapshot();
        const auto id = initial->front()->settings.id;
        const auto oldPrepared = initial->front()->prepared;
        manager.updateSettings(id, [](SampleSettings& settings) { settings.stretchRatio = 4.0f; });
        std::shared_ptr<const SampleManager::Pool> current;
        for (int attempt = 0; attempt < 300; ++attempt)
        {
            current = manager.getSnapshot();
            if (!current->front()->stretchPending)
                break;
            juce::Thread::sleep(5);
        }
        check(current != nullptr && !current->front()->stretchPending
                  && current->front()->prepared->revision == 1
                  && current->front()->prepared->stretchRatio == 4.0f,
              "background stretch result was not atomically published");
        check(oldPrepared != current->front()->prepared,
              "stretch publication mutated prepared data in place");
    }
    check(file.deleteFile(), "could not remove stretch publication fixture");
}
}

int main()
{
    testSupportedFormatsAndPoolState();
    testWeightedSelectionAndPitch();
    testRegionsAndVoices();
    testRateReduction();
    testHostGrid();
    testStateMigration();
    testStretchSemanticsAndPublication();
    if (failures == 0)
        std::cout << "All recompiler.dll foundation tests passed.\n";
    return failures == 0 ? 0 : 1;
}
