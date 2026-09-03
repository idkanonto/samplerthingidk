#include "SampleManager.h"
#include <algorithm>
#include <limits>
#include <new>

namespace
{
constexpr auto samplesType = "SAMPLES";
constexpr auto sampleType = "SAMPLE";
constexpr uint64_t maximumDecodedBytesPerSource = 256ULL * 1024ULL * 1024ULL;
std::atomic<uint64_t> nextRuntimeId { 1 };

std::shared_ptr<const SampleData::WaveformPeaks>
buildWaveformPeaks(const juce::AudioBuffer<float>& audio)
{
    constexpr int maximumPeakCount = 1024;
    const auto sampleCount = audio.getNumSamples();
    const auto channelCount = audio.getNumChannels();
    const auto peakCount = juce::jmin(maximumPeakCount, sampleCount);
    auto peaks = std::make_shared<SampleData::WaveformPeaks>();
    peaks->reserve(static_cast<size_t>(peakCount));

    for (int peak = 0; peak < peakCount; ++peak)
    {
        const auto first = static_cast<int>((static_cast<int64_t>(peak) * sampleCount) / peakCount);
        const auto last = juce::jmax(first + 1,
            static_cast<int>((static_cast<int64_t>(peak + 1) * sampleCount) / peakCount));
        float minimum = 0.0f;
        float maximum = 0.0f;
        for (int channel = 0; channel < channelCount; ++channel)
        {
            const auto* values = audio.getReadPointer(channel);
            for (int frame = first; frame < last; ++frame)
            {
                minimum = juce::jmin(minimum, values[frame]);
                maximum = juce::jmax(maximum, values[frame]);
            }
        }
        peaks->push_back({ minimum, maximum });
    }
    return peaks;
}

SampleSettings readSettings(const juce::ValueTree& node)
{
    SampleSettings s;
    s.id = node.getProperty("id", juce::Uuid().toString()).toString();
    s.displayName = node.getProperty("name").toString();
    s.filePath = node.getProperty("path").toString();
    s.enabled = static_cast<bool>(node.getProperty("enabled", true));
    const auto region = randomchop::clampNormalisedRegion(
        static_cast<double>(node.getProperty("start", 0.0)),
        static_cast<double>(node.getProperty("end", 1.0)));
    s.startNormalised = region.start;
    s.endNormalised = region.end;
    s.sourceKey = juce::jlimit(0, 24, static_cast<int>(node.getProperty("sourceKey", 0)));
    s.gainDb = juce::jlimit(-60.0f, 12.0f, static_cast<float>(node.getProperty("gain", 0.0f)));
    s.transposeSemitones = juce::jlimit(-24, 24, static_cast<int>(node.getProperty("transpose", 0)));
    s.fineTuneCents = juce::jlimit(-100.0f, 100.0f, static_cast<float>(node.getProperty("fineTune", 0.0f)));
    s.stretchRatio = juce::jlimit(0.5f, 2.0f, static_cast<float>(node.getProperty("stretch", 1.0f)));
    s.selectionWeight = juce::jlimit(0.01f, 10.0f, static_cast<float>(node.getProperty("weight", 1.0f)));
    return s;
}
}

SampleManager::SampleManager() { formats.registerBasicFormats(); }

void SampleManager::publish(std::shared_ptr<const Pool> next)
{
    collectGarbageLocked();
    const auto previous = getSnapshot();

    // Retain only sources that disappear or are replaced by a new immutable
    // version. Sources shared unchanged between snapshots remain owned by the
    // new pool and need no additional retirement reference.
    for (const auto& oldSource : *previous)
    {
        const auto stillPublished = std::find(next->begin(), next->end(), oldSource) != next->end();
        if (!stillPublished)
            retiredSamples.push_back(oldSource);
    }

    retiredPools.push_back(previous);
    std::atomic_store_explicit(&pool, std::move(next), std::memory_order_release);
}

