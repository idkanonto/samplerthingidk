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

        manager.updateSettings(id, [](SampleSettings& settings)
        {
            settings.gainDb = std::numeric_limits<float>::quiet_NaN();
            settings.selectionWeight = std::numeric_limits<float>::infinity();
        });
        const auto sanitised = manager.getSnapshot()->front();
        check(sanitised->settings.gainDb == 0.0f
                  && sanitised->settings.selectionWeight == 1.0f,
              "hostile per-source Gain or Weight escaped finite state bounds");

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
        hostileState.appendChild(hostileSource, nullptr);
        SampleManager hostileRestore;
        const auto hostileErrors = hostileRestore.restoreState(hostileState);
        const auto hostileSnapshot = hostileRestore.getSnapshot();
        check(hostileErrors.empty() && !hostileSnapshot->empty()
                  && hostileSnapshot->front()->settings.gainDb == 0.0f
                  && hostileSnapshot->front()->settings.selectionWeight == 1.0f,
              "hostile persisted Gain or Weight escaped finite restore bounds");
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
    SampleManager::Pool hostileWeights {
        makeSource(std::numeric_limits<float>::quiet_NaN()),
        makeSource(std::numeric_limits<float>::infinity())
    };
    bool hostileWeightsSafe = true;
    for (int iteration = 0; iteration < 128; ++iteration)
    {
        const auto selected = randomchop::chooseWeightedSource(hostileWeights, random);
        hostileWeightsSafe = hostileWeightsSafe && (selected == 0 || selected == 1);
    }
    check(hostileWeightsSafe, "non-finite Weight poisoned realtime source selection");

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

void testFractureRateReduction()
{
    check(randomchop::FractureProcessor::rateFactorFromChoice(0) == 1
              && randomchop::FractureProcessor::rateFactorFromChoice(6) == 64
              && randomchop::FractureProcessor::choiceFromRateFactor(16) == 4,
          "rate-reduction choice mapping changed");

    randomchop::FractureProcessor fullRate;
    randomchop::FractureProcessor reducedRate;
    fullRate.prepare(48000.0);
    reducedRate.prepare(48000.0);
    fullRate.setSeed(77);
    reducedRate.setSeed(77);
    auto source = makeTemporalInput(4096);
    auto fullOutput = copyBuffer(source);
    auto reducedOutput = copyBuffer(source);
    fullRate.process(fullOutput, { 100.0f, 80.0f, 1 });
    reducedRate.process(reducedOutput, { 100.0f, 80.0f, 16 });
    check(reducedRate.getLastEffectiveRateFactor() > 1
              && !buffersEqual(fullOutput, reducedOutput)
              && bufferFiniteAndBounded(reducedOutput),
          "Fracture Rate did not participate in the integrated damage path");
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

juce::AudioBuffer<float> renderFracture(float amount,
                                        const juce::AudioBuffer<float>& input)
{
    auto output = copyBuffer(input);
    randomchop::FractureProcessor processor;
    processor.prepare(48000.0);
    processor.setSeed(0xf12a);
    processor.process(output, { amount, 67.0f, 16 });
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
    std::array<float, levels.size()> fractureDistance {};
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
        const auto fracture = renderFracture(level, dry);
        const auto smear = renderSmear(level, dry);
        scrambleDistance[index] = differenceRms(dry, scramble);
        fractureDistance[index] = differenceRms(dry, fracture);
        smearDistance[index] = differenceRms(dry, smear);
        check(bufferFiniteAndBounded(scramble) && bufferFiniteAndBounded(fracture)
                  && bufferFiniteAndBounded(smear),
              "a creative macro listening render was invalid or unbounded");
        if (renderPath.isNotEmpty())
        {
            const auto suffix = juce::String(static_cast<int>(level)).paddedLeft('0', 3) + ".wav";
            check(writeListeningWave(renderDirectory.getChildFile("scramble_" + suffix), scramble)
                      && writeListeningWave(renderDirectory.getChildFile("fracture_" + suffix), fracture)
                      && writeListeningWave(renderDirectory.getChildFile("smear_" + suffix), smear),
                  "could not write a creative macro listening render");
        }
    }

    check(scrambleDistance[0] == 0.0f && fractureDistance[0] == 0.0f
              && smearDistance[0] == 0.0f,
          "a flagship macro was not sample-identical at zero");
    check(scrambleDistance[2] > 0.01f && scrambleDistance[4] > scrambleDistance[1],
          "Scramble did not create consistent medium or stronger maximum transformation");
    check(fractureDistance[2] > 0.01f && fractureDistance[4] > fractureDistance[1],
          "Fracture did not create a meaningful macro intensity progression");
    check(smearDistance[2] > 0.002f && smearDistance[4] > smearDistance[1]
              && firstDifferenceRms(renderSmear(75.0f, dry))
                    > 0.35f * firstDifferenceRms(dry),
          "Smear did not retain meaningful crystalline/high-frequency activity");

    std::cout << "Macro render difference RMS"
              << " | Scramble " << scrambleDistance[0] << ',' << scrambleDistance[1]
              << ',' << scrambleDistance[2] << ',' << scrambleDistance[3]
              << ',' << scrambleDistance[4]
              << " | Fracture " << fractureDistance[0] << ',' << fractureDistance[1]
              << ',' << fractureDistance[2] << ',' << fractureDistance[3]
              << ',' << fractureDistance[4]
              << " | Smear " << smearDistance[0] << ',' << smearDistance[1]
              << ',' << smearDistance[2] << ',' << smearDistance[3]
              << ',' << smearDistance[4] << '\n';
}

