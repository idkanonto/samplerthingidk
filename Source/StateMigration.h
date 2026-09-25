#pragma once

#include <JuceHeader.h>
#include "OutputGain.h"
#include <array>

namespace randomchop
{
inline constexpr int currentStateVersion = 12;

inline void migrateOutputToPercent(juce::ValueTree& state, int restoredVersion)
{
    if (restoredVersion >= 12)
        return;

    if (state.hasProperty("output"))
        state.setProperty("output", outputDecibelsToPercent(
            static_cast<float>(state.getProperty("output"))), nullptr);

    for (auto child : state)
    {
        if (child.getProperty("id").toString() == "output" && child.hasProperty("value"))
            child.setProperty("value", outputDecibelsToPercent(
                static_cast<float>(child.getProperty("value"))), nullptr);
    }
}

inline bool isRemovedParameterId(const juce::String& id) noexcept
{
    constexpr std::array<const char*, 34> removed {
        "reverseChance", "retriggerChance", "retriggerSize", "retriggerCount",
        "skipChance", "reorderChance", "bendChance", "dropChance",
        "stepLength", "bitDepth", "takeSelection",
        "seed", "freezeChance", "freezeSize", "freezeHold",
        "freezeOctaveChance", "scrambleChance", "fractureDrive",
        "fractureFilterMorph", "fractureFrequency", "fractureResonance",
        "codecAmount", "codecQuality", "fractureCharacter", "fractureMix",
        "randomStart", "finalLength", "attack", "release", "rateReduction",
        "meltReverseChance", "rootNote", "globalGrid", "spectralScanRate"
    };
    for (const auto* candidate : removed)
        if (id == candidate)
            return true;
    return false;
}

inline void removeLegacyState(juce::ValueTree& state)
{
    for (int propertyIndex = state.getNumProperties(); --propertyIndex >= 0;)
    {
        const auto propertyName = state.getPropertyName(propertyIndex);
        if (isRemovedParameterId(propertyName.toString()))
            state.removeProperty(propertyName, nullptr);
    }
    for (int index = state.getNumChildren(); --index >= 0;)
    {
        const auto child = state.getChild(index);
        const auto id = child.getProperty("id").toString();
        if (child.hasType("STEP_MASK") || child.hasType("TAKE_HISTORY")
            || isRemovedParameterId(id))
            state.removeChild(index, nullptr);
    }
    state.setProperty("stateVersion", currentStateVersion, nullptr);
}
}

