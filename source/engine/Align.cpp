#include "Align.h"

#include <juce_dsp/juce_dsp.h>

#include <numeric>

namespace pr7s
{
    //==========================================================================
    juce::String AlignSettings::toString() const
    {
        juce::StringArray parts;
        parts.add (juce::String (precision, 3));
        parts.add (juce::String ((int) flexibility));
        parts.add (juce::String (maxShiftSeconds, 4));
        parts.add (matchWords ? "1" : "0");
        parts.add (preferShifting ? "1" : "0");
        parts.add (onlyWhereBothPlay ? "1" : "0");
        return parts.joinIntoString (" ");
    }

    AlignSettings AlignSettings::fromString (const juce::String& text)
    {
        AlignSettings s;
        const auto parts = juce::StringArray::fromTokens (text, " ", "");
        if (parts.size() < 6)
            return s;
        s.precision = juce::jlimit (0.0, 1.0, parts[0].getDoubleValue());
        s.flexibility = (Flexibility) juce::jlimit (0, 2, parts[1].getIntValue());
        s.maxShiftSeconds = juce::jlimit (leastShift, mostShift, parts[2].getDoubleValue());
        s.matchWords = parts[3] == "1";
        s.preferShifting = parts[4] == "1";
        s.onlyWhereBothPlay = parts[5] == "1";
        return s;
    }

    //==========================================================================
    namespace
    {
        constexpr int bands = 24;
        constexpr double hop = 0.01;   // seconds between measurements

        struct Measures
        {
            std::vector<float> loud;     // in tens of dB below the loudest (0 to -6)
            std::vector<float> onset;    // how much louder than the moment before
            std::vector<std::array<float, bands>> shape;   // log band energies, each moment's mean 0, spread 1
            std::vector<char> active;    // sound (within 40 dB of the loudest)
        };

        float sampleAt (const AlignSound& sound, double sampleRate, juce::int64 index)
        {
            juce::ignoreUnused (sampleRate);
            return index >= 0 && index < (juce::int64) sound.samples.size() ? sound.samples[(size_t) index] : 0.0f;
        }

        // `count` moments `hop` apart, the first at song second `first`.
        Measures measure (const AlignSound& sound, double first, int count, double sampleRate)
        {
            constexpr int order = 11, size = 1 << order;
            juce::dsp::FFT fft (order);
            juce::dsp::WindowingFunction<float> window ((size_t) size, juce::dsp::WindowingFunction<float>::hann, false);
            std::array<int, bands + 1> edges {};
            const auto top = juce::jmin (8000.0, sampleRate * 0.45);
            for (int b = 0; b <= bands; ++b)
            {
                const auto f = 80.0 * std::pow (top / 80.0, b / (double) bands);
                edges[(size_t) b] = juce::jlimit (1, size / 2, (int) (f * size / sampleRate));
            }
            for (int b = 1; b <= bands; ++b)
                edges[(size_t) b] = juce::jmax (edges[(size_t) b], edges[(size_t) b - 1] + 1);

            Measures m;
            m.loud.resize ((size_t) count);
            m.onset.resize ((size_t) count);
            m.shape.resize ((size_t) count);
            m.active.resize ((size_t) count);
            std::vector<float> frame ((size_t) size * 2);
            std::vector<float> dB ((size_t) count);
            for (int k = 0; k < count; ++k)
            {
                const auto centre = (juce::int64) std::llround ((first + k * hop - sound.start) * sampleRate);
                auto sum = 0.0;
                std::fill (frame.begin(), frame.end(), 0.0f);
                for (int n = 0; n < size; ++n)
                {
                    const auto v = sampleAt (sound, sampleRate, centre - size / 2 + n);
                    frame[(size_t) n] = v;
                    sum += (double) v * v;
                }
                dB[(size_t) k] = (float) (10.0 * std::log10 (sum / size + 1.0e-12));
                window.multiplyWithWindowingTable (frame.data(), (size_t) size);
                fft.performFrequencyOnlyForwardTransform (frame.data(), true);
                auto& shape = m.shape[(size_t) k];
                auto mean = 0.0f;
                for (int b = 0; b < bands; ++b)
                {
                    auto energy = 0.0f;
                    for (int bin = edges[(size_t) b]; bin < edges[(size_t) b + 1]; ++bin)
                        energy += frame[(size_t) bin] * frame[(size_t) bin];
                    shape[(size_t) b] = std::log (energy + 1.0e-9f);
                    mean += shape[(size_t) b];
                }
                mean /= (float) bands;
                auto spread = 0.0f;
                for (auto& v : shape)
                    spread += (v - mean) * (v - mean);
                spread = std::sqrt (spread / bands) + 1.0e-3f;
                for (auto& v : shape)
                    v = (v - mean) / spread;
            }
            const auto loudest = count > 0 ? *std::max_element (dB.begin(), dB.end()) : -120.0f;
            for (int k = 0; k < count; ++k)
            {
                m.loud[(size_t) k] = juce::jmax (-6.0f, (dB[(size_t) k] - loudest) / 10.0f);
                m.active[(size_t) k] = dB[(size_t) k] > loudest - 40.0f && dB[(size_t) k] > -80.0f;
                m.onset[(size_t) k] = k > 0 ? juce::jmax (0.0f, m.loud[(size_t) k] - m.loud[(size_t) k - 1]) : 0.0f;
            }
            return m;
        }

