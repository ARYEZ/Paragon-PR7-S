#include "Warp.h"
#include "Stretch.h"

#include <juce_dsp/juce_dsp.h>

namespace pr7s
{
    //==========================================================================
    std::vector<Hitpoint> detectHitpoints (const juce::AudioBuffer<float>& sound, double sampleRate)
    {
        // Spectral flux on a log-compressed magnitude spectrum (1024-sample frames, a 256-sample
        // hop), peaks over a moving average; each peak then placed where its sound starts rising.
        std::vector<Hitpoint> found;
        const auto length = sound.getNumSamples();
        if (length < 2048 || sound.getNumChannels() == 0 || sampleRate <= 0.0)
            return found;
        constexpr int order = 10, size = 1 << order, hop = 256;
        std::vector<float> mono ((size_t) length);
        for (int c = 0; c < sound.getNumChannels(); ++c)
        {
            const auto* data = sound.getReadPointer (c);
            for (int i = 0; i < length; ++i)
                mono[(size_t) i] += data[i] / (float) sound.getNumChannels();
        }
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) size, juce::dsp::WindowingFunction<float>::hann, false);
        std::vector<float> frame ((size_t) size * 2), previous ((size_t) size / 2 + 1, 0.0f), flux;
        for (int start = 0; start + size <= length; start += hop)
        {
            std::fill (frame.begin(), frame.end(), 0.0f);
            std::copy (mono.begin() + start, mono.begin() + start + size, frame.begin());
            window.multiplyWithWindowingTable (frame.data(), (size_t) size);
            fft.performFrequencyOnlyForwardTransform (frame.data(), true);
            auto sum = 0.0f;
            for (int bin = 1; bin <= size / 2; ++bin)
            {
                const auto now = std::log1p (100.0f * frame[(size_t) bin]);
                sum += juce::jmax (0.0f, now - previous[(size_t) bin]);
                previous[(size_t) bin] = now;
            }
            flux.push_back (sum);
        }
        if (flux.size() < 3)
            return found;
        const auto loudest = *std::max_element (flux.begin(), flux.end());
        if (loudest <= 0.0f)
            return found;
        // Peaks: the highest within 3 frames either side, and over the average of 8 either side.
        const auto frames = (int) flux.size();
        std::vector<int> peaks;
        for (int i = 1; i < frames; ++i)
        {
            auto isPeak = flux[(size_t) i] > 0.0f;
            for (int j = juce::jmax (0, i - 3); j <= juce::jmin (frames - 1, i + 3) && isPeak; ++j)
                if (j != i && (flux[(size_t) j] > flux[(size_t) i] || (j < i && juce::approximatelyEqual (flux[(size_t) j], flux[(size_t) i]))))
                    isPeak = false;
            if (! isPeak)
                continue;
            auto mean = 0.0f;
            auto count = 0;
            for (int j = juce::jmax (0, i - 8); j <= juce::jmin (frames - 1, i + 8); ++j, ++count)
                mean += flux[(size_t) j];
            mean /= (float) juce::jmax (1, count);
            if (flux[(size_t) i] > mean * 1.3f + loudest * 0.02f)
                peaks.push_back (i);
        }
        // Each one placed where it starts: in the frame's stretch, the first point its envelope
        // (the largest sample in 32) reaches a fifth of the stretch's loudest.
        for (const auto i : peaks)
        {
            const auto from = juce::jmax (0, i * hop - size / 4), to = juce::jmin (length, i * hop + size);
            constexpr int step = 32;
            auto peakValue = 0.0f;
            for (int s = from; s < to; ++s)
                peakValue = juce::jmax (peakValue, std::abs (mono[(size_t) s]));
            auto onset = i * hop + size / 2;
            if (peakValue > 0.0f)
                for (int s = from; s < to; s += step)
                {
                    auto block = 0.0f;
                    for (int k = s; k < juce::jmin (to, s + step); ++k)
                        block = juce::jmax (block, std::abs (mono[(size_t) k]));
                    if (block >= peakValue * 0.2f)
                    {
                        for (int k = s; k < juce::jmin (to, s + step); ++k)   // (to the sample)
                            if (std::abs (mono[(size_t) k]) >= peakValue * 0.2f)
                            {
                                onset = k;
                                break;
                            }
                        break;
                    }
                }
            // An onset is sound starting: louder just after it than just before (a note cut off
            // also changes the spectrum suddenly, but the sound falls there).
            const auto rms = [&] (int a, int b)
            {
                auto sum = 0.0;
                a = juce::jlimit (0, length, a);
                b = juce::jlimit (0, length, b);
                for (int k = a; k < b; ++k)
                    sum += (double) mono[(size_t) k] * mono[(size_t) k];
                return b > a ? std::sqrt (sum / (b - a)) : 0.0;
            };
            const auto span = (int) (0.02 * sampleRate);
            if (rms (onset, onset + span) <= rms (onset - span, onset) * 1.25 + 1.0e-6)
                continue;
            found.push_back ({ onset / sampleRate, flux[(size_t) i] / loudest });
        }
        std::sort (found.begin(), found.end(), [] (const Hitpoint& a, const Hitpoint& b) { return a.seconds < b.seconds; });
        return found;
    }

    std::vector<double> filterHitpoints (const std::vector<Hitpoint>& all, double threshold, double minimumSeconds,
                                         const Project::HitpointEdits* edits)
    {
        std::vector<Hitpoint> strong;
        for (const auto& h : all)
            if (h.strength >= threshold)
                strong.push_back (h);
        // Strongest first, none within the minimum of one already kept.
        std::sort (strong.begin(), strong.end(), [] (const Hitpoint& a, const Hitpoint& b) { return a.strength > b.strength; });
        std::vector<double> kept;
        for (const auto& h : strong)
            if (std::none_of (kept.begin(), kept.end(), [&] (double k) { return std::abs (k - h.seconds) < minimumSeconds; }))
                kept.push_back (h.seconds);
        if (edits != nullptr)
        {
            kept.erase (std::remove_if (kept.begin(), kept.end(), [edits] (double k)
                        {
                            return std::any_of (edits->removed.begin(), edits->removed.end(), [k] (double r) { return std::abs (r - k) < 0.01; });
                        }), kept.end());
            for (const auto a : edits->added)
                if (std::none_of (kept.begin(), kept.end(), [a] (double k) { return std::abs (k - a) < 0.001; }))
                    kept.push_back (a);
        }
        std::sort (kept.begin(), kept.end());
        return kept;
    }

    //==========================================================================
    namespace
    {
        double songSeconds (const Part& part, double beat, const TempoMap& tempo)
        {
            return tempo.secondsAt (part.start + beat);
        }
    }

    double warpSecondsAt (const Part& part, double beat, const TempoMap& tempo)
    {
        const auto at = songSeconds (part, beat, tempo);
        if (! part.isWarped())
            return part.audioOffset + at - songSeconds (part, 0.0, tempo);
        const auto& m = part.warp;
        if (beat <= m.front().beat)
            return m.front().seconds + at - songSeconds (part, m.front().beat, tempo);
        if (beat >= m.back().beat)
            return m.back().seconds + at - songSeconds (part, m.back().beat, tempo);
        for (size_t k = 0; k + 1 < m.size(); ++k)
            if (beat <= m[k + 1].beat)
            {
                const auto a = songSeconds (part, m[k].beat, tempo), b = songSeconds (part, m[k + 1].beat, tempo);
                const auto along = b > a ? (at - a) / (b - a) : 0.0;
                return m[k].seconds + along * (m[k + 1].seconds - m[k].seconds);
            }
        return m.back().seconds;
    }

    double warpBeatAt (const Part& part, double seconds, const TempoMap& tempo)
    {
        const auto beatOfSong = [&] (double songSecs) { return tempo.beatAt (juce::jmax (0.0, songSecs)) - part.start; };
        if (! part.isWarped())
            return beatOfSong (songSeconds (part, 0.0, tempo) + seconds - part.audioOffset);
        const auto& m = part.warp;
        if (seconds <= m.front().seconds)
            return beatOfSong (songSeconds (part, m.front().beat, tempo) + seconds - m.front().seconds);
        for (size_t k = 0; k + 1 < m.size(); ++k)
            if (seconds <= m[k + 1].seconds)
            {
                const auto a = songSeconds (part, m[k].beat, tempo), b = songSeconds (part, m[k + 1].beat, tempo);
                const auto span = m[k + 1].seconds - m[k].seconds;
                const auto along = span > 0.0 ? (seconds - m[k].seconds) / span : 0.0;
                return beatOfSong (a + along * (b - a));
            }
        return beatOfSong (songSeconds (part, m.back().beat, tempo) + seconds - m.back().seconds);
    }

    void ensureWarp (Part& part, const TempoMap& tempo)
    {
        if (part.isWarped() || ! part.isAudio())
            return;
        part.warp = { { 0.0, part.audioOffset }, { part.length, warpSecondsAt (part, part.length, tempo) } };
    }

    void rebaseWarp (const Part& old, Part& updated, const TempoMap& tempo)
    {
        if (! old.isWarped())
            return;
        const auto at = [&] (double songBeat) { return warpSecondsAt (old, songBeat - old.start, tempo); };
        std::vector<WarpMarker> markers { { 0.0, at (updated.start) } };
        for (const auto& m : old.warp)
        {
            const auto songBeat = old.start + m.beat;
            if (songBeat > updated.start + 1.0e-6 && songBeat < updated.start + updated.length - 1.0e-6)
                markers.push_back ({ songBeat - updated.start, m.seconds });
        }
        markers.push_back ({ updated.length, at (updated.start + updated.length) });
        updated.warp = std::move (markers);
        updated.audioOffset = juce::jmax (0.0, updated.warp.front().seconds);
    }

    std::vector<double> warpFactors (const Part& part, const TempoMap& tempo)
    {
        std::vector<double> factors;
        for (size_t k = 0; k < part.warp.size(); ++k)
        {
            if (k == 0)
            {
                factors.push_back (1.0);
                continue;
            }
            const auto song = songSeconds (part, part.warp[k].beat, tempo) - songSeconds (part, part.warp[k - 1].beat, tempo);
            const auto source = part.warp[k].seconds - part.warp[k - 1].seconds;
            factors.push_back (source > 1.0e-9 ? song / source : 1.0);
        }
        return factors;
    }

    void quantizeWarp (Part& part, double grid, const TempoMap& tempo)
    {
        if (! part.isWarped() || grid <= 0.0 || part.warp.size() < 3)
            return;
        auto& m = part.warp;
        const auto last = m.size() - 1;
        // For each grid line inside the clip, the interior marker nearest it (within half a step).
        std::vector<std::optional<double>> target (m.size());
        const auto first = std::ceil ((part.start + 1.0e-9) / grid) * grid;
        for (auto line = first; line < part.end() - 1.0e-9; line += grid)
        {
            std::optional<size_t> best;
            for (size_t k = 1; k < last; ++k)
            {
                const auto d = std::abs (part.start + m[k].beat - line);
                if (d <= grid * 0.5 && (! best || d < std::abs (part.start + m[*best].beat - line)))
                    best = k;
            }
            if (best && ! target[*best])
                target[*best] = line - part.start;
        }
        // Only targets in order (a marker can't pass another), then the rest in proportion.
        target.front() = m.front().beat;
        target.back() = m.back().beat;
        auto lastKept = 0.0;
        for (size_t k = 1; k < last; ++k)
            if (target[k])
            {
                if (*target[k] <= lastKept + 1.0e-6 || *target[k] >= m.back().beat - 1.0e-6)
                    target[k].reset();
                else
                    lastKept = *target[k];
            }
        std::vector<double> beats (m.size());
        for (size_t k = 0; k < m.size(); ++k)
        {
            if (target[k])
            {
                beats[k] = *target[k];
                continue;
            }
            auto a = k, b = k;
            while (! target[a]) --a;
            while (! target[b]) ++b;
            const auto along = (m[k].beat - m[a].beat) / juce::jmax (1.0e-9, m[b].beat - m[a].beat);
            beats[k] = *target[a] + along * (*target[b] - *target[a]);
        }
        for (size_t k = 0; k < m.size(); ++k)
            m[k].beat = beats[k];
        juce::ignoreUnused (tempo);
    }

    //==========================================================================
    std::shared_ptr<const juce::AudioBuffer<float>> WarpRenderer::render (const Part& part, const juce::AudioBuffer<float>& recording,
                                                                          double sampleRate, const TempoMap& tempo)
    {
        if (! part.isWarped() || recording.getNumSamples() == 0)
            return nullptr;
        juce::String key;
        key << part.audioFile << "|" << sampleRate << "|" << part.start << "|" << part.length;
        for (const auto& m : part.warp)
            key << "|" << m.beat << ":" << m.seconds;
        for (const auto& e : tempo.getEvents())
            key << "|t" << e.beat << ":" << e.bpm << (e.ramp ? "r" : "s");
        if (auto found = whole.find (key); found != whole.end())
            return found->second;

        const auto channels = recording.getNumChannels(), available = recording.getNumSamples();
        const auto songAt = [&] (double beat) { return tempo.secondsAt (part.start + beat); };
        const auto origin = songAt (0.0);
        const auto total = juce::jmax (1, (int) std::llround ((songAt (part.length) - origin) * sampleRate));
        auto out = std::make_shared<juce::AudioBuffer<float>> (channels, total);
        out->clear();
        constexpr int crossfade = 128;   // (samples: ~3 ms, the joins between stretches)
        const auto& m = part.warp;
        for (size_t k = 0; k + 1 < m.size(); ++k)
        {
            const auto outStart = (int) std::llround ((songAt (m[k].beat) - origin) * sampleRate);
            const auto outEnd = (int) std::llround ((songAt (m[k + 1].beat) - origin) * sampleRate);
            const auto count = outEnd - outStart;
            if (count <= 0)
                continue;
            const auto srcStart = m[k].seconds * sampleRate, srcEnd = m[k + 1].seconds * sampleRate;
            // Each stretch also makes `crossfade` samples before its start (from the recording just
            // before its marker, at its own speed), faded in over the end of the one before: nothing
            // after its end is stretched with it, so the next hit (at the next marker) isn't smeared early.
            const auto lead = k > 0 ? crossfade : 0;
            const auto wanted = count + lead;
            juce::String pieceKey;
            pieceKey << part.audioFile << "|" << sampleRate << "|" << srcStart << "|" << srcEnd << "|" << count << "|" << lead;
            auto piece = pieces[pieceKey];
            if (piece == nullptr)
            {
                auto made = std::make_shared<juce::AudioBuffer<float>> (channels, wanted);
                made->clear();
                const auto ratio = (srcEnd - srcStart) > 1.0 ? (double) count / (srcEnd - srcStart) : 1.0;
                if (std::abs (ratio - 1.0) < 1.0e-4)
                {
                    // At its own speed: the recording as it is.
                    const auto from = (int) std::llround (srcStart) - lead;
                    for (int c = 0; c < channels; ++c)
                        for (int i = 0; i < wanted; ++i)
                            if (juce::isPositiveAndBelow (from + i, available))
                                made->setSample (c, i, recording.getSample (c, from + i));
                }
                else
                {
                    // Stretched, with some of the recording before it so its start sounds as its middle.
                    const auto pad = 0.03 * sampleRate + lead / ratio;
                    const auto inStart = juce::jmax (0, (int) std::floor (srcStart - pad));
                    const auto inEnd = juce::jmin (available, (int) std::ceil (srcEnd) + 1);
                    if (inEnd - inStart > 16)
                    {
                        juce::AudioBuffer<float> input (channels, inEnd - inStart);
                        for (int c = 0; c < channels; ++c)
                            input.copyFrom (c, 0, recording, c, inStart, inEnd - inStart);
                        const auto stretchedLength = juce::jmax (1, (int) std::llround ((inEnd - inStart) * ratio));
                        const auto stretched = stretchAudio (input, sampleRate, stretchedLength, 0.0);
                        const auto from = (int) std::llround ((srcStart - inStart) * ratio) - lead;
                        for (int c = 0; c < juce::jmin (channels, stretched.getNumChannels()); ++c)
                            for (int i = 0; i < wanted; ++i)
                                if (juce::isPositiveAndBelow (from + i, stretched.getNumSamples()))
                                    made->setSample (c, i, stretched.getSample (c, from + i));
                    }
                }
                piece = made;
                pieces[pieceKey] = made;
                samplesKept += (juce::int64) wanted * channels;
            }
            for (int c = 0; c < channels; ++c)
            {
                auto* dest = out->getWritePointer (c);
                const auto* source = piece->getReadPointer (c);
                for (int i = 0; i < wanted; ++i)
                {
                    const auto at = outStart - lead + i;
                    if (at < 0 || at >= total)
                        continue;
                    if (i < lead)
                    {
                        // Faded in over the end of the stretch before.
                        const auto w = (float) i / (float) lead;
                        dest[at] = dest[at] * (1.0f - w) + source[i] * w;
                    }
                    else
                        dest[at] = source[i];
                }
            }
        }
        // (Kept within about 400 MB of stretches; then forgotten and made again when needed.)
        if (samplesKept > 100'000'000)
        {
            pieces.clear();
            samplesKept = 0;
        }
        if (whole.size() > 64)
            whole.clear();
        whole[key] = out;
        return out;
    }
}
