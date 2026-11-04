#pragma once

#include "engine/Lines.h"

// Better melodies (0.41.0, the owner asked: as their chord tool's melody settings): a melody
// composer with controls, for the chord track's chords. A melody is built from phrases (a few
// bars each); a phrase's material (its rhythm and its shape through the scale: a motif) is made
// once per letter of a form ("AABA") and played again when its letter comes back, bent to the
// chords under it and changed a little (less as REPEAT goes up). Strong beats land on chord
// tones; the notes between walk the key (no note a half step against a chord tone outside it);
// a leap is followed by a step back the other way (as singers do); phrases rise then fall; the
// last phrase ends on the chord's root. Our own rules and names.
namespace pr7s
{
    struct MelodySettings
    {
        float density = 0.45f;      // 0: a note or two a bar .. 1: running sixteenths
        int low = 60, high = 79;    // the range (MIDI: C3 to G4 by default, middle C = C3)
        float leaps = 0.3f;         // 0: steps only .. 1: mostly leaps (thirds to sixths)
        int phraseBars = 2;         // 1, 2 or 4
        float repetition = 0.7f;    // how closely a returning phrase copies the first time (1: exactly)
        float rests = 0.25f;        // room to breathe: notes left out, phrase ends shortened
        bool syncopation = false;   // off-beat pushes

        static constexpr int lowest = 36, highest = 96, leastSpan = 7;
        MelodySettings limited() const;     // in range, low under high by at least `leastSpan`
        juce::String toString() const;
        static MelodySettings fromString (const juce::String&);   // (missing or bad values: the defaults)
        bool operator== (const MelodySettings&) const noexcept;
    };

    struct MelodyForm
    {
        const char* name;       // "Song form"
        const char* letters;    // "AABA": the phrases' material in turn (repeated over the chords)
        const char* description;
        bool rising;            // each return a scale step higher (a sequence)
    };
    const std::vector<MelodyForm>& melodyForms();   // the six the MELODY tab offers

    // One melody in a form under these chords (Part::tune); repeatable for a seed.
    LineIdea composeMelody (const std::vector<ChordSpan>&, double beatsPerBar, const MelodySettings&, const MelodyForm&, juce::uint32 seed);
    // The six forms' melodies (the variation changes the seeds: MORE IDEAS).
    std::vector<LineIdea> composedMelodies (const std::vector<ChordSpan>&, double beatsPerBar, const MelodySettings&, int variation);
}
