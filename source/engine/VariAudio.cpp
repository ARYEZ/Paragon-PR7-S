#include "VariAudio.h"

#include <juce_dsp/juce_dsp.h>

#include "Stretch.h"

namespace pr7s
{
    namespace
    {
        constexpr double hopSeconds = 0.005;

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
    PitchAnalysis analysePitch (const juce::AudioBuffer<float>& sound, double sampleRate, float lowestHz, float highestHz)
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
        auto window = sampleRate > 60000.0 ? 2048 : 1024;   // (the sum's length: about 21 ms)
        while (window < (int) (sampleRate / lowestHz))     // (0.82.0) at least the longest period
            window *= 2;
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
        return seg.pitch + seg.shift + tiltAt (seg, seconds) + (heard - seg.pitch) * (1.0f - straightenAt (seg, seconds));
    }

    float tiltAt (const VariSegment& seg, double seconds)
    {
        if (std::abs (seg.tiltStart) <= 1.0e-6f && std::abs (seg.tiltEnd) <= 1.0e-6f)
            return 0.0f;
        const auto length = juce::jmax (1.0e-6, seg.end - seg.start);
        const auto along = (float) juce::jlimit (0.0, 1.0, (seconds - seg.start) / length);
        const auto anchor = juce::jlimit (0.02f, 0.98f, seg.anchor);
        return along < anchor ? seg.tiltStart * (anchor - along) / anchor : seg.tiltEnd * (along - anchor) / (1.0f - anchor);
    }

    float straightenAt (const VariSegment& seg, double seconds)
    {
        const auto amount = juce::jlimit (0.0f, 1.0f, seg.straighten);
        if (amount <= 0.0f)
            return 0.0f;
        const auto length = juce::jmax (1.0e-6, seg.end - seg.start);
        const auto from = seg.start + juce::jlimit (0.0, 1.0, (double) seg.straightenFrom) * length;
        const auto to = seg.start + juce::jlimit (0.0, 1.0, (double) seg.straightenTo) * length;
        if (seconds < from || seconds > to)
            return 0.0f;
        // Easing in and out at an edge of the range inside the segment (at the segment's own
        // edges none: the whole segment straightened is as in part one).
        constexpr double ease = 0.02;
        auto weight = 1.0;
        if (seg.straightenFrom > 0.0f)
            weight = juce::jmin (weight, (seconds - from) / ease);
        if (seg.straightenTo < 1.0f)
            weight = juce::jmin (weight, (to - seconds) / ease);
        return amount * (float) juce::jlimit (0.0, 1.0, weight);
    }

    void rotateSegment (VariSegment& seg, float semitones, bool atStart)
    {
        const auto anchor = juce::jlimit (0.02f, 0.98f, seg.anchor);
        if (atStart)
        {
            seg.tiltStart += semitones;
            seg.tiltEnd -= semitones * (1.0f - anchor) / anchor;   // (one straight line through the anchor)
        }
        else
        {
            seg.tiltEnd += semitones;
            seg.tiltStart -= semitones * anchor / (1.0f - anchor);
        }
    }

    float averagePitch (const PitchAnalysis& analysis, double start, double end, float fallback)
    {
        const auto hop = analysis.hopSeconds;
        const auto count = (int) analysis.frames.size();
        const auto collect = [&] (double a, double b)
        {
            std::vector<float> values;
            for (int i = juce::jmax (0, (int) std::ceil (a / hop)); i <= juce::jmin (count - 1, (int) std::floor (b / hop)); ++i)
                if (analysis.frames[(size_t) i].isVoiced())
                    values.push_back (analysis.frames[(size_t) i].midi);
            return values;
        };
        const auto length = end - start;
        auto values = collect (start + 0.15 * length, end - 0.15 * length);
        if (values.empty())
            values = collect (start, end);
        if (values.empty())
            return fallback;
        return medianOf (values);
    }

