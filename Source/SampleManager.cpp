#include "SampleManager.h"
#include <limits>

SampleManager::SampleManager()
{
    formats.registerBasicFormats();
}

void SampleManager::publish(std::shared_ptr<const Pool> next)
{
    retiredPools.push_back(getSnapshot());
    std::atomic_store_explicit(&pool, std::move(next), std::memory_order_release);
}

bool SampleManager::isSupported(const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff";
}

std::vector<juce::String> SampleManager::addFiles(const juce::StringArray& paths)
{
    // Only control/host-state threads enter this path. The audio thread only
    // performs an atomic snapshot load and never takes this mutex.
    const std::lock_guard<std::mutex> lock(mutationMutex);
    auto next = std::make_shared<Pool>(*getSnapshot());
    std::vector<juce::String> errors;

    for (const auto& path : paths)
    {
        const juce::File file(path);
        if (!file.existsAsFile() || !isSupported(file))
        {
            errors.push_back(file.getFileName() + ": unsupported or missing");
            continue;
        }

        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr || reader->lengthInSamples < 2 || reader->numChannels < 1)
        {
            errors.push_back(file.getFileName() + ": could not decode");
            continue;
        }

        auto sample = std::make_shared<SampleData>();
        sample->name = file.getFileName();
        sample->file = file;
        sample->sampleRate = reader->sampleRate;
        const auto length = static_cast<int>(juce::jmin<int64_t>(reader->lengthInSamples,
                                                                  std::numeric_limits<int>::max()));
        sample->audio.setSize(juce::jmin(2, static_cast<int>(reader->numChannels)), length);
        if (!reader->read(&sample->audio, 0, length, 0, true, true))
        {
            errors.push_back(file.getFileName() + ": read failed");
            continue;
        }
        next->push_back(std::move(sample));
    }

    publish(std::shared_ptr<const Pool>(std::move(next)));
    return errors;
}

void SampleManager::remove(size_t index)
{
    const std::lock_guard<std::mutex> lock(mutationMutex);
    auto current = getSnapshot();
    if (index >= current->size()) return;
    auto next = std::make_shared<Pool>(*current);
    next->erase(next->begin() + static_cast<std::ptrdiff_t>(index));
    publish(std::shared_ptr<const Pool>(std::move(next)));
}

void SampleManager::clear()
{
    const std::lock_guard<std::mutex> lock(mutationMutex);
    publish(std::make_shared<const Pool>());
}

std::shared_ptr<const SampleManager::Pool> SampleManager::getSnapshot() const noexcept
{
    return std::atomic_load_explicit(&pool, std::memory_order_acquire);
}

juce::StringArray SampleManager::getPaths() const
{
    juce::StringArray result;
    for (const auto& sample : *getSnapshot()) result.add(sample->file.getFullPathName());
    return result;
}

int SampleManager::size() const noexcept { return static_cast<int>(getSnapshot()->size()); }