void testFullCreativeChainSafety()
{
    constexpr double sampleRate = 48000.0;
    randomchop::ScrambleProcessor scramble;
    randomchop::FractureProcessor fracture;
    randomchop::SpectralDrawProcessor spectral;
    randomchop::SmearProcessor smear;
    scramble.prepare(sampleRate);
    fracture.prepare(sampleRate);
    spectral.prepare(sampleRate);
    smear.prepare(sampleRate);
    scramble.setSeed(0x5678);
    fracture.setSeed(0x9abc);
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
        fracture.process(block, { 100.0f, 100.0f, 64 });
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

void testFractureProcessorAndPresets()
{
    check(randomchop::fracturePresets.size() == 30,
          "Fracture factory preset bank does not contain 30 real presets");
    for (std::size_t index = 0; index < randomchop::fracturePresets.size(); ++index)
    {
        const auto& preset = randomchop::getFracturePreset(static_cast<int>(index));
        const auto& settings = preset.settings;
        check(juce::String(preset.name).isNotEmpty()
                  && settings.amount >= 0.0f && settings.amount <= 100.0f
                  && settings.character >= 0.0f && settings.character <= 100.0f
                  && settings.rateFactor >= 1 && settings.rateFactor <= 64,
              "Fracture factory preset contains an invalid name or parameter value");
        randomchop::FractureProcessor presetProcessor;
        presetProcessor.prepare(48000.0);
        presetProcessor.setSeed(0x4000 + index);
        auto audio = makeTemporalInput(1024, static_cast<int>(index) * 17);
        presetProcessor.process(audio, settings);
        check(bufferFiniteAndBounded(audio),
              "Fracture factory preset produced invalid or unbounded audio");
    }
    check(juce::String(randomchop::getFracturePreset(-1).name) == "Glass Teeth"
              && juce::String(randomchop::getFracturePreset(999).name) == "Cold Wire",
          "Fracture preset index bounds changed");

    randomchop::FractureProcessor bypass;
    bypass.prepare(48000.0);
    auto dry = makeTemporalInput(512);
    auto output = copyBuffer(dry);
    bypass.process(output, { 0.0f, 100.0f, 64 });
    check(buffersEqual(dry, output), "Fracture 0 was not sample-identical bypass");

    randomchop::FractureProcessor first;
    randomchop::FractureProcessor second;
    first.prepare(48000.0);
    second.prepare(48000.0);
    first.setSeed(991);
    second.setSeed(991);
    auto animatedA = makeTemporalInput(24000);
    auto animatedB = copyBuffer(animatedA);
    first.process(animatedA, { 72.0f, 63.0f, 8 });
    second.process(animatedB, { 72.0f, 63.0f, 8 });
    check(buffersEqual(animatedA, animatedB)
              && first.getLastMotionDepth() > 0.0f
              && first.getLastEffectiveRateFactor() > 1,
          "Fracture motion/digital character was not deterministic or active");

    randomchop::FractureProcessor extreme;
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
    extreme.process(unsafe, { 1000.0f, 1000.0f, 999 });
    check(bufferFiniteAndBounded(unsafe),
          "Fracture extreme macro state propagated NaN, Inf, or runaway gain");

    randomchop::FractureProcessor silenceProcessor;
    silenceProcessor.prepare(48000.0);
    silenceProcessor.setSeed(321);
    juce::AudioBuffer<float> silence(2, 48000);
    silence.clear();
    silenceProcessor.process(silence, { 100.0f, 100.0f, 64 });
    check(silence.getMagnitude(0, silence.getNumSamples()) == 0.0f
              && silence.getMagnitude(1, silence.getNumSamples()) == 0.0f,
          "Fracture generated self-noise or residual DC from silence");
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
              && continuous.getActiveGrainCount() > 0,
          "Smear did not retain a bounded cross-block crystal-grain cloud");

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
    auto fullyDrawn = makeTemporalInput(8192);
    muted.process(fullyDrawn, fullMask,
        { 100.0f, 1, 120.0, 0.0, false, false });
    check(fullyDrawn.getMagnitude(randomchop::SpectralDrawProcessor::latencySamples,
                                  fullyDrawn.getNumSamples()
                                      - randomchop::SpectralDrawProcessor::latencySamples)
              < 0.00001f,
          "full Spectral canvas at maximum Depth did not attenuate all bins");

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
    auto eventDry = makeTemporalInput(256, 256);
    auto eventA = copyBuffer(eventDry);
    auto eventB = copyBuffer(eventDry);
    first.process(eventA, trigger, 1, { 100.0f });
    second.process(eventB, trigger, 1, { 100.0f });
    check(first.getActivationCount() == 1 && first.getLastCaptureFrames() == 125,
          "Scramble did not begin at the grid boundary with bounded chunk capture");
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
    state.setProperty("randomStart", 75.0f, nullptr);
    state.setProperty("reverseChance", 100.0f, nullptr);
    state.setProperty("bitDepth", 12.0f, nullptr);
    state.setProperty("freezeChance", 80.0f, nullptr);
    state.setProperty("codecAmount", 70.0f, nullptr);
    for (const auto* id : { "randomStart", "reverseChance", "retriggerChance",
                            "stepLength", "bitDepth", "takeSelection", "seed",
                            "freezeSize", "codecQuality", "fractureDrive" })
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
              && !state.hasProperty("freezeChance")
              && !state.hasProperty("codecAmount")
              && state.getNumChildren() == 1
              && state.getChild(0).getProperty("id").toString() == "randomStart"
              && static_cast<int>(state.getProperty("stateVersion"))
                    == randomchop::currentStateVersion,
          "legacy state migration did not ignore only removed controls and nodes");
    check(randomchop::isRemovedParameterId("dropChance")
              && randomchop::isRemovedParameterId("freezeSize")
              && randomchop::isRemovedParameterId("codecQuality")
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

void testStretchStaleJobsAndRemoval()
{
    juce::WavAudioFormat wav;
    const auto file = makeAudioFixture(wav, ".wav");
    check(file.existsAsFile(), "could not create stale-stretch fixture");
    if (!file.existsAsFile())
        return;

    std::atomic<int> callCount { 0 };
    std::atomic<int> blockedCall { 1 };
    std::atomic<bool> allowBlockedCall { false };
    const auto waitUntil = [](const auto& predicate)
    {
        for (int attempt = 0; attempt < 400; ++attempt)
        {
            if (predicate())
                return true;
            juce::Thread::sleep(5);
        }
        return predicate();
    };

    {
        SampleManager manager([&](const auto& decoded, double sampleRate,
                                  float ratio, uint64_t revision)
        {
            const auto call = callCount.fetch_add(1) + 1;
            while (call == blockedCall.load() && !allowBlockedCall.load())
                juce::Thread::sleep(1);
            auto result = std::make_shared<PreparedSampleData>();
            result->sampleRate = sampleRate;
            result->revision = revision;
            result->stretchRatio = ratio;
            const auto frames = std::max(2, static_cast<int>(std::llround(
                static_cast<double>(decoded->getNumSamples()) * ratio)));
            auto output = std::make_shared<juce::AudioBuffer<float>>(
                decoded->getNumChannels(), frames);
            output->clear();
            result->audio = std::move(output);
            return PreparedSamplePtr(result);
        });
        const juce::StringArray paths { file.getFullPathName() };
        const auto errors = manager.addFiles(paths);
        check(errors.empty() && manager.size() == 1,
              "stale-stretch fixture did not load");
        if (manager.size() == 1)
        {
            const auto id = manager.getSnapshot()->front()->settings.id;
            manager.updateSettings(id, [](SampleSettings& settings)
            {
                settings.stretchRatio = 2.0f;
            });
            const auto firstStarted = waitUntil([&] { return callCount.load() >= 1; });
            check(firstStarted, "first stretch revision did not start");
            manager.updateSettings(id, [](SampleSettings& settings)
            {
                settings.stretchRatio = 3.0f;
            });
            allowBlockedCall.store(true);
            const auto newestPublished = waitUntil([&]
            {
                const auto snapshot = manager.getSnapshot();
                return !snapshot->empty() && !snapshot->front()->stretchPending
                    && snapshot->front()->prepared != nullptr
                    && snapshot->front()->prepared->revision == 2
                    && snapshot->front()->prepared->stretchRatio == 3.0f;
            });
            check(newestPublished,
                  "a stale stretch result replaced or blocked the newest revision");

            const auto activeOldVersion = manager.getSnapshot()->front()->prepared;
            allowBlockedCall.store(false);
            const auto removalCall = callCount.load() + 1;
            blockedCall.store(removalCall);
            manager.updateSettings(id, [](SampleSettings& settings)
            {
                settings.stretchRatio = 4.0f;
            });
            const auto removalJobStarted = waitUntil(
                [&] { return callCount.load() >= removalCall; });
            check(removalJobStarted, "removal-race stretch revision did not start");
            manager.remove(id);
            manager.collectGarbage();
            check(manager.size() == 0 && activeOldVersion != nullptr
                      && activeOldVersion->audio != nullptr
                      && activeOldVersion->audio->getNumSamples() > 0,
                  "source removal invalidated an active immutable prepared version");
            allowBlockedCall.store(true);
        }
        else
        {
            allowBlockedCall.store(true);
        }
    }
    check(file.deleteFile(), "could not remove stale-stretch fixture");
}
}

int main()
{
    testSupportedFormatsAndPoolState();
    testWeightedSelectionAndPitch();
    testRegionsAndVoices();
    testFractureRateReduction();
    testHostGrid();
    testFullCreativeChainSafety();
    testCreativeMacroProgressionAndRender();
    testScrambleProcessor();
    testFractureProcessorAndPresets();
    testSmearProcessor();
    testSpectralMaskPublicationAndState();
    testSpectralDrawProcessor();
    testStateMigration();
    testStretchSemanticsAndPublication();
    testStretchStaleJobsAndRemoval();
    if (failures == 0)
        std::cout << "All recompiler.dll foundation tests passed.\n";
    return failures == 0 ? 0 : 1;
}
