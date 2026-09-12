#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>
#include <string_view>

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

inline std::string midiNoteName(int midiNote)
{
    constexpr std::array<std::string_view, 12> names {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    midiNote = std::clamp(midiNote, 0, 127);
    return std::string(names[static_cast<std::size_t>(midiNote % 12)])
        + std::to_string(midiNote / 12 - 1);
}

inline int midiNoteFromName(std::string_view text, int fallback = 60) noexcept
{
    fallback = std::clamp(fallback, 0, 127);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    if (text.empty())
        return fallback;

    if (std::isdigit(static_cast<unsigned char>(text.front())))
    {
        int value = 0;
        for (const auto character : text)
        {
            if (!std::isdigit(static_cast<unsigned char>(character)))
                return fallback;
            value = value * 10 + (character - '0');
        }
        return std::clamp(value, 0, 127);
    }

    const auto letter = static_cast<char>(std::toupper(
        static_cast<unsigned char>(text.front())));
    int pitchClass = letter == 'C' ? 0 : letter == 'D' ? 2 : letter == 'E' ? 4
        : letter == 'F' ? 5 : letter == 'G' ? 7 : letter == 'A' ? 9
        : letter == 'B' ? 11 : -100;
    if (pitchClass < 0)
        return fallback;

    std::size_t cursor = 1;
    if (cursor < text.size() && (text[cursor] == '#' || text[cursor] == 'b'
                                 || text[cursor] == 'B'))
    {
        pitchClass += text[cursor] == '#' ? 1 : -1;
        ++cursor;
    }
    if (cursor >= text.size())
        return fallback;

    bool negative = false;
    if (text[cursor] == '-')
    {
        negative = true;
        ++cursor;
    }
    if (cursor >= text.size())
        return fallback;
    int octave = 0;
    for (; cursor < text.size(); ++cursor)
    {
        if (!std::isdigit(static_cast<unsigned char>(text[cursor])))
            return fallback;
        octave = octave * 10 + (text[cursor] - '0');
    }
    if (negative)
        octave = -octave;
    const auto note = (octave + 1) * 12 + pitchClass;
    return note >= 0 && note <= 127 ? note : fallback;
}
}

