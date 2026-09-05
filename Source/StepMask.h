#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>

namespace randomchop
{
class StepMask final
{
public:
    static constexpr int maximumSteps = 16;
    static constexpr int defaultLength = 8;
    static constexpr std::array<int, 4> allowedLengths { 2, 4, 8, 16 };

    struct StepResult
    {
        int index = 0;
        int length = defaultLength;
        bool chanceAllowed = true;
        bool cycleCompleted = false;
        bool sequenceReset = false;
    };

    static constexpr int lengthFromChoice(int choice) noexcept
    {
        return choice >= 0 && choice < static_cast<int>(allowedLengths.size())
            ? allowedLengths[static_cast<std::size_t>(choice)] : defaultLength;
    }

    static constexpr int sanitiseLength(int length) noexcept
    {
        for (const auto allowed : allowedLengths)
            if (length == allowed)
                return length;
        return defaultLength;
    }

    StepResult consume(int requestedLength) noexcept
    {
        const auto length = sanitiseLength(requestedLength);
        const auto generation = resetGeneration.load(std::memory_order_acquire);
        const bool shouldReset = length != audioLength || generation != observedResetGeneration;
        if (shouldReset)
        {
            cursor = 0;
            audioLength = length;
            observedResetGeneration = generation;
        }

        const auto bits = maskBits.load(std::memory_order_acquire);
        StepResult result;
        result.index = cursor;
        result.length = length;
        result.chanceAllowed = (bits & static_cast<std::uint16_t>(1u << cursor)) != 0;
        result.sequenceReset = shouldReset;

        ++cursor;
        if (cursor >= length)
        {
            cursor = 0;
            result.cycleCompleted = true;
        }
        return result;
    }

    bool isFxStep(int index) const noexcept
    {
        if (index < 0 || index >= maximumSteps)
            return false;
        return (maskBits.load(std::memory_order_acquire)
                & static_cast<std::uint16_t>(1u << index)) != 0;
    }

    void setStep(int index, bool allowChance) noexcept
    {
        if (index < 0 || index >= maximumSteps)
            return;

        const auto bit = static_cast<std::uint16_t>(1u << index);
        auto current = maskBits.load(std::memory_order_relaxed);
        for (;;)
        {
            const auto replacement = allowChance
                ? static_cast<std::uint16_t>(current | bit)
                : static_cast<std::uint16_t>(current & static_cast<std::uint16_t>(~bit));
            if (maskBits.compare_exchange_weak(current, replacement,
                                               std::memory_order_release,
                                               std::memory_order_relaxed))
                return;
        }
    }

    void setAll(bool allowChance) noexcept
    {
        maskBits.store(allowChance ? allFxMask : std::uint16_t { 0 }, std::memory_order_release);
    }

    void setMask(std::uint16_t bits) noexcept
    {
        maskBits.store(bits, std::memory_order_release);
    }

    std::uint16_t getMask() const noexcept
    {
        return maskBits.load(std::memory_order_acquire);
    }

    void requestSequenceReset() noexcept
    {
        resetGeneration.fetch_add(1, std::memory_order_release);
    }

    juce::ValueTree createState() const
    {
        juce::ValueTree state("STEP_MASK");
        state.setProperty("version", 1, nullptr);
        state.setProperty("bits", static_cast<int>(getMask()), nullptr);
        return state;
    }

    void restoreState(const juce::ValueTree& state) noexcept
    {
        std::uint16_t restored = allFxMask;
        if (state.isValid() && state.hasType("STEP_MASK"))
        {
            const auto value = static_cast<int>(
                state.getProperty("bits", static_cast<int>(allFxMask)));
            restored = static_cast<std::uint16_t>(value & allFxMask);
        }
        maskBits.store(restored, std::memory_order_release);
        requestSequenceReset();
    }

private:
    static constexpr std::uint16_t allFxMask = 0xffffu;
    std::atomic<std::uint16_t> maskBits { allFxMask };
    std::atomic<std::uint32_t> resetGeneration { 0 };

    // Only the audio thread reads or writes the event cursor.
    int cursor = 0;
    int audioLength = defaultLength;
    std::uint32_t observedResetGeneration = 0;
};
}
