#include "engine/TempoDetect.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace pr7s
{
    namespace
    {
        constexpr double leastBpm = 50.0, mostBpm = 220.0, preferredBpm = 120.0;

        // Taken from the envelope: a slow mean off (the loudness changing isn't a beat), negatives to 0, scaled to 1.
        void normalise (std::vector<float>& envelope)
        {
            if (envelope.empty())
                return;
            const auto n = (int) envelope.size();
            const auto half = (int) std::lround (0.5 / tempoFrameSeconds);   // a second's window
            std::vector<double> sum ((size_t) n + 1, 0.0);
            for (int i = 0; i < n; ++i)
                sum[(size_t) i + 1] = sum[(size_t) i] + envelope[(size_t) i];
            std::vector<float> out ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const auto a = std::max (0, i - half), b = std::min (n, i + half + 1);
                const auto mean = (sum[(size_t) b] - sum[(size_t) a]) / (b - a);
                out[(size_t) i] = (float) std::max (0.0, envelope[(size_t) i] - mean);
            }
            const auto top = *std::max_element (out.begin(), out.end());
            if (top > 0.0f)
                for (auto& v : out)
                    v /= top;
            envelope = std::move (out);
        }

        double lagWeight (double lagFrames)
        {
            const auto bpm = 60.0 / (lagFrames * tempoFrameSeconds);
            const auto octaves = std::log2 (bpm / preferredBpm);
            return std::exp (-0.5 * octaves * octaves);
        }
    }

    std::vector<float> onsetEnvelope (const float* mono, int numSamples, double sampleRate)
    {
        std::vector<float> envelope;
        if (mono == nullptr || numSamples <= 0 || sampleRate <= 0.0)
            return envelope;
        constexpr int order = 10, size = 1 << order;   // 1024 (about 23 ms at 44.1 kHz)
        const auto hop = sampleRate * tempoFrameSeconds;
        const auto frames = (int) std::floor (numSamples / hop);
        juce::dsp::FFT fft (order);
        std::vector<float> window ((size_t) size), buffer ((size_t) size * 2), previous ((size_t) size / 2 + 1, 0.0f);
        for (int i = 0; i < size; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size);
        envelope.resize ((size_t) std::max (0, frames));
        // Bins from about 30 Hz to 8 kHz (the rumble and the hiss left out).
        const auto lowBin = std::max (1, (int) std::floor (30.0 * size / sampleRate));
        const auto highBin = std::min (size / 2, (int) std::ceil (8000.0 * size / sampleRate));
        for (int f = 0; f < frames; ++f)
        {
            const auto centre = (juce::int64) std::llround (f * hop);
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            for (int i = 0; i < size; ++i)
            {
                const auto at = centre - size / 2 + i;
                if (at >= 0 && at < numSamples)
                    buffer[(size_t) i] = mono[at] * window[(size_t) i];
            }
            fft.performFrequencyOnlyForwardTransform (buffer.data(), true);
            auto flux = 0.0;
            for (int b = lowBin; b <= highBin; ++b)
            {
                const auto magnitude = std::log1p (100.0f * buffer[(size_t) b]);
                flux += std::max (0.0f, magnitude - previous[(size_t) b]);
                previous[(size_t) b] = magnitude;
            }
            envelope[(size_t) f] = (float) flux;
        }
        normalise (envelope);
        return envelope;
    }

    std::vector<float> onsetEnvelope (const std::vector<DetectNote>& notes, double lengthSeconds)
    {
        const auto frames = (int) std::ceil (std::max (0.0, lengthSeconds) / tempoFrameSeconds);
        std::vector<float> envelope ((size_t) std::max (0, frames), 0.0f);
        for (const auto& n : notes)
        {
            // A small bump (three frames: a note's start isn't heard as a single instant), by velocity.
            const auto at = n.seconds / tempoFrameSeconds;
            const auto i = (int) std::floor (at);
            for (int k = -1; k <= 2; ++k)
                if (juce::isPositiveAndBelow (i + k, frames))
                    envelope[(size_t) (i + k)] += n.velocity * (float) std::max (0.0, 1.0 - std::abs (at - (i + k)) / 1.5);
        }
        normalise (envelope);
        return envelope;
    }

    double tempoOfEnvelope (const std::vector<float>& envelope, double* confidence)
    {
        const auto n = (int) envelope.size();
        const auto shortest = (int) std::floor (60.0 / mostBpm / tempoFrameSeconds);
        const auto longest = (int) std::ceil (60.0 / leastBpm / tempoFrameSeconds);
        if (n < longest * 4)
            return 0.0;
        // The autocorrelation up to twice the longest lag (the doubles help the tempo they're of).
        std::vector<double> ac ((size_t) longest * 2 + 2, 0.0);
        for (int lag = shortest / 2; lag < (int) ac.size(); ++lag)
        {
            auto sum = 0.0;
            for (int i = lag; i < n; ++i)
                sum += (double) envelope[(size_t) i] * envelope[(size_t) (i - lag)];
            ac[(size_t) lag] = sum / (n - lag);
        }
        auto zero = 0.0;
        for (const auto v : envelope)
            zero += (double) v * v;
        zero /= n;
        auto bestLag = 0;
        auto bestScore = -1.0;
        for (int lag = shortest; lag <= longest; ++lag)
        {
            const auto score = lagWeight (lag) * (ac[(size_t) lag] + 0.5 * ac[(size_t) lag * 2]);
            if (score > bestScore)
                bestScore = score, bestLag = lag;
        }
        if (bestLag <= 0)
            return 0.0;
        // Between frames: the parabola through the peak and its neighbours.
        auto lag = (double) bestLag;
        if (bestLag > shortest && bestLag < longest)
        {
            const auto a = ac[(size_t) bestLag - 1], b = ac[(size_t) bestLag], c = ac[(size_t) bestLag + 1];
            const auto d = a - 2.0 * b + c;
            if (d < 0.0)
                lag += juce::jlimit (-0.5, 0.5, 0.5 * (a - c) / d);
        }
        if (confidence != nullptr)
            *confidence = zero > 0.0 ? juce::jlimit (0.0, 1.0, ac[(size_t) bestLag] / zero * 2.0) : 0.0;
        return 60.0 / (lag * tempoFrameSeconds);
    }

    BeatAnalysis beatsOfEnvelope (const std::vector<float>& envelope, double bpm)
    {
        BeatAnalysis result;
        auto confidence = 0.0;
        const auto overall = tempoOfEnvelope (envelope, &confidence);
        if (bpm <= 0.0)
            bpm = overall;
        result.confidence = confidence;
        const auto n = (int) envelope.size();
        if (bpm <= 0.0 || n == 0)
            return result;
        const auto period = 60.0 / bpm / tempoFrameSeconds;   // frames
        // Each frame's best score as a beat: the envelope there plus the best beat before it, a
        // half to two periods back, less how far that step is from the period (squared, in octaves).
        constexpr double tightness = 30.0;   // (a drift of a few percent a beat costs little; skipping a beat a lot)
        std::vector<double> score ((size_t) n, 0.0);
        std::vector<int> from ((size_t) n, -1);
        const auto earliest = (int) std::lround (period * 2.0), latest = std::max (1, (int) std::lround (period * 0.5));
        for (int i = 0; i < n; ++i)
        {
            auto best = 0.0;
            auto bestFrom = -1;
            for (int back = latest; back <= earliest && back <= i; ++back)
            {
                const auto miss = std::log (back / period);
                const auto s = score[(size_t) (i - back)] - tightness * miss * miss;
                if (bestFrom < 0 || s > best)
                    best = s, bestFrom = i - back;
            }
            score[(size_t) i] = envelope[(size_t) i] + (bestFrom >= 0 ? best : 0.0);
            from[(size_t) i] = bestFrom;
        }
        // The last beat: the best score in the last period, then back along the chain.
        auto last = std::max (0, n - (int) std::lround (period));
        for (int i = last; i < n; ++i)
            if (score[(size_t) i] > score[(size_t) last])
                last = i;
        std::vector<double> beats;
        for (auto i = last; i >= 0; i = from[(size_t) i])
            beats.push_back (i * tempoFrameSeconds);
        std::reverse (beats.begin(), beats.end());
        // The chain runs back to the start; beats there on next to nothing (a quiet intro) go.
        std::vector<float> heard;
        for (const auto t : beats)
            heard.push_back (envelope[(size_t) std::lround (t / tempoFrameSeconds)]);
        auto sorted = heard;
        std::nth_element (sorted.begin(), sorted.begin() + (std::ptrdiff_t) sorted.size() / 2, sorted.end());
        const auto quiet = sorted[sorted.size() / 2] * 0.1f;
        size_t firstKept = 0, lastKept = beats.size();
        while (firstKept < lastKept && heard[firstKept] < quiet)
            ++firstKept;
        while (lastKept > firstKept && heard[lastKept - 1] < quiet)
            --lastKept;
        result.beats.assign (beats.begin() + (std::ptrdiff_t) firstKept, beats.begin() + (std::ptrdiff_t) lastKept);
        result.bpm = medianBpm (result.beats);
        if (result.bpm <= 0.0)
            result.bpm = bpm;
        return result;
    }

    BeatAnalysis detectBeats (const float* mono, int numSamples, double sampleRate)
    {
        return beatsOfEnvelope (onsetEnvelope (mono, numSamples, sampleRate));
    }

    BeatAnalysis detectBeats (const std::vector<DetectNote>& notes, double lengthSeconds)
    {
        return beatsOfEnvelope (onsetEnvelope (notes, lengthSeconds));
    }

    double medianBpm (const std::vector<double>& beats)
    {
        if (beats.size() < 2)
            return 0.0;
        std::vector<double> steps;
        for (size_t i = 1; i < beats.size(); ++i)
            if (beats[i] > beats[i - 1])
                steps.push_back (beats[i] - beats[i - 1]);
        if (steps.empty())
            return 0.0;
        std::nth_element (steps.begin(), steps.begin() + (std::ptrdiff_t) steps.size() / 2, steps.end());
        return 60.0 / steps[steps.size() / 2];
    }

    std::vector<double> rescaleBeats (const std::vector<double>& beats, double factor, double shift)
    {
        if (beats.size() < 2 || factor <= 0.0)
            return beats;
        const auto count = (int) beats.size();
        const auto at = [&] (double index)
        {
            if (index <= 0.0)
                return beats[0] + index * (beats[1] - beats[0]);
            if (index >= count - 1)
                return beats[(size_t) count - 1] + (index - (count - 1)) * (beats[(size_t) count - 1] - beats[(size_t) count - 2]);
            const auto i = (int) std::floor (index);
            return beats[(size_t) i] + (index - i) * (beats[(size_t) i + 1] - beats[(size_t) i]);
        };
        std::vector<double> out;
        const auto end = beats.back() + 1.0e-9;
        for (int j = 0;; ++j)
        {
            const auto t = at (j / factor + shift);
            if (t > end)
                break;
            if (t >= beats.front() - 1.0e-9 || shift < 0.0)
                out.push_back (t);
            if (j > count * 8 + 16)
                break;
        }
        return out;
    }

    std::vector<double> smoothBeats (const std::vector<double>& beats)
    {
        if (beats.size() < 4)
            return beats;
        std::vector<double> steps;
        for (size_t i = 1; i < beats.size(); ++i)
            steps.push_back (beats[i] - beats[i - 1]);
        std::vector<double> smooth (steps.size());
        for (size_t i = 0; i < steps.size(); ++i)
        {
            const auto a = i >= 3 ? i - 3 : 0, b = std::min (steps.size(), i + 4);
            std::vector<double> around (steps.begin() + (std::ptrdiff_t) a, steps.begin() + (std::ptrdiff_t) b);
            std::nth_element (around.begin(), around.begin() + (std::ptrdiff_t) around.size() / 2, around.end());
            smooth[i] = around[around.size() / 2];
        }
        std::vector<double> out { beats.front() };
        for (size_t i = 0; i < smooth.size(); ++i)
        {
            const auto steady = out.back() + smooth[i];
            const auto pull = juce::jlimit (-0.1 * smooth[i], 0.1 * smooth[i], 0.5 * (beats[i + 1] - steady));
            out.push_back (steady + pull);
        }
        return out;
    }
}