        // A one-millisecond envelope of a stretch of a sound (for placing a point to the sample).
        std::vector<float> envelope (const AlignSound& sound, double sampleRate, double from, int length)
        {
            std::vector<float> env ((size_t) juce::jmax (0, length));
            const auto first = (juce::int64) std::llround ((from - sound.start) * sampleRate);
            const auto coefficient = (float) std::exp (-1.0 / (0.001 * sampleRate));
            auto level = 0.0f;
            for (int n = -64; n < length; ++n)   // (a little before, so it has settled)
            {
                const auto v = std::abs (sampleAt (sound, sampleRate, first + n));
                level = juce::jmax (v, level * coefficient);
                if (n >= 0)
                    env[(size_t) n] = level;
            }
            return env;
        }

        // The best lag (seconds, added to `around`) of the target's envelope against the
        // reference's near `t`, within +/- `range`; nothing if they don't look alike there.
        std::optional<double> placeExactly (const AlignSound& reference, const AlignSound& target, double sampleRate,
                                            double t, double around, double range)
        {
            const auto span = 0.03;
            const auto length = (int) (2 * span * sampleRate);
            const auto lagSamples = (int) (range * sampleRate);
            const auto step = juce::jmax (1, (int) (sampleRate / 4000.0));
            const auto ref = envelope (reference, sampleRate, t - span, length);
            const auto tgt = envelope (target, sampleRate, t + around - span - range, length + 2 * lagSamples + 1);
            const auto meanOf = [] (const float* data, int n)
            {
                auto s = 0.0;
                for (int i = 0; i < n; ++i)
                    s += data[i];
                return n > 0 ? s / n : 0.0;
            };
            const auto refMean = meanOf (ref.data(), length);
            auto refPower = 0.0;
            for (int i = 0; i < length; ++i)
                refPower += (ref[(size_t) i] - refMean) * (ref[(size_t) i] - refMean);
            if (refPower <= 1.0e-9)
                return std::nullopt;
            auto best = -2.0, atZero = -2.0;
            auto bestLag = 0;
            for (int lag = -lagSamples; lag <= lagSamples; lag += step)
            {
                const auto* t0 = tgt.data() + lag + lagSamples;
                const auto tgtMean = meanOf (t0, length);
                auto cross = 0.0, power = 0.0;
                for (int i = 0; i < length; ++i)
                {
                    const auto b = t0[i] - tgtMean;
                    cross += (ref[(size_t) i] - refMean) * b;
                    power += b * b;
                }
                const auto score = power > 1.0e-9 ? cross / std::sqrt (refPower * power) : -1.0;
                if (lag == 0 || (lag < 0 && lag + step > 0))
                    atZero = score;
                if (score > best)
                    best = score, bestLag = lag;
            }
            if (best < 0.6 || best < atZero + 0.02)
                return std::nullopt;
            return bestLag / sampleRate;
        }
    }

