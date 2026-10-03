#pragma once

#include "RandomizationEngine.h"
#include "SampleManager.h"
#include <algorithm>
#include <cmath>

namespace randomchop
{
inline int chooseSource(const SampleManager::Pool& pool,
                        RandomizationEngine& random) noexcept
{
    double totalChance = 0.0;
    for (const auto& source : pool)
        if (source->isPlayable())
            totalChance += std::clamp(
                std::isfinite(source->settings.selectionChance)
                    ? static_cast<double>(source->settings.selectionChance) : 100.0,
                0.0, 100.0);

    if (totalChance <= 0.0)
        return -1;

    auto target = random.unit() * totalChance;
    for (size_t i = 0; i < pool.size(); ++i)
    {
        if (!pool[i]->isPlayable())
            continue;
        const auto chance = std::clamp(
            std::isfinite(pool[i]->settings.selectionChance)
                ? static_cast<double>(pool[i]->settings.selectionChance) : 100.0,
            0.0, 100.0);
        if (chance <= 0.0)
            continue;
        if (target < chance)
            return static_cast<int>(i);
        target -= chance;
    }
    for (size_t i = pool.size(); i-- > 0;)
        if (pool[i]->isPlayable() && pool[i]->settings.selectionChance > 0.0f)
            return static_cast<int>(i);
    return -1;
}
}
