#include "Chords.h"

#include <array>
#include <cmath>
#include <map>
#include <set>

namespace pr7s
{
    namespace
    {
        int pc (int note) { return ((note % 12) + 12) % 12; }

        std::vector<int> typeIntervals (Chord::Type type)
        {
            using T = Chord::Type;
            switch (type)
            {
                case T::major:      return { 0, 4, 7 };
                case T::minor:      return { 0, 3, 7 };
                case T::diminished: return { 0, 3, 6 };
                case T::augmented:  return { 0, 4, 8 };
                case T::sus2:       return { 0, 2, 7 };
                case T::sus4:       return { 0, 5, 7 };
            }
            return { 0, 4, 7 };
        }

        std::vector<int> tensionIntervals (Chord::Tension tension)
        {
            using X = Chord::Tension;
            switch (tension)
            {
                case X::none:         return {};
                case X::six:          return { 9 };
                case X::seven:        return { 10 };
                case X::maj7:         return { 11 };
                case X::dim7:         return { 9 };
                case X::add9:         return { 14 };
                case X::nine:         return { 10, 14 };
                case X::maj9:         return { 11, 14 };
                case X::sixNine:      return { 9, 14 };
                case X::eleven:       return { 10, 14, 17 };
                case X::thirteen:     return { 10, 14, 21 };
                case X::sevenFlat9:   return { 10, 13 };
                case X::sevenSharp9:  return { 10, 15 };
                case X::sevenSharp11: return { 10, 18 };
            }
            return {};
        }

        // How the chord's kind is written after its root (and what of it the
        // roman numerals keep: "m" goes to the numeral's case).
        juce::String suffix (Chord::Type type, Chord::Tension tension)
        {
            using T = Chord::Type;
            using X = Chord::Tension;
            const auto t = Chord::tensionName (tension, type);
            const auto plain = tension == X::none ? juce::String() : t;
            switch (type)
            {
                case T::major:      return plain;
                case T::minor:
                    if (tension == X::maj7) return "m(maj7)";
                    if (tension == X::maj9) return "m(maj9)";
                    return "m" + plain;
                case T::diminished:
                    if (tension == X::seven) return "m7b5";
                    if (tension == X::dim7)  return "dim7";
                    return "dim" + plain;
                case T::augmented:
                    if (tension == X::maj7) return "aug(maj7)";
                    return "aug" + plain;
                case T::sus2:
                    if (tension == X::seven) return "7sus2";
                    return "sus2" + plain;
                case T::sus4:
                    if (tension == X::seven) return "7sus4";
                    if (tension == X::nine)  return "9sus4";
                    return "sus4" + plain;
            }
            return plain;
        }

        // Each note to the nearest of the others, both ways: how far the voices move.
        double movement (const std::vector<int>& a, const std::vector<int>& b)
        {
            if (a.empty() || b.empty())
                return 0.0;
            double total = 0.0;
            for (const auto x : a)
            {
                auto best = 1000;
                for (const auto y : b)
                    best = std::min (best, std::abs (x - y));
                total += best;
            }
            for (const auto y : b)
            {
                auto best = 1000;
                for (const auto x : a)
                    best = std::min (best, std::abs (x - y));
                total += best;
            }
            return total;
        }

        const char* const romanUpper[] = { "I", "bII", "II", "bIII", "III", "IV", "bV", "V", "bVI", "VI", "bVII", "VII" };
        const char* const nashville[] = { "1", "b2", "2", "b3", "3", "4", "b5", "5", "b6", "6", "b7", "7" };
    }

    //==========================================================================
    std::vector<int> Chord::intervals() const
    {
        auto all = typeIntervals (type);
        for (const auto i : tensionIntervals (tension))
            all.push_back (i);
        std::sort (all.begin(), all.end());
        all.erase (std::unique (all.begin(), all.end()), all.end());
        return all;
    }

    std::vector<int> Chord::pitchClasses() const
    {
        std::vector<int> classes;
        for (const auto i : intervals())
            if (std::find (classes.begin(), classes.end(), pc (root + i)) == classes.end())
                classes.push_back (pc (root + i));
        return classes;
    }

    Chord Chord::transposed (int semitones) const
    {
        auto c = *this;
        c.root = pc (root + semitones);
        if (bass >= 0)
            c.bass = pc (bass + semitones);
        return c;
    }

