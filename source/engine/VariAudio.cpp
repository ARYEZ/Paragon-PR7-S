#include "VariAudio.h"

#include <juce_dsp/juce_dsp.h>

#include "Stretch.h"

namespace pr7s
{
    namespace
    {
        constexpr double hopSeconds = 0.005;
        constexpr float lowestHz = 60.0f, highestHz = 1100.0f;

        float midiOf (double hz) { return (float) (69.0 + 12.0 * std::log2 (hz / 440.0)); }

        float medianOf (std::vector<float> values)
        {
            if (values.empty())
                return 0.0f;
            const auto middle = values.begin() + (long) values.size() / 2;
            std::nth_element (values.begin(), middle, values.end());
            return *middle;
        }
    }

    float PitchAnalysis::pitchAt (double seconds) const noexcept
    {
        const auto i = (long) std::llround (seconds / hopSeconds);
        return i >= 0 && i < (long) frames.size() ? frames[(size_t) i].midi : 0.0f;
    }

    //==========================================================================
    PitchAnalysis analysePitch (const juce::AudioBuffer<float>& sound, double sampleRate)
    {
        PitchAnalysis analysis;
        analysis.hopSeconds = hopSeconds;
        const auto length = sound.getNumSamples();
        if (length == 0 || sound.getNumChannels() == 0 || sampleRate <= 0.0)
            return analysis;
        std::vector<float> mono ((size_t) length, 0.0f);
        for (int c = 0; c < sound.getNumChannels(); ++c)
        {
            const auto* data = sound.getReadPointer (c);
            for (int i = 0; i < length; ++i)
                mono[(size_t) i] += data[i] / (float) sound.getNumChannels();
        }
        const auto hop = juce::jmax (1, (int) std::lround (sampleRate * hopSeconds));
        const auto window = sampleRate > 60000.0 ? 2048 : 1024;   // (the sum's length: about 21 ms)
        const auto maxTau = juce::jmin ((int) (sampleRate / lowestHz), window * 2 - 1);
        const auto minTau = juce::jmax (2, (int) (sampleRate / highestHz));
        const auto span = window + maxTau + 2;
        auto order = 1;
        while ((1 << order) < span + window)
            ++order;
        const auto size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> a ((size_t) size * 2), b ((size_t) size * 2), squares ((size_t) span + 1);
        std::vector<double> diff ((size_t) maxTau + 2), normal ((size_t) maxTau + 2);

        const auto frames = length / hop + 1;
        analysis.frames.resize ((size_t) frames);
        for (int f = 0; f < frames; ++f)
        {
            const auto first = f * hop - window / 2;
            const auto at = [&] (int i) { const auto k = first + i; return k >= 0 && k < length ? mono[(size_t) k] : 0.0f; };
            std::fill (a.begin(), a.end(), 0.0f);
            std::fill (b.begin(), b.end(), 0.0f);
            squares[0] = 0.0f;
            for (int i = 0; i < span; ++i)
            {
                a[(size_t) i] = at (i);
                squares[(size_t) i + 1] = squares[(size_t) i] + a[(size_t) i] * a[(size_t) i];
            }
            for (int i = 0; i < window; ++i)
                b[(size_t) i] = a[(size_t) i];
            const auto energy = (double) squares[(size_t) window];
            auto& frame = analysis.frames[(size_t) f];
            frame.levelDb = (float) (10.0 * std::log10 (energy / window + 1.0e-12));
            if (energy < 1.0e-9)
                continue;
            // r(tau) = sum over the window of a[j] a[j + tau]: the spectra's product, back.
            fft.performRealOnlyForwardTransform (a.data());
            fft.performRealOnlyForwardTransform (b.data());
            for (int k = 0; k < size; ++k)
            {
                const auto ar = a[(size_t) (2 * k)], ai = a[(size_t) (2 * k + 1)];
                const auto br = b[(size_t) (2 * k)], bi = b[(size_t) (2 * k + 1)];
                a[(size_t) (2 * k)] = ar * br + ai * bi;   // A x conj (B)
                a[(size_t) (2 * k + 1)] = ai * br - ar * bi;
            }
            fft.performRealOnlyInverseTransform (a.data());
            const auto scale = std::abs (a[0]) > 1.0e-20f ? energy / a[0] : 1.0;   // (whatever the transform's scaling)
            // The difference function and its cumulative-mean normalisation (YIN's steps 2 and 3).
            normal[0] = 1.0;
            auto running = 0.0;
            for (int tau = 1; tau <= maxTau; ++tau)
            {
                const auto shifted = (double) squares[(size_t) (tau + window)] - squares[(size_t) tau];
                diff[(size_t) tau] = juce::jmax (0.0, energy + shifted - 2.0 * a[(size_t) tau] * scale);
                running += diff[(size_t) tau];
                normal[(size_t) tau] = running > 0.0 ? diff[(size_t) tau] * tau / running : 1.0;
            }
            // The first dip under the threshold, followed to its bottom; else the deepest.
            auto best = -1;
            for (int tau = minTau; tau < maxTau; ++tau)
                if (normal[(size_t) tau] < 0.15)
                {
                    while (tau + 1 < maxTau && normal[(size_t) tau + 1] < normal[(size_t) tau])
                        ++tau;
                    best = tau;
                    break;
                }
            if (best < 0)
            {
                best = minTau;
                for (int tau = minTau; tau < maxTau; ++tau)
                    if (normal[(size_t) tau] < normal[(size_t) best])
                        best = tau;
            }
            const auto depth = normal[(size_t) best];
            frame.clarity = (float) juce::jlimit (0.0, 1.0, 1.0 - depth);
            if (depth > 0.3)
                continue;
            // Between samples: a parabola through the dip.
            auto exact = (double) best;
            if (best > minTau && best + 1 < maxTau)
            {
                const auto l = normal[(size_t) best - 1], c = normal[(size_t) best], r = normal[(size_t) best + 1];
                const auto denominator = l - 2.0 * c + r;
                if (denominator > 1.0e-12)
                    exact += juce::jlimit (-0.5, 0.5, 0.5 * (l - r) / denominator);
            }
            frame.midi = midiOf (sampleRate / exact);
        }
        // Single wrong octaves smoothed out: each pitched frame against the median of 5 around it.
        std::vector<float> smoothed (analysis.frames.size());
        for (size_t f = 0; f < analysis.frames.size(); ++f)
        {
            smoothed[f] = analysis.frames[f].midi;
            if (! analysis.frames[f].isVoiced())
                continue;
            std::vector<float> near;
            for (auto k = f >= 2 ? f - 2 : 0; k <= juce::jmin (analysis.frames.size() - 1, f + 2); ++k)
                if (analysis.frames[k].isVoiced())
                    near.push_back (analysis.frames[k].midi);
            const auto median = medianOf (near);
            if (std::abs (analysis.frames[f].midi - median) > 6.0f)
                smoothed[f] = median;
        }
        for (size_t f = 0; f < analysis.frames.size(); ++f)
            analysis.frames[f].midi = smoothed[f];
        return analysis;
    }

