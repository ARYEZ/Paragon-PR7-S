#pragma once

#include "engine/Lines.h"

// SOLO STYLES (0.66.0, the owner asked: solos written in the manner of players they love, their techniques
// and habits, never their solos). The research is in docs/solo-styles (a technique sheet per player, sourced
// from guitar lessons and interviews); the app shows only our own names for the styles (the owner's choice).
//
// A solo here is built around a MOTIF, as a player builds one: a short phrase stated, answered (the same
// rhythm, a new ending, often higher), stated again higher, then developed, with the style's signature passage
// (a burst) in the later phrases, building to a climax and ending on the last chord's root. On top of the notes:
//   - note choice by the chord: the style's scale (minor pentatonic and its added notes, major pentatonic over
//     major chords where the style does that), chord tones on the downbeats and at phrase ends;
//   - density and space: from the style's opening density to its peak, rests between phrases;
//   - register: climbing through the solo;
//   - bends (written under their note and bent up into it, by the style's sizes, bigger ones nearer the peak),
//     vibrato (the style's depth), a whammy dive at the end (Tap & Dive);
//   - techniques: tapping, tremolo, harmonics (keys for the instrument when its INSTRUMENTS profile has them),
//     legato (overlapping notes: hammer-ons and pull-offs on a sampled guitar), swing.
// Nothing faster than `shortest` beats (the instrument's SHORTEST NOTE; 0: as the style plays it).
namespace pr7s
{
    enum class SoloStyle : int { automatic, soaringBends, rockSwagger, tapAndDive, bluesHammer };
    const std::vector<SoloStyle>& soloStyles();      // automatic first
    juce::String soloStyleName (SoloStyle);          // "BY STYLE", "SOARING BENDS", "ROCK 'N' ROLL SWAGGER", "TAP & DIVE", "BLUES HAMMER"
    SoloStyle soloStyleNamed (const juce::String&);  // (else automatic)
    juce::String soloStyleDescription (SoloStyle);

    // A whole solo in that style over the chords (not `automatic`: Riffs.h's `guitarSolo` makes that one).
    LineIdea composeSolo (SoloStyle, const std::vector<ChordSpan>&, double beatsPerBar, const GuitarTuning&,
                          const GuitarVelocities&, int variation, double shortest = 0.0);
}