    juce::String Chord::noteName (int pitchClass, bool flats)
    {
        static const char* const sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const char* const flatNames[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        return (flats ? flatNames : sharpNames)[pc (pitchClass)];
    }

    juce::String Chord::typeName (Type type)
    {
        switch (type)
        {
            case Type::major:      return "Maj";
            case Type::minor:      return "Min";
            case Type::diminished: return "Dim";
            case Type::augmented:  return "Aug";
            case Type::sus2:       return "Sus2";
            case Type::sus4:       return "Sus4";
        }
        return {};
    }

    juce::String Chord::tensionName (Tension tension, Type)
    {
        switch (tension)
        {
            case Tension::none:         return "-";
            case Tension::six:          return "6";
            case Tension::seven:        return "7";
            case Tension::maj7:         return "maj7";
            case Tension::dim7:         return "dim7";
            case Tension::add9:         return "add9";
            case Tension::nine:         return "9";
            case Tension::maj9:         return "maj9";
            case Tension::sixNine:      return "6/9";
            case Tension::eleven:       return "11";
            case Tension::thirteen:     return "13";
            case Tension::sevenFlat9:   return "7b9";
            case Tension::sevenSharp9:  return "7#9";
            case Tension::sevenSharp11: return "7#11";
        }
        return {};
    }

    const std::vector<Chord::Tension>& Chord::allTensions()
    {
        static const std::vector<Tension> all { Tension::none, Tension::six, Tension::seven, Tension::maj7, Tension::dim7, Tension::add9,
                                                Tension::nine, Tension::maj9, Tension::sixNine, Tension::eleven, Tension::thirteen,
                                                Tension::sevenFlat9, Tension::sevenSharp9, Tension::sevenSharp11 };
        return all;
    }

    const std::vector<Chord::Type>& Chord::allTypes()
    {
        static const std::vector<Type> all { Type::major, Type::minor, Type::diminished, Type::augmented, Type::sus2, Type::sus4 };
        return all;
    }

    juce::String Chord::name (bool flats) const
    {
        auto text = noteName (root, flats) + suffix (type, tension);
        if (bass >= 0 && bass != root)
            text << "/" << noteName (bass, flats);
        return text;
    }

    juce::String Chord::romanName (int rootKey) const
    {
        const auto degree = pc (root - rootKey);
        auto numeral = juce::String (romanUpper[degree]);
        auto rest = suffix (type, tension);
        const auto lower = type == Type::minor || type == Type::diminished;
        if (lower)
            numeral = numeral.toLowerCase().replace ("b", "b");   // (the flat stays a flat)
        if (type == Type::minor && rest.startsWith ("m"))
            rest = rest.substring (1);
        if (type == Type::diminished)
            rest = rest == "m7b5" ? juce::String (juce::CharPointer_UTF8 ("\xc3\xb8" "7"))
                 : rest.replace ("dim", juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")));
        if (type == Type::augmented)
            rest = rest.replace ("aug", "+");
        auto text = numeral + rest;
        if (bass >= 0 && bass != root)
            text << "/" << romanUpper[pc (bass - rootKey)];
        return text;
    }

    juce::String Chord::nashvilleName (int rootKey) const
    {
        auto text = juce::String (nashville[pc (root - rootKey)]) + suffix (type, tension);
        if (bass >= 0 && bass != root)
            text << "/" << nashville[pc (bass - rootKey)];
        return text;
    }

    Chord Chord::withTensionStep (int direction) const
    {
        // Each type's ladder (our design: the manual names the step, not the chords).
        using X = Tension;
        const auto dominant = tension == X::seven || tension == X::nine || tension == X::thirteen || tension == X::eleven
                           || tension == X::sevenFlat9 || tension == X::sevenSharp9 || tension == X::sevenSharp11;
        std::vector<X> ladder;
        switch (type)
        {
            case Type::major:      ladder = dominant ? std::vector<X> { X::none, X::seven, X::nine, X::thirteen }
                                                     : std::vector<X> { X::none, X::add9, X::maj7, X::maj9 }; break;
            case Type::minor:      ladder = { X::none, X::seven, X::nine, X::eleven }; break;
            case Type::diminished: ladder = { X::none, X::seven, X::dim7 }; break;
            case Type::augmented:  ladder = { X::none, X::seven }; break;
            case Type::sus2:       ladder = { X::none, X::seven }; break;
            case Type::sus4:       ladder = { X::none, X::seven, X::nine }; break;
        }
        auto at = (int) (std::find (ladder.begin(), ladder.end(), tension) - ladder.begin());
        if (at >= (int) ladder.size())
            at = 0;
        auto c = *this;
        c.tension = ladder[(size_t) juce::jlimit (0, (int) ladder.size() - 1, at + (direction > 0 ? 1 : -1))];
        return c;
    }

    juce::String Chord::toText() const
    {
        return juce::String (root) + " " + juce::String ((int) type) + " " + juce::String ((int) tension) + " " + juce::String (bass);
    }

    std::optional<Chord> Chord::fromText (const juce::String& text)
    {
        const auto parts = juce::StringArray::fromTokens (text, " ", {});
        if (parts.size() != 4)
            return std::nullopt;
        Chord c;
        c.root = parts[0].getIntValue();
        const auto type = parts[1].getIntValue(), tension = parts[2].getIntValue();
        c.bass = parts[3].getIntValue();
        if (c.root < 0 || c.root > 11 || type < 0 || type >= (int) allTypes().size() || tension < 0 || tension >= (int) allTensions().size()
            || c.bass < -1 || c.bass > 11)
            return std::nullopt;
        c.type = (Type) type;
        c.tension = (Tension) tension;
        return c;
    }

    //==========================================================================
    std::optional<Chord> recogniseChord (const std::vector<int>& midiNotes)
    {
        if (midiNotes.size() < 2)
            return std::nullopt;
        std::set<int> played;
        for (const auto n : midiNotes)
            played.insert (pc (n));
        const auto bassClass = pc (*std::min_element (midiNotes.begin(), midiNotes.end()));
        std::optional<Chord> best;
        auto bestScore = 1000;
        for (const auto root : played)
            for (const auto type : Chord::allTypes())
                for (const auto tension : Chord::allTensions())
                {
                    Chord c { root, type, tension, -1 };
                    const auto classes = c.pitchClasses();
                    if (std::set<int> (classes.begin(), classes.end()) != played)
                        continue;
                    // The bass as the root first, then the simplest: few tensions, plain types.
                    const auto score = (root == bassClass ? 0 : 100) + (int) tension * 2 + (int) type;
                    if (score < bestScore)
                    {
                        bestScore = score;
                        best = c;
                        best->bass = root == bassClass ? -1 : bassClass;
                    }
                }
        return best;
    }

    std::vector<Voicing> namedVoicings (const Chord& chord, int low)
    {
        const auto classes = chord.pitchClasses();
        const auto n = (int) classes.size();
        static const char* const inversionNames[] = { "Root position", "1st inversion", "2nd inversion", "3rd inversion" };
        const auto withBass = [&chord] (std::vector<int> v)
        {
            if (chord.bass >= 0 && chord.bass != chord.root && ! v.empty())
            {
                auto bassNote = v.front() - pc (v.front() - chord.bass);
                if (bassNote == v.front())
                    bassNote -= 12;
                v.insert (v.begin(), bassNote);
            }
            return v;
        };
        // Close position: each inversion (which of its notes is lowest), in two octaves.
        std::vector<std::pair<std::vector<int>, int>> close;   // notes, inversion
        for (int k = 0; k < n; ++k)
            for (int octave = 0; octave < 2; ++octave)
            {
                std::vector<int> notes;
                notes.push_back (low + pc (classes[(size_t) k] - low) + octave * 12);
                for (int i = 1; i < n; ++i)
                {
                    const auto want = classes[(size_t) ((k + i) % n)];
                    auto note = notes.back() + 1;
                    while (pc (note) != want)
                        ++note;
                    notes.push_back (note);
                }
                if (notes.front() >= low && notes.front() < low + 18)
                    close.push_back ({ notes, k });
            }
        std::sort (close.begin(), close.end(), [] (const auto& a, const auto& b) { return a.first.front() < b.first.front(); });
        std::vector<Voicing> all;
        const auto add = [&all] (std::vector<int> notes, const juce::String& kind, bool isClose)
        {
            for (const auto& v : all)
                if (v.notes == notes)
                    return;
            all.push_back ({ std::move (notes), kind, isClose });
        };
        for (const auto& [notes, k] : close)
            add (withBass (notes), k < 4 ? juce::String (inversionNames[k]) : "Inversion " + juce::String (k), true);
        // Open (drop 2): the second voice from the top an octave lower, in each close voicing.
        if (n >= 3)
            for (const auto& [notes, k] : close)
            {
                auto open = notes;
                open[(size_t) (n - 2)] -= 12;
                std::sort (open.begin(), open.end());
                if (open.front() >= low - 12)
                    add (withBass (open), "Open (drop 2)", false);
            }
        // Spread: the root low, then the fifth (or seventh), the third an octave up (a tenth).
        const auto iv = chord.intervals();
        if (n == 3 || (n == 4 && iv[3] < 12))
        {
            const auto r = (low - 12) + pc (chord.root - (low - 12));
            std::vector<int> spread;
            if (n == 3)
                spread = { r, r + iv[2], r + 12 + iv[1] };
            else
                spread = { r, r + iv[3], r + 12 + iv[1], r + 12 + iv[2] };
            std::sort (spread.begin(), spread.end());
            add (withBass (spread), "Spread (root, then a tenth)", false);
        }
        return all;
    }

    std::vector<std::vector<int>> voicingsFor (const Chord& chord, int low)
    {
        std::vector<std::vector<int>> notes;
        for (auto& v : namedVoicings (chord, low))
            notes.push_back (std::move (v.notes));
        return notes;
    }

    int closestVoicing (const std::vector<std::vector<int>>& voicings, const std::vector<int>& previous, const std::vector<int>& reference)
    {
        if (voicings.empty())
            return -1;
        if (previous.empty() && reference.empty())
            return (int) voicings.size() / 2;
        auto best = 0;
        auto bestCost = 1.0e9;
        for (size_t i = 0; i < voicings.size(); ++i)
        {
            const auto cost = movement (voicings[i], previous) + 0.5 * movement (voicings[i], reference);
            if (cost < bestCost - 1.0e-9)
            {
                bestCost = cost;
                best = (int) i;
            }
        }
        return best;
    }

    //==========================================================================
    // Keys and spelling (0.15.0).
    namespace
    {
        const int naturals[] = { 0, 2, 4, 5, 7, 9, 11 };
        const int majorSteps[] = { 0, 2, 4, 5, 7, 9, 11 };
        const int minorSteps[] = { 0, 2, 3, 5, 7, 8, 10 };
        int wrapAccidental (int semitones) { auto d = ((semitones % 12) + 12) % 12; return d > 6 ? d - 12 : d; }
        // The major key with `fifths` sharps (+) or flats (-): its tonic's letter and accidental.
        Key majorAt (int fifths)
        {
            static const int lettersByFifth[] = { 3, 0, 4, 1, 5, 2, 6 };   // F C G D A E B
            const auto i = fifths + 1;
            const auto letter = lettersByFifth[((i % 7) + 7) % 7];
            const auto accidental = (int) std::floor ((double) i / 7.0);
            return { letter, accidental, false };
        }
        const char* const numerals[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    }

    int Key::naturalOf (int letter) noexcept { return naturals[((letter % 7) + 7) % 7]; }
    int Key::pitchClass() const noexcept { return pc (naturalOf (letter) + accidental); }

    juce::String Key::Spelled::text() const
    {
        juce::String t = juce::String::charToString ((juce::juce_wchar) "CDEFGAB"[((letter % 7) + 7) % 7]);
        for (int i = 0; i < accidental; ++i)  t << "#";
        for (int i = 0; i > accidental; --i)  t << "b";
        return t;
    }

    juce::String Key::tonicName() const { return Spelled { letter, accidental }.text(); }
    juce::String Key::name() const { return tonicName() + (minor ? " minor" : " major"); }

    int Key::signature() const noexcept
    {
        // A minor key's signature is its relative major's (a minor third up, two letters up).
        auto majorLetter = letter, majorAccidental = accidental;
        if (minor)
        {
            majorLetter = (letter + 2) % 7;
            majorAccidental = wrapAccidental (pitchClass() + 3 - naturalOf (majorLetter));
        }
        static const int fifthOf[] = { 0, 2, 4, -1, 1, 3, 5 };   // C D E F G A B
        return fifthOf[majorLetter] + 7 * majorAccidental;
    }

    std::vector<int> Key::scale() const
    {
        std::vector<int> s;
        for (int d = 0; d < 7; ++d)
            s.push_back (pc (pitchClass() + (minor ? minorSteps : majorSteps)[d]));
        return s;
    }

    const std::vector<Key>& Key::all()
    {
        static const std::vector<Key> keys = []
        {
            std::vector<Key> k;
            for (const auto minor : { false, true })
                for (const auto fifths : { 0, 1, 2, 3, 4, 5, 6, 7, -1, -2, -3, -4, -5, -6, -7 })
                {
                    auto major = majorAt (fifths);
                    if (! minor)
                    {
                        k.push_back (major);
                        continue;
                    }
                    // Its relative minor: a minor third down, two letters down.
                    Key m { (major.letter + 5) % 7, 0, true };
                    m.accidental = wrapAccidental (major.pitchClass() - 3 - naturalOf (m.letter));
                    k.push_back (m);
                }
            return k;
        }();
        return keys;
    }

    Key Key::fromPitchClass (int pitchClass, bool minor, bool flats)
    {
        std::optional<Key> best;
        for (const auto& k : all())
            if (k.minor == minor && k.pitchClass() == pc (pitchClass))
            {
                const auto better = ! best || std::abs (k.signature()) < std::abs (best->signature())
                                 || (std::abs (k.signature()) == std::abs (best->signature()) && (k.signature() < 0) == flats);
                if (better)
                    best = k;
            }
        return best.value_or (Key {});
    }

    Key::Spelled Key::spellAs (int pitchClass, int onLetter)
    {
        onLetter = ((onLetter % 7) + 7) % 7;
        return { onLetter, wrapAccidental (pitchClass - naturalOf (onLetter)) };
    }

    Key::Spelled Key::spell (int pitchClass) const
    {
        pitchClass = pc (pitchClass);
        const auto s = scale();
        for (int d = 0; d < 7; ++d)
            if (s[(size_t) d] == pitchClass)
                return spellAs (pitchClass, letter + d);
        const auto tonic = this->pitchClass();
        // The raised sixth and seventh of a minor key, and the raised fourth: sharpened degrees.
        if (pitchClass == pc (tonic + 6))
            return spellAs (pitchClass, letter + 3);
        if (minor && (pitchClass == pc (tonic + 11) || pitchClass == pc (tonic + 9)))
            return spellAs (pitchClass, letter + (pitchClass == pc (tonic + 11) ? 6 : 5));
        // Else as the key leans: flats in flat keys (and C major / A minor), sharps in sharp keys.
        const auto above = [&] { for (int d = 0; d < 7; ++d) if (s[(size_t) d] == pc (pitchClass + 1)) return spellAs (pitchClass, letter + d); return Spelled { -1, 0 }; };
        const auto below = [&] { for (int d = 0; d < 7; ++d) if (s[(size_t) d] == pc (pitchClass - 1)) return spellAs (pitchClass, letter + d); return Spelled { -1, 0 }; };
        auto first = signature() > 0 ? below() : above(), second = signature() > 0 ? above() : below();
        // (Outside the key, Fb, Cb, E# and B# are written as the plain note: E, B, F, C.)
        const auto plain = [] (Spelled s)
        {
            const auto naturalPc = pc (naturalOf (s.letter) + s.accidental);
            for (int l = 0; l < 7; ++l)
                if (std::abs (s.accidental) == 1 && naturalOf (l) == naturalPc)
                    return Spelled { l, 0 };
            return s;
        };
        if (first.letter >= 0 && std::abs (first.accidental) <= 1)
            return plain (first);
        if (second.letter >= 0 && std::abs (second.accidental) <= 1)
            return plain (second);
        return first.letter >= 0 ? first : spellAs (pitchClass, letter);
    }

    namespace
    {
        // Which letter above the root a chord tone is written on, from its interval.
        int letterStep (int interval, const Chord& c)
        {
            if (interval == 9 && c.tension == Chord::Tension::dim7 && c.type == Chord::Type::diminished)
                return 6;   // (the diminished seventh: Bbb, not Ab)
            switch (interval)
            {
                case 0: return 0;
                case 1: case 2: case 13: case 14: case 15: return 1;   // (b9, 9, #9; sus2)
                case 3: case 4: return 2;
                case 5: case 17: case 18: return 3;                    // (sus4, 11, #11)
                case 6: case 7: case 8: return 4;                      // (b5, 5, #5)
                case 9: case 21: return 5;                             // (6, 13)
                case 10: case 11: return 6;
                default: return 0;
            }
        }

        int degreeOf (const Chord& chord, const Key& key, int& accidental)
        {
            const auto root = key.spell (chord.root);
            const auto degree = ((root.letter - key.letter) % 7 + 7) % 7;
            accidental = wrapAccidental (chord.root - key.scale()[(size_t) degree]);
            return degree;
        }
    }

    juce::StringArray Chord::noteNames (const Key& key) const
    {
        const auto rootLetter = key.spell (root).letter;
        juce::StringArray names;
        for (const auto i : intervals())
            names.add (Key::spellAs (root + i, rootLetter + letterStep (i, *this)).text());
        return names;
    }

    juce::String notesText (const std::vector<int>& notes, const Chord& chord, const Key& key)
    {
        // Each note as the chord spells it (its letters from the root's), with its octave
        // counted from its letter (middle C = C3; Cb3 sounds as B2, B#2 as C3).
        const auto rootLetter = key.spell (chord.root).letter;
        juce::StringArray texts;
        for (const auto n : notes)
        {
            auto sp = key.spell (n);
            for (const auto i : chord.intervals())
                if (pc (chord.root + i) == pc (n))
                    sp = Key::spellAs (n, rootLetter + letterStep (i, chord));
            const auto octave = (int) std::floor ((double) (n - sp.accidental) / 12.0) - 2;
            texts.add (sp.text() + juce::String (octave));
        }
        return texts.joinIntoString (" ");
    }

    juce::String Chord::name (const Key& key) const
    {
        auto text = key.spell (root).text() + suffix (type, tension);
        if (bass >= 0 && bass != root)
        {
            // The bass as the chord spells it if it's one of its notes (Ebm/Gb), else as the key does.
            juce::String b = key.spell (bass).text();
            const auto rootLetter = key.spell (root).letter;
            for (const auto i : intervals())
                if (pc (root + i) == bass)
                    b = Key::spellAs (bass, rootLetter + letterStep (i, *this)).text();
            text << "/" << b;
        }
        return text;
    }

    juce::String Chord::romanName (const Key& key) const
    {
        int accidental = 0;
        const auto degree = degreeOf (*this, key, accidental);
        // A minor key's raised sixth and seventh (its major V's and vii°'s notes) are its own: no sign.
        if (key.minor && accidental == 1 && (degree == 5 || degree == 6))
            accidental = 0;
        juce::String numeral;
        for (int i = 0; i < accidental; ++i)  numeral << "#";
        for (int i = 0; i > accidental; --i)  numeral << "b";
        const auto lower = type == Type::minor || type == Type::diminished;
        numeral << (lower ? juce::String (numerals[degree]).toLowerCase() : juce::String (numerals[degree]));
        auto rest = suffix (type, tension);
        if (type == Type::minor && rest.startsWith ("m"))
            rest = rest.substring (1);
        if (type == Type::diminished)
            rest = rest == "m7b5" ? juce::String (juce::CharPointer_UTF8 ("\xc3\xb8" "7"))
                 : rest.replace ("dim", juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")));
        if (type == Type::augmented)
            rest = rest.replace ("aug", "+");
        auto text = numeral + rest;
        if (bass >= 0 && bass != root)
        {
            Chord b { bass, Type::major, Tension::none, -1 };
            int bassAccidental = 0;
            const auto bassDegree = degreeOf (b, key, bassAccidental);
            text << "/";
            for (int i = 0; i < bassAccidental; ++i)  text << "#";
            for (int i = 0; i > bassAccidental; --i)  text << "b";
            text << numerals[bassDegree];
        }
        return text;
    }

    juce::String Chord::nashvilleName (const Key& key) const
    {
        // Numbers from the tonic's major scale (a minor key's third is b3, as Nashville charts write it).
        const auto number = [&key] (int pitchClass)
        {
            const auto sp = key.spell (pitchClass);
            const auto degree = ((sp.letter - key.letter) % 7 + 7) % 7;
            const auto diff = wrapAccidental (pitchClass - (key.pitchClass() + majorSteps[degree]));
            juce::String t;
            for (int i = 0; i < diff; ++i)  t << "#";
            for (int i = 0; i > diff; --i)  t << "b";
            return t + juce::String (degree + 1);
        };
        auto text = number (root) + suffix (type, tension);
        if (bass >= 0 && bass != root)
            text << "/" << number (bass);
        return text;
    }

    bool Chord::isInKey (const Key& key) const
    {
        // All its notes in the key's scale; in a minor key, or in its harmonic minor (the raised seventh).
        auto scale = key.scale();
        const auto inScale = [this] (const std::vector<int>& s)
        {
            for (const auto p : pitchClasses())
                if (std::find (s.begin(), s.end(), p) == s.end())
                    return false;
            return bass < 0 || std::find (s.begin(), s.end(), bass) != s.end();
        };
        if (inScale (scale))
            return true;
        if (key.minor)
        {
            scale[6] = pc (key.pitchClass() + 11);
            return inScale (scale);
        }
        return false;
    }

    namespace
    {
        Chord chordFromNotes (int root, int third, int fifth, int seventh = -1)
        {
            Chord c;
            c.root = pc (root);
            const auto t = pc (third - root), f = pc (fifth - root);
            c.type = t == 4 ? (f == 8 ? Chord::Type::augmented : Chord::Type::major) : (f == 6 ? Chord::Type::diminished : Chord::Type::minor);
            if (seventh >= 0)
            {
                const auto sv = pc (seventh - root);
                if (c.type == Chord::Type::diminished)
                    c.tension = sv == 9 ? Chord::Tension::dim7 : Chord::Tension::seven;
                else
                    c.tension = sv == 11 ? Chord::Tension::maj7 : Chord::Tension::seven;
            }
            return c;
        }

        Chord degreeChord (const std::vector<int>& s, int degree, bool seventh)
        {
            const auto at = [&s, degree] (int up) { return s[(size_t) ((degree + up) % 7)]; };
            return chordFromNotes (at (0), at (2), at (4), seventh ? at (6) : -1);
        }
    }

    std::vector<Chord> keyChords (const Key& key)
    {
        const auto s = key.scale();
        std::vector<Chord> chords;
        for (int d = 0; d < 7; ++d)
            chords.push_back (degreeChord (s, d, false));
        if (key.minor)
        {
            // Harmonic minor: the raised seventh gives a major V and a vii°.
            auto h = s;
            h[6] = pc (key.pitchClass() + 11);
            chords.push_back (degreeChord (h, 4, false));                       // V
            chords.push_back (degreeChord (h, 6, false));                       // vii°
        }
        for (int d = 0; d < 7; ++d)
            chords.push_back (degreeChord (s, d, true));
        if (key.minor)
        {
            auto h = s;
            h[6] = pc (key.pitchClass() + 11);
            chords.push_back (degreeChord (h, 4, true));                        // V7
        }
        return chords;
    }

    std::vector<Progression> popularProgressions (const Key& key)
    {
        using T = Chord::Type;
        using X = Chord::Tension;
        struct Step { int offset; T type; X tension = X::none; };
        const auto tonic = key.pitchClass();
        const auto make = [tonic] (const char* name, const char* style, std::initializer_list<Step> steps)
        {
            Progression p { name, style, {} };
            for (const auto& s : steps)
                p.chords.push_back ({ pc (tonic + s.offset), s.type, s.tension, -1 });
            return p;
        };
        constexpr auto M = T::major, m = T::minor, d = T::diminished;
        if (! key.minor)
            return { make ("The pop anthem", "countless hits",          { { 0, M }, { 7, M }, { 9, m }, { 5, M } }),
                     make ("Fifties",        "doo-wop, ballads",        { { 0, M }, { 9, m }, { 5, M }, { 7, M } }),
                     make ("Three chords",   "rock, blues, country",    { { 0, M }, { 5, M }, { 7, M }, { 0, M } }),
                     make ("Sad pop",        "the anthem from vi",      { { 9, m }, { 5, M }, { 0, M }, { 7, M } }),
                     make ("Jazz cadence",   "jazz, R&B, gospel",       { { 2, m }, { 7, M, X::seven }, { 0, M } }),
                     make ("Turnaround",     "back to the top",         { { 0, M }, { 9, m }, { 2, m }, { 7, M } }),
                     make ("The Canon",      "Pachelbel, weddings",     { { 0, M }, { 7, M }, { 9, m }, { 4, m }, { 5, M }, { 0, M }, { 5, M }, { 7, M } }),
                     make ("Mixolydian rock","classic rock",            { { 0, M }, { 10, M }, { 5, M }, { 0, M } }),
                     make ("Ballad",         "worship, piano ballads",  { { 0, M }, { 5, M }, { 9, m }, { 7, M } }),
                     make ("Minor plagal",   "bittersweet ending",      { { 0, M }, { 5, M }, { 5, m }, { 0, M } }) };
        return { make ("Epic minor",       "film, pop, EDM",            { { 0, m }, { 8, M }, { 3, M }, { 10, M } }),
                 make ("Minor rock",       "rock, metal",               { { 0, m }, { 10, M }, { 8, M }, { 10, M } }),
                 make ("Natural cadence",  "folk, modal",               { { 0, m }, { 5, m }, { 7, m }, { 0, m } }),
                 make ("Minor cadence",    "classical, R&B",            { { 0, m }, { 5, m }, { 7, M }, { 0, m } }),
                 make ("Andalusian",       "flamenco, surf, rock",      { { 0, m }, { 10, M }, { 8, M }, { 7, M } }),
                 make ("Minor ballad",     "pop, worship",              { { 0, m }, { 8, M }, { 10, M }, { 0, m } }),
                 make ("Minor jazz cadence","jazz, neo-soul",           { { 2, d }, { 7, M, X::seven }, { 0, m } }),
                 make ("Rising minor",     "pop, rock",                 { { 0, m }, { 3, M }, { 10, M }, { 8, M } }),
                 make ("Dramatic",         "film, ballads",             { { 0, m }, { 8, M }, { 5, m }, { 7, M } }),
                 make ("Dorian vamp",      "funk, soul, jams",          { { 0, m }, { 5, M }, { 0, m }, { 5, M } }) };
    }

    std::vector<Suggestion> suggestNext (const Chord& from, const Key& key)
    {
        // How well each scale degree leads to each other one (rows: from; columns: to;
        // I ii iii IV V vi vii in major, i ii° III iv V VI VII in minor). Our weighting of
        // the common-practice and pop tendencies: V-I strongest, then the pre-dominants
        // (IV, ii) to V, vi-IV, the deceptive V-vi; I goes almost anywhere.
        static const int majorTable[7][7] = {
            {  0, 70, 50, 88, 85, 80, 45 },
            { 45,  0, 35, 55, 95, 45, 70 },
            { 45, 60,  0, 80, 50, 90, 30 },
            { 85, 68, 40,  0, 93, 55, 65 },
            { 98, 35, 45, 50,  0, 85, 40 },
            { 50, 88, 55, 86, 75,  0, 40 },
            { 95, 30, 60, 35, 50, 50,  0 } };
        static const int minorTable[7][7] = {
            {  0, 65, 70, 88, 90, 80, 75 },
            { 40,  0, 35, 55, 95, 40, 45 },
            { 50, 45,  0, 75, 55, 85, 70 },
            { 80, 60, 45,  0, 93, 60, 70 },
            { 98, 30, 45, 50,  0, 85, 40 },
            { 55, 75, 60, 80, 75,  0, 80 },
            { 70, 30, 90, 50, 50, 55,  0 } };
        static const char* const majorFunctions[] = { "tonic", "pre-dominant", "tonic", "pre-dominant", "dominant", "tonic", "dominant" };

        // The candidates: the key's chords, its V7, and some favourite colours from outside it.
        std::vector<Chord> candidates;
        const auto addCandidate = [&candidates] (Chord c)
        {
            for (const auto& o : candidates)
                if (o.root == c.root && o.type == c.type && o.tension == c.tension)
                    return;
            candidates.push_back (c);
        };
        const auto chords = keyChords (key);
        for (int d = 0; d < 7; ++d)
            addCandidate (chords[(size_t) d]);
        const auto tonic = key.pitchClass();
        using T = Chord::Type;
        using X = Chord::Tension;
        if (key.minor)
        {
            addCandidate (chords[7]);                                          // V (major)
            addCandidate ({ pc (tonic + 7), T::major, X::seven, -1 });         // V7
            addCandidate (chords[8]);                                          // vii°
            addCandidate ({ pc (tonic + 5), T::major, X::none, -1 });          // IV (borrowed, Dorian)
            addCandidate ({ pc (tonic + 1), T::major, X::none, -1 });          // bII (Neapolitan)
        }
        else
        {
            addCandidate ({ pc (tonic + 7), T::major, X::seven, -1 });         // V7
            addCandidate ({ pc (tonic + 5), T::minor, X::none, -1 });          // iv (from the minor)
            addCandidate ({ pc (tonic + 10), T::major, X::none, -1 });         // bVII
            addCandidate ({ pc (tonic + 8), T::major, X::none, -1 });          // bVI
            addCandidate ({ pc (tonic + 3), T::major, X::none, -1 });          // bIII
            addCandidate ({ pc (tonic + 2), T::major, X::none, -1 });          // II (V of V)
            addCandidate ({ pc (tonic + 4), T::major, X::none, -1 });          // III (V of vi)
            addCandidate ({ pc (tonic + 9), T::major, X::none, -1 });          // VI (V of ii)
        }

        const auto diatonicDegree = [&key] (const Chord& c) -> int
        {
            if (! c.isInKey (key))
                return -1;
            int accidental = 0;
            return degreeOf (c, key, accidental);
        };
        const auto fromDegree = diatonicDegree (from);
        const auto fromClasses = from.pitchClasses();
        std::vector<Suggestion> result;
        for (const auto& c : candidates)
        {
            if (c.root == from.root && c.type == from.type)
                continue;
            juce::StringArray why;
            const auto toDegree = diatonicDegree (c);
            const auto motion = pc (c.root - from.root);
            int score;
            if (fromDegree >= 0 && toDegree >= 0)
            {
                score = (key.minor ? minorTable : majorTable)[fromDegree][toDegree];
                if (key.minor && toDegree == 4 && c.type == T::minor)
                {
                    score -= 22;
                    why.add ("the natural minor's minor five: a softer pull than the major five");
                }
                else if (toDegree == 0 && (fromDegree == 4 || fromDegree == 6))
                    why.add ("dominant to tonic: the strongest pull home");
                else if (toDegree == 0 && fromDegree == 3)
                    why.add ("plagal: subdominant to tonic (\"amen\")");
                else if (fromDegree == 4 && toDegree == 5)
                    why.add ("deceptive: the five goes to the minor six instead of home");
                else if (toDegree == 4)
                    why.add (juce::String (majorFunctions[fromDegree]) + " to dominant: it wants to go home next");
                else
                    why.add (juce::String (majorFunctions[fromDegree]) + " to " + majorFunctions[toDegree]);
            }
            else
            {
                // Outside the key (or from outside it): by how the root moves.
                score = motion == 5 ? 72 : motion == 7 ? 58 : (motion == 2 || motion == 10) ? 52
                      : (motion == 1 || motion == 11) ? 46 : motion == 6 ? 28 : 50;
                if (toDegree < 0)
                {
                    score -= 14;
                    const auto rootDegreeIsTonic = pc (c.root - tonic);
                    const auto secondaryTarget = pc (c.root + 5);   // a major chord pulls a fifth down
                    const auto scale = key.scale();   // (one list: begin and end of the same one; 0.16.0 read past it and crashed)
                    const auto targetInKey = std::find (scale.begin(), scale.end(), secondaryTarget) != scale.end();
                    if (c.type == T::major && targetInKey && secondaryTarget != tonic && ! key.minor
                        && (rootDegreeIsTonic == 2 || rootDegreeIsTonic == 4 || rootDegreeIsTonic == 9))
                    {
                        // Named as the key's own chord there (in C, E pulls to vi: Am, not "VI").
                        auto target = Chord { secondaryTarget, T::major, X::none, -1 };
                        for (int d = 0; d < 7; ++d)
                            if (chords[(size_t) d].root == secondaryTarget)
                                target = chords[(size_t) d];
                        why.add ("secondary dominant: pulls to " + target.romanName (key));
                    }
                    else
                        why.add (key.minor ? "borrowed colour (outside the minor key)" : "borrowed from the parallel minor");
                }
                else
                    why.add ("back into the key");
            }
            if (motion == 5)
            {
                score += 5;
                why.add ("down a fifth round the circle");
            }
            auto common = 0;
            for (const auto p : c.pitchClasses())
                common += std::find (fromClasses.begin(), fromClasses.end(), p) != fromClasses.end() ? 1 : 0;
            if (common > 0)
            {
                score += juce::jmin (9, common * 3);
                why.add (juce::String (common) + (common == 1 ? " note in common" : " notes in common"));
            }
            result.push_back ({ c, juce::jlimit (1, 99, score), why.joinIntoString ("; "), toDegree >= 0 });
        }
        std::stable_sort (result.begin(), result.end(), [] (const Suggestion& a, const Suggestion& b) { return a.score > b.score; });
        return result;
    }

    //==========================================================================
    //==========================================================================
    // The chord track (0.16.0).
    int ChordTrack::indexAt (double beat) const noexcept
    {
        auto found = -1;
        for (int i = 0; i < (int) events.size(); ++i)
            if (events[(size_t) i].beat <= beat + 1.0e-9)
                found = i;
        return found;
    }

    double ChordTrack::endOf (int index, double lastLength) const
    {
        if (! juce::isPositiveAndBelow (index, (int) events.size()))
            return 0.0;
        return index + 1 < (int) events.size() ? events[(size_t) index + 1].beat : events[(size_t) index].beat + lastLength;
    }

    Key ChordTrack::keyAt (double beat, const Key& fallback) const
    {
        auto key = fallback;
        for (const auto& s : scales)
            if (s.beat <= beat + 1.0e-9)
                key = s.key;
        return key;
    }

    void ChordTrack::sort()
    {
        std::stable_sort (events.begin(), events.end(), [] (const ChordEvent& a, const ChordEvent& b) { return a.beat < b.beat; });
        std::stable_sort (scales.begin(), scales.end(), [] (const ScaleEvent& a, const ScaleEvent& b) { return a.beat < b.beat; });
    }

    std::unique_ptr<juce::XmlElement> ChordTrack::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("ChordTrack");
        if (record)
            xml->setAttribute ("record", true);
        xml->setAttribute ("shown", shown);
        xml->setAttribute ("autoScales", autoScales);
        if (shiftTicks != 0)
            xml->setAttribute ("shiftTicks", shiftTicks);
        if (! (voicing == VoicingSettings {}))
            xml->addChildElement (voicing.toXml().release());
        for (const auto& e : events)
        {
            auto* x = xml->createNewChildElement ("Chord");
            x->setAttribute ("beat", e.beat);
            x->setAttribute ("chord", e.chord.toText());
        }
        for (const auto& sc : scales)
        {
            auto* x = xml->createNewChildElement ("Scale");
            x->setAttribute ("beat", sc.beat);
            x->setAttribute ("letter", sc.key.letter);
            x->setAttribute ("accidental", sc.key.accidental);
            x->setAttribute ("minor", sc.key.minor);
        }
        return xml;
    }

    ChordTrack ChordTrack::fromXml (const juce::XmlElement& xml)
    {
        ChordTrack t;
        t.shown = xml.getBoolAttribute ("shown", false);
        t.record = xml.getBoolAttribute ("record", false);
        t.autoScales = xml.getBoolAttribute ("autoScales", true);
        t.shiftTicks = juce::jlimit (-480, 480, xml.getIntAttribute ("shiftTicks", 0));
        if (const auto* v = xml.getChildByName ("Voicing"))
            t.voicing = VoicingSettings::fromXml (*v);
        for (const auto* x : xml.getChildWithTagNameIterator ("Chord"))
            if (const auto chord = Chord::fromText (x->getStringAttribute ("chord")))
                t.events.push_back ({ juce::jmax (0.0, x->getDoubleAttribute ("beat")), *chord });
        for (const auto* x : xml.getChildWithTagNameIterator ("Scale"))
            t.scales.push_back ({ juce::jmax (0.0, x->getDoubleAttribute ("beat")),
                                  Key { juce::jlimit (0, 6, x->getIntAttribute ("letter")), juce::jlimit (-1, 1, x->getIntAttribute ("accidental")),
                                        x->getBoolAttribute ("minor") } });
        t.sort();
        return t;
    }

    std::vector<ScaleEvent> scalesFromChords (const std::vector<ChordEvent>& events, const Key& start)
    {
        // The 24 usual keys (each pitch class's usual spelling, major and minor).
        std::vector<Key> keys;
        for (int p = 0; p < 12; ++p)
            for (const auto minor : { false, true })
                keys.push_back (Key::fromPitchClass (p, minor, start.signature() < 0));
        std::vector<ScaleEvent> result;
        auto current = start;
        for (size_t i = 0; i < events.size(); ++i)
        {
            // How well a key holds this chord and the next three (this one counts double;
            // its tonic chord a little more), and the key already in force a little more again.
            const auto fit = [&] (const Key& k)
            {
                double score = 0.0;
                for (size_t j = i; j < events.size() && j < i + 4; ++j)
                    score += events[j].chord.isInKey (k) ? (j == i ? 2.0 : 1.0) : 0.0;
                if (events[i].chord.root == k.pitchClass() && (events[i].chord.type == Chord::Type::minor) == k.minor)
                    score += 0.3;
                if (k == current)
                    score += 0.6;
                return score;
            };
            // The key changes only when a chord no longer fits it (G7 before Eb is still
            // C major's V7); then to the key that holds it and the most of what follows.
            auto best = current;
            if (! events[i].chord.isInKey (current) || i == 0)
            {
                auto bestScore = i == 0 && events[i].chord.isInKey (current) ? fit (current) : -1.0;
                for (const auto& k : keys)
                    if (events[i].chord.isInKey (k))
                        if (const auto f = fit (k); f > bestScore + 1.0e-9)
                        {
                            best = k;
                            bestScore = f;
                        }
            }
            if (i == 0 || best != current)
                result.push_back ({ i == 0 ? 0.0 : events[i].beat, best });
            current = best;
        }
        if (result.empty())
            result.push_back ({ 0.0, start });
        return result;
    }

    std::vector<ChordEvent> chordsFromNotes (const std::vector<HeardNote>& notes, double from, double to, double step)
    {
        std::vector<ChordEvent> result;
        step = juce::jmax (0.125, step);
        // The chords tried: plain ones first (fewer tensions win ties), as heard.
        using T = Chord::Type;
        using X = Chord::Tension;
        const std::vector<std::pair<T, X>> shapes {
            { T::major, X::none }, { T::minor, X::none }, { T::diminished, X::none }, { T::augmented, X::none },
            { T::sus4, X::none }, { T::sus2, X::none }, { T::major, X::seven }, { T::major, X::maj7 }, { T::minor, X::seven },
            { T::minor, X::maj7 }, { T::diminished, X::seven }, { T::diminished, X::dim7 }, { T::major, X::six }, { T::minor, X::six },
            { T::sus4, X::seven }, { T::major, X::add9 }, { T::major, X::nine }, { T::minor, X::nine } };
        for (auto t = from; t < to - 1.0e-9; t += step)
        {
            const auto end = juce::jmin (to, t + step);
            std::array<double, 12> weight {};
            auto lowest = 128;
            auto total = 0.0;
            for (const auto& n : notes)
            {
                const auto overlap = juce::jmin (end, n.end) - juce::jmax (t, n.start);
                if (overlap <= 1.0e-9)
                    continue;
                const auto w = overlap * (0.4 + 0.6 * n.velocity);
                weight[(size_t) pc (n.pitch)] += w;
                total += w;
                if (overlap >= (end - t) * 0.25)
                    lowest = juce::jmin (lowest, n.pitch);
            }
            if (total <= 1.0e-9)
                continue;   // (silence: the chord before goes on)
            const auto heaviest = *std::max_element (weight.begin(), weight.end());
            std::optional<Chord> best;
            auto bestScore = 0.0;
            for (int root = 0; root < 12; ++root)
                for (size_t sh = 0; sh < shapes.size(); ++sh)
                {
                    const Chord c { root, shapes[sh].first, shapes[sh].second, -1 };
                    const auto classes = c.pitchClasses();
                    double in = 0.0;
                    auto missing = 0;
                    for (const auto p : classes)
                    {
                        in += weight[(size_t) p];
                        missing += weight[(size_t) p] < heaviest * 0.05 ? 1 : 0;
                    }
                    if ((int) classes.size() - missing < 2 || weight[(size_t) root] < heaviest * 0.05)
                        continue;   // (its root heard, and at least two of its notes)
                    auto score = in - 1.3 * (total - in) - 0.35 * heaviest * missing - 0.04 * heaviest * (double) sh;
                    if (lowest < 128 && pc (lowest) == root)
                        score += 0.15 * heaviest;   // (the bass on the root: the likeliest reading)
                    if (score > bestScore)
                    {
                        bestScore = score;
                        best = c;
                    }
                }
            if (! best)
                continue;
            if (lowest < 128 && pc (lowest) != best->root)
            {
                const auto classes = best->pitchClasses();
                if (std::find (classes.begin(), classes.end(), pc (lowest)) != classes.end())
                    best->bass = pc (lowest);   // an inversion: C/E
            }
            if (result.empty() || result.back().chord != *best)
                result.push_back ({ t, *best });
        }
        return result;
    }

    Key ChordPads::key() const
    {
        if (keyLetter >= 0)
        {
            const auto sp = Key::spellAs (rootKey, keyLetter);
            if (std::abs (sp.accidental) <= 1)
                return { keyLetter, sp.accidental, minor };
        }
        return Key::fromPitchClass (rootKey, minor, flats);
    }

    void ChordPads::setKey (const Key& k)
    {
        rootKey = k.pitchClass();
        keyLetter = k.letter;
        minor = k.minor;
        flats = k.signature() < 0;
    }

    void ChordPads::fillFromKey (const Key& k)
    {
        setKey (k);
        const auto s = k.scale();
        auto h = s;
        h[6] = pc (k.pitchClass() + 11);
        std::vector<Chord> whites, blacks;
        for (int d = 0; d < 7; ++d)
            whites.push_back (degreeChord (s, d, false));
        if (k.minor)
            blacks = { degreeChord (s, 0, true), degreeChord (s, 2, true), degreeChord (s, 3, true), degreeChord (h, 4, false), degreeChord (h, 4, true) };
        else
            blacks = { degreeChord (s, 0, true), degreeChord (s, 1, true), degreeChord (s, 3, true), degreeChord (s, 4, true), degreeChord (s, 5, true) };
        const auto put = [this] (size_t i, const Chord& c)
        {
            if (i >= pads.size() || pads[i].locked)
                return;
            pads[i] = {};
            pads[i].chord = c;
        };
        for (size_t i = 0; i < pads.size(); ++i)
            if (! pads[i].locked)
                pads[i] = {};
        if (layout == Layout::keyboard)
        {
            const int whiteKeys[] = { 0, 2, 4, 5, 7, 9, 11 }, blackKeys[] = { 1, 3, 6, 8, 10 };
            for (int i = 0; i < (int) pads.size() && i < 12; ++i)
            {
                const auto keyPc = pc (startNote + i);
                for (int d = 0; d < 7; ++d)
                    if (whiteKeys[d] == keyPc)
                        put ((size_t) i, whites[(size_t) d]);
                for (int b = 0; b < 5; ++b)
                    if (blackKeys[b] == keyPc)
                        put ((size_t) i, blacks[(size_t) b]);
            }
        }
        else
        {
            for (size_t i = 0; i < 7; ++i)
                put (i, whites[i]);
            // (A grid: the minor key's major V first after its seven, then the rest.)
            const std::vector<Chord> rest = k.minor ? std::vector<Chord> { blacks[3], blacks[4], blacks[0], blacks[2], blacks[1] } : blacks;
            for (size_t i = 0; i < rest.size(); ++i)
                put (7 + i, rest[i]);
        }
        if (adaptiveReference >= 0 && (adaptiveReference >= (int) pads.size() || ! pads[(size_t) adaptiveReference].chord))
            adaptiveReference = -1;
    }

    bool ChordPads::operator== (const ChordPads& o) const noexcept
    {
        return pads == o.pads && rootKey == o.rootKey && flats == o.flats && adaptiveReference == o.adaptiveReference
            && octaveOffset == o.octaveOffset && overlap == o.overlap && layout == o.layout && startNote == o.startNote
            && rows == o.rows && columns == o.columns && primary == o.primary && secondary == o.secondary
            && showVoicing == o.showVoicing && triggerStart == o.triggerStart && triggerEnd == o.triggerEnd && latch == o.latch
            && minor == o.minor && key() == o.key() && sectionsMode == o.sectionsMode && sectionKeys == o.sectionKeys
            && subsectionKeys == o.subsectionKeys && modifiersOn == o.modifiersOn && modifierKeys == o.modifierKeys;
    }

    juce::String ChordPads::modifierName (Modifier m)
    {
        switch (m)
        {
            case Modifier::nextVoicing:     return "Next Voicing";
            case Modifier::previousVoicing: return "Previous Voicing";
            case Modifier::moreTensions:    return "More Tensions";
            case Modifier::fewerTensions:   return "Fewer Tensions";
            case Modifier::transposeUp:     return "Transpose Up";
            case Modifier::transposeDown:   return "Transpose Down";
        }
        return {};
    }

    juce::String ChordPads::displayName (const Chord& chord, Display display) const
    {
        switch (display)
        {
            case Display::noteName:  return chord.name (key());
            case Display::roman:     return chord.romanName (key());
            case Display::nashville: return chord.nashvilleName (key());
            case Display::none:      return {};
        }
        return {};
    }

    std::vector<int> ChordPads::notesFor (int index, const std::vector<int>& previous) const
    {
        if (! juce::isPositiveAndBelow (index, (int) pads.size()) || ! pads[(size_t) index].chord)
            return {};
        const auto& pad = pads[(size_t) index];
        if (! pad.adaptive && ! pad.notes.empty())
            return pad.notes;
        const auto named = namedVoicings (*pad.chord, 48 + 12 * octaveOffset);
        if (named.empty())
            return {};
        std::vector<std::vector<int>> close;
        for (const auto& v : named)
            if (v.close)
                close.push_back (v.notes);
        if (pad.adaptive)
        {
            // (Adaptive voicing chooses among the close voicings: the voices move the least.)
            std::vector<int> reference;
            if (juce::isPositiveAndBelow (adaptiveReference, (int) pads.size()) && adaptiveReference != index
                && pads[(size_t) adaptiveReference].chord)
            {
                auto fixed = *this;
                fixed.pads[(size_t) adaptiveReference].adaptive = false;   // (its own voicing, not chosen again)
                reference = fixed.notesFor (adaptiveReference, {});
            }
            return close[(size_t) closestVoicing (close, previous, reference)];
        }
        if (pad.voicing < 0)
            return close[close.size() / 2];
        return named[(size_t) juce::jlimit (0, (int) named.size() - 1, pad.voicing)].notes;
    }

    int ChordPads::padForKey (int midiNote) const noexcept
    {
        if (midiNote < triggerStart || midiNote > triggerEnd)
            return -1;
        const auto index = midiNote - triggerStart;
        return index < (int) pads.size() ? index : -1;
    }

    std::unique_ptr<juce::XmlElement> ChordPads::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("ChordPads");
        xml->setAttribute ("rootKey", rootKey);
        xml->setAttribute ("flats", flats);
        xml->setAttribute ("minor", minor);
        xml->setAttribute ("keyLetter", key().letter);
        xml->setAttribute ("adaptiveReference", adaptiveReference);
        xml->setAttribute ("octaveOffset", octaveOffset);
        xml->setAttribute ("overlap", (int) overlap);
        xml->setAttribute ("layout", (int) layout);
        xml->setAttribute ("startNote", startNote);
        xml->setAttribute ("rows", rows);
        xml->setAttribute ("columns", columns);
        xml->setAttribute ("primary", (int) primary);
        xml->setAttribute ("secondary", (int) secondary);
        xml->setAttribute ("showVoicing", showVoicing);
        xml->setAttribute ("triggerStart", triggerStart);
        xml->setAttribute ("triggerEnd", triggerEnd);
        xml->setAttribute ("latch", latch);
        // (0.44.0) Sections and the modifiers' keys.
        const auto keysText = [] (const auto& keys) { juce::StringArray t; for (const auto k : keys) t.add (juce::String (k)); return t.joinIntoString (" "); };
        xml->setAttribute ("sectionsMode", sectionsMode);
        xml->setAttribute ("sectionKeys", keysText (sectionKeys));
        xml->setAttribute ("subsectionKeys", keysText (subsectionKeys));
        xml->setAttribute ("modifiersOn", modifiersOn);
        xml->setAttribute ("modifierKeys", keysText (modifierKeys));
        xml->setAttribute ("count", (int) pads.size());
        for (size_t i = 0; i < pads.size(); ++i)
        {
            const auto& pad = pads[i];
            if (! pad.chord && ! pad.locked)
                continue;
            auto* e = xml->createNewChildElement ("Pad");
            e->setAttribute ("index", (int) i);
            if (pad.chord)
                e->setAttribute ("chord", pad.chord->toText());
            e->setAttribute ("voicing", pad.voicing);
            e->setAttribute ("adaptive", pad.adaptive);
            e->setAttribute ("locked", pad.locked);
            if (! pad.notes.empty())
            {
                juce::StringArray notes;
                for (const auto n : pad.notes)
                    notes.add (juce::String (n));
                e->setAttribute ("notes", notes.joinIntoString (" "));
            }
        }
        return xml;
    }

    ChordPads ChordPads::fromXml (const juce::XmlElement& xml)
    {
        ChordPads p;
        const auto clampInt = [&xml] (const char* name, int fallback, int low, int high)
        {
            return juce::jlimit (low, high, xml.getIntAttribute (name, fallback));
        };
        p.rootKey = clampInt ("rootKey", 0, 0, 11);
        p.flats = xml.getBoolAttribute ("flats", false);
        p.minor = xml.getBoolAttribute ("minor", false);
        p.keyLetter = juce::jlimit (-1, 6, xml.getIntAttribute ("keyLetter", -1));
        p.octaveOffset = clampInt ("octaveOffset", 0, -3, 3);
        p.overlap = (Overlap) clampInt ("overlap", (int) Overlap::legato, 0, 2);
        p.layout = (Layout) clampInt ("layout", 0, 0, 1);
        p.startNote = clampInt ("startNote", 0, 0, 11);
        p.rows = clampInt ("rows", 2, 1, 8);
        p.columns = clampInt ("columns", 8, 1, 16);
        p.primary = (Display) clampInt ("primary", 0, 0, 3);
        p.secondary = (Display) clampInt ("secondary", 1, 0, 3);
        p.showVoicing = xml.getBoolAttribute ("showVoicing", true);
        p.triggerStart = clampInt ("triggerStart", 36, 0, 127);
        p.triggerEnd = clampInt ("triggerEnd", 47, p.triggerStart, 127);
        p.latch = xml.getBoolAttribute ("latch", false);
        const auto readKeys = [&xml] (const char* name, auto& keys)
        {
            const auto t = juce::StringArray::fromTokens (xml.getStringAttribute (name), " ", {});
            if (t.size() == (int) keys.size())
                for (size_t i = 0; i < keys.size(); ++i)
                    keys[i] = juce::jlimit (-1, 127, t[(int) i].getIntValue());
        };
        p.sectionsMode = xml.getBoolAttribute ("sectionsMode");
        readKeys ("sectionKeys", p.sectionKeys);
        readKeys ("subsectionKeys", p.subsectionKeys);
        p.modifiersOn = xml.getBoolAttribute ("modifiersOn");
        readKeys ("modifierKeys", p.modifierKeys);
        p.pads = std::vector<ChordPad> ((size_t) clampInt ("count", 12, 1, 128));
        p.adaptiveReference = clampInt ("adaptiveReference", -1, -1, (int) p.pads.size() - 1);
        for (const auto* e : xml.getChildWithTagNameIterator ("Pad"))
        {
            const auto index = e->getIntAttribute ("index", -1);
            if (! juce::isPositiveAndBelow (index, (int) p.pads.size()))
                continue;
            auto& pad = p.pads[(size_t) index];
            if (e->hasAttribute ("chord"))
                pad.chord = Chord::fromText (e->getStringAttribute ("chord"));
            pad.voicing = e->getIntAttribute ("voicing", -1);
            pad.adaptive = e->getBoolAttribute ("adaptive", true);
            pad.locked = e->getBoolAttribute ("locked", false);
            for (const auto& n : juce::StringArray::fromTokens (e->getStringAttribute ("notes"), " ", {}))
                if (n.isNotEmpty())
                    pad.notes.push_back (juce::jlimit (0, 127, n.getIntValue()));
        }
        return p;
    }

    ChordPads ChordPads::defaults()
    {
        ChordPads p;
        p.fillFromScale (Scale::major, 0);
        return p;
    }

    juce::StringArray ChordPads::scaleNames()
    {
        return { "Major (Ionian)", "Natural Minor (Aeolian)", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian" };
    }

    void ChordPads::fillFromScale (Scale scale, int key)
    {
        // Major and minor (0.15.0): through the key, spelled, with the minor key's major V too.
        if (scale == Scale::major || scale == Scale::naturalMinor)
        {
            fillFromKey (Key::fromPitchClass (key, scale == Scale::naturalMinor, flats));
            return;
        }
        static const int steps[7][7] = {
            { 0, 2, 4, 5, 7, 9, 11 },   // major
            { 0, 2, 3, 5, 7, 8, 10 },   // natural minor
            { 0, 2, 3, 5, 7, 9, 10 },   // dorian
            { 0, 1, 3, 5, 7, 8, 10 },   // phrygian
            { 0, 2, 4, 6, 7, 9, 11 },   // lydian
            { 0, 2, 4, 5, 7, 9, 10 },   // mixolydian
            { 0, 1, 3, 5, 6, 8, 10 },   // locrian
        };
        const auto& s = steps[(int) scale];
        const auto chordOn = [&s, key] (int degree, bool seventh)
        {
            const auto at = [&s, degree] (int up) { return pc (s[(degree + up) % 7] - s[degree]); };
            const auto third = at (2), fifth = at (4), sev = at (6);
            Chord c;
            c.root = pc (key + s[degree]);
            c.type = third == 4 ? (fifth == 8 ? Chord::Type::augmented : Chord::Type::major)
                                : (fifth == 6 ? Chord::Type::diminished : Chord::Type::minor);
            if (seventh)
            {
                if (c.type == Chord::Type::diminished)
                    c.tension = sev == 9 ? Chord::Tension::dim7 : Chord::Tension::seven;
                else
                    c.tension = sev == 11 ? Chord::Tension::maj7 : Chord::Tension::seven;
            }
            return c;
        };
        for (auto& pad : pads)
            pad = {};
        const int sevenths[] = { 0, 1, 3, 4, 5 };
        if (layout == Layout::keyboard)
        {
            // As on a keyboard: the scale's chords on the white keys (C D E F G A B:
            // I to VII), its sevenths on the black keys between them (C#: I7,
            // D#: II7, F#: IV7, G#: V7, A#: VI7); the first octave of pads.
            const int whites[] = { 0, 2, 4, 5, 7, 9, 11 }, blacks[] = { 1, 3, 6, 8, 10 };
            for (int i = 0; i < (int) pads.size() && i < 12; ++i)
            {
                const auto key = pc (startNote + i);
                for (int d = 0; d < 7; ++d)
                    if (whites[d] == key)
                        pads[(size_t) i].chord = chordOn (d, false);
                for (int b = 0; b < 5; ++b)
                    if (blacks[b] == key)
                        pads[(size_t) i].chord = chordOn (sevenths[b], true);
            }
        }
        else
        {
            // A grid: the seven, then the sevenths, in order.
            for (int d = 0; d < 7 && d < (int) pads.size(); ++d)
                pads[(size_t) d].chord = chordOn (d, false);
            for (int i = 0; i < 5 && 7 + i < (int) pads.size(); ++i)
                pads[(size_t) (7 + i)].chord = chordOn (sevenths[i], true);
        }
        // (The modes named in the major or minor key on their tonic, as their third is.)
        setKey (Key::fromPitchClass (key, scale == Scale::dorian || scale == Scale::phrygian || scale == Scale::locrian, flats));
        adaptiveReference = -1;
    }

    //==========================================================================
    void PadPlayer::setPads (const ChordPads& p)
    {
        const juce::ScopedLock sl (lock);
        pads = p;
    }

    void PadPlayer::noteOn (int note, juce::uint8 velocity, int channel)
    {
        if (send)
            send (juce::MidiMessage::noteOn (channel, juce::jlimit (0, 127, note), velocity));
        if (std::find (sounding.begin(), sounding.end(), note) == sounding.end())
            sounding.push_back (note);
    }

    void PadPlayer::noteOff (int note, int channel)
    {
        if (send)
            send (juce::MidiMessage::noteOff (channel, juce::jlimit (0, 127, note)));
        sounding.erase (std::remove (sounding.begin(), sounding.end(), note), sounding.end());
    }

    void PadPlayer::press (int pad, juce::uint8 velocity, int channel)
    {
        auto on = false;
        {
            const juce::ScopedLock sl (lock);
            if (pads.latch && latched.count (pad) > 0)
            {
                latched.erase (pad);   // a latched pad pressed again: it ends
                if (! releaseHeld (pad, channel))
                    return;
            }
            else
            {
                const auto notes = pads.notesFor (pad, lastChord);
                if (notes.empty())
                    return;
                // The pads already down, as the overlap mode says.
                for (const auto other : held)
                {
                    auto& theirs = padNotes[other];
                    if (pads.overlap == ChordPads::Overlap::holdFirst)
                        continue;
                    for (const auto n : theirs)
                        if (pads.overlap == ChordPads::Overlap::stopFirst || std::find (notes.begin(), notes.end(), n) == notes.end())
                            noteOff (n, channel);
                    theirs.clear();   // (its sound is over, or carried on by the new chord)
                }
                // (0.44.0) Sections mode: the pad picks the chord; the section keys play it.
                if (! pads.sectionsMode && ! (onPerform && onPerform (notes, velocity, channel)))
                    for (const auto n : notes)
                        if (std::find (sounding.begin(), sounding.end(), n) == sounding.end())
                            noteOn (n, velocity, channel);
                padNotes[pad] = notes;
                held.erase (std::remove (held.begin(), held.end(), pad), held.end());
                held.push_back (pad);
                lastChord = notes;
                if (pads.latch)
                    latched.insert (pad);
                on = true;
            }
        }
        if (onPadChanged)
            onPadChanged (pad, on);
    }

    bool PadPlayer::releaseHeld (int pad, int channel)
    {
        const auto found = padNotes.find (pad);
        if (found == padNotes.end())
            return false;
        const auto mine = found->second;
        padNotes.erase (found);
        held.erase (std::remove (held.begin(), held.end(), pad), held.end());
        for (const auto n : mine)
        {
            auto stillWanted = false;
            for (const auto& [other, notes] : padNotes)
                stillWanted = stillWanted || std::find (notes.begin(), notes.end(), n) != notes.end();
            if (! stillWanted && std::find (sounding.begin(), sounding.end(), n) != sounding.end())
                noteOff (n, channel);
        }
        return true;
    }

    void PadPlayer::release (int pad, int channel)
    {
        {
            const juce::ScopedLock sl (lock);
            if (pads.latch && latched.count (pad) > 0)
                return;   // (latched: plays until pressed again)
            if (! releaseHeld (pad, channel))
                return;
            if (held.empty() && onPerformStop)
                onPerformStop();
        }
        if (onPadChanged)
            onPadChanged (pad, false);
    }

    void PadPlayer::releaseAll (int channel)
    {
        std::vector<int> wereHeld;
        {
            const juce::ScopedLock sl (lock);
            sectionNotes.clear();
            for (const auto n : std::vector<int> (sounding))
                noteOff (n, channel);
            wereHeld = held;
            held.clear();
            padNotes.clear();
            latched.clear();
            if (onPerformStop)
                onPerformStop();
        }
        if (onPadChanged)
            for (const auto p : wereHeld)
                onPadChanged (p, false);
    }

    std::optional<Chord> PadPlayer::chordOf (int pad) const
    {
        const juce::ScopedLock sl (lock);
        return juce::isPositiveAndBelow (pad, (int) pads.pads.size()) ? pads.pads[(size_t) pad].chord : std::nullopt;
    }

    void PadPlayer::pressSection (int key, int section, bool octaveUp, juce::uint8 velocity, int channel)
    {
        const juce::ScopedLock sl (lock);
        auto chord = ! held.empty() && padNotes.count (held.back()) > 0 ? padNotes[held.back()] : lastChord;
        if (chord.empty())
            return;
        std::sort (chord.begin(), chord.end());
        const auto count = (int) chord.size();
        const auto note = juce::jlimit (0, 127, chord[(size_t) (section % count)] + 12 * (section / count) + (octaveUp ? 12 : 0));
        if (const auto old = sectionNotes.find (key); old != sectionNotes.end())
        {
            noteOff (old->second, channel);
            sectionNotes.erase (old);
        }
        noteOn (note, velocity, channel);
        sectionNotes[key] = note;
    }

    void PadPlayer::releaseSection (int key, int channel)
    {
        const juce::ScopedLock sl (lock);
        if (const auto found = sectionNotes.find (key); found != sectionNotes.end())
        {
            const auto note = found->second;
            sectionNotes.erase (found);
            auto stillWanted = false;
            for (const auto& [k, n] : sectionNotes)
                stillWanted = stillWanted || n == note;
            if (! stillWanted)
                noteOff (note, channel);
        }
    }

    bool PadPlayer::handle (const juce::MidiMessage& message)
    {
        if (! message.isNoteOnOrOff())
            return false;
        const auto key = message.getNoteNumber();
        // (0.44.0) MIDI Learn: the key is reported, not played.
        if (learning.load())
        {
            if (message.isNoteOn())
            {
                learning = false;
                if (onLearn)
                    onLearn (key);
            }
            return true;
        }
        std::optional<ChordPads::Modifier> modifier;
        auto section = -1;
        auto octaveUp = false;
        {
            const juce::ScopedLock sl (lock);
            if (pads.modifiersOn)
                for (int m = 0; m < ChordPads::numModifiers; ++m)
                    if (pads.modifierKeys[(size_t) m] == key)
                        modifier = (ChordPads::Modifier) m;
            if (pads.sectionsMode)
                for (int s = 0; s < 5; ++s)
                {
                    if (pads.sectionKeys[(size_t) s] == key)
                        section = s;
                    if (pads.subsectionKeys[(size_t) s] == key)
                        section = s, octaveUp = true;
                }
        }
        if (modifier)
        {
            if (message.isNoteOn() && onModifier)
                onModifier (*modifier);
            return true;
        }
        if (section >= 0)
        {
            if (message.isNoteOn())
                pressSection (key, section, octaveUp, message.getVelocity());
            else
                releaseSection (key);
            return true;
        }
        int pad;
        {
            const juce::ScopedLock sl (lock);
            if (message.getNoteNumber() < pads.triggerStart || message.getNoteNumber() > pads.triggerEnd)
            {
                if (onOtherKey)
                    onOtherKey (message.getNoteNumber(), message.isNoteOn());
                return false;
            }
            pad = pads.padForKey (message.getNoteNumber());
        }
        // A trigger key: never played as itself (the manual's remote keys).
        if (pad >= 0)
        {
            if (message.isNoteOn())
                press (pad, message.getVelocity());
            else
                release (pad);
        }
        return true;
    }

    std::vector<int> PadPlayer::getSounding() const
    {
        const juce::ScopedLock sl (lock);
        return sounding;
    }

    std::vector<int> PadPlayer::getLastChord() const
    {
        const juce::ScopedLock sl (lock);
        return lastChord;
    }

    void PadPlayer::setLastChord (std::vector<int> notes)
    {
        const juce::ScopedLock sl (lock);
        lastChord = std::move (notes);
    }

    bool PadPlayer::isPressed (int pad) const
    {
        const juce::ScopedLock sl (lock);
        return padNotes.count (pad) > 0;
    }
}
