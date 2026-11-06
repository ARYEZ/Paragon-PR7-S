#include "engine/Instruments.h"

namespace pr7s
{
    int InstrumentProfile::rangeLow() const
    {
        if (lowest >= 0)
            return lowest;
        return strings.empty() ? 0 : *std::min_element (strings.begin(), strings.end());
    }

    int InstrumentProfile::rangeHigh() const
    {
        if (highest >= 0)
            return highest;
        return strings.empty() ? 127 : juce::jmin (127, *std::max_element (strings.begin(), strings.end()) + frets);
    }

    GuitarTuning InstrumentProfile::tuning() const
    {
        GuitarTuning t;
        t.name = name;
        t.strings = strings.empty() ? std::vector<int> { rangeLow() } : strings;
        return t;
    }

    int InstrumentProfile::keyFor (int string) const
    {
        for (const auto& k : keyswitches)
            if (k.forcesString == string && string > 0)
                return k.note;
        return -1;
    }

    int InstrumentProfile::releaseKey() const
    {
        for (const auto& k : keyswitches)
            if (k.forcesString < 0)
                return k.note;
        return -1;
    }

    int InstrumentProfile::keyUsedFor (Keyswitch::Use use) const
    {
        for (const auto& k : keyswitches)
            if (k.use == use && use != Keyswitch::Use::none)
                return k.note;
        return -1;
    }

    bool InstrumentProfile::isKeyswitch (int note) const
    {
        return std::any_of (keyswitches.begin(), keyswitches.end(), [note] (const Keyswitch& k) { return k.note == note; });
    }

