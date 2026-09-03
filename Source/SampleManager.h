#pragma once

#include <JuceHeader.h>
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
    SampleSettings settings;
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double sampleRate = 44100.0;

    bool isPlayable() const noexcept
    {
        return settings.enabled && !settings.missing && audio != nullptr
            && audio->getNumChannels() > 0 && audio->getNumSamples() > 1;
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
    void remove(size_t index);
    void clear();
    void setEnabled(size_t index, bool enabled);
    void setAllEnabled(bool enabled);
    void updateSettings(size_t index, const std::function<void(SampleSettings&)>& update);
    std::shared_ptr<const Pool> getSnapshot() const noexcept;
    juce::ValueTree createState() const;
    std::vector<juce::String> restoreState(const juce::ValueTree& state);
    int size() const noexcept;
    static bool isSupported(const juce::File& file);

private:
    SamplePtr loadFile(const juce::File& file, const SampleSettings* restored,
                       std::vector<juce::String>& errors);
    void publish(std::shared_ptr<const Pool> next);
    juce::AudioFormatManager formats;
    std::shared_ptr<const Pool> pool { std::make_shared<const Pool>() };
    // Retired snapshots keep RT shared_ptr releases from becoming deallocations.
    std::vector<std::shared_ptr<const Pool>> retiredPools;
    std::mutex mutationMutex;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleManager)
};
