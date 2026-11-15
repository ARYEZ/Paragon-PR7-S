#pragma once

#include "engine/Lines.h"

// SOLO STYLES (0.66.0, the owner asked: solos written in the manner of players they love, their techniques
// and habits, never their solos). The research is in docs/solo-styles (a technique sheet per player, sourced
// from guitar lessons and interviews); the app shows only our own names for the styles (the owner's choice).
//
// (0.116.0, the owner's Pink Floyd and Guns N' Roses solos read for their shapes: every style now phrases as those
// do. A phrase is a pickup running into its longest note, held and bent or shaken; most phrases start off beat 1;
// busy passages come as waves with calm after them; the register peaks past halfway and comes back down; chord
// tones where a phrase lands, not on every beat; whole-step bends, some pre-bent and let down; a call played again,
// a theme brought back when the chords come round. What follows was the 0.66.0 design; the motif became the plan.)
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
// (0.128.0, the owner's Metallica and Def Leppard solos, read for their shapes: runs at a player's speed, not a
// grid's; bends aimed at the notes that sing over the chord, and the same bend struck again and again, climbing;
// slides into notes and fall-offs out of them; held first and last notes; and the endings those solos use: bent up
// into the root and held, a fall-off, a dive, a drop to the low root back into the riff, the top note repeated,
// a triplet climb out.)
// Nothing faster than `shortest` beats (the instrument's SHORTEST NOTE; 0: as the style plays it).
namespace pr7s
{
    enum class SoloStyle : int { automatic, soaringBends, rockSwagger, tapAndDive, bluesHammer,
                                 screamAndDive, thrashFire, pinchSqueal, progPrecision,   // (0.67.0: round two)
                                 glamFlash, sleazeBlues, topHatMelody, purpleScream,      // (0.68.0: round three)
                                 gypsyShred, tensionRelease, torturedBends, grungeMelody,   // (0.69.0: round four)
                                 // (0.116.0, from the owner's Pink Floyd and Guns N' Roses solos, shapes only; our names)
                                 singingBends, shuffleConversation, slowBuildRakes, fallingMajorMelody, sungInTheGaps,
                                 rollingSequences, laidBackBends, cycleTheme, twoSpeeds, lickTwice,
                                 // (0.128.0, from the owner's Metallica and Def Leppard solos, shapes only; our names)
                                 repeatedBendClimb, chordChasingArpeggios, pedalDyadHook, hotStartCoolDown,
                                 // (0.129.0, round two from the same files; our names)
                                 unisonBendAnthem, octaveMelodyBuild, openStringPedalFire, twinLeadHarmony, easternTremoloStorm,
                                 whammyWail, trillFallTag,
                                 // (0.133.0, from the owner's Van Halen solos, shapes only; our names)
                                 cellWithTheChanges, holdAndAccelerate, whammyFire, legatoThrows, squealsAndSextuplets,
                                 chimingFourths,
                                 // (0.133.0, round two from the same files; our names)
                                 dottedBendChain, hookThirdsHook, bentRiffSolo, tremoloPedalOutro, burstAndBend,
                                 // (0.134.0, from the owner's Pantera solos, shapes only; our names)
                                 burstBlocksSqueals, bendsThenClimb, bottomToTopClimb, burstBarBurst, balladIntoFire,
                                 halfSpeedBentCells, pedalClimbTremolo, slowStaircaseBends, loopedPullOffs,
                                 // (0.134.0, the rest of the Pantera ideas; our names)
                                 tritoneStaircase, shuffleSnarl, breakdownTrill, slowEasternTwins, heldLineRisingCells,
                                 statedRaisedDropped,
                                 // (0.135.0, from the owner's Oasis, Motley Crue and Dream Theater solos, shapes only; our names)
                                 tradedTurns, callAndChant, fixedHead, shuffleTrade, sextupletLoopsDives, pickedHymn, oneTrickABlock,
                                 turnsDownTheScale, octaveBallad, steppingBendCalls, contourCells, loopedCellsAcross,
                                 fallingPedalSextuplets, easternLegatoRuns, cryingHalfSteps, floatingLine, addNineMelody,
                                 gearShiftStream, barOstinato, triadCellSweeps, heldOverFallingBass,
                                 // (0.136.0, from the owner's Stone Temple Pilots solos, shapes only; our names)
                                 heldBendTalker, sprintBendRunDown, slideInOutro, sourBendsStabs, valleyLaunch,
                                 chromaticVampFlow, riffRegisterBreak };
    const std::vector<SoloStyle>& soloStyles();      // automatic first
    juce::String soloStyleName (SoloStyle);          // "BY STYLE", "SOARING BENDS", "ROCK 'N' ROLL SWAGGER", "TAP & DIVE", "BLUES HAMMER",
                                                     // "SCREAM & DIVE", "THRASH FIRE", "PINCH SQUEAL", "PROG PRECISION",
                                                     // "GLAM FLASH", "SLEAZE BLUES", "TOP HAT MELODY", "PURPLE SCREAM",
                                                     // "GYPSY SHRED", "TENSION & RELEASE", "TORTURED BENDS", "GRUNGE MELODY"
    SoloStyle soloStyleNamed (const juce::String&);  // (else automatic)
    juce::String soloStyleDescription (SoloStyle);

    // (0.116.0) BY STYLE: the solo style the guitar STYLE plays (Lines.h's GuitarStyle by number).
    SoloStyle soloStyleForGuitarStyle (int guitarStyle);

    // A whole solo in that style over the chords (`automatic`: nothing; guitarLeadIdeas asks soloStyleForGuitarStyle).
    LineIdea composeSolo (SoloStyle, const std::vector<ChordSpan>&, double beatsPerBar, const GuitarTuning&,
                          const GuitarVelocities&, int variation, double shortest = 0.0, double bpm = 0.0);
    // (0.128.0) `bpm`: the song's tempo where the solo starts. With it, the fast passages run at 8 to 12 notes a second
    // whatever the tempo (the owner's Metallica and Def Leppard solos: 32nds at 66 BPM, 16ths at 120, triplets at 200),
    // never faster than `shortest`; 0: their written grids, as before 0.128.0.
}
