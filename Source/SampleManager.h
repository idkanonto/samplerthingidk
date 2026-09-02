#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

struct SampleData final
{
    juce::String name;
    juce::File file;
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
};

class SampleManager final
{
public:
    using SamplePtr = std::shared_ptr<const SampleData>;
    using Pool = std::vector<SamplePtr>;

    SampleManager();
    std::vector<juce::String> addFiles(const juce::StringArray& paths);
    void remove(size_t index);
    void clear();
    std::shared_ptr<const Pool> getSnapshot() const noexcept;
    juce::StringArray getPaths() const;
    int size() const noexcept;
    static bool isSupported(const juce::File& file);

private:
    void publish(std::shared_ptr<const Pool> next);
    juce::AudioFormatManager formats;
    std::shared_ptr<const Pool> pool { std::make_shared<const Pool>() };
    // Keep replaced snapshots alive until processor teardown. This prevents the
    // audio thread from ever becoming the last owner and running a deallocation.
    std::vector<std::shared_ptr<const Pool>> retiredPools;
    std::mutex mutationMutex;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleManager)
};
