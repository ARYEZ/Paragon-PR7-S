#pragma once

#include "engine/Lines.h"

#include <optional>

// INSTRUMENTS (0.62.0, the owner asked: "a settings menu in the Hub ... rules for each instrument":
// each sample library's range, tuning, velocity layers and keyswitches, so the composer writes what
// that instrument can play and can tell it which string to use).
//
//   a profile    a name, bass or guitar, its strings low to high (MIDI notes; the studio names notes
//                as most sample libraries do, middle C = C3, so Djinn Bass 2's "A0" is the studio's
//                A0, note 33), how many frets, the lowest and highest note it may play (none: the
//                lowest string to the top string's last fret), the bend range, the keyswitches, the
//                guitar's velocity layers (IDEAS' VELOCITIES, per instrument), and which composer
//                lanes it's used for (BASS, GUITAR, LEAD: one profile each).
//   keyswitches  a name and a note each; one that forces a string says which (1: the lowest). Only
//                those are used yet (ONLY STRING n in the composer holds that key through the part);
//                the others (slap, pop, palm mute...) are kept for rules to come.
//   kept         in the settings file ("instrumentProfiles"), as XML.
// Djinn Bass 2 is built in (its keyswitch map as the owner's screenshot shows it, the articulations
// the library leaves out not listed) with the owner's tuning, A0 D1 A1 D2 G2.
namespace pr7s
{
    struct Keyswitch
    {
        juce::String name;
        int note = 0;
        int forcesString = 0;   // 1..: holding it plays that string (1 the lowest); 0: an articulation;
                                // -1 (0.62.2): it turns a forced string off again (Shreddage's G-2: its force keys latch)
        // (0.63.0) What an articulation is for in the riffs: power chords (a root alone plays the chord, as
        // Shreddage's power chord articulations) or single notes (the sustain the other notes go back to).
        enum class Use { none, powerChords, singleNotes, tapping, tremolo, harmonics };   // (0.66.0: the last three)
        Use use = Use::none;
        bool operator== (const Keyswitch&) const = default;
    };

    struct InstrumentProfile
    {
        enum class Kind { bass, guitar };
        juce::String name;
        Kind kind = Kind::bass;
        std::vector<int> strings { 28, 33, 38, 43 };   // low to high
        int frets = 24;
        int lowest = -1, highest = -1;                 // -1: from the strings and frets
        int bendRange = 2;
        // (0.63.1, the owner: "the shortest a note should ever be is 1/16th, and even that is very uncommon for
        // chugs") The shortest note in beats (0: any), and how many palm-muted hits (velocity 1-59) may come
        // a sixteenth apart in a row (0: any).
        double shortestNote = 0.25;
        int chugRun = 0;
        std::vector<Keyswitch> keyswitches;
        bool forBass = false, forGuitar = false, forLead = false;
        std::optional<GuitarVelocities> velocities;    // (guitars: the layers this library uses)

        int rangeLow() const;
        int rangeHigh() const;
        GuitarTuning tuning() const;                   // (the name: the profile's)
        int keyFor (int string) const;                 // the key forcing that string (1 the lowest); -1: none
        int releaseKey() const;                        // (0.62.2) the key turning forcing off; -1: none
        int keyUsedFor (Keyswitch::Use) const;         // (0.63.0) the power chords' / single notes' key; -1: none
        bool isKeyswitch (int note) const;
        bool operator== (const InstrumentProfile&) const = default;

        std::unique_ptr<juce::XmlElement> toXml() const;
        static std::optional<InstrumentProfile> fromXml (const juce::XmlElement&);

        static InstrumentProfile djinnBass2();         // the owner's: A0 D1 A1 D2 G2, the map from Djinn's KEYSWITCH MAP
        // (0.62.1) The owner's Shreddage guitars (Jupiter, Rogue): 7-string drop A (A0 E1 A1 D2 G2 B2 E3),
        // 24 frets, the velocity layers given (the split IDEAS' VELOCITIES uses), Sustain C-1 and Power Chords
        // C#-1 (the owner's map); from the Jupiter manual (0.62.2): Force String C-2 (the low A) to F#-2 (the
        // high E), latching, and G-2 turning it off; pitch bend 2.
        static InstrumentProfile shreddage (const juce::String& name, const GuitarVelocities& = {});
        static InstrumentProfile blank (Kind);
    };

    juce::String profilesToText (const std::vector<InstrumentProfile>&);
    std::vector<InstrumentProfile> profilesFromText (const juce::String&);

    // "A0", "C#-1", "Eb3", "33" (middle C = C3, as the studio's MIDI editor); none: unreadable.
    std::optional<int> parseNoteText (const juce::String&);

    // ONLY STRING (0.62.0): every note moved by octaves onto that string (its open note to its last
    // fret); notes played together keep only the lowest (a string plays one note at a time). With a
    // keyswitch for it, that key held through the whole part (`LineNote::keyswitch`), from the part's
    // start, so the library plays the string chosen whether its keyswitches latch or are held. With a
    // `release` key (0.62.2), that key pressed after the part's last note starts: a latching force (Shreddage)
    // doesn't stay on for what's played after the part.
    void playOnString (LineIdea&, const std::vector<int>& strings, int string, int frets, int keyswitch, double length, int release = -1);

    // POWER CHORDS BY KEYSWITCH (0.63.0, the owner asked: Shreddage's recorded power chords sound better than
    // three notes on the single-note articulation). Every power chord (notes starting together: a root, its
    // fifth, and the octaves of either; at least two) becomes its root alone, the longest of their lengths,
    // the loudest of their velocities (its velocity layer still picks the palm mute: the owner's power chord
    // key has the same layers as Sustain). Before it, the power chords' key; before the next single note
    // (or any other chord), the single notes' key: only where the kind changes (the keys latch), and before
    // the first notes always (the instrument may be left in either). A key on the same beat as its notes
    // (Shreddage takes keys first). Notes strummed apart aren't a power chord: they stay as they are.
    void usePowerChordKeys (LineIdea&, int powerChordKey, int singleNoteKey);

    // (0.66.0) The same with the techniques' keys too: a note marked tapping, tremolo or harmonic (`LineNote::
    // technique`) gets that key before it (when the profile has one), the next plain note the single notes' key.
    // A key only where the kind changes, as for power chords. -1: no key for that.
    struct ArticulationKeys
    {
        int power = -1, single = -1, tapping = -1, tremolo = -1, harmonics = -1;
        static ArticulationKeys of (const InstrumentProfile&);
    };
    void useArticulationKeys (LineIdea&, const ArticulationKeys&);

    // PLAYABLE (0.63.1): notes played together count as one hit. A hit starting less than `shortest` beats
    // after the hit before is left out (a little less is allowed: humanize moves notes by a few ms); every
    // note at least `shortest` long (but not past the next hit or the part's `length`). Then palm-muted
    // hits (velocity 1-59) a sixteenth apart: at most `chugRun` in a row, every later one left out until the
    // run is broken. Keyswitch notes are left alone.
    void keepPlayable (LineIdea&, double shortest, int chugRun, double length);
}
