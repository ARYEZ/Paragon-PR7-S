#pragma once

#include "engine/Lines.h"
#include "engine/Riffs.h"

// TAKES (0.61.0, the owner asked: "hit ROLL and get a DIFFERENT version of the approach for this
// progression"; the approach itself is only ever changed by choosing it in the style menu).
//
// IDEAS' bass and piano ideas each have one written way of playing (MORE IDEAS only turns them
// round), so the composer's bass and chords lanes are made here instead: each approach is a set
// of rules (where its notes may land in a bar, which notes it may use, how it goes from one chord
// to the next), and a take is one draw within those rules from its seed. Our own rules.
//
//   the bar      a take draws a one-bar rhythm cell for its approach (a hit at the bar's start
//                always), a second cell a hit or two different, and a fill cell (more notes in
//                the last beat, leading into the next chord). Bars go A, A or B, A, fill in each
//                four-bar phrase, so a take repeats like a part a player would play, not a new
//                bar every bar.
//   the notes    each hit of a cell has a role (root, fifth, octave, seventh, a lead-in...) kept
//                for every bar, realised on each chord. Walking bass walks: through the chord and
//                the key towards the next chord's root, its direction and its approach note
//                (a half step under or over, a scale step, the fifth) drawn per take.
//   BUSY         0 sparse .. 0.5 the approach as usual .. 1 busy: how many hits a cell draws
//                (Driving eighths: eighths left out below, sixteenths added above).
//   LOW STRING   the bass's roots on its lowest string (from the open string to the 12th fret),
//                fifths under the root where they fit; a riff's chords and notes folded down
//                onto it where they can be (guitar).
//   range        nothing outside [lowest, highest].
// The same seed, approach, chords and settings always give the same notes.
namespace pr7s
{
    struct TakeOptions
    {
        juce::uint32 seed = 1;
        float busy = 0.5f;           // 0 sparse, 0.5 the approach's usual, 1 busy
        bool lowString = false;
        int lowest = 28, highest = 52;   // the instrument's range: its lowest string and the top it may reach
    };

    // The bass's six approaches (the names IDEAS' bass ideas have) and the piano's six.
    const juce::StringArray& bassApproaches();
    const juce::StringArray& pianoApproaches();
    LineIdea bassTake (int approach, const std::vector<ChordSpan>&, double beatsPerBar, const TakeOptions&);
    LineIdea pianoTake (int approach, const std::vector<ChordSpan>&, double beatsPerBar, const TakeOptions&);

    // A rhythm-guitar idea made a take of: hits on the chord's root moved to the octave, the fifth
    // (or, in the metal styles, the flat second or minor third) at a few places of the bar, and an
    // accent or two between palm muted and open; the same choices at the same place in every bar
    // (the fill bars of each four-bar phrase their own), so the riff still repeats. LOW STRING
    // folds what's above the lowest string's 12th fret down an octave where it fits.
    void riffTake (LineIdea&, const std::vector<ChordSpan>&, double beatsPerBar, const TakeOptions&, GuitarStyle);
    // A lead line thinned when `busy` is under 0.5 (notes off the beat joined to the note before).
    void thinTake (LineIdea&, double beatsPerBar, const TakeOptions&);
    // Notes outside [lowest, highest] moved by octaves into it.
    void keepInRange (LineIdea&, int lowest, int highest);

    // The bass's tunings (0.61.0: LOW STRING and the bass's range), low to high.
    const std::vector<GuitarTuning>& bassTunings();
    const GuitarTuning& bassTuningNamed (const juce::String&);   // (unknown: the first, 4-string standard)
    // The range a bass in this tuning plays: the lowest string to two octaves over it.
    inline int bassTop (const GuitarTuning& t) { return t.lowest() + 24; }
}
