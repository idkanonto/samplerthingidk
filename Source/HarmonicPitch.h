#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace randomchop
{
constexpr int noTonic = 0;
constexpr int chromaticTonicCount = 12;

inline constexpr std::array<const char*, chromaticTonicCount + 1> tonicNames
{
    "NONE", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

inline int clampTonic(int tonic) noexcept
{
    return std::clamp(tonic, noTonic, chromaticTonicCount);
}

inline int clampTranspose(int semitones) noexcept
{
    return std::clamp(semitones, -24, 24);
}

inline float clampFineTune(float cents) noexcept
{
    return std::isfinite(cents) ? std::clamp(cents, -100.0f, 100.0f) : 0.0f;
}

inline int shortestTonicCorrection(int sourceTonic, int targetTonic) noexcept
{
    sourceTonic = clampTonic(sourceTonic);
    targetTonic = clampTonic(targetTonic);
    if (sourceTonic == noTonic || targetTonic == noTonic)
        return 0;

    const auto sourcePitchClass = sourceTonic - 1;
    const auto targetPitchClass = targetTonic - 1;
    auto distance = (targetPitchClass - sourcePitchClass + chromaticTonicCount)
        % chromaticTonicCount;
    if (distance > 6)
        distance -= chromaticTonicCount;
    return distance;
}

inline double totalPitchSemitones(int sourceTonic, int targetTonic,
                                  int transposeSemitones, float fineTuneCents,
                                  bool midiPitchEnabled, int midiNote,
                                  int rootMidiNote) noexcept
{
    const auto fineTune = static_cast<double>(clampFineTune(fineTuneCents)) / 100.0;
    const auto midiOffset = midiPitchEnabled
        ? std::clamp(midiNote, 0, 127) - std::clamp(rootMidiNote, 0, 127)
        : 0;
    return static_cast<double>(shortestTonicCorrection(sourceTonic, targetTonic)
                               + clampTranspose(transposeSemitones) + midiOffset)
        + fineTune;
}

inline double pitchRatioForSemitones(double semitones) noexcept
{
    if (!std::isfinite(semitones))
        return 1.0;
    return std::exp2(std::clamp(semitones, -192.0, 192.0) / 12.0);
}
}