    bool splitSegment (std::vector<VariSegment>& segments, int index, double seconds, const PitchAnalysis& analysis)
    {
        if (! juce::isPositiveAndBelow (index, (int) segments.size()))
            return false;
        const auto seg = segments[(size_t) index];
        if (seconds < seg.start + shortestSegment || seconds > seg.end - shortestSegment)
            return false;
        // Both halves keep its edits (but the tilt and the ranges, which were its own shape).
        auto first = seg, second = seg;
        for (auto* half : { &first, &second })
        {
            half->tiltStart = half->tiltEnd = 0.0f;
            half->anchor = 0.5f;
            half->straightenFrom = 0.0f;
            half->straightenTo = 1.0f;
        }
        first.end = seconds;
        second.start = seconds;
        first.pitch = averagePitch (analysis, first.start, first.end, seg.pitch);
        second.pitch = averagePitch (analysis, second.start, second.end, seg.pitch);
        segments[(size_t) index] = first;
        segments.insert (segments.begin() + index + 1, second);
        return true;
    }

    bool glueSegments (std::vector<VariSegment>& segments, int first, int last, const PitchAnalysis& analysis)
    {
        if (first > last)
            std::swap (first, last);
        if (first < 0 || last >= (int) segments.size() || first == last)
            return false;
        // One segment from the first's start to the last's end, the first's edits.
        auto glued = segments[(size_t) first];
        glued.end = segments[(size_t) last].end;
        glued.tiltStart = glued.tiltEnd = 0.0f;
        glued.anchor = 0.5f;
        glued.straightenFrom = 0.0f;
        glued.straightenTo = 1.0f;
        glued.pitch = averagePitch (analysis, glued.start, glued.end, glued.pitch);
        segments.erase (segments.begin() + first + 1, segments.begin() + last + 1);
        segments[(size_t) first] = glued;
        return true;
    }

    bool setSegmentEdges (std::vector<VariSegment>& segments, int index, double start, double end, const PitchAnalysis& analysis)
    {
        if (! juce::isPositiveAndBelow (index, (int) segments.size()))
            return false;
        // Up to the next segments' edges (segments never overlap), inside the recording.
        const auto low = index > 0 ? segments[(size_t) index - 1].end : 0.0;
        const auto high = index + 1 < (int) segments.size() ? segments[(size_t) index + 1].start
                                                              : juce::jmax (segments[(size_t) index].end, analysis.frames.size() * analysis.hopSeconds);
        start = juce::jlimit (low, high, start);
        end = juce::jlimit (low, high, end);
        if (end - start < shortestSegment)
            return false;
        auto& seg = segments[(size_t) index];
        seg.start = start;
        seg.end = end;
        seg.pitch = averagePitch (analysis, start, end, seg.pitch);
        return true;
    }

    int harmonyPitch (int sung, const std::vector<int>& chord, int bass, int voice)
    {
        static const int apart[] { 3, -3, -6, -9 };
        voice = juce::jlimit (0, 3, voice);
        if (chord.empty())
            return juce::jlimit (0, 127, sung + apart[voice]);
        const auto inChord = [&chord] (int n) { return std::find (chord.begin(), chord.end(), (n % 12 + 12) % 12) != chord.end(); };
        const auto below = [&] (int from) { for (int n = from; n > from - 13; --n) if (inChord (n)) return n; return from; };
        const auto soprano = [&] { for (int n = sung + 2; n < sung + 14; ++n) if (inChord (n)) return n; return sung + 3; };
        const auto alto = below (sung - 2);
        const auto tenor = below (alto - 2);
        auto result = sung;
        switch (voice)
        {
            case 0: result = soprano(); break;
            case 1: result = alto; break;
            case 2: result = tenor; break;
            default:
            {
                const auto root = bass >= 0 ? bass : chord.front();
                result = tenor - 2;
                while (((result % 12) + 12) % 12 != root)
                    --result;
                break;
            }
        }
        return juce::jlimit (0, 127, result);
    }

