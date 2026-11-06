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
        bool keyswitch = false;            // (0.62.0) a library's keyswitch held, not a note heard (not drawn)
        // (0.64.0) What the hand does to the note while it rings, written into the part as controllers when it
        // becomes one (Studio): vibrato (the mod wheel, CC 1, rising after the attack: Shreddage's vibrato
        // controller) or a bend up a whole step (pitch bend, by the instrument's bend range; back to none
        // as it ends). A pitch bend bends the whole channel, so everything starting with the note bends too.
        // `ring`: no controller, but a stab left ringing (the riff settings never palm mute or drop one).
        enum class Effect { none, vibrato, bend, ring };
        Effect effect = Effect::none;
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
        enum class Part { bass, melody, chords, rhythm, lead, pattern, tune, drums, groove };   // (0.54.0: drums, groove: the DRUMS tab's generated beats and library grooves)   // (0.39.0: patterns; 0.41.0: tune, the MELODY tab's composed melodies)
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
    // (0.53.0) `style`: rock is these six; the other styles' are Riffs.h's. RIFF SETTINGS shape every style's.
    enum class GuitarStyle;
    struct RiffSettings;
    std::vector<LineIdea> guitarRhythmIdeas (const std::vector<ChordSpan>&, double beatsPerBar, int variation, const GuitarTuning&,
                                              const GuitarVelocities& = {});
    std::vector<LineIdea> guitarRhythmIdeas (const std::vector<ChordSpan>&, double beatsPerBar, int variation, const GuitarTuning&,
                                              const GuitarVelocities&, GuitarStyle, const RiffSettings&);
    // Guitar lead (0.19.0): Pentatonic box, Blues scale run, Repeating triplet lick, Descending
    // fours, Neoclassical run, Tremolo picking. Every bar's first note on a chord tone (the
    // lick lands on the chord). No bends: parts don't hold pitch bend yet.
    std::vector<LineIdea> guitarLeadIdeas (const std::vector<ChordSpan>&, double beatsPerBar, int variation, const GuitarTuning&,
                                            const GuitarVelocities& = {});
    std::vector<LineIdea> guitarLeadIdeas (const std::vector<ChordSpan>&, double beatsPerBar, int variation, const GuitarTuning&,
                                            const GuitarVelocities&, GuitarStyle);

    // Humanize (0.20.0, the owner asked): small, controlled differences so a part sounds played,
    // not programmed. Our rules:
    //   timing   each note early or late by up to 12 ms x amount (a triangle: small moves likely,
    //            big ones rare), in milliseconds at the tempo there, not in beats; a bar's first
    //            beat moves least (x0.3), the other beats x0.6, everything between x1 (players lock
    //            the "one"). Notes starting together (a chord, a strum: within 0.11 beats) move
    //            together, so chords stay chords; a strum's spread varies by up to 25% x amount.
    //   feel     a lean for the whole part, in ms (negative: ahead of the beat), added as is.
    //   velocity up to 10 x amount harder or softer, kept inside the layer it started in
    //            (`layers`: a guitar library's 1-59 / 60-119 / 120-126 / 127, so a palm mute stays
    //            a palm mute and a pinch harmonic stays 127).
    //   length   up to 12% x amount longer or shorter (at least 0.02 beats).
    // `seed` makes it repeatable: the same notes and seed give the same result (what HEAR plays is
    // what USE IT adds). Amount 0 changes nothing.
    struct HumanizeOptions
    {
        float amount = 0.0f;         // 0 to 1
        double feelMs = 0.0;         // the whole part's lean (negative: ahead)
        double bpm = 120.0;          // the tempo there (ms to beats)
        double beatsPerBar = 4.0;
        double originBeat = 0.0;     // the song beat the notes' beat 0 is at (for bar and beat positions)
        std::vector<std::pair<int, int>> layers;   // velocity layers (inclusive); none: 1-127
        juce::uint32 seed = 1;
    };
    std::vector<LineNote> humanize (std::vector<LineNote>, const HumanizeOptions&);
    namespace HumanizeLimits
    {
        constexpr double timingMs = 12.0, velocity = 10.0, length = 0.12, strum = 0.25, together = 0.11;
        constexpr double feelAheadMs = -3.0, feelBehindMs = 5.0;   // IDEAS' FEEL at amount 1: rhythm and bass push, leads and melodies lay back
    }
    // A guitar library's velocity layers as the owner described Shreddage's.
    const std::vector<std::pair<int, int>>& guitarLayers();

    // Ranges (MIDI notes).
    namespace LineRanges
    {
        constexpr int bassLow = 28, bassHigh = 50, melodyLow = 60, melodyHigh = 81;
    }

    // Notes that can sound over a chord in a key: its chord tones and the key's notes
    // that don't rub a half step against a chord tone outside the key (pitch classes).
    std::vector<int> availableNotes (const Chord&, const Key&);
}