    std::unique_ptr<juce::XmlElement> InstrumentProfile::toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement> ("Instrument");
        xml->setAttribute ("name", name);
        xml->setAttribute ("kind", kind == Kind::bass ? "bass" : "guitar");
        juce::StringArray s;
        for (const auto n : strings)
            s.add (juce::String (n));
        xml->setAttribute ("strings", s.joinIntoString (" "));
        xml->setAttribute ("frets", frets);
        xml->setAttribute ("lowest", lowest);
        xml->setAttribute ("highest", highest);
        xml->setAttribute ("bendRange", bendRange);
        xml->setAttribute ("forBass", forBass);
        xml->setAttribute ("forGuitar", forGuitar);
        xml->setAttribute ("forLead", forLead);
        if (velocities.has_value())
            xml->setAttribute ("velocities", velocities->toString());
        for (const auto& k : keyswitches)
        {
            auto* e = xml->createNewChildElement ("Keyswitch");
            e->setAttribute ("name", k.name);
            e->setAttribute ("note", k.note);
            if (k.forcesString != 0)
                e->setAttribute ("string", k.forcesString);
            if (k.use != Keyswitch::Use::none)
                e->setAttribute ("use", k.use == Keyswitch::Use::powerChords ? "powerChords" : "singleNotes");
        }
        return xml;
    }

    std::optional<InstrumentProfile> InstrumentProfile::fromXml (const juce::XmlElement& xml)
    {
        if (! xml.hasTagName ("Instrument"))
            return {};
        InstrumentProfile p;
        p.name = xml.getStringAttribute ("name").trim();
        if (p.name.isEmpty())
            return {};
        p.kind = xml.getStringAttribute ("kind") == "guitar" ? Kind::guitar : Kind::bass;
        p.strings.clear();
        for (const auto& t : juce::StringArray::fromTokens (xml.getStringAttribute ("strings"), " ", ""))
            if (t.containsOnly ("0123456789") && t.isNotEmpty())
                p.strings.push_back (juce::jlimit (0, 127, t.getIntValue()));
        std::sort (p.strings.begin(), p.strings.end());
        p.frets = juce::jlimit (1, 36, xml.getIntAttribute ("frets", 24));
        p.lowest = juce::jlimit (-1, 127, xml.getIntAttribute ("lowest", -1));
        p.highest = juce::jlimit (-1, 127, xml.getIntAttribute ("highest", -1));
        p.bendRange = juce::jlimit (0, 48, xml.getIntAttribute ("bendRange", 2));
        p.forBass = xml.getBoolAttribute ("forBass");
        p.forGuitar = xml.getBoolAttribute ("forGuitar");
        p.forLead = xml.getBoolAttribute ("forLead");
        if (xml.hasAttribute ("velocities"))
            p.velocities = GuitarVelocities::fromString (xml.getStringAttribute ("velocities"));
        for (auto* e : xml.getChildWithTagNameIterator ("Keyswitch"))
        {
            Keyswitch k { e->getStringAttribute ("name"), juce::jlimit (0, 127, e->getIntAttribute ("note")),
                          juce::jmax (-1, e->getIntAttribute ("string")) };
            const auto use = e->getStringAttribute ("use");
            k.use = use == "powerChords" ? Keyswitch::Use::powerChords : use == "singleNotes" ? Keyswitch::Use::singleNotes : Keyswitch::Use::none;
            p.keyswitches.push_back (k);
        }
        return p;
    }

    InstrumentProfile InstrumentProfile::djinnBass2()
    {
        InstrumentProfile p;
        p.name = "Djinn Bass 2";
        p.kind = Kind::bass;
        // The owner's tuning (Djinn's default is B0 E1 A1 D2 G2): A0 D1 A1 D2 G2.
        p.strings = { 33, 38, 45, 50, 55 };
        p.frets = 24;
        p.bendRange = 4;   // (Djinn's MISCELLANEOUS: Bend Range 4)
        p.forBass = true;
        // Djinn's KEYSWITCH MAP (the articulations shown grey there, not in the library, left out).
        p.keyswitches = {
            { "Main Alternating", 0, 0 }, { "Main Up / Index", 1, 0 }, { "Main Down / Mid", 2, 0 }, { "Dead Note", 3, 0 },
            { "Slap", 4, 0 }, { "Dead Slap", 5, 0 }, { "Pop", 6, 0 }, { "Dead Pop", 7, 0 }, { "Harmonic", 8, 0 },
            { "Hammer/Pull", 9, 0 }, { "Tapping", 10, 0 }, { "Slide Trigger", 11, 0 }, { "Fret Hand Mute", 16, 0 },
            { "Palm Mute Alt", 17, 0 }, { "Palm Mute Up", 18, 0 }, { "Palm Mute Down", 19, 0 }, { "Dead Strum", 20, 0 },
            { "Dead Palm Rest", 98, 0 },
            { "Force B String (your A0)", 127, 1 }, { "Force E String (your D1)", 126, 2 }, { "Force A String", 125, 3 },
            { "Force D String", 124, 4 }, { "Force G String", 123, 5 } };
        return p;
    }

    InstrumentProfile InstrumentProfile::shreddage (const juce::String& name, const GuitarVelocities& velocities)
    {
        InstrumentProfile p;
        p.name = name;
        p.kind = Kind::guitar;
        p.strings = { 33, 40, 45, 50, 55, 59, 64 };   // A0 E1 A1 D2 G2 B2 E3
        p.frets = 24;
        p.velocities = velocities;   // (1-59 palm mute, 60-119 sustain, 120-126 rakes, 127 pinch harmonics)
        p.bendRange = 2;
        p.keyswitches = {
            { "Sustain (palm mute 1-59, sustain 60-119, rake 120-126, pinch 127)", 12, 0, Keyswitch::Use::singleNotes },   // C-1
            { "Power Chords (the same velocity layers)", 13, 0, Keyswitch::Use::powerChords } };                         // C#-1 (the owner's)
        // The manual's Behavioral Keyswitches: C-2 forces the lowest A string, each key up the next string.
        const char* names[] = { "low A", "E", "A", "D", "G", "B", "high E" };
        for (int s = 1; s <= 7; ++s)
            p.keyswitches.push_back ({ "Force String " + juce::String (s) + " (" + names[s - 1] + ")", s - 1, s });
        p.keyswitches.push_back ({ "Force String Off", 7, -1 });   // G-2
        return p;
    }

    InstrumentProfile InstrumentProfile::blank (Kind kind)
    {
        InstrumentProfile p;
        p.kind = kind;
        if (kind == Kind::guitar)
        {
            p.name = "New Guitar";
            p.strings = GuitarTuning::all().front().strings;
            p.velocities = GuitarVelocities {};
        }
        else
        {
            p.name = "New Bass";
            p.strings = { 28, 33, 38, 43 };
        }
        return p;
    }

    juce::String profilesToText (const std::vector<InstrumentProfile>& profiles)
    {
        juce::XmlElement root ("Instruments");
        for (const auto& p : profiles)
            root.addChildElement (p.toXml().release());
        return root.toString (juce::XmlElement::TextFormat().singleLine().withoutHeader());
    }

    std::vector<InstrumentProfile> profilesFromText (const juce::String& text)
    {
        std::vector<InstrumentProfile> out;
        if (const auto xml = juce::parseXML (text); xml != nullptr && xml->hasTagName ("Instruments"))
            for (auto* e : xml->getChildIterator())
                if (auto p = InstrumentProfile::fromXml (*e))
                    out.push_back (std::move (*p));
        return out;
    }

    std::optional<int> parseNoteText (const juce::String& raw)
    {
        const auto text = raw.trim().removeCharacters (" ");
        if (text.isEmpty())
            return {};
        if (text.containsOnly ("0123456789"))
            return text.getIntValue() <= 127 ? std::optional<int> (text.getIntValue()) : std::nullopt;
        static const int letters[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
        const auto first = juce::CharacterFunctions::toUpperCase (text[0]);
        if (first < 'A' || first > 'G')
            return {};
        auto pc = letters[first - 'A'];
        auto i = 1;
        while (i < text.length() && (text[i] == '#' || text[i] == 'b' || text[i] == 'B'))
        {
            if (text[i] == 'B' && i + 1 >= text.length())
                break;
            pc += text[i] == '#' ? 1 : -1;
            ++i;
        }
        const auto rest = text.substring (i);
        if (rest.isEmpty() || ! rest.trimCharactersAtStart ("-").containsOnly ("0123456789") || rest.trimCharactersAtStart ("-").isEmpty())
            return {};
        const auto octave = rest.getIntValue();
        const auto note = (octave + 2) * 12 + pc;   // C-2 = 0, C3 = 60
        if (note < 0 || note > 127)
            return {};
        return note;
    }

    void playOnString (LineIdea& idea, const std::vector<int>& strings, int string, int frets, int keyswitch, double length, int release)
    {
        if (strings.empty() || string < 1 || string > (int) strings.size())
            return;
        const auto open = strings[(size_t) string - 1], top = open + frets;
        auto& notes = idea.notes;
        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b)
                          { return a.beat < b.beat || (a.beat == b.beat && a.pitch < b.pitch); });
        std::vector<LineNote> kept, keys;
        for (const auto& n : notes)
        {
            if (n.keyswitch)
            {
                keys.push_back (n);   // (0.63.0: the articulations' keys stay)
                continue;
            }
            if (! kept.empty() && std::abs (kept.back().beat - n.beat) < 1.0e-4)
                continue;   // (a string plays one note at a time: the lowest kept)
            auto m = n;
            while (m.pitch < open && m.pitch + 12 <= 127)
                m.pitch += 12;
            while (m.pitch > top && m.pitch - 12 >= open)
                m.pitch -= 12;
            if (m.pitch < open || m.pitch > top)
                continue;   // (a string under an octave long can't hold it)
            kept.push_back (m);
        }
        // A note now overlapping the next on the string ends where it starts.
        for (size_t i = 0; i + 1 < kept.size(); ++i)
            kept[i].length = juce::jmin (kept[i].length, juce::jmax (0.01, kept[i + 1].beat - kept[i].beat));
        if (keyswitch >= 0 && length > 0.0)
        {
            LineNote key;
            key.beat = 0.0;
            key.length = length;
            key.pitch = keyswitch;
            key.velocity = 100.0f / 127.0f;
            key.keyswitch = true;
            kept.insert (kept.begin(), key);
            if (release >= 0)
            {
                // After the last note starts (a force only steers where a note is played), a 1/32 beat long.
                auto last = 0.0;
                for (const auto& n : kept)
                    if (! n.keyswitch)
                        last = juce::jmax (last, n.beat);
                LineNote off = key;
                off.pitch = release;
                off.beat = juce::jmin (last + 1.0 / 64.0, juce::jmax (0.0, length - 1.0 / 32.0));
                off.length = juce::jmax (1.0 / 128.0, juce::jmin (1.0 / 32.0, length - off.beat));
                kept.push_back (off);
            }
        }
        kept.insert (kept.end(), keys.begin(), keys.end());
        std::stable_sort (kept.begin(), kept.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        notes = std::move (kept);
    }
}