    double alignedTargetAt (const AlignResult& result, double t)
    {
        const auto& p = result.points;
        if (p.empty())
            return t + result.shift;
        if (t <= p.front().first)
            return t + p.front().second - p.front().first;
        if (t >= p.back().first)
            return t + p.back().second - p.back().first;
        for (size_t k = 0; k + 1 < p.size(); ++k)
            if (t <= p[k + 1].first)
            {
                const auto span = p[k + 1].first - p[k].first;
                const auto along = span > 0.0 ? (t - p[k].first) / span : 0.0;
                return p[k].second + along * (p[k + 1].second - p[k].second);
            }
        return t + p.back().second - p.back().first;
    }

    //==========================================================================
    AlignResult alignSounds (const AlignSound& reference, const AlignSound& target, double from, double to,
                             double sampleRate, const AlignSettings& settings,
                             const std::vector<AlignSyncPoint>& syncPoints,
                             const std::vector<juce::Range<double>>& protectedRanges)
    {
        AlignResult result;
        if (sampleRate <= 0.0 || to - from < 0.1)
        {
            result.error = "The reference and the target overlap too little to align.";
            return result;
        }
        const auto W = juce::jmax (1, (int) std::ceil (juce::jlimit (AlignSettings::leastShift, AlignSettings::mostShift,
                                                                     settings.maxShiftSeconds) / hop));
        const auto B = 2 * W + 1;
        const auto N = (int) std::floor ((to - from) / hop) + 1;
        const auto ref = measure (reference, from, N, sampleRate);
        const auto tgt = measure (target, from - W * hop, N + 2 * W, sampleRate);
        const auto tolerance = settings.tolerance();

        // How unalike two moments are (the target's moment j = i + k).
        const auto shapeWeight = settings.matchWords ? 0.8f : 0.3f;
        std::vector<float> cost ((size_t) N * (size_t) B);
        auto activeSum = 0.0;
        auto activeCount = 0;
        for (int i = 0; i < N; ++i)
            for (int k = 0; k < B; ++k)
            {
                const auto j = i + k;
                const auto both = ref.active[(size_t) i] && tgt.active[(size_t) j];
                if (settings.onlyWhereBothPlay && ! both)
                {
                    cost[(size_t) i * (size_t) B + (size_t) k] = 0.0f;
                    continue;
                }
                auto shapeDistance = 0.0f;
                for (int b = 0; b < bands; ++b)
                {
                    const auto d = ref.shape[(size_t) i][(size_t) b] - tgt.shape[(size_t) j][(size_t) b];
                    shapeDistance += d * d;
                }
                const auto c = shapeWeight * std::sqrt (shapeDistance / bands)
                             + std::abs (ref.loud[(size_t) i] - tgt.loud[(size_t) j])
                             + 2.0f * std::abs (ref.onset[(size_t) i] - tgt.onset[(size_t) j]);
                cost[(size_t) i * (size_t) B + (size_t) k] = c;
                if (both)
                    activeSum += c, ++activeCount;
            }
        if (activeCount == 0)
        {
            result.error = "Nothing is heard where both play.";
            return result;
        }
        const auto factor = settings.flexibility == AlignSettings::Flexibility::low ? 1.0
                          : settings.flexibility == AlignSettings::Flexibility::normal ? 0.35 : 0.1;
        const auto penalty = (float) (factor * activeSum / activeCount);

        // Sync points: their moment held to where they say (within a moment), in order and at a
        // pace the path can keep (twice as fast or half as fast at most), else not met.
        struct Sync { int index, i, k; double offset; };
        std::vector<Sync> syncs;
        {
            std::vector<int> order ((size_t) syncPoints.size());
            std::iota (order.begin(), order.end(), 0);
            std::sort (order.begin(), order.end(), [&] (int a, int b) { return syncPoints[(size_t) a].reference < syncPoints[(size_t) b].reference; });
            for (const auto s : order)
            {
                const auto& sp = syncPoints[(size_t) s];
                const auto i = (int) std::llround ((sp.reference - from) / hop);
                const auto k = (int) std::llround ((sp.target - (from - W * hop)) / hop) - i;
                auto fits = i >= 0 && i < N && k >= 1 && k < B - 1;
                if (fits && ! syncs.empty())
                {
                    const auto& last = syncs.back();
                    const auto dt = sp.reference - syncPoints[(size_t) last.index].reference;
                    const auto dg = sp.target - syncPoints[(size_t) last.index].target;
                    fits = dt > 0.02 && dg / dt > 0.55 && dg / dt < 1.8;
                }
                if (fits)
                    syncs.push_back ({ s, i, k, sp.target - sp.reference });
                else
                    result.unmetSyncPoints.push_back (s);
            }
        }

        // The path (dynamic time warping in the band).
        constexpr auto infinity = 1.0e30f;
        std::vector<float> total ((size_t) N * (size_t) B, infinity);
        std::vector<char> steps ((size_t) N * (size_t) B, 0);
        std::vector<int> syncK ((size_t) N, -1);
        for (const auto& s : syncs)
            syncK[(size_t) s.i] = s.k;
        const auto allowed = [&] (int i, int k) { return syncK[(size_t) i] < 0 || std::abs (k - syncK[(size_t) i]) <= 1; };
        const auto at = [B] (int i, int k) { return (size_t) i * (size_t) B + (size_t) k; };
        for (int i = 0; i < N; ++i)
            for (int k = 0; k < B; ++k)
            {
                if (! allowed (i, k))
                    continue;
                const auto c = cost[at (i, k)];
                if (i == 0)
                {
                    total[at (i, k)] = c;
                    continue;
                }
                auto best = total[at (i - 1, k)];
                char step = 0;
                if (k >= 1 && total[at (i - 1, k - 1)] + penalty < best)   // the target twice as fast
                    best = total[at (i - 1, k - 1)] + penalty, step = 1;
                if (i >= 2 && k + 1 < B && allowed (i - 1, k)
                    && total[at (i - 2, k + 1)] + cost[at (i - 1, k)] + penalty < best)   // half as fast
                    best = total[at (i - 2, k + 1)] + cost[at (i - 1, k)] + penalty, step = 2;
                if (best >= infinity)
                    continue;
                total[at (i, k)] = c + best;
                steps[at (i, k)] = step;
            }
        auto endK = 0;
        for (int k = 1; k < B; ++k)
            if (total[at (N - 1, k)] < total[at (N - 1, endK)])
                endK = k;
        if (total[at (N - 1, endK)] >= infinity)
        {
            // (Shouldn't happen after the checks above: aligned without them.)
            auto without = alignSounds (reference, target, from, to, sampleRate, settings, {}, protectedRanges);
            for (const auto& s : syncs)
                without.unmetSyncPoints.push_back (s.index);
            return without;
        }
        std::vector<int> kAt ((size_t) N, W);
        for (int i = N - 1, k = endK; i >= 0;)
        {
            kAt[(size_t) i] = k;
            if (i == 0)
                break;
            const auto step = steps[at (i, k)];
            if (step == 0)
                --i;
            else if (step == 1)
                --i, --k;
            else
            {
                kAt[(size_t) i - 1] = k;
                i -= 2, ++k;
            }
        }
        std::vector<double> raw ((size_t) N);
        for (int i = 0; i < N; ++i)
            raw[(size_t) i] = (kAt[(size_t) i] - W) * hop;
        const auto bothAt = [&] (int i) { return ref.active[(size_t) i] && tgt.active[(size_t) (i + kAt[(size_t) i])]; };

        // Smoothed by the precision; protected ranges held at their average; sync points exact.
        const auto half = (int) std::lround ((1.0 - juce::jlimit (0.0, 1.0, settings.precision)) * 15.0) + 1;
        std::vector<double> smooth ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            auto sum = 0.0;
            auto n = 0;
            for (int j = juce::jmax (0, i - half); j <= juce::jmin (N - 1, i + half); ++j, ++n)
                sum += raw[(size_t) j];
            smooth[(size_t) i] = sum / n;
        }
        std::vector<char> forced ((size_t) N, 0), held ((size_t) N, 0);
        forced.front() = forced.back() = 1;
        for (const auto& range : protectedRanges)
        {
            const auto a = juce::jlimit (0, N - 1, (int) std::ceil ((range.getStart() - from) / hop));
            const auto b = juce::jlimit (0, N - 1, (int) std::floor ((range.getEnd() - from) / hop));
            if (b <= a)
                continue;
            auto sum = 0.0;
            for (int i = a; i <= b; ++i)
                sum += smooth[(size_t) i];
            for (int i = a; i <= b; ++i)
                smooth[(size_t) i] = sum / (b - a + 1), held[(size_t) i] = 1;
            forced[(size_t) a] = forced[(size_t) b] = 1;
        }
        for (const auto& s : syncs)
        {
            smooth[(size_t) s.i] = s.offset;
            forced[(size_t) s.i] = 1;
        }

