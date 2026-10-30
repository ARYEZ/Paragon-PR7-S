#pragma once

#include <atomic>
#include <cmath>

// Hands the peak level of the audio from the audio thread to the UI thread.
// The audio thread calls pushSamples() every block; the UI timer calls
// takePeak(), which returns the highest peak since the last call. Lock-free,
// so it is safe to use on the audio thread.
class LevelMeterSource
{
public:
    void pushSamples (const float* samples, int numSamples) noexcept
    {
        float blockPeak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            blockPeak = std::fmax (blockPeak, std::abs (samples[i]));

        auto current = peak.load (std::memory_order_relaxed);
        while (blockPeak > current
               && ! peak.compare_exchange_weak (current, blockPeak, std::memory_order_relaxed))
        {
        }
    }

    float takePeak() noexcept { return peak.exchange (0.0f, std::memory_order_relaxed); }

private:
    std::atomic<float> peak { 0.0f };
};
