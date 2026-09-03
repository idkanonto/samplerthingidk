#pragma once

#include <JuceHeader.h>
#include "PlaybackRegion.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

struct SampleSettings final
{
    juce::String id;
    juce::String displayName;
    juce::String filePath;
    bool enabled = true;
    bool missing = false;
    double startNormalised = 0.0;
    double endNormalised = 1.0;
    int sourceKey = 0;
    float gainDb = 0.0f;
    int transposeSemitones = 0;
    float fineTuneCents = 0.0f;
    float stretchRatio = 1.0f;
    float selectionWeight = 1.0f;
};

struct SampleData final
{
    struct WaveformPeak final
    {
        float minimum = 0.0f;
        float maximum = 0.0f;
    };

    using WaveformPeaks = std::vector<WaveformPeak>;

    SampleSettings settings;
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    std::shared_ptr<const WaveformPeaks> waveformPeaks;
    double sampleRate = 44100.0;
    uint64_t runtimeId = 0;

    bool isPlayable() const noexcept
    {
        return settings.enabled && !settings.missing && audio != nullptr
            && audio->getNumChannels() > 0
            && randomchop::makeFrameRegion(audio->getNumSamples(), settings.startNormalised,
                                           settings.endNormalised).canInterpolate();
    }
};

class SampleManager final
{
public:
    static constexpr int maximumSamples = 20;
    using SamplePtr = std::shared_ptr<const SampleData>;
    using Pool = std::vector<SamplePtr>;

    SampleManager();
    std::vector<juce::String> addFiles(const juce::StringArray& paths);
    void remove(const juce::String& id);
    void clear();
    void setEnabled(const juce::String& id, bool enabled);
    void setAllEnabled(bool enabled);
    void updateSettings(const juce::String& id, const std::function<void(SampleSettings&)>& update);
    void collectGarbage();
    std::shared_ptr<const Pool> getSnapshot() const noexcept;
    juce::ValueTree createState() const;
    std::vector<juce::String> restoreState(const juce::ValueTree& state);
    int size() const noexcept;
    static bool isSupported(const juce::File& file);

private:
    SamplePtr loadFile(const juce::File& file, const SampleSettings* restored,
                       std::vector<juce::String>& errors);
    void publish(std::shared_ptr<const Pool> next);
    void collectGarbageLocked();
    static size_t findSource(const Pool&, const juce::String& id) noexcept;
    juce::AudioFormatManager formats;
    std::shared_ptr<const Pool> pool { std::make_shared<const Pool>() };
    // Replaced objects remain owned here until no realtime snapshot, voice, or
    // future Take references them. Collection is performed only by control
    // threads, so the audio thread can never become the last owner.
    std::vector<std::shared_ptr<const Pool>> retiredPools;
    std::vector<SamplePtr> retiredSamples;
    std::mutex mutationMutex;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleManager)
};