        // How far apart they were.
        auto apart = 0.0;
        auto counted = 0;
        std::vector<double> offsets;
        for (int i = 0; i < N; ++i)
            if (bothAt (i))
                apart += std::abs (raw[(size_t) i]), ++counted, offsets.push_back (raw[(size_t) i]);
        result.before = counted > 0 ? apart / counted : 0.0;
        if (offsets.empty())
            offsets = raw;

        const auto finish = [&]
        {
            auto off = 0.0;
            auto n = 0;
            for (int i = 0; i < N; ++i)
                if (bothAt (i))
                    off += std::abs (raw[(size_t) i] - (alignedTargetAt (result, from + i * hop) - (from + i * hop))), ++n;
            result.after = n > 0 ? off / n : 0.0;
            result.ok = true;
            return result;
        };

        // Prefer Time Shifting: one move when that keeps (nearly) every moment within reach.
        if (settings.preferShifting)
        {
            std::nth_element (offsets.begin(), offsets.begin() + (long) offsets.size() / 2, offsets.end());
            auto median = offsets[offsets.size() / 2];
            const auto reach = juce::jmax (tolerance, hop * 1.01);
            auto within = 0;
            for (const auto o : offsets)
                if (std::abs (o - median) <= reach)
                    ++within;
            const auto syncsAgree = std::all_of (syncs.begin(), syncs.end(), [&] (const Sync& s) { return std::abs (s.offset - median) <= reach; });
            if (within >= (int) offsets.size() * 8 / 10 && syncsAgree)
            {
                // To the sample: the waveforms themselves (several microphones on one performance)
                // over the busiest two seconds.
                const auto window = juce::jmin (N, 200);
                auto bestStart = 0, bestCount = -1, running = 0;
                for (int i = 0; i < N; ++i)
                {
                    running += bothAt (i) ? 1 : 0;
                    if (i >= window)
                        running -= bothAt (i - window) ? 1 : 0;
                    if (i >= window - 1 && running > bestCount)
                        bestCount = running, bestStart = i - window + 1;
                }
                const auto length = (int) (window * hop * sampleRate);
                const auto first = from + bestStart * hop;
                const auto range = (int) ((hop + 0.003) * sampleRate);
                const auto r0 = (juce::int64) std::llround ((first - reference.start) * sampleRate);
                const auto t0 = (juce::int64) std::llround ((first + median - target.start) * sampleRate);
                auto refPower = 0.0;
                for (int n = 0; n < length; ++n)
                    refPower += (double) sampleAt (reference, sampleRate, r0 + n) * sampleAt (reference, sampleRate, r0 + n);
                auto best = 0.0;
                auto bestLag = 0;
                for (int lag = -range; lag <= range; ++lag)
                {
                    auto cross = 0.0, power = 0.0;
                    for (int n = 0; n < length; ++n)
                    {
                        const auto b = (double) sampleAt (target, sampleRate, t0 + lag + n);
                        cross += sampleAt (reference, sampleRate, r0 + n) * b;
                        power += b * b;
                    }
                    const auto score = power > 0.0 && refPower > 0.0 ? cross / std::sqrt (refPower * power) : 0.0;
                    if (score > best)
                        best = score, bestLag = lag;
                }
                if (best > 0.3)
                    median += bestLag / sampleRate;
                result.shiftOnly = true;
                result.shift = median;
                result.points = { { from, from + median }, { to, to + median } };
                return finish();
            }
        }

