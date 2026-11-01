#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <map>
#include <optional>
#include <set>

// Chord pads (0.14.0, the owner asked: as the other DAW has them; the manual's
// "Using Chord Pads"). The data (chords, pads, the root key, the layout), the
// voicings (our own rules: the manual's voicing libraries aren't published),
// chord names, chord recognition (Assign from MIDI Input), the scale presets,
// and the player that turns a pad pressed into notes.
namespace pr7s
{
    struct Chord
    {
        enum class Type { major, minor, diminished, augmented, sus2, sus4 };
        enum class Tension { none, six, seven, maj7, dim7, add9, nine, maj9, sixNine, eleven, thirteen, sevenFlat9, sevenSharp9, sevenSharp11 };

        int root = 0;        // pitch class, 0 = C
        Type type = Type::major;
        Tension tension = Tension::none;
        int bass = -1;       // a pitch class under the chord (a slash chord), -1: none

        bool operator== (const Chord& o) const noexcept { return root == o.root && type == o.type && tension == o.tension && bass == o.bass; }
        bool operator!= (const Chord& o) const noexcept { return ! (*this == o); }

        // The chord's tones as intervals above the root (sorted, the root first).
        std::vector<int> intervals() const;
        // Its pitch classes (0-11), the root first.
        std::vector<int> pitchClasses() const;
        Chord transposed (int semitones) const;

        static juce::String noteName (int pitchClass, bool flats);
        static juce::String typeName (Type);
        static juce::String tensionName (Tension, Type);
        static const std::vector<Tension>& allTensions();
        static const std::vector<Type>& allTypes();

        // "C", "Dm7", "Bdim", "F/A", "G7sus4"...
        juce::String name (bool flats = false) const;
        // Relative to the root key: "ii7", "V", "bVII"; and the Nashville numbers: "2m7", "5", "b7".
        juce::String romanName (int rootKey) const;
        juce::String nashvilleName (int rootKey) const;

        // More or fewer tensions: the next step along this type's ladder.
        Chord withTensionStep (int direction) const;

        juce::String toText() const;                          // for the song file: "0 major seven -1"
        static std::optional<Chord> fromText (const juce::String&);
    };

    // A chord recognised from notes played (the manual's Assign from MIDI
    // Input); none if the notes make no chord we know.
    std::optional<Chord> recogniseChord (const std::vector<int>& midiNotes);

    // The voicings a chord can take: close-position inversions with their
    // lowest note from `low` up an octave and a half (a slash chord's bass an
    // octave under them), lowest first.
    std::vector<std::vector<int>> voicingsFor (const Chord&, int low = 48);
    // The one that moves least from `previous` (smallest total distance each
    // way), staying near `reference` if there is one (the manual's Adaptive
    // Voicing and Adaptive Voicing Reference); an index into voicingsFor.
    int closestVoicing (const std::vector<std::vector<int>>& voicings, const std::vector<int>& previous, const std::vector<int>& reference);

    struct ChordPad
    {
        std::optional<Chord> chord;
        int voicing = -1;            // an index into voicingsFor (-1: the middle one)
        bool adaptive = true;        // the voicing chosen by voice leading when played
        bool locked = false;         // no editing, no remote changes
        std::vector<int> notes;      // a voicing as it was played in (Assign from MIDI Input); empty: none

        bool operator== (const ChordPad& o) const noexcept
        {
            return chord == o.chord && voicing == o.voicing && adaptive == o.adaptive && locked == o.locked && notes == o.notes;
        }
    };

    struct ChordPads
    {
        enum class Layout { keyboard, grid };
        enum class Display { noteName, roman, nashville, none };
        enum class Overlap { holdFirst, legato, stopFirst };

        std::vector<ChordPad> pads = std::vector<ChordPad> (12);
        int rootKey = 0;             // pitch class
        bool flats = false;          // the manual's Enh. Shift
        int adaptiveReference = -1;  // a pad index, or -1
        int octaveOffset = 0;        // the Basic player's
        Overlap overlap = Overlap::legato;
        // Display (the manual's Chord Pads Display Settings pane).
        Layout layout = Layout::keyboard;
        int startNote = 0;           // keyboard layout: the first pad's key (pitch class)
        int rows = 2, columns = 8;   // grid layout
        Display primary = Display::noteName, secondary = Display::roman;
        bool showVoicing = true;
        // Remote (the manual's Pad Remote Control): the keys that trigger the pads, C1 to B1.
        int triggerStart = 36, triggerEnd = 47;
        bool latch = false;

        bool operator== (const ChordPads& o) const noexcept;
        // A new song's: the major scale's chords in C (as a preset would fill them; our choice).
        static ChordPads defaults();

        juce::String displayName (const Chord&, Display) const;
        // What the pad would play now (its voicing, or the adaptive choice after `previous`).
        std::vector<int> notesFor (int pad, const std::vector<int>& previous) const;
        int padForKey (int midiNote) const noexcept;   // -1 if not a trigger key

        std::unique_ptr<juce::XmlElement> toXml() const;
        static ChordPads fromXml (const juce::XmlElement&);

        // The manual's scale presets in the root key (our arrangement of them): on
        // the keyboard layout the scale's triads on the white keys, its sevenths
        // on I, II, IV, V and VI on the black keys; on a grid, in that order.
        enum class Scale { major, naturalMinor, dorian, phrygian, lydian, mixolydian, locrian };
        static juce::StringArray scaleNames();
        void fillFromScale (Scale, int key);
    };

    // Plays the pads: a pad pressed sends its chord's notes, released ends them,
    // as the overlap mode says when another comes before (the manual's Hold
    // First, Legato, Stop First). Called from the screen's thread (a click) and
    // from the MIDI devices' thread (a trigger key): it keeps its own lock.
    class PadPlayer
    {
    public:
        std::function<void (const juce::MidiMessage&)> send;   // where the notes go (the live track's input)
        std::function<void (int pad, bool on)> onPadChanged;   // (any thread: the screen asks for an update)
        std::function<void (int note, bool on)> onOtherKey;   // (the devices' thread) a key outside the trigger range

        void setPads (const ChordPads&);
        void press (int pad, juce::uint8 velocity, int channel = 1);
        void release (int pad, int channel = 1);
        void releaseAll (int channel = 1);
        // A MIDI message from a device: true if it was a trigger key (taken here).
        bool handle (const juce::MidiMessage&);

        std::vector<int> getSounding() const;     // the notes sounding now
        std::vector<int> getLastChord() const;    // the last chord played (adaptive voicing's previous)
        bool isPressed (int pad) const;
        void setLastChord (std::vector<int>);

    private:
        mutable juce::CriticalSection lock;
        ChordPads pads;
        std::vector<int> held;                   // pads pressed, oldest first
        std::map<int, std::vector<int>> padNotes; // the notes each pressed pad sounds
        std::vector<int> sounding, lastChord;
        std::set<int> latched;
        void noteOn (int note, juce::uint8 velocity, int channel);
        bool releaseHeld (int pad, int channel);   // (under the lock)
        void noteOff (int note, int channel);
    };
}
