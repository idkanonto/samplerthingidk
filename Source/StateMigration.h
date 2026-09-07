#pragma once

#include <JuceHeader.h>
#include <array>

namespace randomchop
{
inline constexpr int currentStateVersion = 3;

inline bool isRemovedParameterId(const juce::String& id) noexcept
{
    constexpr std::array<const char*, 11> removed {
        "reverseChance", "retriggerChance", "retriggerSize", "retriggerCount",
        "skipChance", "reorderChance", "bendChance", "dropChance",
        "stepLength", "bitDepth", "takeSelection"
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