        // As few points as keep every moment within the tolerance (Douglas-Peucker between the
        // points that must stay).
        std::vector<char> keep = forced;
        std::vector<std::pair<int, int>> stack;
        auto previous = 0;
        for (int i = 1; i < N; ++i)
            if (forced[(size_t) i])
            {
                stack.push_back ({ previous, i });
                previous = i;
            }
        while (! stack.empty())
        {
            const auto [a, b] = stack.back();
            stack.pop_back();
            auto worst = 0.0;
            auto worstAt = -1;
            for (int i = a + 1; i < b; ++i)
            {
                const auto along = (double) (i - a) / (b - a);
                const auto line = smooth[(size_t) a] + along * (smooth[(size_t) b] - smooth[(size_t) a]);
                const auto d = std::abs (smooth[(size_t) i] - line);
                if (d > worst)
                    worst = d, worstAt = i;
            }
            if (worstAt > 0 && worst > tolerance)
            {
                keep[(size_t) worstAt] = 1;
                stack.push_back ({ a, worstAt });
                stack.push_back ({ worstAt, b });
            }
        }

        // The points, each placed to the sample where both sound alike there; then kept at a pace
        // the stretcher can take, at least 20 ms apart.
        struct Point { double t, g; bool must; };
        std::vector<Point> points;
        for (int i = 0; i < N; ++i)
        {
            if (! keep[(size_t) i])
                continue;
            const auto t = i == N - 1 ? to : from + i * hop;
            auto offset = smooth[(size_t) i];
            const auto sync = std::find_if (syncs.begin(), syncs.end(), [i] (const Sync& s) { return s.i == i; });
            if (sync != syncs.end())
            {
                points.push_back ({ syncPoints[(size_t) sync->index].reference, syncPoints[(size_t) sync->index].target, true });
                continue;
            }
            if (! held[(size_t) i] && i > 0 && i < N - 1 && bothAt (i))
                if (const auto exact = placeExactly (reference, target, sampleRate, t, offset, 0.012))
                    offset += *exact;
            points.push_back ({ t, t + offset, forced[(size_t) i] != 0 });
        }
        // (Points that must stay win: sync points, protected edges and the ends; a point too
        // close to one of them, or at a pace the stretcher can't take, goes.)
        const auto fits = [] (const Point& a, const Point& b)
        {
            const auto dt = b.t - a.t, dg = b.g - a.g;
            return dt >= 0.02 && dg > 0.0 && dg / dt >= 0.4 && dg / dt <= 2.5;
        };
        std::vector<Point> paced;
        for (const auto& point : points)
        {
            while (! paced.empty() && ! fits (paced.back(), point))
            {
                if (! point.must)
                    break;
                if (paced.back().must)
                    break;   // (two that must stay but can't both: the later one waits below)
                paced.pop_back();
            }
            if (! paced.empty() && ! fits (paced.back(), point))
            {
                if (! point.must || paced.size() == 1)
                    continue;
                // Both must stay: the earlier one, unless it's the start.
                paced.pop_back();
            }
            paced.push_back (point);
        }
        result.points.clear();
        for (const auto& p : paced)
            result.points.push_back ({ p.t, p.g });
        return finish();
    }
}
