#pragma once

#include "engine/Chords.h"

// Bass lines and melodies to go with chords (0.18.0, the owner asked: "I select a
// chord progression and then I get suggestions for bass lines and also for
// melodies"). Our own rules, not taken from any other product:
//
//   bass      in a bass range (E0 to C2 here, middle C = C3), each chord's root
//             (or its slash bass) in the octave nearest the note before; styles
//             differ in rhythm and in the notes between roots (the fifth, the
//             octave, chord tones, a step into the next chord's root).
//   melody    in a singing range (C3 to A4); chord tones on the strong beats (a
//             bar's first beat and its middle), the key's notes between them,
//             stepping where it can; a key note a half step from a chord tone
//             it doesn't belong to is left out (no rubs); the last note lands on
//             the chord, the root if it's near.
//
// `variation` changes the choices (up or down, which chord tone, the hook's
// shape): the same chords, style and variation always give the same notes.
namespace pr7s
{
    struct LineNote
    {
        double beat = 0.0, length = 0.0;   // from the first chord's start, in beats
        int pitch = 60;
        float velocity = 0.8f;
        bool operator== (const LineNote&) const = default;
    };

    // One chord over its stretch of time, and the key in force there.
    struct ChordSpan
    {
        double start = 0.0, end = 0.0;   // in the song, beats
        Chord chord;
        Key key;
    };

    struct LineIdea
    {
        // (0.19.0: piano chords; guitar rhythm and lead.)
        enum class Part { bass, melody, chords, rhythm, lead };
        Part part = Part::bass;
        juce::String style, description;
        std::vector<LineNote> notes;   // beats from the first span's start
    };

    // The six bass ideas and the six melody ideas for these chords (in order: the styles below).
    std::vector<LineIdea> bassIdeas (const std::vector<ChordSpan>&, double beatsPerBar, int variation = 0);
    std::vector<LineIdea> melodyIdeas (const std::vector<ChordSpan>&, double beatsPerBar, int variation = 0);

    // A guitar's tuning (0.19.0): its open strings, low to high (MIDI notes). The owner plays
    // 7-string guitars in drop A (Shreddage Jupiter and Rogue): the default. Nothing a guitar
    // idea plays goes below the lowest string (sample libraries put keyswitches down there)
    // or above the top string's 24th fret.
    struct GuitarTuning
    {
        juce::String name;
        std::vector<int> strings;
        int lowest() const noexcept { return strings.front(); }
        int highest() const noexcept { return strings.back() + 24; }
        static const std::vector<GuitarTuning>& all();   // the first: 7-string drop A
        static const GuitarTuning& named (const juce::String&);   // (else the first)
    };

    // The guitar ideas' velocities (0.19.1, the owner's library, Shreddage: velocity picks the
    // articulation). 1-59 palm muted, tighter the lower; 60-119 sustained (open); 120-126 rakes
    // (the pick dragged across muted strings into the note); 127 a pinch harmonic (0.19.2, the
    // owner's split). Exact MIDI values: a note's velocity is value / 127 (the engine rounds
    // velocity * 127 back to the same number). On a library without such layers they're only
    // dynamics: muted notes quieter, which suits them too.
    namespace GuitarVelocity
    {
        constexpr int muteTight = 34, muteMid = 44, muteLoose = 54, mutedMost = 59;
        constexpr int sustainSoft = 80, sustain = 96, sustainHard = 110, sustainMost = 119;
        constexpr int rakeLeast = 120, rake = 123, rakeMost = 126;
        constexpr int pinch = 127;
        constexpr float of (int midi) noexcept { return (float) midi / 127.0f; }
        constexpr int midiOf (float velocity) noexcept { return (int) (velocity * 127.0f + 0.5f); }
    }

    // The guitar ideas' velocities as the owner sets them (0.19.3: the VELOCITIES panel, so each
    // layer can be matched by ear; defaults: GuitarVelocity's, Shreddage's layout). Values in
    // between (a softer note in a lick) stay within [sustainSoft, sustainHard]: `open`.
    struct GuitarVelocities
    {
        int muteTight = GuitarVelocity::muteTight, muteMid = GuitarVelocity::muteMid, muteLoose = GuitarVelocity::muteLoose;
        int sustainSoft = GuitarVelocity::sustainSoft, sustain = GuitarVelocity::sustain, sustainHard = GuitarVelocity::sustainHard;
        int rake = GuitarVelocity::rake, pinch = GuitarVelocity::pinch;

        static constexpr int count = 8;
        int get (int index) const noexcept;
        void set (int index, int value) noexcept;   // (1 to 127)
        static juce::String nameOf (int index);    // "PALM MUTE, TIGHT"...
        static juce::String layerOf (int index);   // the range Shreddage puts it in: "1-59"...
        int open (int value) const noexcept { return juce::jlimit (juce::jmin (sustainSoft, sustainHard), juce::jmax (sustainSoft, sustainHard), value); }
        juce::String toString() const;             // "34,44,54,80,96,110,123,127"
        static GuitarVelocities fromString (const juce::String&);   // (anything unreadable: the defaults)
        bool operator== (const GuitarVelocities&) const = default;
    };

    // Piano chords (0.19.0): Block chords, Broken chords, Ballad arpeggio, Stride, Octaves and
    // chords, Pop pulse. The right hand voiced near the chord before; a left hand where it's a style's.
    std::vector<LineIdea> pianoChordIdeas (const std::vector<ChordSpan>&, double beatsPerBar, int variation = 0);
    // Guitar rhythm (0.19.0): Power chords, Octave chords, Palm-muted chugs, Gallop, Pedal-point
    // riff, Open strummed shapes. Power chords are root, fifth and octave (no third: they fit
    // major and minor); the low ones on the lowest strings, as rock and metal riffs are played.
    std::vector<LineIdea> guitarRhythmIdeas (const std::vector<ChordSpan>&, double beatsPerBar, int variation, const GuitarTuning&,
                                              const GuitarVelocities& = {});
    // Guitar lead (0.19.0): Pentatonic box, Blues scale run, Repeating triplet lick, Descending
    // fours, Neoclassical run, Tremolo picking. Every bar's first note on a chord tone (the
    // lick lands on the chord). No bends: parts don't hold pitch bend yet.
    std::vector<LineIdea> guitarLeadIdeas (const std::vector<ChordSpan>&, double beatsPerBar, int variation, const GuitarTuning&,
                                            const GuitarVelocities& = {});

    // Ranges (MIDI notes).
    namespace LineRanges
    {
        constexpr int bassLow = 28, bassHigh = 50, melodyLow = 60, melodyHigh = 81;
    }

    // Notes that can sound over a chord in a key: its chord tones and the key's notes
    // that don't rub a half step against a chord tone outside the key (pitch classes).
    std::vector<int> availableNotes (const Chord&, const Key&);
}
