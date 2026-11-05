#include "engine/ChordDetect.h"

#include <juce_dsp/juce_dsp.h>

#include <numeric>
#include <set>

namespace pr7s
{
    namespace
    {
        int pcOf (int note) { return ((note % 12) + 12) % 12; }

        using T = Chord::Type;
        using X = Chord::Tension;
        using Shape = std::pair<T, X>;

        const std::vector<Shape>& shapesFor (bool plainOnly)
        {
            // Plain ones first: fewer tensions win a near tie.
            static const std::vector<Shape> plain { { T::major, X::none }, { T::minor, X::none }, { T::diminished, X::none } };
            static const std::vector<Shape> all {
                { T::major, X::none }, { T::minor, X::none }, { T::diminished, X::none }, { T::augmented, X::none },
                { T::sus4, X::none }, { T::sus2, X::none }, { T::major, X::seven }, { T::major, X::maj7 }, { T::minor, X::seven },
                { T::minor, X::maj7 }, { T::diminished, X::seven }, { T::diminished, X::dim7 }, { T::major, X::six }, { T::minor, X::six },
                { T::sus4, X::seven }, { T::major, X::add9 }, { T::major, X::nine }, { T::minor, X::nine } };
            return plainOnly ? plain : all;
        }

        constexpr double notACandidate = -10.0;

        // How well a chord fits a step: its notes heard, notes outside it, its own notes missing,
        // its root in the bass; as a share of what was heard (about 1 for a clean triad).
        double fit (const Chord& c, size_t shapeIndex, const ChromaStep& s, double total, double heaviest, int bassPc)
        {
            const auto classes = c.pitchClasses();
            double in = 0.0;
            auto missing = 0;
            for (const auto p : classes)
            {
                in += s.weight[(size_t) p];
                missing += s.weight[(size_t) p] < heaviest * 0.08 ? 1 : 0;
            }
            if ((int) classes.size() - missing < 2 || s.weight[(size_t) c.root] < heaviest * 0.08)
                return notACandidate;   // (its root heard, and at least two of its notes)
            auto score = in - 1.3 * (total - in) - 0.35 * heaviest * missing - 0.04 * heaviest * (double) shapeIndex;
            if (bassPc == c.root)
                score += 0.15 * heaviest;   // (the bass on the root: the likeliest reading)
            return score / total;
        }

        // The bass heard in a step: its strongest pitch class, if clear enough to count.
        int bassOf (const ChromaStep& s)
        {
            const auto sum = std::accumulate (s.bass.begin(), s.bass.end(), 0.0);
            if (sum <= 1.0e-12)
                return -1;
            const auto at = std::max_element (s.bass.begin(), s.bass.end());
            return *at >= sum * 0.3 ? (int) (at - s.bass.begin()) : -1;
        }
    }

    double ChromaStep::total() const
    {
        return std::accumulate (weight.begin(), weight.end(), 0.0);
    }

    juce::String DetectSettings::toString() const
    {
        return juce::String (stepBeats, 3) + " " + (plainOnly ? "1" : "0") + " " + juce::String (changeCost, 3);
    }

    DetectSettings DetectSettings::fromString (const juce::String& text)
    {
        const auto v = juce::StringArray::fromTokens (text, " ", {});
        DetectSettings d;
        if (v.size() != 3)
            return d;
        d.stepBeats = juce::jlimit (0.25, 16.0, v[0].getDoubleValue());
        d.plainOnly = v[1] == "1";
        d.changeCost = juce::jlimit (0.0, 2.0, v[2].getDoubleValue());
        return d;
    }

