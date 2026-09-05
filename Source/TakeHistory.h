#pragma once

#include "ChanceEvents.h"
#include "HarmonicPitch.h"
#include "StretchPreparation.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <utility>

namespace randomchop
{
using StableSourceId = std::array<char, 37>;

inline StableSourceId copyStableSourceId(const juce::String& id) noexcept
{
    StableSourceId result {};
    const auto* text = id.toRawUTF8();
    for (std::size_t index = 0; index + 1 < result.size() && text[index] != '\0'; ++index)
        result[index] = text[index];
    return result;
}

struct TakeEvent final
{
    // The UUID is copied into fixed storage so capture never allocates or owns
    // a ref-counted string on the audio thread.
    StableSourceId sourceId {};
    PreparedSamplePtr prepared;
    FrameRegion region;
    double randomStart = 0.0;
    std::uint64_t sourceRuntimeId = 0;
    int sourceKey = noTonic;
    int transposeSemitones = 0;
    float fineTuneCents = 0.0f;
    float sourceGain = 1.0f;
    bool chanceAllowed = true;
    EventDecision decision;

    bool isPlayable() const noexcept
    {
        return prepared != nullptr && prepared->audio != nullptr && region.canInterpolate();
    }
};

struct Take final
{
    std::array<TakeEvent, 16> events;
    std::uint8_t eventCount = 0;
};

class TakeHistory final
{
public:
    static constexpr int maximumTakes = 8;

    struct BeginResult final
    {
        const TakeEvent* replayEvent = nullptr;
        bool isLive = true;
        bool resetLiveSequence = false;
    };

    BeginResult beginTrigger() noexcept
    {
        BeginResult result;
        const auto generation = requestGeneration.load(std::memory_order_acquire);
        if (generation != observedRequestGeneration)
        {
            observedRequestGeneration = generation;
            discardIncompleteLiveTake();
            const auto request = requestedSelection.load(std::memory_order_relaxed);
            if (request == clearRequest)
            {
                for (auto& take : takes)
                    clearTake(take);
                oldestTake = 0;
                takeCount = 0;
                publishedCount.store(0, std::memory_order_release);
                enterLive();
                result.resetLiveSequence = true;
            }
            else if (request >= 0 && request < takeCount)
            {
                mode = Mode::history;
                selectedTake = request;
                replayCursor = 0;
                publishedSelection.store(selectedTake, std::memory_order_release);
            }
            else
            {
                enterLive();
                result.resetLiveSequence = true;
            }
        }

        if (mode == Mode::history && selectedTake >= 0 && selectedTake < takeCount)
        {
            const auto& take = takeAt(selectedTake);
            if (take.eventCount > 0)
            {
                if (replayCursor >= take.eventCount)
                    replayCursor = 0;
                result.replayEvent = &take.events[replayCursor];
                replayCursor = static_cast<std::uint8_t>((replayCursor + 1) % take.eventCount);
                result.isLive = false;
                return result;
            }
        }

        if (mode == Mode::history)
        {
            enterLive();
            result.resetLiveSequence = true;
        }
        result.isLive = true;
        return result;
    }

    bool recordLiveEvent(const TakeEvent& event, int expectedLength,
                         bool cycleCompleted) noexcept
    {
        if (mode != Mode::live)
            return false;

        expectedLength = std::clamp(expectedLength, 1, 16);
        if (currentTake.eventCount >= expectedLength)
            discardIncompleteLiveTake();

        currentTake.events[currentTake.eventCount] = event;
        ++currentTake.eventCount;

        if (!cycleCompleted)
            return false;
        if (currentTake.eventCount != expectedLength)
        {
            discardIncompleteLiveTake();
            return false;
        }

        const auto writeIndex = takeCount < maximumTakes
            ? (oldestTake + takeCount) % maximumTakes : oldestTake;
        takes[writeIndex] = std::move(currentTake);
        currentTake.eventCount = 0;
        if (takeCount < maximumTakes)
            ++takeCount;
        else
            oldestTake = (oldestTake + 1) % maximumTakes;
        publishedCount.store(takeCount, std::memory_order_release);
        return true;
    }

    void discardIncompleteLiveTake() noexcept
    {
        clearTake(currentTake);
    }

    void requestLive() noexcept
    {
        publishRequest(liveRequest);
    }

    void requestHistory(int logicalIndex) noexcept
    {
        publishRequest(logicalIndex);
    }

    void requestClear() noexcept
    {
        publishedCount.store(0, std::memory_order_release);
        publishedSelection.store(liveRequest, std::memory_order_release);
        publishRequest(clearRequest);
    }

    int getTakeCount() const noexcept
    {
        return publishedCount.load(std::memory_order_acquire);
    }

    int getSelectedTake() const noexcept
    {
        return publishedSelection.load(std::memory_order_acquire);
    }

private:
    enum class Mode { live, history };
    static constexpr int liveRequest = -1;
    static constexpr int clearRequest = -2;

    void publishRequest(int selection) noexcept
    {
        requestedSelection.store(selection, std::memory_order_relaxed);
        requestGeneration.fetch_add(1, std::memory_order_release);
    }

    void enterLive() noexcept
    {
        mode = Mode::live;
        selectedTake = liveRequest;
        replayCursor = 0;
        publishedSelection.store(liveRequest, std::memory_order_release);
    }

    const Take& takeAt(int logicalIndex) const noexcept
    {
        return takes[(oldestTake + logicalIndex) % maximumTakes];
    }

    static void clearTake(Take& take) noexcept
    {
        for (auto& event : take.events)
            event.prepared.reset();
        take.eventCount = 0;
    }

    // Take arrays and their cursors are audio-thread-owned. Prepared pointers
    // originate in SampleManager, whose current/retired roots guarantee that
    // replacing a Take cannot perform final large-buffer destruction here.
    // The editor observes only the atomics below and sends selection requests.
    std::array<Take, maximumTakes> takes;
    Take currentTake;
    int oldestTake = 0;
    int takeCount = 0;
    int selectedTake = liveRequest;
    std::uint8_t replayCursor = 0;
    Mode mode = Mode::live;

    std::atomic<int> requestedSelection { liveRequest };
    std::atomic<std::uint32_t> requestGeneration { 0 };
    std::uint32_t observedRequestGeneration = 0;
    std::atomic<int> publishedCount { 0 };
    std::atomic<int> publishedSelection { liveRequest };
};
}