    ExtractedMidi extractMidi (const std::vector<VariSegment>& segments, const PitchAnalysis& analysis, const ExtractMidiOptions& options)
    {
        ExtractedMidi out;
        const auto range = (float) juce::jlimit (1, 24, options.bendRange);
        const auto bendFor = [range] (float semitones) { return juce::jlimit (0, 16383, 8192 + juce::roundToInt (semitones / range * 8192.0f)); };
        const auto levelAt = [&analysis] (double seconds)
        {
            const auto i = juce::jlimit (0, juce::jmax (0, (int) analysis.frames.size() - 1), (int) std::lround (seconds / analysis.hopSeconds));
            return analysis.frames.empty() ? -120.0f : analysis.frames[(size_t) i].levelDb;
        };
        constexpr double step = 0.01;   // (curves: every 10 ms)
        auto lastBend = 8192, lastCc = -1;
        for (const auto& seg : segments)
        {
            const auto note = juce::jlimit (0, 127, (int) std::lround (seg.pitchNow()));
            auto velocity = juce::jlimit (1, 127, options.velocity);
            if (options.volume == ExtractMidiOptions::Volume::dynamic)
            {
                // Its loudest moment (our mapping: -48 dBFS is 1, 0 dBFS is 127), with its volume edit.
                auto loudest = -120.0f;
                for (auto t = seg.start; t < seg.end; t += step)
                    loudest = juce::jmax (loudest, levelAt (t));
                velocity = juce::jlimit (1, 127, juce::roundToInt (127.0f + (loudest + seg.volumeDb) * 126.0f / 48.0f));
            }
            out.notes.push_back ({ seg.start, seg.end, note, velocity });
            const auto addBend = [&] (double t, int value)
            {
                if (value != lastBend)
                    out.events.push_back ({ t, true, 0, value }), lastBend = value;
            };
            if (options.pitch == ExtractMidiOptions::Pitch::staticBend)
                addBend (seg.start, bendFor (seg.pitchNow() - (float) note));
            else if (options.pitch == ExtractMidiOptions::Pitch::continuousBend)
                for (auto t = seg.start; t < seg.end; t += step)
                    addBend (t, bendFor (editedPitchAt (seg, analysis, t) - (float) note));
            if (options.volume == ExtractMidiOptions::Volume::controller)
                for (auto t = seg.start; t < seg.end; t += step)
                {
                    // The level as a controller (our mapping: -60 dBFS to 0 dBFS over 0 to 127).
                    const auto value = juce::jlimit (0, 127, juce::roundToInt ((levelAt (t) + seg.volumeDb + 60.0f) / 60.0f * 127.0f));
                    if (value != lastCc)
                        out.events.push_back ({ t, false, juce::jlimit (0, 127, options.controller), value }), lastCc = value;
                }
        }
        if (lastBend != 8192 && ! segments.empty())
            out.events.push_back ({ segments.back().end, true, 0, 8192 });   // (the bend back to the middle after)
        std::stable_sort (out.events.begin(), out.events.end(), [] (const auto& a, const auto& b) { return a.seconds < b.seconds; });
        return out;
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
            if (s.changesPitch())
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
        // (0.35.0) Formants: each segment's shift, easing in and out over 10 ms at its edges.
        const auto anyFormant = std::any_of (segments.begin(), segments.end(), [] (const VariSegment& s) { return std::abs (s.formant) > 1.0e-3f; });
        const auto formantAt = [&] (double seconds)
        {
            for (const auto& s : segments)
                if (seconds >= s.start && seconds < s.end)
                {
                    const auto ease = (float) juce::jlimit (0.0, 1.0, juce::jmin (seconds - s.start, s.end - seconds) / 0.01);
                    return formantSemitones (s.formant) * ease;
                }
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
            const auto output = anyFormant
                ? shiftPitchAlong (input, sampleRate, [&] (int sample) { return changeAt ((feedFrom + sample) / sampleRate); },
                                   [&] (int sample) { return formantAt ((feedFrom + sample) / sampleRate); })
                : shiftPitchAlong (input, sampleRate, [&] (int sample) { return changeAt ((feedFrom + sample) / sampleRate); });
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