    //==========================================================================
    std::vector<VariSegment> segmentPitch (const PitchAnalysis& analysis)
    {
        std::vector<VariSegment> segments;
        const auto& frames = analysis.frames;
        const auto count = (int) frames.size();
        if (count == 0)
            return segments;
        auto loudest = -120.0f;
        for (const auto& f : frames)
            if (f.isVoiced())
                loudest = juce::jmax (loudest, f.levelDb);
        std::vector<char> voiced ((size_t) count);
        for (int i = 0; i < count; ++i)
            voiced[(size_t) i] = frames[(size_t) i].isVoiced() && frames[(size_t) i].levelDb > loudest - 40.0f;
        // (A frame or two lost inside a note: bridged.)
        for (int i = 1; i + 1 < count; ++i)
            if (! voiced[(size_t) i])
            {
                auto j = i;
                while (j < count && ! voiced[(size_t) j] && j - i < 3)
                    ++j;
                if (j < count && voiced[(size_t) j] && j - i <= 2 && voiced[(size_t) i - 1])
                    for (int k = i; k < j; ++k)
                        voiced[(size_t) k] = 1;
            }
        const auto pitch = [&] (int i)
        {
            if (frames[(size_t) i].isVoiced())
                return frames[(size_t) i].midi;
            for (int d = 1; d < 4; ++d)   // (a bridged frame: its neighbours')
            {
                if (i - d >= 0 && frames[(size_t) (i - d)].isVoiced()) return frames[(size_t) (i - d)].midi;
                if (i + d < count && frames[(size_t) (i + d)].isVoiced()) return frames[(size_t) (i + d)].midi;
            }
            return 0.0f;
        };
        const auto medianPitch = [&] (int from, int to)   // [from, to)
        {
            std::vector<float> values;
            for (int i = juce::jmax (0, from); i < juce::jmin (count, to); ++i)
                values.push_back (pitch (i));
            return medianOf (values);
        };
        constexpr int look = 10;        // 50 ms either side
        constexpr int shortest = 12;    // 60 ms
        constexpr float apart = 0.8f;   // semitones: a different note
        const auto hop = analysis.hopSeconds;
        const auto addSegment = [&] (int from, int to)   // [from, to)
        {
            VariSegment seg;
            seg.start = juce::jmax (0.0, (from - 0.5) * hop);
            seg.end = (to - 0.5) * hop;
            const auto length = to - from;
            seg.pitch = medianPitch (from + length * 15 / 100, to - length * 15 / 100);
            if (seg.pitch <= 0.0f)
                seg.pitch = medianPitch (from, to);
            segments.push_back (seg);
        };
        for (int i = 0; i < count;)
        {
            if (! voiced[(size_t) i])
            {
                ++i;
                continue;
            }
            auto end = i;
            while (end < count && voiced[(size_t) end])
                ++end;
            // Where it settles on another note: the steepest change between two steady sides.
            std::vector<int> cuts;
            auto from = i;
            for (int k = i + look; k + look <= end; ++k)
            {
                const auto before = medianPitch (k - look, k), after = medianPitch (k, k + look);
                if (std::abs (after - before) <= apart || k - from < shortest)
                    continue;
                auto bestK = k;
                auto bestJump = std::abs (after - before);
                for (int m = k + 1; m < juce::jmin (end - look + 1, k + look); ++m)
                {
                    const auto jump = std::abs (medianPitch (m, m + look) - medianPitch (m - look, m));
                    if (jump > bestJump)
                        bestJump = jump, bestK = m;
                }
                // Steady after it: the next 50 ms near one pitch (not a vibrato's swing).
                if (end - bestK < shortest)
                    break;
                cuts.push_back (bestK);
                from = bestK;
                k = bestK + look - 1;
            }
            auto start = i;
            for (const auto cut : cuts)
            {
                // A short piece still on its way (a scoop, a fall: its ends more than a note apart)
                // isn't a note of its own: it joins the one it leads into.
                const auto gliding = cut - start < 24
                                  && std::abs (medianPitch (cut - 4, cut) - medianPitch (start, start + 4)) > apart;
                if (gliding)
                    continue;
                if (cut - start >= shortest)
                    addSegment (start, cut);
                start = cut;
            }
            if (end - start >= shortest)
                addSegment (start, end);
            i = end;
        }
        return segments;
    }

