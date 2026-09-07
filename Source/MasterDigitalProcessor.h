#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>

namespace randomchop
{
class MasterDigitalProcessor final
{
public:
    static constexpr int rateFactorFromChoice(int choice) noexcept
    {
        constexpr std::array<int, 7> factors { 1, 2, 4, 8, 16, 32, 64 };
        return choice >= 0 && choice < static_cast<int>(factors.size())
            ? factors[static_cast<std::size_t>(choice)] : 1;
    }

    void reset() noexcept
    {
        held.fill(0.0f);
        samplesUntilCapture = 0;
        previousFactor = 1;
    }

    void process(juce::AudioBuffer<float>& buffer, int rateFactor) noexcept
    {
        rateFactor = std::clamp(rateFactor, 1, 64);
        if (rateFactor != previousFactor)
        {
            samplesUntilCapture = 0;
            previousFactor = rateFactor;
        }

        const auto channels = std::min(2, buffer.getNumChannels());
        for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
        {
            const bool capture = samplesUntilCapture == 0;
            for (int channel = 0; channel < channels; ++channel)
            {
                if (capture)
                    held[static_cast<std::size_t>(channel)] = sanitise(
                        buffer.getSample(channel, frame));
                buffer.setSample(channel, frame, held[static_cast<std::size_t>(channel)]);
            }

            if (capture)
                samplesUntilCapture = rateFactor - 1;
            else
                --samplesUntilCapture;
        }
    }

private:
    static float sanitise(float value) noexcept
    {
        if (!std::isfinite(value))
            return 0.0f;
        return std::clamp(value, -64.0f, 64.0f);
    }

    std::array<float, 2> held { 0.0f, 0.0f };
    int samplesUntilCapture = 0;
    int previousFactor = 1;
};
}