    std::vector<ChordEvent> chordsFromChroma (const std::vector<ChromaStep>& steps, double from, double step, const DetectSettings& settings)
    {
        std::vector<ChordEvent> result;
        const auto& shapes = shapesFor (settings.plainOnly);
        std::vector<Chord> states;
        for (int root = 0; root < 12; ++root)
            for (const auto& [type, tension] : shapes)
                states.push_back ({ root, type, tension, -1 });
        const auto n = states.size();

        // The best total for each chord at each step; where it came from (the same, or a change).
        std::vector<double> best (n, 0.0), next (n);
        std::vector<std::vector<int>> cameFrom (steps.size(), std::vector<int> (n, -1));
        std::vector<bool> heard (steps.size(), false);
        for (size_t t = 0; t < steps.size(); ++t)
        {
            const auto& s = steps[t];
            const auto total = s.total();
            const auto heaviest = total > 0.0 ? *std::max_element (s.weight.begin(), s.weight.end()) : 0.0;
            std::vector<double> scores (n, notACandidate);
            auto any = false;
            const auto bass = bassOf (s);
            if (total > 1.0e-12)
                for (size_t i = 0; i < n; ++i)
                {
                    scores[i] = fit (states[i], i % shapes.size(), s, total, heaviest, bass);
                    any = any || scores[i] > notACandidate;
                }
            heard[t] = any;
            const auto leader = (size_t) (std::max_element (best.begin(), best.end()) - best.begin());
            for (size_t i = 0; i < n; ++i)
            {
                // Silence: every chord simply carries on.
                if (! any)
                {
                    next[i] = best[i];
                    cameFrom[t][i] = (int) i;
                    continue;
                }
                const auto stay = best[i], change = best[leader] - settings.changeCost;
                cameFrom[t][i] = stay >= change ? (int) i : (int) leader;
                next[i] = juce::jmax (stay, change) + scores[i];
            }
            best = next;
        }
        if (steps.empty() || std::none_of (heard.begin(), heard.end(), [] (bool b) { return b; }))
            return result;

        // Back from the best ending: each step's chord.
        std::vector<int> chosen (steps.size());
        auto at = (int) (std::max_element (best.begin(), best.end()) - best.begin());
        for (auto t = (int) steps.size() - 1; t >= 0; --t)
        {
            chosen[(size_t) t] = at;
            at = cameFrom[(size_t) t][(size_t) at];
        }
        for (size_t t = 0; t < steps.size(); ++t)
        {
            if (! heard[t] && result.empty())
                continue;   // (nothing heard yet)
            auto chord = states[(size_t) chosen[t]];
            // An inversion: the bass heard on another of its notes (C/E).
            if (heard[t])
                if (const auto b = bassOf (steps[t]); b >= 0 && b != chord.root)
                {
                    const auto classes = chord.pitchClasses();
                    if (std::find (classes.begin(), classes.end(), b) != classes.end())
                        chord.bass = b;
                }
            if (result.empty() || ! (result.back().chord.root == chord.root && result.back().chord.type == chord.type
                                     && result.back().chord.tension == chord.tension))
                result.push_back ({ from + step * (double) t, chord });
        }
        return result;
    }

    std::vector<ChromaStep> chromaFromNotes (const std::vector<HeardNote>& notes, double from, double to, double step)
    {
        std::vector<ChromaStep> steps;
        step = juce::jmax (0.125, step);
        for (auto t = from; t < to - 1.0e-9; t += step)
        {
            ChromaStep s;
            const auto end = juce::jmin (to, t + step);
            auto lowest = 128;
            for (const auto& n : notes)
            {
                const auto overlap = juce::jmin (end, n.end) - juce::jmax (t, n.start);
                if (overlap <= 1.0e-9)
                    continue;
                s.weight[(size_t) pcOf (n.pitch)] += overlap * (0.4 + 0.6 * n.velocity);
                if (overlap >= (end - t) * 0.25)
                    lowest = juce::jmin (lowest, n.pitch);
            }
            if (lowest < 128)
                s.bass[(size_t) pcOf (lowest)] = 1.0;
            steps.push_back (s);
        }
        return steps;
    }

    std::vector<ChordEvent> detectChordsInNotes (const std::vector<HeardNote>& notes, double from, double to, const DetectSettings& settings)
    {
        const auto step = juce::jmax (0.125, settings.stepBeats);
        return chordsFromChroma (chromaFromNotes (notes, from, to, step), from, step, settings);
    }

