#include "engine/RiffPaste.h"

namespace pr7s
{
    namespace
    {
        int pcOf (int pitch) { return ((pitch % 12) + 12) % 12; }

        // The diatonic triad on a root in a key (none when the root is outside it).
        std::optional<Chord> diatonicTriad (int root, const Key& key)
        {
            const auto scale = key.scale();
            const auto at = std::find (scale.begin(), scale.end(), root);
            if (at == scale.end())
                return std::nullopt;
            const auto i = (size_t) std::distance (scale.begin(), at);
            const auto third = (scale[(i + 2) % 7] - root + 12) % 12;
            const auto fifth = (scale[(i + 4) % 7] - root + 12) % 12;
            Chord c;
            c.root = root;
            c.type = third == 4 ? Chord::Type::major : fifth == 6 ? Chord::Type::diminished : Chord::Type::minor;
            return c;
        }
    }

    ComposerRiff riffFromNotes (const std::vector<Note>& notes, double startBeat, double beatsPerBar, const juce::String& name, DrumSection section)
    {
        ComposerRiff riff;
        riff.name = name;
        riff.section = section;
        if (notes.empty() || beatsPerBar <= 0.0)
            return riff;
        // From the bar line at or before the part's first note.
        auto first = notes.front().start;
        for (const auto& n : notes)
            first = juce::jmin (first, n.start);
        const auto songFirst = startBeat + first;
        const auto barLine = std::floor (songFirst / beatsPerBar + 1.0e-6) * beatsPerBar;
        auto end = 0.0;
        for (const auto& n : notes)
        {
            LineNote l;
            l.beat = startBeat + n.start - barLine;
            l.length = n.length;
            l.pitch = n.pitch;
            l.velocity = n.velocity;
            riff.notes.push_back (l);
            end = juce::jmax (end, l.beat + juce::jmax (0.0, l.length - 1.0e-3));
        }
        std::stable_sort (riff.notes.begin(), riff.notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        riff.bars = juce::jlimit (1, 16, (int) std::ceil (end / beatsPerBar - 1.0e-6));
        const auto length = riff.bars * beatsPerBar;
        riff.notes.erase (std::remove_if (riff.notes.begin(), riff.notes.end(), [length] (const LineNote& n) { return n.beat >= length - 1.0e-9; }),
                          riff.notes.end());
        for (auto& n : riff.notes)
            n.length = juce::jmin (n.length, length - n.beat);
        return riff;
    }

    Key guessKey (const std::vector<LineNote>& notes)
    {
        std::array<double, 12> weight {}, low {};
        if (notes.empty())
            return Key::fromPitchClass (9, true);
        auto lowest = 127;
        for (const auto& n : notes)
            lowest = juce::jmin (lowest, n.pitch);
        for (const auto& n : notes)
        {
            // (Each hit counts as well as its length: a riff's chugs are short but they're its centre.)
            const auto w = (0.25 + juce::jmax (0.0, n.length)) * (0.5 + n.velocity);
            weight[(size_t) pcOf (n.pitch)] += w;
            if (n.pitch <= lowest + 7)   // (the low register: the pedal and the roots)
                low[(size_t) pcOf (n.pitch)] += w;
        }
        auto best = -1.0e9;
        Key chosen = Key::fromPitchClass (9, true);
        for (int tonic = 0; tonic < 12; ++tonic)
            for (const auto minor : { true, false })
            {
                const auto key = Key::fromPitchClass (tonic, minor);
                const auto scale = key.scale();
                auto score = 0.0;
                for (int pc = 0; pc < 12; ++pc)
                    score += std::find (scale.begin(), scale.end(), pc) != scale.end() ? weight[(size_t) pc] : -1.5 * weight[(size_t) pc];
                score += 1.5 * low[(size_t) tonic];
                // Minor or major: by the third heard (none heard: minor, a riff's usual).
                const auto minorThird = weight[(size_t) ((tonic + 3) % 12)], majorThird = weight[(size_t) ((tonic + 4) % 12)];
                score += minor ? (minorThird >= majorThird ? 0.01 : -0.01) : (majorThird > minorThird ? 0.01 : -0.01);
                if (score > best + 1.0e-9)
                {
                    best = score;
                    chosen = key;
                }
            }
        return chosen;
    }

    std::vector<RiffChord> riffChords (const ComposerRiff& riff, const Key& key, double beatsPerBar)
    {
        std::vector<RiffChord> out;
        if (beatsPerBar <= 0.0)
            return out;
        const auto even = std::abs (std::fmod (beatsPerBar, 2.0)) < 1.0e-6;
        const auto step = even ? beatsPerBar / 2.0 : beatsPerBar;
        const auto length = riff.bars * beatsPerBar;
        std::optional<Chord> last;
        for (double at = 0.0; at < length - 1.0e-6; at += step)
        {
            const auto to = at + step;
            std::array<double, 12> lowWeight {}, all {};
            auto lowest = 127;
            for (const auto& n : riff.notes)
                if (! n.keyswitch && n.beat < to && n.beat + n.length > at)
                    lowest = juce::jmin (lowest, n.pitch);
            for (const auto& n : riff.notes)
            {
                if (n.keyswitch)
                    continue;
                const auto overlap = juce::jmin (to, n.beat + n.length) - juce::jmax (at, n.beat);
                if (overlap <= 0.0)
                    continue;
                const auto w = overlap * (0.5 + n.velocity);
                all[(size_t) pcOf (n.pitch)] += w;
                if (n.pitch <= lowest + 7)
                    lowWeight[(size_t) pcOf (n.pitch)] += n.pitch == lowest ? 2.0 * w : w;   // (a power chord's root, not its fifth)
            }
            std::optional<Chord> chord;
            if (lowest < 127)
            {
                const auto root = (int) std::distance (lowWeight.begin(), std::max_element (lowWeight.begin(), lowWeight.end()));
                const auto minorThird = all[(size_t) ((root + 3) % 12)], majorThird = all[(size_t) ((root + 4) % 12)];
                Chord c;
                c.root = root;
                if (minorThird > 0.0 || majorThird > 0.0)
                    c.type = majorThird > minorThird ? Chord::Type::major : Chord::Type::minor;
                else if (const auto own = diatonicTriad (root, key))
                    c = *own;
                else
                    c.type = Chord::Type::major;   // (outside the key: kept as played, a power chord's root)
                chord = c;
            }
            else
                chord = last;   // (silence: the chord carries on)
            if (! chord.has_value())
                chord = diatonicTriad (key.pitchClass(), key).value_or (Chord {});
            if (! out.empty() && out.back().chord == *chord)
                out.back().length += step;
            else
                out.push_back ({ at, step, *chord });
            last = chord;
        }
        return out;
    }

    LineIdea tileRiff (const ComposerRiff& riff, double totalBeats, double beatsPerBar)
    {
        LineIdea idea;
        idea.part = LineIdea::Part::rhythm;
        idea.style = "My riff";
        idea.description = "Your own riff" + (riff.name.isNotEmpty() ? " (" + riff.name + ")" : juce::String());
        const auto length = riff.bars * beatsPerBar;
        if (length <= 0.0 || riff.notes.empty())
            return idea;
        for (double from = 0.0; from < totalBeats - 1.0e-6; from += length)
            for (auto n : riff.notes)
            {
                n.beat += from;
                if (n.beat >= totalBeats - 1.0e-9)
                    continue;
                n.length = juce::jmin (n.length, totalBeats - n.beat);
                idea.notes.push_back (n);
            }
        return idea;
    }

    std::vector<ChordSpan> riffSpans (const std::vector<RiffChord>& chords, const Key& key, double start, double totalBeats, double riffBeats)
    {
        std::vector<ChordSpan> out;
        if (chords.empty() || riffBeats <= 0.0)
            return out;
        for (double from = 0.0; from < totalBeats - 1.0e-6; from += riffBeats)
            for (const auto& c : chords)
            {
                const auto a = from + c.start;
                if (a >= totalBeats - 1.0e-9)
                    break;
                const auto b = juce::jmin (totalBeats, a + c.length);
                // The same chord going on across the repeat: one span.
                if (! out.empty() && out.back().chord == c.chord && std::abs (out.back().end - (start + a)) < 1.0e-9)
                    out.back().end = start + b;
                else
                    out.push_back ({ start + a, start + b, c.chord, key });
            }
        return out;
    }
}