    //==========================================================================
    float editedPitchAt (const VariSegment& seg, const PitchAnalysis& analysis, double seconds)
    {
        auto heard = analysis.pitchAt (seconds);
        if (heard <= 0.0f || std::abs (heard - seg.pitch) > 3.0f)
            heard = seg.pitch;   // (unpitched or a stray: its pitch)
        return seg.pitch + seg.shift + (heard - seg.pitch) * (1.0f - juce::jlimit (0.0f, 1.0f, seg.straighten));
    }

    float pitchChangeAt (const VariSegment& seg, const PitchAnalysis& analysis, double seconds)
    {
        auto heard = analysis.pitchAt (seconds);
        if (heard <= 0.0f || std::abs (heard - seg.pitch) > 3.0f)
            heard = seg.pitch;
        return editedPitchAt (seg, analysis, seconds) - heard;
    }

    float movedShift (const VariSegment& seg, float semitones, PitchSnap snap)
    {
        const auto now = seg.pitchNow();
        switch (snap)
        {
            case PitchSnap::absolute: return std::round (now + semitones) - seg.pitch;
            case PitchSnap::relative: return now + std::round (semitones) - seg.pitch;
            case PitchSnap::off:      break;
        }
        return now + semitones - seg.pitch;
    }

    float correctedShift (const VariSegment& seg, float amount)
    {
        const auto now = seg.pitchNow();
        return now + juce::jlimit (0.0f, 1.0f, amount) * (std::round (now) - now) - seg.pitch;
    }

    float quantizedShift (const VariSegment& seg, const Key& key, PitchSnap snap)
    {
        const auto now = seg.pitchNow();
        const auto scale = key.scale();
        auto best = std::round (now);
        auto bestDistance = 1.0e9f;
        for (auto n = std::floor (now) - 2.0f; n <= std::ceil (now) + 2.0f; n += 1.0f)
        {
            const auto pc = ((int) n % 12 + 12) % 12;
            if (std::find (scale.begin(), scale.end(), pc) == scale.end())
                continue;
            const auto distance = std::abs (n - now);
            if (distance < bestDistance)
                bestDistance = distance, best = n;
        }
        const auto cents = snap == PitchSnap::relative ? now - std::round (now) : 0.0f;
        return best + cents - seg.pitch;
    }

