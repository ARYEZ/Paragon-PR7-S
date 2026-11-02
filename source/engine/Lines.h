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
        enum class Part { bass, melody };
        Part part = Part::bass;
        juce::String style, description;
        std::vector<LineNote> notes;   // beats from the first span's start
    };

    // The six bass ideas and the six melody ideas for these chords (in order: the styles below).
    std::vector<LineIdea> bassIdeas (const std::vector<ChordSpan>&, double beatsPerBar, int variation = 0);
    std::vector<LineIdea> melodyIdeas (const std::vector<ChordSpan>&, double beatsPerBar, int variation = 0);

    // Ranges (MIDI notes).
    namespace LineRanges
    {
        constexpr int bassLow = 28, bassHigh = 50, melodyLow = 60, melodyHigh = 81;
    }

    // Notes that can sound over a chord in a key: its chord tones and the key's notes
    // that don't rub a half step against a chord tone outside the key (pitch classes).
    std::vector<int> availableNotes (const Chord&, const Key&);
}
