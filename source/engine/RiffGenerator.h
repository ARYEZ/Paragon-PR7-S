#pragma once

#include "engine/Riffs.h"
#include "engine/DrumGenerator.h"

#include <string>

// THE RIFF GENERATOR (0.85.0, the owner: "every pattern is basically the same ... if I'm looking for a
// tritone chug I can hit generate a thousand times and I'll basically just get the same riff"). The
// composer's guitar had six written riffs a style and ROLL only nudged a few of their notes; now each
// riff is built from scratch by rules (ours), as the drums' beats have been since 0.55.0.
//
// How a riff is built:
//   - A bar is a row of sixteenths (12 or 14 in other signatures), filled from the left by CELLS, the
//     building blocks of a metal or rock rhythm part: single chugs, chug runs, gallops, rests, power
//     chords held, muted dyads, single notes a half step / minor third / fourth / flat fifth / fifth /
//     flat seventh over the low note, tritone dyads, half-step creeps, high stabs (groove's tritone with
//     vibrato, bent whole step, unison), octave shapes, high chords, pinch squeals, stops.
//   - Each cell has a weight: the STYLE's (thrash gallops and chromatic notes, nu-metal bounce and
//     stops, metalcore breakdown chugs and space, groove's stabs, rock's root and fifth, shred's driving
//     dyads), times the FLAVOUR's (below), times the SECTION's (a verse tighter and muted, a chorus open
//     and ringing, a breakdown half-time with space, a bridge creeping, an intro sparse, an ending on
//     held chords), times how well it sits where it would start (a long chord on the beat; a style's
//     taste for syncopation; rests on the weak sixteenths), and BUSY (more or fewer rests and runs).
//   - The bar's first sixteenth is always a hit.
//   - The riff is 1, 2 or 4 bars (RIFF SETTINGS' LENGTH), shaped like a riff: bar A; B keeps A's first
//     half and answers it with a new second half; four bars play A B A C, C ending in a turnaround.
//   - The bars are written in Riffs.cpp's riff characters and played by `riffFromBars`, as the written
//     riffs are (the same voicings, velocity layers and stab effects), then RIFF SETTINGS shape them.
//
// The FLAVOUR leans the choice towards a sound without fixing the riff: every seed gives another riff
// of that flavour. ANY leans towards a different flavour each time.
namespace pr7s
{
    enum class RiffFlavour { any, chugs, gallop, tritone, creep, stabs, octaves, stopStart, openChords };
    const std::vector<RiffFlavour>& riffFlavours();
    juce::String riffFlavourName (RiffFlavour);          // "ANY", "CHUGS", "GALLOP", "TRITONE CHUG", ...
    juce::String riffFlavourDescription (RiffFlavour);
    RiffFlavour riffFlavourNamed (const juce::String&);   // (else any)

    struct RiffRecipe
    {
        GuitarStyle style {};
        RiffFlavour flavour = RiffFlavour::any;
        DrumSection section = DrumSection::verse;
        int bars = 2;            // 1, 2 or 4
        float busy = 0.5f;       // 0 sparse .. 0.5 the style's own .. 1 busy
        juce::uint32 seed = 1;
    };

    // The riff's own bars (`bars` of them), each `stepsPerBar` characters (Riffs.cpp's riff characters).
    std::vector<std::string> generateRiffBars (const RiffRecipe&, int stepsPerBar = 16);
    // What makes two riffs the same riff (their bars joined), to tell repeats apart.
    juce::String riffFingerprint (const std::vector<std::string>& bars);
    // The riff played over the chords (bar k plays the riff's bar k % bars), named for the flavour and
    // described by what it's made of.
    LineIdea generateRiff (const RiffRecipe&, const std::vector<ChordSpan>&, double beatsPerBar, const GuitarTuning&, const GuitarVelocities&);
    // Its steps per bar in a signature (a sixteenth each: 16 in 4/4, 12 in 3/4, 14 in 7/8).
    int riffStepsPerBar (double beatsPerBar);
}
