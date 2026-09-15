#pragma once

#include "RandomizationEngine.h"
#include "SampleManager.h"
namespace randomchop
{
inline int chooseSource(const SampleManager::Pool& pool,
                        RandomizationEngine& random) noexcept
{
    int playableCount = 0;
    for (const auto& source : pool)
        if (source->isPlayable())
            ++playableCount;

    if (playableCount == 0)
        return -1;

    auto target = static_cast<int>(random.unit() * static_cast<double>(playableCount));
    if (target >= playableCount)
        target = playableCount - 1;
    for (size_t i = 0; i < pool.size(); ++i)
    {
        if (!pool[i]->isPlayable())
            continue;
        if (target-- == 0)
            return static_cast<int>(i);
    }
    return -1;
}
}
