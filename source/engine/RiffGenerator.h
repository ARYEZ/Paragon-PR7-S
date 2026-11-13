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
// of that flavour. ANY leans towards a different flavour each time (not DRIVING EIGHTHS: chosen on purpose).
//
// DRIVING EIGHTHS (0.87.0, the owner: "the rhythm guitar's version of the driving 8th notes riffs ... a type
// of chorus specifically"; their example Killswitch Engage's This Fire Burns, its chorus: the shape learned
// from the tab, nothing of it copied): power chords (root, fifth, octave) on every eighth note, left to
// ring, changing on the bar line; a palm-muted stretch (muted power chords, still every eighth) for
// contrast; and a walk into the next chord: the last beats of a bar before a chord change as single notes
// a whole step and a half step over (or under) the next chord's root. Then (the owner's three more:
// Killswitch Engage's The End of Heartache, Linkin Park's Faint, Within Temptation's Faster; their
// choruses' shapes, nothing copied) every riff takes one of five chorus shapes: the wall then a muted
// stretch and a walk; all open with the power chord itself stepping into the next chord; power chords
// moving inside the bar in groups of eighths (3+3+2...); a punch bar (ringing hits with space) and a
// bar half and half; or the plain wall. And (the owner's last three: Bullet For My Valentine's Tears Don't
// Fall, All That Remains' This Calling, The Plot In You's Left Behind) full chords strummed on every eighth
// with the top voice moving and dead-string scratches; big chords rung a bar each; a chord a bar with
// the last bar split in half. And (In Flames' Come Clarity, its pre-chorus: the chorus has no guitar in that
// file; As I Lay Dying's Parallels, its double-time chorus) the chord picked a note an eighth, or the muted
// root as a pedal under high notes. A verse gets the muted stretch first, a breakdown the same in
// quarter notes.
//
// DRONING (0.88.0, the owner: "the second kind of chorus ... the rhythm guitar especially plays usually open
// whole notes that ring out across each bar in the progression and most of the rhythmic or interesting stuff
// comes from the lead guitar, the bass, the keyboards and the drums"; their examples Dayseeker's Without Me,
// Thirty Seconds To Mars' Capricorn, Korn's Clown, Spiritbox's Circle With Me, read for their shapes only):
// a chord struck on the bar line and left to ring the whole bar (full chords with the third, or power
// chords), sometimes struck again halfway, sometimes the power chord held while a top note moves over it
// (Capricorn), sometimes the shape sliding down a half step at a time over the bar (Clown). Few hits, long
// notes: the space is for the other instruments. A verse rings power chords only.
namespace pr7s
{
    // (0.95.0, the owner's STACCATO CHUG verses: Sevendust's Black and Waffle, Disturbed's Stricken, read for their shapes
// only) pedalStabs: the low string chugged in sixteenths, a power chord every third sixteenth (the stabs run in threes
// across the bar), the stab's degree climbing every bar or two; chugThrees: a rest and two chugs over and over (threes
// across the bar), a chromatic walk up on the bar's last sixteenths, every fourth bar power chords climbing; burstSilence:
// a hit, a gap, a burst of four (the last held), a pair, then silence, a dissonant cluster stab every other bar.
// (0.96.0, the owner's SPLIT CHUG verses: Evanescence's Going Under and Call Me When You're Sober, Linkin Park's
// Papercut, shapes only; the bar split into a played part and a part left to the vocal) secondHalfStabs: a chord on 1,
// silence, then short stabs through the bar's second half; firstHalfBurst: a burst of chugs and power chords stepping
// down in the first half, the second half silent, a pickup every fourth bar; hitPush: a chord on 1 and the chord a whole
// step under the next one pushed in on the last sixteenths, the rest of the bar left open.
enum class RiffFlavour { any, chugs, gallop, tritone, creep, stabs, octaves, stopStart, openChords, drivingEighths, droning,
                         pedalStabs, chugThrees, burstSilence, secondHalfStabs, firstHalfBurst, hitPush,
                         // (0.99.0, the owner's Another Life chorus with Faster's syncopated bar, shapes only) the driving
                         // eighths opened by stabs (chord, chord, silence), a bar rung across the beats, or the change pushed
                         // in an eighth early ('_': the next chord's power chord on the bar's last eighth). Made by the
                         // DRIVING EIGHTHS generator with its own shapes.
                         syncopatedDrive };
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
        // (0.94.0) A section type's guitar approach (sectionApproaches: DRONING's rung whole / pushed / moving top /
        // struck twice / sliding; 0.98.0: DRIVING EIGHTHS' straight / syncopated third bar / open and muted in turn /
        // slides into the changes / strummed / moving power chords); -1: any, drawn by the seed as before.
        int approach = -1;
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
