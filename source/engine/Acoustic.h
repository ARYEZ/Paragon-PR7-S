#pragma once

#include "engine/Lines.h"

#include <optional>
#include <vector>

// THE ACOUSTIC GUITAR LANE (0.193.0, from the owner's song files: strummed and picked steel and nylon parts, read for
// their shapes only). Our own rules where the files were silent (they had no strum direction or dynamics):
//   voicings  open chords from the root's string: root, fifth, octave, third, fifth, octave (5-6 notes, E2-E5), with an
//             open string's colour on top where it fits (the ninth; on a major chord a major seventh or a sixth);
//             "power" = root, fifth, octave (no third); "low half" = the lowest three notes; "top" = the highest two or
//             three; a "high shape" five notes with the top in 69-77 (as a capo shape would ring).
//   strums    spread 5-15 ms: down strokes low to high, up strokes high to low. On the beat down; the and of a beat up
//             (in a sixteenth feel down, its sixteenths between up). Velocities: beat downs ~100, off-eighth ups ~80,
//             sixteenth ups ~65, a partial top-string strum ~70, soft partials 30-50, accents ~110.
//   rings     a strum rings until the next one starts (cut there); picked notes ring until the same note comes again,
//             the chord changes, or a bar has gone.
// SECOND ACOUSTIC: what the lane's second track plays when the lane is on two tracks or more (ARCHITECT TRACKS). Its
// notes are written as `harmony` notes, so Studio::doubledCopy puts them on the second track (on one track both play
// there). LOOSER COPY writes none: the second track plays a looser copy of the first, as every lane's copies do.
namespace pr7s
{
    const juce::StringArray& acousticGuitarStyles();   // the lane's styles, in the style menu's order
    const juce::StringArray& secondAcousticParts();    // SECOND ACOUSTIC's parts (0: LOOSER COPY)

    struct AcousticRequest
    {
        int style = 0, second = 0, variation = 0;
        double beatsPerBar = 4.0, bpm = 120.0;
        std::optional<Chord> nextChord;          // the chord after the section (none: its first chord)
        // The guitar lane's hits and accents (idea beats), for the styles that follow the riff.
        std::vector<double> riffHits, riffAccents;
    };

    // The lane's part for these chords (beats from the first span's start; Part::acoustic).
    LineIdea acousticGuitarPart (const std::vector<ChordSpan>&, const AcousticRequest&);

    // The voicings (exposed for the tests and the shared options).
    namespace AcousticVoicing
    {
        constexpr int lowest = 40, highest = 76;   // E2-E5
        std::vector<int> open (const Chord&, bool colour = true);   // 5-6 notes from the root's string
        std::vector<int> power (const Chord&, int notes = 3);       // root, fifth, octave (5: and fifth, octave)
        std::vector<int> high (const Chord&);                       // five notes, the top in 69-77
        std::vector<int> triad (const Chord&, int low = 55);        // a close three-note triad from `low` up
        int root (const Chord&);                                    // the chord's root (or slash bass) in 40-51
    }
}
