#pragma once
#include <cstdint>

class RandomizationEngine final
{
public:
    void setSeed(uint64_t newSeed) noexcept { state = newSeed != 0 ? newSeed : 0x9e3779b97f4a7c15ULL; }
    uint64_t next() noexcept
    {
        auto x = state;
        x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
        state = x;
        return x * 2685821657736338717ULL;
    }
    uint32_t bounded(uint32_t bound) noexcept { return bound == 0 ? 0 : static_cast<uint32_t>(next() % bound); }
    double unit() noexcept { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
private:
    uint64_t state = 0x9e3779b97f4a7c15ULL;
};

