#pragma once

#include "RandomizationEngine.h"
#include "SampleManager.h"
#include <algorithm>
#include <cmath>

namespace randomchop
{
inline int chooseWeightedSource(const SampleManager::Pool& pool,
                                RandomizationEngine& random) noexcept
{
    const auto safeWeight = [](float weight) noexcept
    {
        return std::isfinite(weight) ? std::clamp(weight, 0.01f, 10.0f) : 1.0f;
    };
    double total = 0.0;
    for (const auto& source : pool)
        if (source->isPlayable())
            total += safeWeight(source->settings.selectionWeight);

    if (total <= 0.0)
        return -1;

    double target = random.unit() * total;
    for (size_t i = 0; i < pool.size(); ++i)
    {
        if (!pool[i]->isPlayable())
            continue;

        target -= safeWeight(pool[i]->settings.selectionWeight);
        if (target <= 0.0)
            return static_cast<int>(i);
    }

    // Floating-point rounding can leave a tiny positive remainder.
    for (size_t i = pool.size(); i-- > 0;)
        if (pool[i]->isPlayable())
            return static_cast<int>(i);

    return -1;
}
}
