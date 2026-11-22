#include "engine/ChordRoll.h"

#include "engine/Markers.h"
#include "engine/Progressions.h"
#include "engine/SongSections.h"

#include <array>
#include <cmath>

namespace pr7s
{
    namespace
    {
        // The chords drawn from: the key's seven triads (keyChords' first seven), a minor key's V and raised vii° too.
        std::vector<Chord> rollChordsOf (const Key& key)
        {
            const auto all = keyChords (key);
            std::vector<Chord> out;
            const auto count = key.minor ? 9 : 7;
            for (int i = 0; i < count && i < (int) all.size(); ++i)
                if (std::find (out.begin(), out.end(), all[(size_t) i]) == out.end())
                    out.push_back (all[(size_t) i]);
            return out;
        }

        std::vector<int> degreesOf (const Key& key, const std::vector<Chord>& chords)
        {
            std::vector<int> out;
            for (size_t i = 0; i < chords.size(); ++i)
                out.push_back (i < 7 ? (int) i : (i == 7 ? 4 : 6));   // (a minor key's V: the fifth; its raised vii°: the seventh)
            juce::ignoreUnused (key);
            return out;
        }

        Key cMajor() { return Key { 0, 0, false }; }
        Key aMinor() { return Key { 5, 0, true }; }

        int plainIndex (const std::vector<Chord>& chords, const Chord& c)
        {
            const Chord plain { c.root, c.type, Chord::Tension::none, -1 };
            for (size_t i = 0; i < chords.size(); ++i)
                if (chords[i] == plain)
                    return (int) i;
            return -1;
        }

        struct Counts
        {
            std::vector<std::vector<double>> next;
            std::vector<double> start;
        };

        Counts countFor (bool minor)
        {
            const auto key = minor ? aMinor() : cMajor();
            const auto chords = rollChordsOf (key);
            const auto n = chords.size();
            Counts c { std::vector<std::vector<double>> (n, std::vector<double> (n, 0.0)), std::vector<double> (n, 0.0) };
            const auto add = [&] (const std::vector<Chord>& sequence, double weight, bool opening)
            {
                // Each chord as one of the key's (its tension and bass dropped); a chord outside them breaks the chain.
                std::vector<int> run;
                for (const auto& ch : sequence)
                    run.push_back (plainIndex (chords, ch));
                std::vector<int> tidy;   // (the same chord again: one)
                for (const auto i : run)
                    if (tidy.empty() || tidy.back() != i)
                        tidy.push_back (i);
                for (size_t i = 1; i < tidy.size(); ++i)
                    if (tidy[i - 1] >= 0 && tidy[i] >= 0)
                        c.next[(size_t) tidy[i - 1]][(size_t) tidy[i]] += weight;
                if (tidy.size() > 2 && tidy.front() >= 0 && tidy.back() >= 0 && tidy.front() != tidy.back())
                    c.next[(size_t) tidy.back()][(size_t) tidy.front()] += weight;   // (played round)
                if (opening && ! tidy.empty() && tidy.front() >= 0)
                    c.start[(size_t) tidy.front()] += 1.0;
            };
            for (const auto& p : progressionLibrary())
            {
                std::vector<Chord> sequence;
                for (const auto& s : p.in (key))
                    sequence.push_back (s.chord);
                const auto own = p.minor == minor;
                add (sequence, own ? 1.0 : 0.5, own);
            }
            for (const auto& p : popularProgressions (key))
                add (p.chords, 1.0, true);
            return c;
        }

        const Counts& countsOf (bool minor)
        {
            static const Counts major = countFor (false), minorCounts = countFor (true);
            return minor ? minorCounts : major;
        }

        // The section's feel: multipliers by scale degree for the opening and for every next chord. Ours.
        struct Feel
        {
            std::array<double, 7> start { 1, 1, 1, 1, 1, 1, 1 }, next { 1, 1, 1, 1, 1, 1, 1 };
            double loopPull = 1.5, endOnTonic = 1.0, toFifthAtEnd = 1.0, twoBarChance = 0.25, redrawChance = 0.0;
        };

