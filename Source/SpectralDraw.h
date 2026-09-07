#pragma once

#include <JuceHeader.h>
#include <signalsmith-linear/fft.h>
#include <array>
#include <atomic>
#include <complex>
#include <cstdint>
#include <mutex>

namespace randomchop
{
class SpectralMaskStore final
{
public:
    static constexpr int canvasWidth = 128;
    static constexpr int canvasHeight = 64;
    static constexpr int cellCount = canvasWidth * canvasHeight;
    using Canvas = std::array<float, static_cast<std::size_t>(cellCount)>;

    struct Snapshot
    {
        Canvas values {};
        uint64_t generation = 0;
        bool hasContent = false;
    };

    struct ReadHandle
    {
        const Snapshot* snapshot = nullptr;
        int slot = -1;
    };

    SpectralMaskStore() noexcept;
    bool setCanvas(const Canvas& canvas);
    void clear();
    Canvas copyCanvas() const;
    juce::String encodeCanvas() const;
    bool restoreEncodedCanvas(const juce::String& encoded);
    uint64_t getPublishedGeneration() const noexcept;

    ReadHandle acquire() noexcept;
    void release(ReadHandle handle) noexcept;

private:
    enum SlotState : int { free = 0, writing, published, reading };
    struct Slot
    {
        std::atomic<int> state { free };
        Snapshot snapshot;
    };

    bool publishLocked(const Canvas& canvas) noexcept;

    static constexpr int slotCount = 4;
    std::array<Slot, slotCount> slots;
    std::atomic<int> publishedSlot { 0 };
    std::atomic<uint64_t> generationCounter { 0 };
    mutable std::mutex canonicalMutex;
    Canvas canonicalCanvas {};
};

struct SpectralDrawSettings
{
    float depth = 0.0f;
    int scanRateChoice = 1;
    double bpm = 120.0;
    double ppq = 0.0;
    bool useHostPpq = false;
    bool transportDiscontinuity = false;
};

class SpectralDrawProcessor final
{
public:
    static constexpr int fftSize = 1024;
    static constexpr int hopSize = fftSize / 4;
    static constexpr int latencySamples = fftSize;

    static constexpr double cycleQuarterNotes(int choice) noexcept
    {
        constexpr std::array<double, 4> cycles { 2.0, 4.0, 8.0, 16.0 };
        return choice >= 0 && choice < static_cast<int>(cycles.size())
            ? cycles[static_cast<std::size_t>(choice)] : cycles[1];
    }

    void prepare(double newSampleRate);
    void reset() noexcept;
    void process(juce::AudioBuffer<float>& buffer, SpectralMaskStore& maskStore,
                 SpectralDrawSettings settings) noexcept;
    float getScanPosition() const noexcept
    {
        return publishedScanPosition.load(std::memory_order_relaxed);
    }

private:
    using Complex = std::complex<float>;

    static float sanitise(float value) noexcept;
    static float maskValue(const SpectralMaskStore::Snapshot* snapshot,
                           float scanPosition, float frequencyNormalised) noexcept;
    void processFrame(const SpectralMaskStore::Snapshot* snapshot,
                      float depth, float scanPosition) noexcept;

    signalsmith::linear::SimpleFFT<float> fft;
    std::array<float, fftSize> window {};
    std::array<std::array<float, fftSize>, 2> inputRing {};
    std::array<std::array<float, fftSize>, 2> outputRing {};
    std::array<std::array<float, fftSize>, 2> dryDelay {};
    std::array<Complex, fftSize> timeData {};
    std::array<Complex, fftSize> frequencyData {};
    std::atomic<float> publishedScanPosition { 0.0f };
    double sampleRate = 44100.0;
    double scannerPhase = 0.0;
    int inputWritePosition = 0;
    int outputReadPosition = 0;
    int dryDelayPosition = 0;
    int samplesUntilFrame = hopSize;
};
}