void SampleManager::collectGarbageLocked()
{
    std::erase_if(retiredPools, [] (const auto& retired) { return retired.use_count() == 1; });
    std::erase_if(retiredSamples, [] (const auto& retired) { return retired.use_count() == 1; });
}

void SampleManager::collectGarbage()
{
    const std::lock_guard<std::mutex> lock(mutationMutex);
    collectGarbageLocked();
}

size_t SampleManager::findSource(const Pool& sources, const juce::String& id) noexcept
{
    for (size_t i = 0; i < sources.size(); ++i)
        if (sources[i]->settings.id == id)
            return i;
    return sources.size();
}

bool SampleManager::isSupported(const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff"
        || ext == ".mp3" || ext == ".flac";
}

SampleManager::SamplePtr SampleManager::loadFile(const juce::File& file,
                                                  const SampleSettings* restored,
                                                  std::vector<juce::String>& errors)
{
    if (!file.existsAsFile() || !isSupported(file))
    {
        errors.push_back(file.getFileName() + ": unsupported or missing");
        return {};
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples < 2 || reader->numChannels < 1)
    {
        errors.push_back(file.getFileName() + ": could not decode");
        return {};
    }

    const auto channelCount = juce::jmin(2, static_cast<int>(reader->numChannels));
    const auto frameCount = static_cast<uint64_t>(reader->lengthInSamples);
    const auto maximumFrameCount = maximumDecodedBytesPerSource
        / (sizeof(float) * static_cast<uint64_t>(channelCount));
    if (frameCount > maximumFrameCount
        || frameCount > static_cast<uint64_t>(std::numeric_limits<int>::max()))
    {
        errors.push_back(file.getFileName() + ": decoded audio exceeds 256 MiB source limit");
        return {};
    }

    std::shared_ptr<juce::AudioBuffer<float>> buffer;
    try
    {
        buffer = std::make_shared<juce::AudioBuffer<float>>();
        buffer->setSize(channelCount, static_cast<int>(frameCount));
    }
    catch (const std::bad_alloc&)
    {
        errors.push_back(file.getFileName() + ": insufficient memory to decode");
        return {};
    }

    const auto length = static_cast<int>(frameCount);
    if (!reader->read(buffer.get(), 0, length, 0, true, true))
    {
        errors.push_back(file.getFileName() + ": read failed");
        return {};
    }

    try
    {
        auto sample = std::make_shared<SampleData>();
        if (restored != nullptr) sample->settings = *restored;
        if (sample->settings.id.isEmpty()) sample->settings.id = juce::Uuid().toString();
        if (sample->settings.displayName.isEmpty()) sample->settings.displayName = file.getFileName();
        sample->settings.filePath = file.getFullPathName();
        sample->settings.missing = false;
        sample->waveformPeaks = buildWaveformPeaks(*buffer);
        sample->audio = std::move(buffer);
        sample->sampleRate = reader->sampleRate;
        sample->runtimeId = nextRuntimeId.fetch_add(1, std::memory_order_relaxed);
        return sample;
    }
    catch (const std::bad_alloc&)
    {
        errors.push_back(file.getFileName() + ": insufficient memory to prepare waveform");
        return {};
    }
}

std::vector<juce::String> SampleManager::addFiles(const juce::StringArray& paths)
{
    const std::lock_guard<std::mutex> lock(mutationMutex);
    auto next = std::make_shared<Pool>(*getSnapshot());
    std::vector<juce::String> errors;

    for (const auto& path : paths)
    {
        if (static_cast<int>(next->size()) >= maximumSamples)
        {
            errors.push_back(juce::File(path).getFileName() + ": pool is full (20 / 20)");
            continue;
        }
        if (auto loaded = loadFile(juce::File(path), nullptr, errors)) next->push_back(std::move(loaded));
    }

    publish(std::shared_ptr<const Pool>(std::move(next)));
    return errors;
}

