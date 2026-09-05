#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace randomchop
{
class MasterDigitalProcessor final
{
public:
    static constexpr int bitDepthFromChoice(int choice) noexcept
    {
        return choice <= 0 ? 0 : std::clamp(choice + 3, 4, 24);
    }

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

    void process(juce::AudioBuffer<float>& buffer, int bitDepth,
                 int rateFactor) noexcept
    {
        bitDepth = bitDepth <= 0 ? 0 : std::clamp(bitDepth, 4, 24);
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
                const auto crushed = quantise(sanitise(buffer.getSample(channel, frame)), bitDepth);
                if (capture)
                    held[static_cast<std::size_t>(channel)] = crushed;
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

    static float quantise(float value, int bitDepth) noexcept
    {
        if (bitDepth == 0)
            return value;
        const auto scale = std::ldexp(1.0f, bitDepth - 1);
        return std::round(value * scale) / scale;
    }

    std::array<float, 2> held { 0.0f, 0.0f };
    int samplesUntilCapture = 0;
    int previousFactor = 1;
};
}