        Feel feelOf (const juce::String& name)
        {
            Feel f;
            if (name.trim().isEmpty())
                return f;
            const auto base = sectionBaseOf (name);
            if (base == "Pre-Chorus")
            {
                f.start = { 0.5, 1.4, 1, 1.5, 1, 1.4, 1 };
                f.next = { 0.85, 1.2, 1, 1.3, 1.3, 1.1, 1 };
                f.toFifthAtEnd = 2.5;
                return f;
            }
            if (base == "Post-Chorus")
            {
                f.start = { 1, 1, 1.1, 1.2, 1, 1.3, 1.05 };
                f.next = { 1, 1, 1.15, 1.15, 1, 1.2, 1.15 };
                return f;
            }
            switch (composerSectionFor (name))
            {
                case 0:   // intro
                    f.start = { 1.3, 1, 1, 1.1, 1, 1.1, 1 };
                    f.next = { 1.1, 1, 1, 1.15, 1, 1.15, 1 };
                    break;
                case 2:   // chorus
                    f.start = { 1, 1, 1.2, 1.3, 1, 1.6, 1.1 };
                    f.next = { 1, 1, 1.3, 1.3, 1, 1.4, 1.3 };
                    break;
                case 3:   // bridge
                    f.start = { 0.25, 1.2, 1, 1.5, 1.3, 1.5, 1 };
                    f.next = { 0.7, 1.1, 1, 1.4, 1.3, 1.4, 1 };
                    f.redrawChance = 0.4;
                    break;
                case 4:   // breakdown
                    f.start = { 1.5, 1.2, 1, 1, 1, 1.2, 1.2 };
                    f.next = { 1.4, 1.3, 1, 1, 1, 1.2, 1.2 };
                    f.twoBarChance = 0.5;
                    break;
                case 5:   // outro / ending
                    f.start = { 1.4, 1, 1, 1, 1, 1, 1 };
                    f.next = { 1.2, 1, 1, 1, 1, 1, 1 };
                    f.endOnTonic = 8.0;
                    break;
                case 6:   // solo
                    f.start = { 1.1, 1, 1, 1, 1, 1.2, 1.2 };
                    f.next = { 1, 1, 1, 1, 1, 1.25, 1.25 };
                    f.loopPull = 5.0;
                    f.redrawChance = 0.3;
                    break;
                default:  // verse (and what plays as one)
                    f.start = { 1.2, 1, 1, 1, 1, 1, 1 };
                    f.next = { 1.2, 1, 1, 1.2, 0.9, 1.15, 1 };
                    break;
            }
            return f;
        }

        // The tonic's opening weight at most `share` of all the opening weight.
        void capTonic (std::vector<double>& start, double share)
        {
            auto others = 0.0;
            for (size_t i = 1; i < start.size(); ++i)
                others += start[i];
            if (! start.empty())
                start[0] = juce::jmin (start[0], others * share / (1.0 - share));
        }

        int pick (const std::vector<double>& weights, juce::Random& random)
        {
            auto sum = 0.0;
            for (const auto w : weights)
                sum += juce::jmax (0.0, w);
            if (sum <= 0.0)
                return 0;
            auto r = random.nextDouble() * sum;
            for (size_t i = 0; i < weights.size(); ++i)
            {
                r -= juce::jmax (0.0, weights[i]);
                if (r < 0.0)
                    return (int) i;
            }
            for (int i = (int) weights.size() - 1; i >= 0; --i)
                if (weights[(size_t) i] > 0.0)
                    return i;
            return 0;
        }

        // How much a chord leads into `target`, 0-1 (its weight toward it over its strongest move).
        double leadsInto (const ChordRollWeights& w, int from, int target)
        {
            const auto& row = w.next[(size_t) from];
            const auto most = *std::max_element (row.begin(), row.end());
            return most > 0.0 ? row[(size_t) target] / most : 0.0;
        }