    std::vector<Key> suggestScales (const std::vector<VariSegment>& segments, int count)
    {
        std::array<double, 12> weight {};
        auto total = 0.0;
        for (const auto& s : segments)
        {
            const auto pc = ((int) std::lround (s.pitchNow()) % 12 + 12) % 12;
            weight[(size_t) pc] += s.end - s.start;
            total += s.end - s.start;
        }
        if (total <= 0.0)
            return {};
        const auto lastPc = segments.empty() ? -1 : ((int) std::lround (segments.back().pitchNow()) % 12 + 12) % 12;
        std::vector<std::pair<double, Key>> scored;
        for (int pc = 0; pc < 12; ++pc)
            for (const auto minor : { false, true })
            {
                const auto key = Key::fromPitchClass (pc, minor);
                const auto scale = key.scale();
                auto in = 0.0, out = 0.0;
                for (int n = 0; n < 12; ++n)
                    (std::find (scale.begin(), scale.end(), n) != scale.end() ? in : out) += weight[(size_t) n];
                auto score = in - 1.5 * out + 0.3 * weight[(size_t) pc] + 0.15 * weight[(size_t) ((pc + 7) % 12)];
                if (pc == lastPc)
                    score += 0.1 * total;
                scored.push_back ({ score, key });
            }
        std::stable_sort (scored.begin(), scored.end(), [] (const auto& x, const auto& y) { return x.first > y.first; });
        std::vector<Key> keys;
        for (int i = 0; i < juce::jmin (count, (int) scored.size()); ++i)
            keys.push_back (scored[(size_t) i].second);
        return keys;
    }

    //==========================================================================
    juce::AudioBuffer<float> renderVariAudio (const juce::AudioBuffer<float>& recording, double sampleRate,
                                              const PitchAnalysis& analysis, const std::vector<VariSegment>& segments)
    {
        juce::AudioBuffer<float> out (recording);
        const auto length = recording.getNumSamples();
        const auto channels = recording.getNumChannels();
        if (length == 0 || channels == 0 || sampleRate <= 0.0)
            return out;
        const auto toSample = [&] (double seconds) { return juce::jlimit (0, length, (int) std::llround (seconds * sampleRate)); };

        // Pitch: edited segments in regions (30 ms either side; regions that meet are one).
        constexpr double margin = 0.03, fade = 0.01;
        std::vector<std::pair<int, int>> regions;
        for (const auto& s : segments)
            if (std::abs (s.shift) > 1.0e-4f || s.straighten > 1.0e-4f)
            {
                const auto a = toSample (s.start - margin), b = toSample (s.end + margin);
                if (! regions.empty() && a <= regions.back().second)
                    regions.back().second = juce::jmax (regions.back().second, b);
                else
                    regions.push_back ({ a, b });
            }
        const auto changeAt = [&] (double seconds)
        {
            for (const auto& s : segments)
                if (seconds >= s.start && seconds < s.end)
                    return pitchChangeAt (s, analysis, seconds);
            return 0.0f;
        };
        for (const auto& [a, b] : regions)
        {
            if (b <= a)
                continue;
            // Real sound before it (so the stretcher has settled when the region starts).
            const auto preRoll = juce::jmin (a, (int) (0.25 * sampleRate));
            const auto feedFrom = a - preRoll;
            const auto total = b - feedFrom;
            juce::AudioBuffer<float> input (channels, total);
            for (int c = 0; c < channels; ++c)
                input.copyFrom (c, 0, recording, c, feedFrom, total);
            const auto output = shiftPitchAlong (input, sampleRate, [&] (int sample) { return changeAt ((feedFrom + sample) / sampleRate); });
            // Into the recording, crossfaded over 10 ms at both ends (where nothing is shifted).
            const auto fadeSamples = juce::jmax (1, (int) (fade * sampleRate));
            for (int k = a; k < b; ++k)
            {
                const auto w = (float) juce::jmin (1.0, juce::jmin ((k - a) / (double) fadeSamples, (b - 1 - k) / (double) fadeSamples));
                const auto from = k - feedFrom;
                for (int c = 0; c < channels; ++c)
                {
                    const auto shifted = from >= 0 && from < total ? output.getSample (c, from) : 0.0f;
                    out.setSample (c, k, out.getSample (c, k) * (1.0f - w) + shifted * w);
                }
            }
        }
        // Volume: each segment's level, ramped in and out over 5 ms.
        const auto ramp = juce::jmax (1, (int) (0.005 * sampleRate));
        for (const auto& s : segments)
        {
            if (std::abs (s.volumeDb) <= 1.0e-3f)
                continue;
            const auto gain = juce::Decibels::decibelsToGain (s.volumeDb, -96.0f);
            const auto a = toSample (s.start), b = toSample (s.end);
            for (int k = a; k < b; ++k)
            {
                const auto w = (float) juce::jmin (1.0, juce::jmin ((k - a + 1) / (double) ramp, (b - k) / (double) ramp));
                const auto g = 1.0f + (gain - 1.0f) * w;
                for (int c = 0; c < channels; ++c)
                    out.setSample (c, k, out.getSample (c, k) * g);
            }
        }
        return out;
    }
}