namespace pr7s
{
    void usePowerChordKeys (LineIdea& idea, int powerChordKey, int singleNoteKey)
    {
        if (powerChordKey < 0 || singleNoteKey < 0 || idea.notes.empty())
            return;
        std::vector<LineNote> keys, played;
        for (const auto& n : idea.notes)
            (n.keyswitch ? keys : played).push_back (n);
        std::stable_sort (played.begin(), played.end(), [] (const LineNote& a, const LineNote& b)
                          { return a.beat < b.beat || (a.beat == b.beat && a.pitch < b.pitch); });
        std::vector<LineNote> out;
        auto mode = 0;   // 1 power chords, 2 single notes
        const auto press = [&] (double beat, int wanted)
        {
            if (mode == wanted)
                return;
            mode = wanted;
            LineNote k;
            k.beat = beat;
            k.length = 1.0 / 16.0;
            k.pitch = wanted == 1 ? powerChordKey : singleNoteKey;
            k.velocity = 100.0f / 127.0f;
            k.keyswitch = true;
            out.push_back (k);
        };
        for (size_t i = 0; i < played.size();)
        {
            auto j = i + 1;
            while (j < played.size() && std::abs (played[j].beat - played[i].beat) < 1.0e-4)
                ++j;
            const auto root = played[i].pitch;
            auto power = j - i >= 2;
            auto hasFifth = false;
            for (auto k = i; k < j && power; ++k)
            {
                const auto interval = played[k].pitch - root;
                if (interval % 12 == 7)
                    hasFifth = true;
                else if (interval % 12 != 0)
                    power = false;
            }
            power = power && hasFifth;
            press (played[i].beat, power ? 1 : 2);
            if (power)
            {
                auto n = played[i];
                for (auto k = i + 1; k < j; ++k)
                {
                    n.length = juce::jmax (n.length, played[k].length);
                    n.velocity = juce::jmax (n.velocity, played[k].velocity);
                }
                out.push_back (n);
            }
            else
                out.insert (out.end(), played.begin() + (long) i, played.begin() + (long) j);
            i = j;
        }
        out.insert (out.end(), keys.begin(), keys.end());
        std::stable_sort (out.begin(), out.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        idea.notes = std::move (out);
    }
}
