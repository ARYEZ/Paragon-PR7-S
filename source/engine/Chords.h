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
    // A key (0.15.0, the owner: "the chords must be accurate"): its tonic spelled
    // (a letter and an accidental) and major or minor. Notes are spelled from
    // it: in Eb minor the sixth is Cb, not B; in F# major the seventh is E#.
    struct Key
    {
        int letter = 0;       // 0 C, 1 D, 2 E, 3 F, 4 G, 5 A, 6 B
        int accidental = 0;   // -1 flat, 0, 1 sharp
        bool minor = false;

        bool operator== (const Key& o) const noexcept { return letter == o.letter && accidental == o.accidental && minor == o.minor; }
        bool operator!= (const Key& o) const noexcept { return ! (*this == o); }

        int pitchClass() const noexcept;
        juce::String tonicName() const;            // "Eb"
        juce::String name() const;                 // "Eb minor"
        int signature() const noexcept;            // sharps (+) or flats (-) in its key signature
        std::vector<int> scale() const;            // its seven pitch classes (natural minor for minor keys)
        // The 30 keys a key signature can have: 15 major, 15 minor, round the circle of fifths.
        static const std::vector<Key>& all();
        // A pitch class's usual key (the spelling with fewer accidentals; ties: flats if asked).
        static Key fromPitchClass (int pitchClass, bool minor, bool flats = false);

        struct Spelled
        {
            int letter = 0, accidental = 0;
            juce::String text() const;             // "Cb", "F#", "Bbb"
        };
        static int naturalOf (int letter) noexcept;
        static Spelled spellAs (int pitchClass, int letter);   // the pitch class written on that letter
        // A pitch class in this key: its scale degree's letter if it's in the
        // scale; else flat in flat keys (and C major / A minor), sharp in sharp keys.
        Spelled spell (int pitchClass) const;
    };

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

        // In a key (0.15.0): spelled as the key spells it ("Cb", "Bb7", "Ebm/Gb"), its notes
        // spelled from its root's letter (Bb D F, never A# D F), its numeral by the key's own
        // scale (minor: i ii° III iv v VI VII; a chord outside it: bVII, #iv°...).
        juce::String name (const Key&) const;
        juce::StringArray noteNames (const Key&) const;
        juce::String romanName (const Key&) const;
        juce::String nashvilleName (const Key&) const;
        // Is it one of the key's own chords (or the minor key's major V, or its vii° on the raised seventh)?
        bool isInKey (const Key&) const;

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

    // The voicings by name (0.15.0): close position (root position, 1st, 2nd,
    // 3rd inversion, in two octaves), then open (drop 2: the second voice from
    // the top an octave down) and spread (root, fifth, tenth: a pianist's left and
    // right hand). Every one holds exactly the chord's notes. voicingsFor gives
    // the same notes in the same order.
    struct Voicing
    {
        std::vector<int> notes;
        juce::String kind;   // "Root position", "1st inversion", "Open (drop 2)", "Spread"...
        bool close = true;   // (adaptive voicing chooses among the close ones)
    };
    std::vector<Voicing> namedVoicings (const Chord&, int low = 48);
    juce::String notesText (const std::vector<int>& notes, const Chord&, const Key&);   // "Eb3 Gb3 Bb3" (middle C = C3)

    // Every chord a key offers (0.15.0): its seven chords, their sevenths, and in a
    // minor key the major V, V7 and the vii° of the raised seventh (harmonic minor).
    std::vector<Chord> keyChords (const Key&);

    // What could come next (0.15.0, the owner's scoring): each candidate scored 1-99 from
    // its harmonic function in the key (V to I strongest, then IV-V, ii-V, vi-IV...),
    // root movement round the circle of fifths (down a fifth best), notes in common, and
    // staying in the key (borrowed chords and secondary dominants score lower: adventurous).
    // Our weighting of standard rules, best first.
    struct Suggestion
    {
        Chord chord;
        int score = 0;
        juce::String reason;
        bool inKey = true;
    };
    std::vector<Suggestion> suggestNext (const Chord& from, const Key&);

    // The ten most popular chord progressions in a key (0.17.2, the owner asked), the
    // best known first, as chords in that key. Our list from common pop, rock, R&B,
    // jazz and worship practice (not counted from data): major I-V-vi-IV, I-vi-IV-V,
    // I-IV-V-I, vi-IV-I-V, ii-V7-I, I-vi-ii-V, the Canon, I-bVII-IV-I, I-IV-vi-V,
    // I-IV-iv-I; minor i-VI-III-VII, i-VII-VI-VII, i-iv-v-i, i-iv-V-i, the Andalusian
    // i-VII-VI-V, i-VI-VII-i, ii°-V7-i, i-III-VII-VI, i-VI-iv-V, i-IV (Dorian).
    struct Progression { juce::String name, style; std::vector<Chord> chords; };
    std::vector<Progression> popularProgressions (const Key&);

    // The chord track (0.16.0, the owner's choice; the manual's "Chord Functions"):
    // chord events (each until the next starts, as on a lead sheet) and scale
    // events (the key in force from each one on: worked out from the chords
    // unless set by hand).
    struct ChordEvent
    {
        double beat = 0.0;
        Chord chord;
        bool operator== (const ChordEvent& o) const noexcept { return beat == o.beat && chord == o.chord; }
    };
    struct ScaleEvent
    {
        double beat = 0.0;
        Key key;
        bool operator== (const ScaleEvent& o) const noexcept { return beat == o.beat && key == o.key; }
    };
    struct ChordTrack
    {
        bool shown = false;
        std::vector<ChordEvent> events;   // by beat
        std::vector<ScaleEvent> scales;   // by beat
        bool autoScales = true;           // the scales follow the chords (off once one is set by hand)

        bool operator== (const ChordTrack& o) const noexcept
        {
            return shown == o.shown && events == o.events && scales == o.scales && autoScales == o.autoScales;
        }
        int indexAt (double beat) const noexcept;               // the event sounding at `beat`, -1 if none yet
        double endOf (int index, double lastLength) const;       // the next one's start (the last: + lastLength)
        Key keyAt (double beat, const Key& fallback) const;      // the scale event in force
        void sort();
        std::unique_ptr<juce::XmlElement> toXml() const;
        static ChordTrack fromXml (const juce::XmlElement&);
    };

    // The keys the chords suggest (0.16.0): from the first chord on, the key holding
    // most of the next few chords (the one before kept unless another holds more;
    // `start` first if it holds as many).
    std::vector<ScaleEvent> scalesFromChords (const std::vector<ChordEvent>&, const Key& start);

    // Chords heard in notes (0.16.0, the manual's Create Chord Events): beat by beat
    // (`step`), each note weighed by how long and loud it sounds then; the chord that
    // holds the most of it and least else, plain triads before tensions; the same chord
    // in a row merged. A bass note of the chord under it makes an inversion (C/E).
    struct HeardNote { double start, end; int pitch; float velocity; };
    std::vector<ChordEvent> chordsFromNotes (const std::vector<HeardNote>&, double from, double to, double step = 1.0);

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
        // The key (0.15.0): rootKey is its tonic's pitch class; keyLetter its spelling
        // (-1: worked out from rootKey and flats, as songs from 0.14 have it).
        bool minor = false;
        int keyLetter = -1;
        Key key() const;
        void setKey (const Key&);
        // The key's chords on the pads (keyboard layout: its seven on the white keys;
        // on the black keys, major: I7 ii7 IV7 V7 vi7; minor: i7 III7 iv7, the major V
        // next to v, V7). Locked pads keep theirs.
        void fillFromKey (const Key&);
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
