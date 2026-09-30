#include "StretchPreparation.h"
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <cmath>
#include <limits>
#include <new>

namespace randomchop
{
PreparedSamplePtr prepareStretch(
    const std::shared_ptr<const juce::AudioBuffer<float>>& decodedAudio,
    double sampleRate, float playbackSpeed, uint64_t revision)
{
    if (decodedAudio == nullptr || decodedAudio->getNumChannels() < 1
        || decodedAudio->getNumChannels() > 2 || decodedAudio->getNumSamples() < 2
        || !std::isfinite(sampleRate) || sampleRate <= 0.0)
        return {};

    const auto speed = clampStretchSpeed(playbackSpeed);
    try
    {
        auto prepared = std::make_shared<PreparedSampleData>();
        prepared->sampleRate = sampleRate;
        prepared->revision = revision;
        prepared->stretchSpeed = speed;
        if (std::abs(speed - 1.0f) < 0.000001f)
        {
            prepared->audio = decodedAudio;
            return prepared;
        }

        using Stretcher = signalsmith::stretch::SignalsmithStretch<float>;
        Stretcher stretcher;
        const auto channels = decodedAudio->getNumChannels();
        stretcher.presetDefault(channels, sampleRate);

        // Stretch is a timing-speed control: 0.25x produces four times the
        // source duration, while 2x produces half the source duration. The
        // stretcher preserves pitch; source tuning remains a separate voice
        // playback-rate control.
        const auto durationMultiplier = 1.0 / static_cast<double>(speed);
        const auto inputFrames = decodedAudio->getNumSamples();
        const auto wantedOutput64 = static_cast<int64_t>(std::llround(
            static_cast<double>(inputFrames) * durationMultiplier));
        if (wantedOutput64 < 2 || wantedOutput64 > std::numeric_limits<int>::max())
            return {};

        const auto minimumInput = juce::jmax(2, stretcher.outputSeekLength(speed));
        const auto processingInputFrames = juce::jmax(inputFrames, minimumInput);
        const auto processingOutput64 = static_cast<int64_t>(std::llround(
            static_cast<double>(processingInputFrames) * durationMultiplier));
        if (processingOutput64 < wantedOutput64
            || processingOutput64 > std::numeric_limits<int>::max())
            return {};

        std::shared_ptr<juce::AudioBuffer<float>> paddedInput;
        const juce::AudioBuffer<float>* input = decodedAudio.get();
        if (processingInputFrames != inputFrames)
        {
            paddedInput = std::make_shared<juce::AudioBuffer<float>>(
                channels, processingInputFrames);
            paddedInput->clear();
            for (int channel = 0; channel < channels; ++channel)
                paddedInput->copyFrom(channel, 0, *decodedAudio, channel, 0, inputFrames);
            input = paddedInput.get();
        }

        auto output = std::make_shared<juce::AudioBuffer<float>>(
            channels, static_cast<int>(processingOutput64));
        output->clear();
        if (!stretcher.exact(input->getArrayOfReadPointers(), processingInputFrames,
                             output->getArrayOfWritePointers(),
                             static_cast<int>(processingOutput64)))
            return {};

        output->setSize(channels, static_cast<int>(wantedOutput64), true, false, true);
        for (int channel = 0; channel < channels; ++channel)
        {
            auto* values = output->getWritePointer(channel);
            for (int frame = 0; frame < output->getNumSamples(); ++frame)
                values[frame] = std::isfinite(values[frame])
                    ? std::clamp(values[frame], -8.0f, 8.0f) : 0.0f;
        }
        prepared->audio = std::move(output);
        return prepared;
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
}
}
