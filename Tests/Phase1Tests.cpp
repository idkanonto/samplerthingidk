#include <JuceHeader.h>
#include "RandomSamplerVoice.h"
#include "SampleManager.h"
#include "SourceSelection.h"
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
            settings.gainDb = -7.5f;
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
        check(std::abs(restored->front()->settings.gainDb + 7.5f) < 0.001f,
              "source gain was not restored");
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
    state.appendChild(source, nullptr);

    SampleManager manager;
    const auto errors = manager.restoreState(state);
    const auto restored = manager.getSnapshot();
    check(errors.size() == 1 && restored->size() == 1,
          "missing invalid-region source was not represented safely");
    check(std::abs(restored->front()->settings.startNormalised - 0.9) < 0.000001
              && std::abs(restored->front()->settings.endNormalised - 0.9) < 0.000001,
          "invalid restored marker order was not clamped");
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
    SampleManager::SamplePtr source = mutableSource;
    const randomchop::FrameRegion region { 3, 7 };

    auto renderDirection = [&](bool reverse)
    {
        RandomSamplerVoice voice;
        voice.prepare(1000.0);
        juce::AudioBuffer<float> output(2, 16);
        output.clear();
        const auto start = reverse ? randomchop::lastInterpolationPosition(region) : 3.0;
        voice.start(source, 60, 1.0f, start, region, reverse,
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
}

int main()
{
    testWeightedSelection();
    testPoolLimitAndPersistentSettings();
    testParameterOnlyStateClearsSamples();
    testRegionClampingAndRestore();
    testRandomStartAndReadBounds();
    testVoiceRegionBoundaries();
    testWaveformPeakExtrema();
    if (failures == 0)
        std::cout << "All Phase 1 tests passed.\n";
    return failures == 0 ? 0 : 1;
}