        std::vector<int> drawRound (const ChordRollWeights& w, int count, int notFirst, juce::Random& random)
        {
            std::vector<int> round;
            if (count <= 0)
                return round;
            auto start = w.start;
            if (notFirst >= 0)
                start[(size_t) notFirst] = 0.0;
            round.push_back (pick (start, random));
            for (int k = 1; k < count; ++k)
            {
                auto weights = w.next[(size_t) round.back()];
                if (k == count - 1)
                {
                    for (size_t b = 0; b < weights.size(); ++b)
                    {
                        weights[b] *= 1.0 + w.loopPull * leadsInto (w, (int) b, round.front());
                        if (w.degrees[b] == 4)
                            weights[b] *= w.toFifthAtEnd;
                    }
                    if (count >= 2)
                        weights[(size_t) round.front()] = 0.0;   // (round again: never the same chord twice in a row)
                }
                round.push_back (pick (weights, random));
            }
            return round;
        }

        std::vector<RolledChord> drawOnce (const ChordRollRequest& request, const ChordRollWeights& w, juce::Random& random)
        {
            std::vector<RolledChord> out;
            const auto bpb = juce::jmax (0.25, request.beatsPerBar);
            const auto total = juce::jmax (0, request.bars) * bpb;
            if (total <= 0.0 || w.chords.empty())
                return out;

            // The lengths.
            std::vector<double> lengths;
            auto laid = 0.0;
            for (const auto l : request.layout)
                laid += l;
            int cycle = 4;
            if (request.layout.size() >= 2 && std::abs (laid - total) < 1.0e-6)
            {
                lengths = request.layout;
                const auto n = (int) lengths.size();
                cycle = n > 4 && n % 4 == 0 ? 4 : n;
            }
            else
            {
                const auto twoBars = request.bars >= 8 && random.nextDouble() < w.twoBarChance;
                const auto chordBeats = (twoBars ? 2.0 : 1.0) * bpb;
                for (auto t = 0.0; t < total - 1.0e-6; t += chordBeats)
                    lengths.push_back (juce::jmin (chordBeats, total - t));
                cycle = request.bars < 4 ? (int) lengths.size() : 4;
            }
            const auto n = (int) lengths.size();
            cycle = juce::jlimit (1, juce::jmax (1, n), cycle);

            // The rounds: the first drawn, then repeated (bridges and solos sometimes draw a round anew).
            std::vector<int> sequence;
            const auto first = drawRound (w, juce::jmin (cycle, n), -1, random);
            sequence = first;
            while ((int) sequence.size() < n)
            {
                auto round = first;
                if (random.nextDouble() < w.redrawChance)
                    round = drawRound (w, cycle, sequence.back(), random);
                for (const auto i : round)
                    if ((int) sequence.size() < n)
                        sequence.push_back (i);
            }

            // The section's last chord: toward the next section's first, and home in an outro.
            const auto nextIndex = request.next.has_value() ? w.indexOf (*request.next) : -1;
            if (n >= 3 && w.endOnTonic > 1.0 && w.degrees[(size_t) sequence[(size_t) n - 2]] == 0)
            {
                auto weights = w.next[(size_t) sequence[(size_t) n - 3]];   // (the chord before away from home, so the last can come home)
                for (size_t b = 0; b < weights.size(); ++b)
                    if (w.degrees[b] == 0)
                        weights[b] = 0.0;
                sequence[(size_t) n - 2] = pick (weights, random);
            }
            if (n >= 2 && (nextIndex >= 0 || w.endOnTonic > 1.0))
            {
                const auto before = sequence[(size_t) n - 2];
                auto weights = w.next[(size_t) before];
                for (size_t b = 0; b < weights.size(); ++b)
                {
                    if (nextIndex >= 0)
                        weights[b] *= 1.0 + 1.5 * leadsInto (w, (int) b, nextIndex);
                    if (w.degrees[b] == 0)
                        weights[b] *= w.endOnTonic;
                }
                if (nextIndex >= 0 && nextIndex != before)
                    weights[(size_t) nextIndex] *= 0.2;   // (not the next section's chord already)
                sequence[(size_t) n - 1] = pick (weights, random);
            }

            // Never the same chord twice in a row (a round's join, a redrawn round).
            for (int i = 1; i < n; ++i)
                if (sequence[(size_t) i] == sequence[(size_t) i - 1])
                {
                    auto weights = w.next[(size_t) sequence[(size_t) i - 1]];
                    if (i + 1 < n)
                        weights[(size_t) sequence[(size_t) i + 1]] = 0.0;
                    sequence[(size_t) i] = pick (weights, random);
                }

            for (int i = 0; i < n; ++i)
                out.push_back ({ w.chords[(size_t) sequence[(size_t) i]], lengths[(size_t) i] });
            return out;
        }
    }