    std::vector<ChromaStep> chromaFromAudio (const float* samples, int numSamples, double rate, double startBeat,
                                             const TempoMap& tempo, double from, double to, double step)
    {
        step = juce::jmax (0.125, step);
        const auto count = (size_t) juce::jmax (0, (int) std::ceil ((to - from) / step - 1.0e-9));
        std::vector<ChromaStep> steps (count);
        if (samples == nullptr || numSamples <= 0 || rate <= 0.0 || count == 0)
            return steps;
        // About 0.37 s a frame (16384 at 44.1 kHz: fine enough for A1's semitones), a quarter of it apart.
        auto order = 10;
        while ((1 << (order + 1)) <= rate * 0.4 && order < 16)
            ++order;
        const auto size = 1 << order, hop = size / 4;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) size, juce::dsp::WindowingFunction<float>::hann, false);
        std::vector<float> frame ((size_t) size * 2);
        constexpr int lowNote = 28, highNote = 96;   // E1 .. C7
        // Each bin's note (or -1 when it's between two).
        std::vector<int> binNote ((size_t) size / 2 + 1, -1);
        for (int bin = 1; bin <= size / 2; ++bin)
        {
            const auto f = bin * rate / size;
            const auto note = 69.0 + 12.0 * std::log2 (f / 440.0);
            const auto nearest = juce::roundToInt (note);
            if (nearest >= lowNote && nearest <= highNote && std::abs (note - nearest) < 0.4)
                binNote[(size_t) bin] = nearest;
        }
        const auto soundStart = tempo.secondsAt (startBeat);
        std::vector<std::array<double, highNote + 1>> frames;
        std::vector<size_t> frameStep;
        auto loudest = 0.0;
        for (auto start = -size / 2; start + size / 2 < numSamples; start += hop)
        {
            const auto beat = tempo.beatAt (soundStart + (start + size * 0.5) / rate);
            if (beat < from || beat >= from + step * (double) count)
                continue;
            std::fill (frame.begin(), frame.end(), 0.0f);
            for (int i = 0; i < size; ++i)
                if (const auto at = start + i; at >= 0 && at < numSamples)
                    frame[(size_t) i] = samples[at];
            window.multiplyWithWindowingTable (frame.data(), (size_t) size);
            fft.performFrequencyOnlyForwardTransform (frame.data(), true);
            std::array<double, highNote + 1> notes {};
            auto energy = 0.0;
            for (int bin = 1; bin <= size / 2; ++bin)
                if (const auto note = binNote[(size_t) bin]; note >= 0)
                {
                    notes[(size_t) note] += frame[(size_t) bin];
                    energy += frame[(size_t) bin];
                }
            // Each note's first overtones partly taken from the notes they land on (the octave, the
            // twelfth, two octaves, the seventeenth): a C would otherwise also read as G and E.
            static const std::pair<int, double> overtones[] { { 12, 0.6 }, { 19, 0.5 }, { 24, 0.35 }, { 28, 0.3 } };
            for (auto note = lowNote; note <= highNote; ++note)
                for (const auto& [up, share] : overtones)
                    if (note + up <= highNote)
                        notes[(size_t) (note + up)] = juce::jmax (0.0, notes[(size_t) (note + up)] - share * notes[(size_t) note]);
            loudest = juce::jmax (loudest, energy);
            frames.push_back (notes);
            frameStep.push_back ((size_t) ((beat - from) / step));
            frames.back()[0] = energy;   // (kept here for the silence check: note 0 is never used)
        }
        for (size_t f = 0; f < frames.size(); ++f)
        {
            if (frames[f][0] < loudest * 0.01 || frameStep[f] >= count)
                continue;   // (near silence)
            auto& s = steps[frameStep[f]];
            for (auto note = lowNote; note <= highNote; ++note)
            {
                const auto v = std::sqrt (frames[f][(size_t) note]);
                if (note >= 45)
                    s.weight[(size_t) pcOf (note)] += v;   // (A2 up: the harmony)
                if (note <= 59)
                    s.bass[(size_t) pcOf (note)] += v;     // (E1 to B2/C3: the bass)
            }
        }
        // Each step: what's barely there left out (noise, leftovers of overtones).
        for (auto& s : steps)
        {
            const auto top = *std::max_element (s.weight.begin(), s.weight.end());
            for (auto& w : s.weight)
                if (w < top * 0.15)
                    w = 0.0;
        }
        return steps;
    }

    std::vector<ChordEvent> detectChordsInAudio (const float* samples, int numSamples, double rate, double startBeat,
                                                 const TempoMap& tempo, double from, double to, const DetectSettings& settings)
    {
        const auto step = juce::jmax (0.125, settings.stepBeats);
        return chordsFromChroma (chromaFromAudio (samples, numSamples, rate, startBeat, tempo, from, to, step), from, step, settings);
    }

    std::optional<Chord> chordOfNotes (const std::vector<int>& pitches, bool plainOnly)
    {
        std::set<int> classes;
        for (const auto p : pitches)
            classes.insert (pcOf (p));
        if (classes.size() < 2)
            return std::nullopt;
        if (const auto exact = recogniseChord (pitches); exact && (! plainOnly || exact->tension == X::none))
            return exact;
        ChromaStep s;
        for (const auto p : pitches)
            s.weight[(size_t) pcOf (p)] += 1.0;
        s.bass[(size_t) pcOf (*std::min_element (pitches.begin(), pitches.end()))] = 1.0;
        DetectSettings d;
        d.plainOnly = plainOnly;
        const auto found = chordsFromChroma ({ s }, 0.0, 1.0, d);
        if (found.empty())
            return std::nullopt;
        return found.front().chord;
    }
}
