#include "Chords.h"

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

    std::vector<std::vector<int>> voicingsFor (const Chord& chord, int low)
    {
        const auto classes = chord.pitchClasses();
        const auto n = (int) classes.size();
        std::vector<std::vector<int>> all;
        for (int k = 0; k < n; ++k)
            for (int octave = 0; octave < 2; ++octave)
            {
                std::vector<int> notes;
                auto first = low + pc (classes[(size_t) k] - low) + octave * 12;
                notes.push_back (first);
                for (int i = 1; i < n; ++i)
                {
                    const auto want = classes[(size_t) ((k + i) % n)];
                    auto note = notes.back() + 1;
                    while (pc (note) != want)
                        ++note;
                    notes.push_back (note);
                }
                if (notes.front() >= low && notes.front() < low + 18)
                    all.push_back (notes);
            }
        std::sort (all.begin(), all.end(), [] (const auto& a, const auto& b) { return a.front() < b.front(); });
        all.erase (std::unique (all.begin(), all.end()), all.end());
        if (chord.bass >= 0)
            for (auto& v : all)
            {
                auto bassNote = v.front() - pc (v.front() - chord.bass);
                if (bassNote == v.front())
                    bassNote -= 12;
                v.insert (v.begin(), bassNote);
            }
        return all;
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
    bool ChordPads::operator== (const ChordPads& o) const noexcept
    {
        return pads == o.pads && rootKey == o.rootKey && flats == o.flats && adaptiveReference == o.adaptiveReference
            && octaveOffset == o.octaveOffset && overlap == o.overlap && layout == o.layout && startNote == o.startNote
            && rows == o.rows && columns == o.columns && primary == o.primary && secondary == o.secondary
            && showVoicing == o.showVoicing && triggerStart == o.triggerStart && triggerEnd == o.triggerEnd && latch == o.latch;
    }

    juce::String ChordPads::displayName (const Chord& chord, Display display) const
    {
        switch (display)
        {
            case Display::noteName:  return chord.name (flats);
            case Display::roman:     return chord.romanName (rootKey);
            case Display::nashville: return chord.nashvilleName (rootKey);
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
        const auto voicings = voicingsFor (*pad.chord, 48 + 12 * octaveOffset);
        if (voicings.empty())
            return {};
        if (pad.adaptive)
        {
            std::vector<int> reference;
            if (juce::isPositiveAndBelow (adaptiveReference, (int) pads.size()) && adaptiveReference != index
                && pads[(size_t) adaptiveReference].chord)
            {
                auto fixed = *this;
                fixed.pads[(size_t) adaptiveReference].adaptive = false;   // (its own voicing, not chosen again)
                reference = fixed.notesFor (adaptiveReference, {});
            }
            return voicings[(size_t) closestVoicing (voicings, previous, reference)];
        }
        const auto at = pad.voicing < 0 ? (int) voicings.size() / 2 : juce::jlimit (0, (int) voicings.size() - 1, pad.voicing);
        return voicings[(size_t) at];
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
        rootKey = pc (key);
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
        }
        if (onPadChanged)
            onPadChanged (pad, false);
    }

    void PadPlayer::releaseAll (int channel)
    {
        std::vector<int> wereHeld;
        {
            const juce::ScopedLock sl (lock);
            for (const auto n : std::vector<int> (sounding))
                noteOff (n, channel);
            wereHeld = held;
            held.clear();
            padNotes.clear();
            latched.clear();
        }
        if (onPadChanged)
            for (const auto p : wereHeld)
                onPadChanged (p, false);
    }

    bool PadPlayer::handle (const juce::MidiMessage& message)
    {
        if (! message.isNoteOnOrOff())
            return false;
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