    int ChordRollWeights::indexOf (const Chord& c) const
    {
        for (size_t i = 0; i < chords.size(); ++i)
            if (chords[i] == c)
                return (int) i;
        return -1;
    }

    const std::vector<std::vector<double>>& chordRollCounts (bool minor)
    {
        return countsOf (minor).next;
    }

    ChordRollWeights chordRollWeights (const Key& key, const juce::String& sectionName)
    {
        ChordRollWeights w;
        w.chords = rollChordsOf (key);
        w.degrees = degreesOf (key, w.chords);
        const auto& counts = countsOf (key.minor);
        const auto n = w.chords.size();
        const auto feel = feelOf (sectionName);
        const auto smoothing = [&w] (size_t i) { return w.chords[i].type == Chord::Type::diminished ? 0.1 : 0.5; };
        w.start.assign (n, 0.0);
        w.next.assign (n, std::vector<double> (n, 0.0));
        for (size_t i = 0; i < n && i < counts.start.size(); ++i)
            w.start[i] = counts.start[i] + smoothing (i);
        capTonic (w.start, 1.3 / 2.3);
        for (size_t i = 0; i < n; ++i)
            w.start[i] *= feel.start[(size_t) w.degrees[i]];
        capTonic (w.start, 0.65);
        for (size_t a = 0; a < n; ++a)
            for (size_t b = 0; b < n; ++b)
                if (a != b)
                    w.next[a][b] = (a < counts.next.size() && b < counts.next[a].size() ? counts.next[a][b] : 0.0) + smoothing (b);
        for (size_t a = 0; a < n; ++a)
            for (size_t b = 0; b < n; ++b)
                w.next[a][b] *= feel.next[(size_t) w.degrees[b]];
        w.loopPull = feel.loopPull;
        w.endOnTonic = feel.endOnTonic;
        w.toFifthAtEnd = feel.toFifthAtEnd;
        w.twoBarChance = feel.twoBarChance;
        w.redrawChance = feel.redrawChance;
        return w;
    }

    juce::String rolledFingerprint (const std::vector<RolledChord>& chords)
    {
        juce::StringArray parts;
        for (const auto& c : chords)
            parts.add (c.chord.toText() + "*" + juce::String (c.beats, 4));
        return parts.joinIntoString (";");
    }

    std::vector<RolledChord> rollChords (const ChordRollRequest& request, juce::uint32 seed)
    {
        const auto w = chordRollWeights (request.key, request.section);
        std::vector<RolledChord> out;
        for (int attempt = 0; attempt < 16; ++attempt)
        {
            juce::Random random ((juce::int64) seed * 2654435761LL + attempt * 7919 + 17);
            out = drawOnce (request, w, random);
            const auto print = rolledFingerprint (out);
            if (std::find (request.avoid.begin(), request.avoid.end(), print) == request.avoid.end())
                break;
        }
        return out;
    }

    std::vector<RolledChord> rollChords (const Key& key, const juce::String& sectionName, int bars, double beatsPerBar,
                                         juce::uint32 seed, const std::vector<juce::String>& avoid)
    {
        ChordRollRequest request;
        request.key = key;
        request.section = sectionName;
        request.bars = bars;
        request.beatsPerBar = beatsPerBar;
        request.avoid = avoid;
        return rollChords (request, seed);
    }
}