void SampleManager::remove(const juce::String& id)
{
    const std::lock_guard<std::mutex> lock(mutationMutex);
    auto current = getSnapshot();
    const auto index = findSource(*current, id);
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

void SampleManager::setEnabled(const juce::String& id, bool enabled)
{
    updateSettings(id, [enabled](SampleSettings& s) { s.enabled = enabled; });
}

void SampleManager::setAllEnabled(bool enabled)
{
    const std::lock_guard<std::mutex> lock(mutationMutex);
    const auto current = getSnapshot();
    auto next = std::make_shared<Pool>();
    next->reserve(current->size());
    for (const auto& source : *current)
    {
        auto copy = std::make_shared<SampleData>(*source);
        copy->settings.enabled = enabled;
        next->push_back(std::move(copy));
    }
    publish(std::shared_ptr<const Pool>(std::move(next)));
}

void SampleManager::updateSettings(const juce::String& id,
                                   const std::function<void(SampleSettings&)>& update)
{
    const std::lock_guard<std::mutex> lock(mutationMutex);
    auto current = getSnapshot();
    const auto index = findSource(*current, id);
    if (index >= current->size()) return;
    auto next = std::make_shared<Pool>(*current);
    auto copy = std::make_shared<SampleData>(*(*next)[index]);
    update(copy->settings);
    const auto region = randomchop::clampNormalisedRegion(copy->settings.startNormalised,
                                                          copy->settings.endNormalised);
    copy->settings.startNormalised = region.start;
    copy->settings.endNormalised = region.end;
    copy->settings.selectionWeight = juce::jlimit(0.01f, 10.0f, copy->settings.selectionWeight);
    (*next)[index] = std::move(copy);
    publish(std::shared_ptr<const Pool>(std::move(next)));
}

std::shared_ptr<const SampleManager::Pool> SampleManager::getSnapshot() const noexcept
{
    return std::atomic_load_explicit(&pool, std::memory_order_acquire);
}

juce::ValueTree SampleManager::createState() const
{
    juce::ValueTree root(samplesType);
    for (const auto& sample : *getSnapshot())
    {
        const auto& s = sample->settings;
        juce::ValueTree node(sampleType);
        node.setProperty("id", s.id, nullptr);
        node.setProperty("name", s.displayName, nullptr);
        node.setProperty("path", s.filePath, nullptr);
        node.setProperty("enabled", s.enabled, nullptr);
        node.setProperty("start", s.startNormalised, nullptr);
        node.setProperty("end", s.endNormalised, nullptr);
        node.setProperty("sourceKey", s.sourceKey, nullptr);
        node.setProperty("gain", s.gainDb, nullptr);
        node.setProperty("transpose", s.transposeSemitones, nullptr);
        node.setProperty("fineTune", s.fineTuneCents, nullptr);
        node.setProperty("stretch", s.stretchRatio, nullptr);
        node.setProperty("weight", s.selectionWeight, nullptr);
        root.appendChild(node, nullptr);
    }
    return root;
}

std::vector<juce::String> SampleManager::restoreState(const juce::ValueTree& state)
{
    const std::lock_guard<std::mutex> lock(mutationMutex);
    auto next = std::make_shared<Pool>();
    std::vector<juce::String> errors;

    for (int i = 0; i < state.getNumChildren() && static_cast<int>(next->size()) < maximumSamples; ++i)
    {
        auto settings = readSettings(state.getChild(i));
        const juce::File file(settings.filePath);
        if (auto loaded = loadFile(file, &settings, errors))
        {
            next->push_back(std::move(loaded));
        }
        else
        {
            auto missing = std::make_shared<SampleData>();
            missing->settings = settings;
            missing->settings.missing = true;
            missing->runtimeId = nextRuntimeId.fetch_add(1, std::memory_order_relaxed);
            if (missing->settings.displayName.isEmpty()) missing->settings.displayName = file.getFileName();
            next->push_back(std::move(missing));
        }
    }
    publish(std::shared_ptr<const Pool>(std::move(next)));
    return errors;
}

int SampleManager::size() const noexcept { return static_cast<int>(getSnapshot()->size()); }
