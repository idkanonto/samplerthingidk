#include <JuceHeader.h>
#include "SampleManager.h"
#include "SourceSelection.h"
#include <iostream>

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
        check(std::abs(restored->front()->settings.gainDb + 7.5f) < 0.001f,
              "source gain was not restored");
        check(std::abs(restored->front()->settings.selectionWeight - 3.25f) < 0.001f,
              "source weight was not restored");
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
}

int main()
{
    testWeightedSelection();
    testPoolLimitAndPersistentSettings();
    testParameterOnlyStateClearsSamples();
    if (failures == 0)
        std::cout << "All Phase 1 tests passed.\n";
    return failures == 0 ? 0 : 1;
}
