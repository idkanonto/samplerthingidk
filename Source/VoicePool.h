#pragma once

#include "RandomSamplerVoice.h"
#include <array>

namespace randomchop
{
enum class VoiceMode
{
    poly = 0,
    mono = 1
};

class VoicePool final
{
public:
    static constexpr size_t capacity = 16;

    void prepare(double outputRate) noexcept
    {
        for (auto& voice : voices)
        {
            voice.forceStop();
            voice.prepare(outputRate);
        }
    }

    RandomSamplerVoice& acquire(VoiceMode mode, float monoReleaseSeconds = 0.003f) noexcept
    {
        if (mode == VoiceMode::mono)
        {
            for (size_t index = 1; index < voices.size(); ++index)
                if (voices[index].isActive())
                    voices[index].release(monoReleaseSeconds);
            return voices.front();
        }

        auto* selected = &voices.front();
        for (auto& candidate : voices)
        {
            if (!candidate.isActive())
                return candidate;
            if (candidate.getAge() < selected->getAge())
                selected = &candidate;
        }
        return *selected;
    }

    void noteOff(int note, float releaseSeconds) noexcept
    {
        for (auto& voice : voices)
            if (voice.isActive() && voice.getNote() == note)
                voice.release(releaseSeconds);
    }

    void render(juce::AudioBuffer<float>& output, int startSample, int numSamples) noexcept
    {
        for (auto& voice : voices)
            voice.render(output, startSample, numSamples);
    }

    size_t activeCount() const noexcept
    {
        size_t count = 0;
        for (const auto& voice : voices)
            if (voice.isActive())
                ++count;
        return count;
    }

    const RandomSamplerVoice& operator[](size_t index) const noexcept { return voices[index]; }

private:
    std::array<RandomSamplerVoice, capacity> voices;
};
}
