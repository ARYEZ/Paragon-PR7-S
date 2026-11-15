#include "engine/SoloEngine.h"

#include <set>

namespace pr7s
{
    namespace
    {
        int pcOf (int n) { return ((n % 12) + 12) % 12; }

        // A style's rules (from docs/solo-styles: what the lessons say each player does; the numbers are
        // starting points to tune by ear).
        enum class Burst { run, cell, tapping, displaced, chromaticShift, repeatLick, sixes, sequence, fourSequence, risingFlurry, sweep, noise,
                           turning, flurry,     // (0.116.0: turning cells, the rhythm kept; sextuplet flurries falling)
                           repeatedBend, chordArpeggio, pedalHook,
                           lowPedal, tremoloRun,
                           // (0.133.0, the owner's Van Halen solos) a cell moved with each chord onto its third; one note
                           // held, then struck faster and faster; a staircase of repeated notes; legato cells in the key's
                           // box; three-note cells cycled in sextuplets; a bent note and two fourths cycled in threes
                           chordCell, accelerate, stairs, boxCells, cycledCells, fourthCycle,
                           // (round two) bends struck every dotted quarter, then a chromatic fall in triplets; major
                           // thirds with the chord's third on top; a bent note and fourths in eighths; one note
                           // tremolo-picked through the changes; repeated double stops answered by bends
                           dottedBends, thirdDyads, bentRiff, tremoloPedal, dyadBurst,
                           // (0.134.0, the owner's Pantera solos) diminished cells climbing by fourths, then a top note
                           // struck four times and dropped; three-note cells climbing a scale step a beat; a one-beat
                           // pull-off cell and a bend looped; a pedal under a line climbing on; fiery thirds (cells
                           // cycled, the climb, then cells a half step lower each beat); bent cells for slow songs
                           // (pre-bends let down, a note shaken between two bends); one bend struck again and again
                           dimCells, cellClimb, cellLoop, pedalClimb, fire, bentCells, sameBend,
                           // (the rest of the Pantera ideas) cells of a half step and a major third moved up a fourth
                           // each beat; diminished triplets climbing a minor third each beat; the minor and major
                           // third trilled under a top note, harmonised a minor third under; two-beat cells (a note,
                           // a half-step bend into the next, held) climbing a scale step each time
                           tritoneCells, dimTriplets, thirdTrill, risingBentCells };   // (0.129.0: the low tonic picked under a climbing line; tremolo moving along the scale)   // (0.128.0: the same bend struck again and again, climbing;
                                                                       // arpeggios chasing the chords; a pedal note under a moving line)
        // (0.128.0) How a solo ends: held with vibrato (as before), bent up into the root or fifth and held, a fall-off,
        // a whammy dive, dropped to the low root back into the riff, the top note repeated, a triplet climb out.
        enum class Ending { held, bendUp, fallOff, dive, dropLow, repeatedTop, tripletClimb, trillFall,
                            harmonicDives, divedDyad,
                            steppingDown,
                            bendThenDive, squealOut, tremoloDyads };   // (0.134.0: bent into the root and dived; a low dyad squealed up; tremolo dyads over a pedal)   // (0.133.0 round two: held notes stepping down the chord, each in a beat early)   // (0.133.0: harmonics dived stepping down; thirds climbing into a dived double stop)   // (0.129.0: trilled, then fallen off)
        struct Profile
        {
            bool sixth = false, ninth = false, flatFive = false, majorOnMajor = false, curls = false;
            bool majorThird = false, aeolian = false;                   // (0.67.0) the major 3rd over minor; natural minor's 2 and b6
            bool pinchSqueals = false, scream = false, upbeatEntry = false, endHigh = false;   // (0.67.0)
            // (0.68.0) thirds for double stops (else 6ths); one whammy dip; melody behind the beat; the blue b3 and b7
            // over major chords; bends bigger as the solo goes; the last burst outside the key.
            bool thirdDoubleStops = false, dip = false, blueOnMajor = false, escalate = false, outburst = false;
            double lateBeats = 0.0;
            // (0.69.0) gypsy colour (natural minor's 2 and b6, harmonic minor's 7); phrase ends on the chord's 3rd;
            // the higher statement harmonised a 3rd above; a tense half step above each phrase's last note; bends left
            // a little short (a rub against the chord); the motif restated (an octave up in the second half); a
            // chromatic passing note now and then; vibrato only on the last note; no legato.
            bool gypsy = false, thirdTargets = false, harmonyThirds = false, tension = false, shortBends = false,
                 restate = false, chromaticPassing = false, vibratoLastOnly = false;
            float startDensity = 4.0f, peakDensity = 10.0f;   // notes a bar
            double restBeats = 1.0;                            // between phrases
            int phraseBars = 2;
            Burst burst = Burst::run;
            float burstFrom = 0.5f;                            // along the solo
            bool burstEveryOther = true;                       // else only the phrase before the last
            float bendChance = 0.5f;
            std::vector<std::pair<float, float>> bends;        // semitones, weight (over a whole step: only late)
            float bigFrom = 0.6f;
            float vibrato = 1.0f, vibratoSkip = 0.0f;
            float swing = 0.5f;                                // 0.5 straight; 16ths unless swing8
            bool swing8 = false;
            int climb = 12;                                    // semitones up over the solo
            bool doubleStops = false, tremolo = false, harmonics = false, dive = false, legato = true;
            // (0.116.0, the owner's Pink Floyd and Guns N' Roses solos) The phrase plan: the pickups' notes a beat
            // (0: from the old start and peak densities), the held landing note's length, how long a pickup runs,
            // how often a phrase lands on beat 1 (else beat 3 or the last eighth, tied over), a real rest's chance,
            // the register's peak along the solo, the waves (-1: from the burst settings), bends and vibrato on held
            // notes, pre-bends released, and the habits of the new styles.
            float notesPerBeat = 0.0f;
            double holdMin = 1.5, holdMax = 3.0, pickupMax = 2.0;
            float landBeat1 = 0.4f, restChance = 0.25f, peakAt = 0.7f;
            int waves = -1;
            float releaseShare = 0.1f, vibratoHeld = 0.75f;
            bool falling = false, octaveDownLate = false, twoSpeed = false, lickTwice = false, rakes = false, graceNotes = false,
                 walkDown = false, openHeld = false, noSixteenths = false, repeatPair = false, cycleTheme = false,
                 landThirdFifth = false, fourthDoubleStops = false;
            float seqShare = 0.0f;
            // (0.128.0, the owner's Metallica and Def Leppard solos) Bends aimed at the notes that sing over the chord
            // (root, fifth, the minor key's fourth; over a major chord the third and the sixth too); a slide into a
            // phrase's first note and a fall-off out of a held one now and then; the first and last notes held long;
            // a busy start that cools down; the climax written as repeated bends; the endings (empty: held, as before).
            bool aimedBends = true, longEnds = true, hotStart = false, climaxRepeatedBends = false;
            float slideShare = 0.2f, fallShare = 0.06f;
            std::vector<std::pair<Ending, float>> endings;
            // (0.129.0, round two) A twin lead's harmony (thirds, fourths or octaves by the take), every note doubled
            // an octave, the eastern colour (phrygian dominant: b2, 3, b6), anthem bends (the note struck, then bent
            // slowly up into it again), and the shares of whammy dives at phrase ends, whammy wobbles and trills on
            // held notes.
            bool twin = false, octaveDouble = false, phrygianDominant = false, unisonBends = false;
            float diveShare = 0.0f, wobbleShare = 0.0f, trillShare = 0.0f;
            // (0.133.0, the owner's Van Halen solos) In on the downbeat with a high harmonic scooped up into with the
            // whammy; a two-note pickup thrown two octaves under the phrase now and then; the whammy scooping into
            // phrases' first notes and held notes (from 2, 3 or 12 under, in 0.06 s).
            bool screamEntry = false, throws = false;
            float scoopShare = 0.0f;
            // (round two) the rock vamp's scale (the key's 1 2 4 5 b7: no third at all); one long busy passage a
            // third of the solo long in its middle (the ABA solo's B part).
            bool noThird = false, middlePart = false;
            // (0.134.0, the owner's Pantera solos) whammy squeals at phrase ends (pull-ups of 6 to 8 on a low note, so
            // many in 16 bars); the busy passages as blocks starting on four-bar lines; where the waves go (empty: the
            // usual places) and how many bars each lasts (0: the usual).
            // (the rest of the Pantera ideas) four bars of held notes on the key's root and flat five rising a tritone a
            // bar; the first half one held note a chord, each in an eighth early with a rake; the restated motif an
            // octave up from its first return; the last quarter two octaves lower; a long silence after the first
            // phrase; one over-bend past halfway; a twin an octave and a third over; a scoop always this size.
            bool tritoneOpening = false, heldChordOpening = false, restateEarly = false, lowLastQuarter = false,
                 longFirstRest = false, oneOverbend = false, twinTenth = false;
            float scoopAmount = 0.0f;
            int squeals = 0, waveBars = 0;
            bool blocks = false;
            std::vector<float> waveAts;
            const char* description = "";
        };

        Profile profileOf (SoloStyle s)
        {
            Profile p;
            switch (s)
            {
                case SoloStyle::soaringBends:
                    p.sixth = p.ninth = p.majorOnMajor = true;
                    p.startDensity = 3.0f; p.peakDensity = 9.0f; p.restBeats = 1.5; p.phraseBars = 2;
                    p.notesPerBeat = 1.0f;   // (0.116.0: slow and spacious)
                    p.burst = Burst::run; p.burstFrom = 0.7f; p.burstEveryOther = false;
                    p.bendChance = 0.65f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.4f }, { 3.0f, 0.3f }, { 4.0f, 0.15f } }; p.bigFrom = 0.55f;
                    p.vibrato = 0.6f; p.vibratoSkip = 0.3f; p.climb = 12;
                    p.description = "Slow and spacious: a singing motif that keeps coming back, long notes bent slowly into their pitch, "
                                    "restrained vibrato, climbing to big bends and one short fast run near the end";
                    break;
                case SoloStyle::rockSwagger:
                    p.sixth = p.ninth = p.flatFive = p.majorOnMajor = true;
                    p.startDensity = 5.0f; p.peakDensity = 12.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::cell; p.burstFrom = 0.45f;
                    p.bendChance = 0.55f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.6f }, { 3.0f, 0.2f } };
                    p.vibrato = 1.0f; p.swing = 0.62f; p.climb = 7; p.doubleStops = true; p.tremolo = true;
                    p.description = "Loose, swung bluesy rock: short call-and-response phrases, the major 6th and both thirds, "
                                    "6th double stops, slides and pull-offs, wide vibrato, repeated licks midway";
                    break;
                case SoloStyle::tapAndDive:
                    p.sixth = p.flatFive = p.majorOnMajor = true;
                    p.startDensity = 4.0f; p.peakDensity = 14.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::tapping; p.burstFrom = 0.25f;
                    p.bendChance = 0.45f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f }, { 3.0f, 0.3f }, { 4.0f, 0.3f } }; p.bigFrom = 0.4f;
                    p.vibrato = 1.0f; p.swing = 0.56f; p.swing8 = true; p.climb = 12; p.tremolo = p.harmonics = p.dive = true;
                    p.description = "Melodic phrases traded with tapped arpeggios that follow the chords, legato runs, harmonics, "
                                    "wide vibrato and big bends, ending on a pinch harmonic and a whammy dive";
                    break;
                case SoloStyle::bluesHammer:
                    p.sixth = p.ninth = p.flatFive = p.majorOnMajor = p.curls = true;
                    p.startDensity = 5.0f; p.peakDensity = 14.0f; p.restBeats = 0.75; p.phraseBars = 1;
                    p.burst = Burst::displaced; p.burstFrom = 0.35f;
                    p.bendChance = 0.55f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.4f }, { 3.0f, 0.3f }, { 4.0f, 0.2f }, { 5.0f, 0.1f } };
                    p.vibrato = 0.9f; p.climb = 12; p.climaxRepeatedBends = true;   // (0.128.0: the climax its description promised)
                    p.description = "Bluesy and loose: major and minor mixed, flat thirds curled sharp, licks repeated so they "
                                    "shift against the bar, crammed bursts, big over-bends and a climax of repeated bends";
                    break;
                case SoloStyle::screamAndDive:
                    // (0.134.0: put right from the owner's Pantera files: whole steps, no major third over the minor, no
                    // rests to speak of, diminished cells climbing by fourths instead of a fret at a time, squeals.)
                    p.flatFive = true;
                    p.startDensity = 4.0f; p.peakDensity = 14.0f; p.restBeats = 0.5; p.restChance = 0.03f; p.phraseBars = 1;
                    p.burst = Burst::dimCells; p.burstFrom = 0.3f; p.blocks = true;
                    p.bendChance = 0.6f; p.bends = { { 2.0f, 1.0f }, { 3.0f, 0.25f }, { 1.0f, 0.15f }, { 4.0f, 0.1f } }; p.bigFrom = 0.5f;
                    p.vibrato = 1.3f; p.climb = 14; p.dive = p.scream = true; p.squeals = 3; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.description = "Aggressive groove-metal lead: the blues scale with the flat five, whole-step bends, diminished "
                                    "cells climbing by fourths in blocks, whammy squeals at phrase ends, a harmonic screamed up "
                                    "with the bar and a dive bomb to end";
                    break;
                case SoloStyle::thrashFire:
                    p.flatFive = true;
                    p.startDensity = 5.0f; p.peakDensity = 14.0f; p.restBeats = 0.75; p.phraseBars = 2;
                    p.burst = Burst::repeatLick; p.burstFrom = 0.3f;
                    p.bendChance = 0.45f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.5f }, { 3.0f, 0.2f } };
                    p.vibrato = 1.2f; p.climb = 14; p.upbeatEntry = p.endHigh = true;
                    p.description = "Thrash pentatonic fire: in on an upbeat with a vocal-like melody, then a picked triplet lick "
                                    "repeated so it drifts across the bar, wide vibrato, ending high";
                    break;
                case SoloStyle::pinchSqueal:
                    p.flatFive = p.majorOnMajor = true;
                    p.startDensity = 4.0f; p.peakDensity = 16.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::sixes; p.burstFrom = 0.45f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 3.0f, 0.3f }, { 1.0f, 0.3f } };
                    p.vibrato = 1.6f; p.climb = 12; p.pinchSqueals = true;
                    p.description = "Heavy pentatonic: a melody first, then picked six-note runs down the boxes near the end, "
                                    "pinch-harmonic squeals in the gaps, very wide vibrato on every phrase end";
                    break;
                case SoloStyle::progPrecision:
                    p.aeolian = true;
                    p.startDensity = 3.5f; p.peakDensity = 16.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::sequence; p.burstFrom = 0.3f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f }, { 3.0f, 0.15f } };
                    p.vibrato = 0.9f; p.climb = 14;
                    p.description = "Precise and composed: singing in-tune bends traded with picked six-note scale sequences and "
                                    "seventh-chord arpeggios, building in curves to something outside at the climax";
                    break;
                case SoloStyle::glamFlash:
                    p.majorOnMajor = p.flatFive = true;
                    p.startDensity = 3.0f; p.peakDensity = 12.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::run; p.burstFrom = 0.5f; p.burstEveryOther = false;
                    p.bendChance = 0.65f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f }, { 3.0f, 0.2f } }; p.bigFrom = 0.5f;
                    p.vibrato = 1.3f; p.climb = 10; p.doubleStops = p.thirdDoubleStops = true;
                    p.description = "Glam hard rock: sustained notes and long bends held with vibrato, lyrical double stops in thirds, "
                                    "melodic answers, and the fast picked run saved for the very end";
                    break;
                case SoloStyle::sleazeBlues:
                    p.flatFive = p.majorOnMajor = p.curls = true;
                    p.startDensity = 3.0f; p.peakDensity = 8.0f; p.restBeats = 1.5; p.phraseBars = 2;
                    p.burst = Burst::run; p.burstFrom = 0.5f; p.burstEveryOther = false;
                    p.bendChance = 0.7f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f } };
                    p.vibrato = 1.1f; p.climb = 7; p.dip = true;
                    p.description = "A bluesman's melody you could hum: two-bar phrases and their answers, a bend held on every bar, "
                                    "space for the singer, one whammy dip, a quick run near the end and a long held note to finish";
                    break;
                case SoloStyle::topHatMelody:
                    p.flatFive = p.majorOnMajor = true;
                    p.startDensity = 3.5f; p.peakDensity = 12.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::fourSequence; p.burstFrom = 0.5f;
                    p.bendChance = 0.6f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.4f }, { 3.0f, 0.3f } }; p.bigFrom = 0.5f;
                    p.vibrato = 1.2f; p.climb = 12; p.lateBeats = 0.03;
                    p.description = "Melody first: slow, loose bends and held notes laid back behind the beat, a motif played again "
                                    "higher, then a wilder second half of four-note sequences climbing the scale";
                    break;
                case SoloStyle::purpleScream:
                    p.majorOnMajor = p.blueOnMajor = true;
                    p.startDensity = 2.5f; p.peakDensity = 12.0f; p.restBeats = 1.5; p.phraseBars = 2;
                    p.burst = Burst::risingFlurry; p.burstFrom = 0.4f;
                    p.bendChance = 0.7f; p.bends = { { 2.0f, 1.0f } };
                    p.vibrato = 1.4f; p.climb = 14; p.escalate = p.outburst = p.endHigh = true;
                    p.description = "Restrained, then soaring: a motif brought back higher each time, bends bigger every time, "
                                    "rising flurries, one outburst outside the key at the peak and a screaming held high note";
                    break;
                case SoloStyle::gypsyShred:
                    p.gypsy = p.thirdTargets = p.harmonyThirds = p.dip = true;
                    p.startDensity = 4.0f; p.peakDensity = 14.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::sweep; p.burstFrom = 0.3f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f }, { 12.0f, 0.15f } }; p.bigFrom = 0.6f;
                    p.vibrato = 1.2f; p.climb = 12; p.legato = false;
                    p.description = "Melody first, landing on the chords' thirds, with gypsy-jazz minor colour; the higher phrases "
                                    "harmonised a third above; picked sweeps of the chords' arpeggios, octave bends and a whammy dip";
                    break;
                case SoloStyle::tensionRelease:
                    p.flatFive = p.tension = true;
                    p.startDensity = 4.0f; p.peakDensity = 12.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::run; p.burstFrom = 0.45f;
                    p.bendChance = 0.55f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.4f }, { 3.0f, 0.2f } };
                    p.vibrato = 1.2f; p.climb = 14;
                    p.description = "Note choice over speed: high, clean legato phrases, each ending on a dissonant note a half step "
                                    "above its target before it resolves, clean picked runs held back for later";
                    break;
                case SoloStyle::torturedBends:
                    p.sixth = p.majorThird = p.shortBends = p.harmonyThirds = true;
                    p.startDensity = 3.0f; p.peakDensity = 8.0f; p.restBeats = 1.5; p.phraseBars = 2;
                    p.burst = Burst::run; p.burstFrom = 0.5f; p.burstEveryOther = false;
                    p.bendChance = 0.75f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.4f } };
                    p.vibrato = 1.5f; p.climb = 7;
                    p.description = "Short and singable, a scene change in the song: slow bends held so they rub against the chord, "
                                    "wide dark vibrato, a phrase harmonised a third above, one quick legato burst";
                    break;
                case SoloStyle::grungeMelody:
                    p.restate = p.chromaticPassing = p.vibratoLastOnly = true;
                    p.startDensity = 3.0f; p.peakDensity = 5.0f; p.restBeats = 1.0; p.phraseBars = 2;
                    p.burst = Burst::noise; p.burstFrom = 2.0f;   // (noisy takes only: set in composeSolo)
                    p.bendChance = 0.3f; p.bends = { { 2.0f, 1.0f } };
                    p.vibrato = 0.5f; p.climb = 0; p.legato = false;
                    p.description = "Sparse and simple: the opening tune played again (an octave up in the second half) like a "
                                    "vocal line on guitar, a chromatic passing note now and then, no whammy; some takes noisy";
                    break;
                // (0.116.0) From the owner's Pink Floyd and Guns N' Roses solos (shapes only; our names).
                case SoloStyle::singingBends:
                    p.sixth = p.ninth = true;
                    p.notesPerBeat = 0.9f; p.holdMin = 2.0; p.holdMax = 4.5; p.pickupMax = 1.5; p.landBeat1 = 0.35f;
                    p.waves = 0; p.peakAt = 0.6f; p.climb = 12; p.restChance = 0.2f; p.restBeats = 2.0;
                    p.bendChance = 0.65f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.2f }, { 3.0f, 0.12f } }; p.bigFrom = 0.4f;
                    p.releaseShare = 0.3f; p.vibrato = 0.8f;
                    p.description = "Few notes: each phrase a short pickup into one long note bent into its pitch with vibrato, "
                                    "some bent before they're struck and let down; it peaks past halfway and comes back down";
                    break;
                case SoloStyle::shuffleConversation:
                    p.ninth = p.curls = p.majorOnMajor = true;
                    p.swing = 0.667f; p.swing8 = true; p.noSixteenths = true;
                    p.notesPerBeat = 1.2f; p.holdMin = 1.5; p.holdMax = 3.0; p.pickupMax = 2.0; p.landBeat1 = 0.2f;
                    p.waves = 0; p.peakAt = 0.78f; p.climb = 12; p.repeatPair = true; p.restChance = 0.3f;
                    p.bendChance = 0.55f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.25f } };
                    p.releaseShare = 0.05f; p.vibrato = 1.0f;
                    p.description = "A blues shuffle in triplets, about a note a beat: phrases in off the beat, a call and its "
                                    "answer played twice, held bends to the fifth and the root, the last third higher";
                    break;
                case SoloStyle::slowBuildRakes:
                    p.ninth = true;
                    p.notesPerBeat = 1.4f; p.holdMin = 1.5; p.holdMax = 3.0; p.landBeat1 = 0.3f;
                    p.waves = 2; p.burst = Burst::flurry; p.peakAt = 0.7f; p.climb = 10; p.rakes = true;
                    p.bendChance = 0.45f; p.bends = { { 1.0f, 1.0f }, { 2.0f, 0.9f }, { 3.0f, 0.3f } }; p.bigFrom = 0.5f;
                    p.releaseShare = 0.15f; p.vibrato = 0.9f;
                    p.description = "A slow build for a ballad: sparse at first, then waves of fast falling flurries with chord "
                                    "rakes between them, the biggest wave past halfway, ending on a held root";
                    break;
                case SoloStyle::fallingMajorMelody:
                    p.majorOnMajor = p.falling = p.graceNotes = p.landThirdFifth = true;
                    p.notesPerBeat = 1.2f; p.holdMin = 2.0; p.holdMax = 4.0; p.pickupMax = 1.5; p.landBeat1 = 0.8f;
                    p.waves = 0; p.climb = 14; p.restChance = 0.0f;
                    p.longEnds = false;   // (0.128.0: a tune sung from the top down, phrase by phrase: no long first note)
                    p.bendChance = 0.4f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.5f } };
                    p.releaseShare = 0.3f; p.vibrato = 0.8f;
                    p.description = "A major-key melody sung from the top down: phrases from the and of three into a held third "
                                    "or fifth, each lower than the last, quick grace notes into the held notes";
                    break;
                case SoloStyle::sungInTheGaps:
                    p.majorOnMajor = p.graceNotes = p.cycleTheme = p.octaveDownLate = p.noSixteenths = true;
                    p.notesPerBeat = 1.3f; p.holdMin = 2.0; p.holdMax = 3.0; p.pickupMax = 2.0; p.landBeat1 = 0.3f;
                    p.waves = 0; p.climb = 7; p.restChance = 0.15f;
                    p.longEnds = false;   // (0.128.0: a singable tune that comes round with the chords)
                    p.bendChance = 0.3f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.7f } };
                    p.releaseShare = 0.35f; p.vibrato = 0.8f;
                    p.description = "A singable eighth-note melody starting mid-bar, played again when the chords come round, "
                                    "the second half an octave lower, grace notes from a whole step under";
                    break;
                case SoloStyle::rollingSequences:
                    p.sixth = p.ninth = p.majorThird = p.openHeld = true;
                    p.notesPerBeat = 1.8f; p.holdMin = 1.5; p.holdMax = 2.5; p.landBeat1 = 0.5f;
                    p.waves = 3; p.burst = Burst::turning; p.peakAt = 0.75f; p.climb = 14; p.seqShare = 0.3f; p.restChance = 0.1f;
                    p.bendChance = 0.35f; p.bends = { { 2.0f, 1.0f } };
                    p.vibrato = 1.0f;
                    p.description = "Opens on one held low note, then builds in waves of turning cells (the rhythm kept, the notes "
                                    "moving), sequences of the last phrase, the top past halfway and a quiet end";
                    break;
                case SoloStyle::laidBackBends:
                    p.majorOnMajor = p.noSixteenths = p.walkDown = true;
                    p.lateBeats = 0.08;
                    p.notesPerBeat = 0.8f; p.holdMin = 2.0; p.holdMax = 4.0; p.pickupMax = 1.5; p.landBeat1 = 0.4f;
                    p.waves = 0; p.peakAt = 0.75f; p.climb = 12; p.restChance = 0.15f;
                    p.bendChance = 0.7f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.15f } };
                    p.releaseShare = 0.1f; p.vibrato = 0.9f;
                    p.description = "Sweet and behind the beat, no fast notes: slow bends up into the held notes, late vibrato, "
                                    "walking down to a long low root at the end";
                    break;
                case SoloStyle::cycleTheme:
                    p.ninth = p.cycleTheme = true;
                    p.notesPerBeat = 1.6f; p.holdMin = 1.5; p.holdMax = 3.0; p.landBeat1 = 0.4f;
                    p.waves = 2; p.burst = Burst::cell; p.peakAt = 0.85f; p.climb = 12; p.restChance = 0.1f;
                    p.bendChance = 0.55f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.25f } };
                    p.vibrato = 1.0f;
                    p.description = "A theme that comes back every time the chords come round, with new endings, and fast repeated "
                                    "figures near the end of the later rounds";
                    break;
                case SoloStyle::twoSpeeds:
                    p.ninth = true;
                    p.notesPerBeat = 0.8f; p.holdMin = 2.0; p.holdMax = 4.0; p.landBeat1 = 0.35f;
                    p.twoSpeed = true; p.burst = Burst::cell; p.climb = 12; p.peakAt = 0.8f;
                    p.bendChance = 0.65f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.2f } };
                    p.releaseShare = 0.2f; p.vibrato = 1.0f;
                    p.description = "Two halves: slow and bendy, then one fast pentatonic passage right through to the last bar";
                    break;
                case SoloStyle::lickTwice:
                    p.flatFive = p.fourthDoubleStops = p.doubleStops = p.lickTwice = true;
                    p.notesPerBeat = 2.0f; p.holdMin = 1.0; p.holdMax = 2.0; p.landBeat1 = 0.4f; p.restChance = 0.0f;
                    p.waves = 1; p.burst = Burst::cell; p.climb = 7;
                    p.bendChance = 0.45f; p.bends = { { 2.0f, 1.0f } };
                    p.vibrato = 1.0f;
                    p.description = "Blues scale over a vamp, phrases running into each other, fourth double stops, and a "
                                    "two-bar lick played twice to finish";
                    break;
                // (0.128.0, from the owner's Metallica and Def Leppard solos, shapes only; our names)
                case SoloStyle::repeatedBendClimb:
                    p.flatFive = true;
                    p.notesPerBeat = 1.5f; p.holdMin = 1.5; p.holdMax = 4.0; p.landBeat1 = 0.45f; p.phraseBars = 2;
                    p.waves = 2; p.burst = Burst::repeatedBend; p.climb = 12; p.peakAt = 0.75f;
                    p.bendChance = 0.7f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.4f }, { 3.0f, 0.2f } };
                    p.releaseShare = 0.15f; p.vibrato = 1.1f;
                    p.endings = { { Ending::bendUp, 1.0f }, { Ending::repeatedTop, 0.6f } };
                    p.description = "Bends that aim for the root and the fifth, then the same bend struck over and over, "
                                    "each group a step higher, ending bent up into the root or on the top note hammered out";
                    break;
                case SoloStyle::chordChasingArpeggios:
                    p.aeolian = p.landThirdFifth = true;
                    p.notesPerBeat = 2.0f; p.holdMin = 1.5; p.holdMax = 3.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.waves = 3; p.burst = Burst::chordArpeggio; p.climb = 12; p.peakAt = 0.7f;
                    p.bendChance = 0.45f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f } };
                    p.vibrato = 1.0f;
                    p.endings = { { Ending::tripletClimb, 1.0f }, { Ending::held, 0.5f } };
                    p.description = "Melodic phrases landing on the chord's third or fifth, and fast arpeggios that follow "
                                    "every chord change, ending with a triplet climb to the top root";
                    break;
                case SoloStyle::pedalDyadHook:
                    p.sixth = p.majorOnMajor = p.doubleStops = p.fourthDoubleStops = p.repeatPair = true;
                    p.notesPerBeat = 1.5f; p.holdMin = 1.5; p.holdMax = 3.0; p.landBeat1 = 0.5f; p.phraseBars = 1;
                    p.waves = 2; p.burst = Burst::pedalHook; p.climb = 9; p.peakAt = 0.7f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f } };
                    p.vibrato = 0.9f;
                    p.endings = { { Ending::repeatedTop, 1.0f }, { Ending::bendUp, 0.6f } };
                    p.description = "Hooky and melodic: short calls played twice, fourth double stops, a high pedal note picked "
                                    "between a line walking down under it, ending on the top note repeated";
                    break;
                case SoloStyle::hotStartCoolDown:
                    p.flatFive = p.hotStart = true;
                    p.notesPerBeat = 1.0f; p.holdMin = 2.0; p.holdMax = 4.5; p.landBeat1 = 0.4f; p.phraseBars = 2;
                    p.burst = Burst::run; p.climb = 12;
                    p.bendChance = 0.6f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f }, { 3.0f, 0.2f } };
                    p.releaseShare = 0.2f; p.vibrato = 1.0f; p.vibratoHeld = 0.9f; p.fallShare = 0.12f;
                    p.endings = { { Ending::fallOff, 1.0f }, { Ending::held, 0.6f }, { Ending::dropLow, 0.4f } };
                    p.description = "In at full speed with a fast run high up, then calming down: longer notes, lower and "
                                    "slower, bent and shaken, ending in a fall-off, a held note or a drop back into the riff";
                    break;
                // (0.129.0, round two from the owner's Metallica and Def Leppard solos, shapes only; our names)
                case SoloStyle::unisonBendAnthem:
                    p.sixth = p.majorOnMajor = p.unisonBends = true;
                    p.notesPerBeat = 0.9f; p.holdMin = 2.0; p.holdMax = 5.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.waves = 1; p.burst = Burst::run; p.climb = 9; p.peakAt = 0.75f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.2f } };
                    p.vibrato = 0.9f; p.vibratoHeld = 0.9f;
                    p.endings = { { Ending::bendUp, 1.0f }, { Ending::held, 0.5f } };
                    p.description = "Slow and singing: long notes struck and then bent slowly up into themselves, "
                                    "wide and anthemic, one short run near the end";
                    break;
                case SoloStyle::octaveMelodyBuild:
                    p.majorOnMajor = p.sixth = p.octaveDouble = true;
                    p.notesPerBeat = 1.1f; p.holdMin = 2.0; p.holdMax = 4.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.waves = 1; p.burst = Burst::run; p.climb = 9; p.peakAt = 0.85f;
                    p.bendChance = 0.15f; p.bends = { { 2.0f, 1.0f } }; p.releaseShare = 0.0f;
                    p.vibrato = 0.8f;
                    p.endings = { { Ending::held, 1.0f }, { Ending::repeatedTop, 0.5f } };
                    p.description = "A melody played in octaves, every note doubled, building from held notes to a busy end";
                    break;
                case SoloStyle::openStringPedalFire:
                    p.flatFive = true;
                    p.notesPerBeat = 2.0f; p.holdMin = 1.0; p.holdMax = 2.5; p.landBeat1 = 0.4f; p.phraseBars = 1;
                    p.waves = 3; p.burst = Burst::lowPedal; p.climb = 12; p.peakAt = 0.7f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.4f } };
                    p.vibrato = 1.1f;
                    p.endings = { { Ending::dropLow, 1.0f }, { Ending::held, 0.5f } };
                    p.description = "Fast picking against the low tonic: the open note struck between a line climbing "
                                    "over it, aggressive bends, ending back down on the low root";
                    break;
                case SoloStyle::twinLeadHarmony:
                    p.aeolian = p.twin = true;
                    p.notesPerBeat = 1.5f; p.holdMin = 1.5; p.holdMax = 3.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.waves = 2; p.burst = Burst::run; p.climb = 9; p.peakAt = 0.7f;
                    p.bendChance = 0.45f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f } };
                    p.vibrato = 1.0f;
                    p.endings = { { Ending::held, 1.0f }, { Ending::bendUp, 0.4f } };
                    p.description = "One guitar alone, then two in harmony (thirds, fourths or octaves by the take): on a "
                                    "LEAD lane with two tracks the second plays the harmony";
                    break;
                case SoloStyle::easternTremoloStorm:
                    p.phrygianDominant = true;
                    p.notesPerBeat = 1.5f; p.holdMin = 1.5; p.holdMax = 3.0; p.landBeat1 = 0.4f; p.phraseBars = 2;
                    p.waves = 2; p.burst = Burst::tremoloRun; p.climb = 12; p.peakAt = 0.7f;
                    p.bendChance = 0.4f; p.bends = { { 1.0f, 1.0f }, { 2.0f, 0.8f } };
                    p.vibrato = 1.2f; p.diveShare = 0.1f;
                    p.endings = { { Ending::dive, 1.0f }, { Ending::held, 0.5f } };
                    p.description = "An eastern colour (the flat second and the major third over the minor): tremolo-picked "
                                    "notes moving along the scale, half-step bends, a dive to end";
                    break;
                case SoloStyle::whammyWail:
                    p.flatFive = true;
                    p.notesPerBeat = 1.3f; p.holdMin = 1.5; p.holdMax = 3.5; p.landBeat1 = 0.4f; p.phraseBars = 2;
                    p.waves = 2; p.burst = Burst::run; p.climb = 12; p.peakAt = 0.7f;
                    p.bendChance = 0.45f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f } };
                    p.vibrato = 1.1f; p.diveShare = 0.25f; p.wobbleShare = 0.3f;
                    p.endings = { { Ending::dive, 1.0f }, { Ending::fallOff, 1.0f } };
                    p.description = "The whammy bar everywhere: held notes shaken, phrase ends dived away, a dive or a "
                                    "fall-off to finish";
                    break;
                case SoloStyle::trillFallTag:
                    p.majorOnMajor = p.sixth = true;
                    p.notesPerBeat = 1.4f; p.holdMin = 2.0; p.holdMax = 3.5; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.waves = 1; p.burst = Burst::cell; p.climb = 9; p.peakAt = 0.75f;
                    p.bendChance = 0.4f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f } };
                    p.vibrato = 0.9f; p.trillShare = 0.3f; p.fallShare = 0.2f;
                    p.endings = { { Ending::trillFall, 1.0f } };
                    p.description = "Held notes trilled with the note above, phrases falling away at their ends, and a tag "
                                    "that trills and falls off to finish";
                    break;
                // (0.133.0, the owner's Van Halen solos, shapes only. Every one: phrases running into each other
                // (rests of half a beat at most, the held notes the breath), the busiest passage in the first half,
                // whole-step bends, vibrato on every held note.)
                case SoloStyle::cellWithTheChanges:
                    p.majorOnMajor = p.landThirdFifth = true;
                    p.notesPerBeat = 2.2f; p.holdMin = 1.0; p.holdMax = 2.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.05f; p.restBeats = 0.5;
                    p.waves = 2; p.burst = Burst::chordCell; p.climb = 10; p.peakAt = 0.5f;
                    p.bendChance = 0.35f; p.bends = { { 2.0f, 1.0f } }; p.releaseShare = 0.05f;
                    p.vibrato = 1.0f; p.vibratoHeld = 1.0f; p.slideShare = 0.3f; p.scoopShare = 0.1f; p.diveShare = 0.1f;
                    p.endings = { { Ending::divedDyad, 1.0f } };
                    p.description = "A one-bar cell of sixteenths moved with each chord so it lands on that chord's third, "
                                    "a held bend shaken with the whammy, then thirds climbing a half step at a time into a "
                                    "double stop dived away";
                    break;
                case SoloStyle::holdAndAccelerate:
                    p.flatFive = p.screamEntry = p.doubleStops = p.fourthDoubleStops = true;
                    p.notesPerBeat = 2.4f; p.holdMin = 1.0; p.holdMax = 2.5; p.landBeat1 = 0.4f; p.phraseBars = 1;
                    p.restChance = 0.05f; p.restBeats = 0.5;
                    p.waves = 2; p.burst = Burst::accelerate; p.climb = 12; p.peakAt = 0.45f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.15f } };
                    p.vibrato = 1.1f; p.vibratoHeld = 1.0f; p.wobbleShare = 0.3f; p.scoopShare = 0.2f;
                    p.endings = { { Ending::harmonicDives, 1.0f } };
                    p.description = "In on a screaming harmonic, short bursts answered by bends, then one note held and "
                                    "shaken before it's struck faster and faster; harmonics dived away stepping down to end";
                    break;
                case SoloStyle::whammyFire:
                    p.majorOnMajor = p.hotStart = true;
                    p.notesPerBeat = 2.2f; p.holdMin = 1.0; p.holdMax = 2.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.05f; p.restBeats = 0.5;
                    p.waves = 2; p.burst = Burst::stairs; p.climb = 12; p.peakAt = 0.75f;
                    p.bendChance = 0.12f; p.bends = { { 2.0f, 1.0f } };
                    p.vibrato = 1.0f; p.vibratoHeld = 1.0f; p.diveShare = 0.45f; p.scoopShare = 0.4f; p.slideShare = 0.0f;
                    p.endings = { { Ending::dive, 1.0f } };
                    p.description = "In at full speed, then staircases of repeated notes, the whammy scooping into the "
                                    "held notes and diving phrase ends away, a slow climb to the top and a dive to end";
                    break;
                case SoloStyle::legatoThrows:
                    p.throws = true;
                    p.notesPerBeat = 2.4f; p.holdMin = 1.0; p.holdMax = 1.5; p.landBeat1 = 0.3f; p.phraseBars = 2;
                    p.restChance = 0.1f; p.restBeats = 0.5;
                    p.waves = 2; p.burst = Burst::boxCells; p.climb = 12; p.peakAt = 0.55f;
                    p.bendChance = 0.3f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.15f } };
                    p.vibrato = 1.0f; p.vibratoHeld = 1.0f; p.scoopShare = 0.25f;
                    p.endings = { { Ending::tripletClimb, 1.0f } };
                    p.description = "High notes thrown against fast legato runs down to low landings, pickups two octaves "
                                    "under, cells played in place then stepped down the box, a run up into a long note to end";
                    break;
                case SoloStyle::squealsAndSextuplets:
                    p.screamEntry = p.harmonics = true;
                    p.notesPerBeat = 2.0f; p.holdMin = 1.25; p.holdMax = 2.2; p.landBeat1 = 0.5f; p.phraseBars = 1;
                    p.restChance = 0.05f; p.restBeats = 0.5;
                    p.waves = 2; p.burst = Burst::cycledCells; p.climb = 12; p.peakAt = 0.4f;
                    p.bendChance = 0.06f; p.bends = { { 2.0f, 1.0f } }; p.releaseShare = 0.0f;
                    p.vibrato = 1.0f; p.vibratoHeld = 1.0f; p.seqShare = 0.25f;
                    p.endings = { { Ending::held, 1.0f } };
                    p.description = "Harmonic squeals up high, then down into three-note cells cycled in sextuplets, a "
                                    "melody of pentatonic leaps between, every held note shaken, hardly a bend";
                    break;
                case SoloStyle::chimingFourths:
                    p.majorOnMajor = p.sixth = p.doubleStops = p.fourthDoubleStops = true;
                    p.notesPerBeat = 2.2f; p.holdMin = 1.0; p.holdMax = 2.0; p.landBeat1 = 0.5f; p.phraseBars = 1;
                    p.restChance = 0.1f; p.restBeats = 0.5;
                    p.waves = 2; p.burst = Burst::fourthCycle; p.climb = 12; p.peakAt = 0.75f;
                    p.bendChance = 0.55f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.1f } }; p.releaseShare = 0.45f;
                    p.vibrato = 1.3f; p.vibratoHeld = 1.0f;
                    p.endings = { { Ending::bendUp, 1.0f } };
                    p.description = "Bright and major: bends struck and let down, fourths chiming as double stops, a bent "
                                    "note and two fourths cycled three against four, a high bend held to end";
                    break;
                case SoloStyle::dottedBendChain:
                    p.majorOnMajor = p.lickTwice = true;
                    p.notesPerBeat = 1.4f; p.holdMin = 1.5; p.holdMax = 3.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.05f; p.restBeats = 0.5;
                    p.waves = 2; p.burst = Burst::dottedBends; p.climb = 9; p.peakAt = 0.5f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.1f } }; p.releaseShare = 0.0f;
                    p.vibrato = 1.0f; p.vibratoHeld = 1.0f;
                    p.endings = { { Ending::held, 1.0f } };
                    p.description = "Chains of whole-step bends struck every dotted quarter, three against four, each held "
                                    "and shaken; between them a chromatic fall in triplets; a lick played twice to end";
                    break;
                case SoloStyle::hookThirdsHook:
                    p.majorOnMajor = p.restate = p.middlePart = true;
                    p.notesPerBeat = 2.6f; p.holdMin = 1.5; p.holdMax = 2.0; p.landBeat1 = 0.5f; p.phraseBars = 1;
                    p.restChance = 0.2f; p.restBeats = 0.25; p.pickupMax = 1.5;
                    p.waves = 1; p.burst = Burst::thirdDyads; p.climb = 7; p.peakAt = 0.5f;
                    p.bendChance = 0.15f; p.bends = { { 2.0f, 1.0f } }; p.releaseShare = 0.0f;
                    p.vibrato = 0.8f; p.vibratoHeld = 1.0f; p.noSixteenths = false;
                    p.endings = { { Ending::held, 1.0f } };
                    p.description = "A short hook played again and again, a middle part of major thirds moving with the "
                                    "chords (each chord's third on top), then the hook back; nothing faster than sixteenths";
                    break;
                case SoloStyle::bentRiffSolo:
                    p.noThird = p.unisonBends = true;
                    p.notesPerBeat = 1.5f; p.holdMin = 1.0; p.holdMax = 2.25; p.landBeat1 = 0.5f; p.phraseBars = 1;
                    p.restChance = 0.05f; p.restBeats = 0.25;
                    p.waves = 2; p.burst = Burst::bentRiff; p.climb = 9; p.peakAt = 0.4f;
                    p.bendChance = 0.6f; p.bends = { { 2.0f, 1.0f } }; p.releaseShare = 0.2f;
                    p.vibrato = 0.6f; p.vibratoHeld = 0.5f;
                    p.endings = { { Ending::dropLow, 1.0f }, { Ending::held, 0.5f } };
                    p.description = "A riff more than a run: whole-step bends traded with fourths in eighths, the rock "
                                    "scale with no third at all, one fast roll, falling an octave at the end";
                    break;
                case SoloStyle::tremoloPedalOutro:
                    p.majorOnMajor = true;
                    p.notesPerBeat = 1.0f; p.holdMin = 1.5; p.holdMax = 3.0; p.landBeat1 = 0.4f; p.phraseBars = 2;
                    p.restChance = 0.05f; p.restBeats = 0.5;
                    p.waves = 2; p.burst = Burst::tremoloPedal; p.climb = 12; p.peakAt = 0.6f;
                    p.bendChance = 0.3f; p.bends = { { 2.0f, 1.0f } };
                    p.vibrato = 1.0f; p.vibratoHeld = 1.0f; p.diveShare = 0.2f;
                    p.endings = { { Ending::steppingDown, 1.0f } };
                    p.description = "One note tremolo-picked through the moving chords, slow climbs in held notes, and to "
                                    "end held notes stepping down the chord, each coming in a beat early";
                    break;
                case SoloStyle::burstAndBend:
                    p.screamEntry = true;
                    p.notesPerBeat = 2.4f; p.holdMin = 1.0; p.holdMax = 2.0; p.landBeat1 = 0.5f; p.phraseBars = 1;
                    p.restChance = 0.05f; p.restBeats = 0.5;
                    p.waves = 2; p.burst = Burst::dyadBurst; p.climb = 12; p.peakAt = 0.35f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.15f } };
                    p.vibrato = 1.1f; p.vibratoHeld = 1.0f; p.wobbleShare = 0.3f; p.diveShare = 0.25f;
                    p.endings = { { Ending::dive, 1.0f } };
                    p.description = "In on a scream, then repeated double stops answered by single bent notes, a flurry "
                                    "of triplet cells under a fixed top note, held notes shaken with the whammy, a dive to end";
                    break;
                // (0.134.0, the owner's Pantera solos, shapes only. Every one: straight, almost no rests, whole-step
                // bends with one big over-bend, the key's notes over the riff, the whammy bar at phrase ends.)
                case SoloStyle::burstBlocksSqueals:
                    p.flatFive = p.aeolian = p.blocks = true;
                    p.notesPerBeat = 1.6f; p.holdMin = 1.0; p.holdMax = 3.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.03f; p.restBeats = 0.5;
                    p.waves = 2; p.waveBars = 4; p.burst = Burst::dimCells; p.climb = 14; p.peakAt = 0.8f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 3.0f, 0.2f } }; p.releaseShare = 0.05f;
                    p.vibrato = 1.2f; p.vibratoHeld = 0.8f; p.squeals = 4; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::squealOut, 1.0f }, { Ending::bendThenDive, 1.0f } };
                    p.description = "Calm bars of held bends and whole blocks of fire: diminished cells climbing by fourths, a "
                                    "top note struck four times and dropped, whammy squeals at the phrase ends, a squeal to end";
                    break;
                case SoloStyle::bendsThenClimb:
                    p.notesPerBeat = 0.8f; p.holdMin = 2.0; p.holdMax = 4.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.03f; p.restBeats = 0.5;
                    p.waves = 1; p.waveAts = { 0.8f }; p.burst = Burst::cellClimb; p.climb = 12; p.peakAt = 0.9f;
                    p.bendChance = 0.85f; p.bends = { { 2.0f, 1.0f }, { 3.0f, 0.15f } }; p.releaseShare = 0.1f;
                    p.vibrato = 1.0f; p.vibratoHeld = 0.6f; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::bendThenDive, 1.0f } };
                    p.description = "Three quarters held bends, nearly every note bent; then one burst of cells climbing a "
                                    "step a beat for two octaves, bent into the last note and dived away";
                    break;
                case SoloStyle::bottomToTopClimb:
                    p.endHigh = true;
                    p.notesPerBeat = 2.2f; p.holdMin = 1.0; p.holdMax = 3.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.08f; p.restBeats = 0.5;
                    p.waves = 2; p.waveAts = { 0.1f, 0.6f }; p.burst = Burst::cellLoop; p.climb = 30;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.25f }, { 3.0f, 0.25f } }; p.releaseShare = 0.1f;
                    p.vibrato = 1.0f; p.vibratoHeld = 0.5f; p.squeals = 2; p.diveShare = 0.15f; p.scoopShare = 0.1f;
                    p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::bendThenDive, 1.0f } };
                    p.description = "From the bottom of the neck to the top: lines climbing three octaves, a pull-off cell and "
                                    "a bend looped eight times, squeals and dives, the top note bent into the root and dived";
                    break;
                case SoloStyle::burstBarBurst:
                    p.flatFive = true;
                    p.notesPerBeat = 1.4f; p.holdMin = 1.0; p.holdMax = 2.5; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.0f; p.restBeats = 0.5;
                    p.waves = 2; p.waveAts = { 0.25f, 0.88f }; p.burst = Burst::cellClimb; p.climb = 14; p.peakAt = 0.9f;
                    p.bendChance = 0.4f; p.bends = { { 2.0f, 1.0f }, { 3.0f, 0.3f } };
                    p.vibrato = 1.2f; p.vibratoHeld = 0.8f; p.wobbleShare = 0.5f; p.scoopShare = 0.25f; p.squeals = 4;
                    p.doubleStops = p.fourthDoubleStops = true; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::bendUp, 1.0f } };
                    p.description = "A storm of fast cells, then a wild middle on the whammy bar (notes shaken, scooped, "
                                    "squealed, fourths rocked), then a climb to the top note bent into the root and held";
                    break;
                case SoloStyle::balladIntoFire:
                    p.aeolian = p.twoSpeed = true;
                    p.notesPerBeat = 1.0f; p.holdMin = 2.0; p.holdMax = 5.0; p.landBeat1 = 0.4f; p.landThirdFifth = true;
                    p.restChance = 0.03f; p.restBeats = 0.5; p.phraseBars = 2;
                    p.burst = Burst::fire; p.climb = 14;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.3f }, { 5.0f, 0.1f } }; p.bigFrom = 0.45f;
                    p.vibrato = 1.0f; p.vibratoHeld = 0.6f; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::dive, 1.0f } };
                    p.description = "Half a ballad (long held notes on the chord's third and root), then fire when the band "
                                    "lifts: cells cycled, a climb, cells falling a half step each beat, and the top note dived";
                    break;
                case SoloStyle::halfSpeedBentCells:
                    p.aeolian = true;
                    p.notesPerBeat = 2.4f; p.holdMin = 1.0; p.holdMax = 2.0; p.landBeat1 = 0.5f; p.phraseBars = 1;
                    p.restChance = 0.0f; p.restBeats = 0.5;
                    p.waves = 2; p.burst = Burst::bentCells; p.climb = 20; p.peakAt = 0.7f;
                    p.bendChance = 0.6f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.5f }, { 3.0f, 0.4f } }; p.releaseShare = 0.3f;
                    p.vibrato = 1.0f; p.vibratoHeld = 0.9f; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::held, 1.0f } };
                    p.description = "For slow songs: thirty-seconds of bent cells, pre-bends let down again and again, a note "
                                    "shaken between a whole-step and a minor-third bend, climbing to the end";
                    break;
                case SoloStyle::pedalClimbTremolo:
                    p.notesPerBeat = 1.8f; p.holdMin = 1.0; p.holdMax = 3.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.03f; p.restBeats = 0.5;
                    p.waves = 2; p.burst = Burst::pedalClimb; p.climb = 14; p.peakAt = 0.67f;
                    p.bendChance = 0.55f; p.bends = { { 2.0f, 1.0f }, { 3.0f, 0.5f }, { 1.0f, 0.3f } }; p.releaseShare = 0.2f;
                    p.vibrato = 1.0f; p.vibratoHeld = 0.6f; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::tremoloDyads, 1.0f } };
                    p.description = "A pedal note under a line climbing on, minor-third bends on the held notes, and to end "
                                    "a tremolo-picked double stop over the pedal climbing into a held fifth";
                    break;
                case SoloStyle::slowStaircaseBends:
                    p.endHigh = true;
                    p.notesPerBeat = 0.6f; p.holdMin = 2.0; p.holdMax = 5.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.0f; p.restBeats = 0.5;
                    p.waves = 1; p.waveAts = { 0.75f }; p.burst = Burst::sameBend; p.climb = 19;
                    p.bendChance = 0.6f; p.bends = { { 2.0f, 1.0f }, { 3.0f, 0.5f } }; p.releaseShare = 0.3f;
                    p.vibrato = 1.0f; p.vibratoHeld = 0.9f; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::held, 1.0f } };
                    p.description = "Slow and climbing: long notes struck again, each phrase a step higher, then one "
                                    "minor-third bend struck again and again at the top, and the root held long";
                    break;
                case SoloStyle::loopedPullOffs:
                    p.flatFive = true;
                    p.notesPerBeat = 1.8f; p.holdMin = 1.0; p.holdMax = 2.5; p.landBeat1 = 0.5f; p.phraseBars = 1;
                    p.restChance = 0.05f; p.restBeats = 0.5;
                    p.waves = 3; p.burst = Burst::cellLoop; p.climb = 14; p.peakAt = 0.6f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 3.0f, 0.2f } }; p.releaseShare = 0.1f;
                    p.vibrato = 1.2f; p.vibratoHeld = 0.5f; p.squeals = 2; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::bendThenDive, 1.0f }, { Ending::bendUp, 0.5f } };
                    p.description = "Built from one-beat cells: three notes pulled off, then a bend, looped and moved up, "
                                    "held bends between them, squeals, the last note bent and dived";
                    break;
                case SoloStyle::tritoneStaircase:
                    p.flatFive = p.tritoneOpening = p.blocks = true;
                    p.notesPerBeat = 1.6f; p.holdMin = 1.5; p.holdMax = 3.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.03f; p.restBeats = 0.5;
                    p.waves = 2; p.waveBars = 4; p.burst = Burst::tritoneCells; p.climb = 14; p.peakAt = 0.85f;
                    p.bendChance = 0.5f; p.bends = { { 2.0f, 1.0f }, { 3.0f, 0.15f } };
                    p.vibrato = 1.2f; p.vibratoHeld = 0.9f; p.squeals = 3; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::bendUp, 1.0f } };
                    p.description = "Four bars of held notes on the key's root and flat five, each a tritone higher, then "
                                    "blocks of cells moved up a fourth each beat, squeals, a bend into the root to end";
                    break;
                case SoloStyle::shuffleSnarl:
                    p.flatFive = p.majorThird = p.curls = true;
                    p.notesPerBeat = 1.8f; p.holdMin = 1.0; p.holdMax = 2.5; p.landBeat1 = 0.4f; p.phraseBars = 2;
                    p.restChance = 0.03f; p.restBeats = 0.5; p.swing = 0.67f; p.swing8 = true;
                    p.waves = 2; p.burst = Burst::dimTriplets; p.climb = 12; p.peakAt = 0.4f;
                    p.bendChance = 0.55f; p.bends = { { 2.0f, 1.0f }, { 3.0f, 0.4f }, { 4.0f, 0.15f } }; p.bigFrom = 0.3f;
                    p.vibrato = 1.1f; p.vibratoHeld = 0.7f; p.squeals = 2; p.slideShare = 0.1f; p.fallShare = 0.0f;
                    p.endings = { { Ending::bendThenDive, 1.0f } };
                    p.description = "A snarling shuffle blues: both thirds, the flat five, triplet runs, diminished triplets "
                                    "climbing a minor third a beat, big bends, the bar fluttering, a bend into a dive to end";
                    break;
                case SoloStyle::breakdownTrill:
                    p.aeolian = p.middlePart = true;
                    p.notesPerBeat = 1.2f; p.holdMin = 1.5; p.holdMax = 3.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.03f; p.restBeats = 0.5;
                    p.waves = 1; p.burst = Burst::thirdTrill; p.climb = 9; p.peakAt = 0.5f;
                    p.bendChance = 0.4f; p.bends = { { 2.0f, 1.0f } };
                    p.vibrato = 1.0f; p.vibratoHeld = 0.8f; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::dive, 1.0f } };
                    p.description = "For a breakdown: the minor and major third trilled in thirty-seconds under a top note "
                                    "that swaps between the fifth and the flat sixth each bar, harmonised a minor third under";
                    break;
                case SoloStyle::slowEasternTwins:
                    p.phrygianDominant = p.twin = p.twinTenth = true;
                    p.notesPerBeat = 0.5f; p.holdMin = 4.0; p.holdMax = 7.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.0f; p.restBeats = 0.5;
                    p.waves = 0; p.climb = 5; p.peakAt = 0.7f;
                    p.bendChance = 0.2f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.5f } };
                    p.vibrato = 0.8f; p.vibratoHeld = 0.6f; p.scoopShare = 0.45f; p.scoopAmount = 3.0f; p.wobbleShare = 0.1f;
                    p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::tripletClimb, 1.0f } };
                    p.description = "Slow and eastern (the flat second and the major third over the minor): long notes "
                                    "scooped up from a minor third under, a second guitar an octave and a third above, a climb out to end";
                    break;
                case SoloStyle::heldLineRisingCells:
                    p.aeolian = p.heldChordOpening = true;
                    p.notesPerBeat = 0.8f; p.holdMin = 1.5; p.holdMax = 3.0; p.landBeat1 = 0.4f; p.phraseBars = 2;
                    p.restChance = 0.0f; p.restBeats = 0.5;
                    p.waves = 1; p.waveAts = { 0.62f }; p.waveBars = 4; p.burst = Burst::risingBentCells; p.climb = 10; p.peakAt = 0.8f;
                    p.bendChance = 0.3f; p.bends = { { 2.0f, 1.0f } };
                    p.vibrato = 0.9f; p.vibratoHeld = 0.9f; p.diveShare = 0.05f; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::held, 1.0f } };
                    p.description = "One long note on each chord, each coming in an eighth early with a rake into it, then a "
                                    "three-note cell bent a half step climbing a scale step each time into a long last note";
                    break;
                case SoloStyle::statedRaisedDropped:
                    p.restate = p.restateEarly = p.lowLastQuarter = p.longFirstRest = p.oneOverbend = p.rakes = true;
                    p.notesPerBeat = 1.2f; p.holdMin = 1.5; p.holdMax = 3.0; p.landBeat1 = 0.5f; p.phraseBars = 2;
                    p.restChance = 0.03f; p.restBeats = 0.5;
                    p.waves = 0; p.climb = 12; p.peakAt = 0.25f;
                    p.bendChance = 0.6f; p.bends = { { 2.0f, 1.0f } };
                    p.vibrato = 1.0f; p.vibratoHeld = 0.6f; p.diveShare = 0.2f; p.slideShare = 0.0f; p.fallShare = 0.0f;
                    p.endings = { { Ending::dive, 1.0f } };
                    p.description = "A raked phrase held and bent, then a long silence; the same idea an octave higher; one "
                                    "big over-bend past halfway; the last quarter down low, dived an octave to end";
                    break;
                case SoloStyle::automatic: break;
            }
            if (p.notesPerBeat <= 0.0f)
                p.notesPerBeat = (p.startDensity + p.peakDensity) * 0.5f / 4.0f;   // (the player styles: their old densities)
            return p;
        }

        const ChordSpan& spanAt (const std::vector<ChordSpan>& spans, double songBeat)
        {
            for (const auto& s : spans)
                if (songBeat < s.end - 1.0e-9)
                    return s;
            return spans.back();
        }

        bool has (const std::vector<int>& list, int v) { return std::find (list.begin(), list.end(), v) != list.end(); }

        std::vector<int> chordPcs (const Chord& c)
        {
            const auto t = c.pitchClasses();
            return { t.begin(), t.end() };
        }

        // The scale over a chord: minor pentatonic on the key's minor root (its relative minor in a major key:
        // the major pentatonic) and the style's added notes; over a major chord, where the style does it, the
        // chord's own major pentatonic.
        std::vector<int> scaleOver (const ChordSpan& span, const Profile& p)
        {
            std::vector<int> out;
            const auto add = [&out] (int pc) { if (! has (out, pcOf (pc))) out.push_back (pcOf (pc)); };
            if (p.noThird)
            {
                // (0.133.0) The rock vamp's: the key's tonic, 2nd, 4th, 5th and b7, whatever the chord.
                for (const auto s : { 0, 2, 5, 7, 10 })
                    add (span.key.pitchClass() + s);
                return out;
            }
            if (p.majorOnMajor && span.chord.type == Chord::Type::major)
            {
                for (const auto s : { 0, 2, 4, 7, 9 })
                    add (span.chord.root + s);
                if (p.blueOnMajor) { add (span.chord.root + 3); add (span.chord.root + 10); }
                return out;
            }
            const auto minorRoot = span.key.minor ? span.key.pitchClass() : span.key.pitchClass() + 9;
            for (const auto s : { 0, 3, 5, 7, 10 })
                add (minorRoot + s);
            if (p.ninth) add (minorRoot + 2);
            if (p.sixth) add (minorRoot + 9);
            if (p.majorThird) add (minorRoot + 4);
            if (p.aeolian) { add (minorRoot + 2); add (minorRoot + 8); }
            if (p.gypsy) { add (minorRoot + 2); add (minorRoot + 8); add (minorRoot + 11); }
            if (p.flatFive) add (minorRoot + 6);
            if (p.phrygianDominant) { add (minorRoot + 1); add (minorRoot + 4); add (minorRoot + 8); }
            return out;
        }

        std::vector<int> poolOf (const std::vector<int>& pcs, int low, int high)
        {
            std::vector<int> out;
            for (auto n = low; n <= high; ++n)
                if (has (pcs, pcOf (n)))
                    out.push_back (n);
            return out;
        }

        int nearestIndex (const std::vector<int>& pool, double pitch)
        {
            auto best = 0;
            for (size_t i = 1; i < pool.size(); ++i)
                if (std::abs (pool[i] - pitch) < std::abs (pool[(size_t) best] - pitch))
                    best = (int) i;
            return best;
        }

        // The nearest pitch in [low, high] with one of `pcs`.
        int nearestOf (const std::vector<int>& pcs, double around, int low, int high)
        {
            auto best = -1;
            for (auto n = low; n <= high; ++n)
                if (has (pcs, pcOf (n)) && (best < 0 || std::abs (n - around) < std::abs (best - around)))
                    best = n;
            return best < 0 ? juce::jlimit (low, high, juce::roundToInt (around)) : best;
        }

        struct Motif
        {
            std::vector<double> at;   // beats into the phrase
            std::vector<int> steps;   // scale steps from the note before (the first: 0)
        };

        // A phrase's rhythm: `count` notes on the grid inside `playable` beats, strong places first.
        std::vector<double> rhythmOf (int count, double playable, double grid, juce::Random& r, bool pickup)
        {
            std::vector<double> slots;
            for (auto x = 0.0; x < playable - 1.0e-9; x += grid)
                slots.push_back (x);
            count = juce::jlimit (1, juce::jmax (1, (int) slots.size()), count);
            std::vector<std::pair<double, double>> scored;   // (score, place)
            for (const auto x : slots)
            {
                const auto onBeat = std::abs (x - std::round (x)) < 1.0e-6;
                const auto onHalf = std::abs (x * 2.0 - std::round (x * 2.0)) < 1.0e-6;
                auto w = onBeat ? 1.0 : onHalf ? 0.7 : 0.45;
                if (pickup && x < 1.0e-6)
                    w *= 0.4;   // (a pickup start: off the first beat)
                scored.push_back ({ w * (0.6 + 0.8 * r.nextDouble()), x });
            }
            std::sort (scored.begin(), scored.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
            std::vector<double> out;
            for (int i = 0; i < count; ++i)
                out.push_back (scored[(size_t) i].second);
            std::sort (out.begin(), out.end());
            return out;
        }

        // An arch: up through the first half, down the second; steps of one, now and then two.
        std::vector<int> contourOf (int count, juce::Random& r)
        {
            std::vector<int> steps { 0 };
            for (int i = 1; i < count; ++i)
            {
                const auto rising = i < (count + 1) / 2;
                auto s = r.nextFloat() < 0.25f ? 2 : 1;
                if (r.nextFloat() < 0.2f)
                    s = -s;   // (a turn)
                steps.push_back (rising ? s : -s);
            }
            return steps;
        }

        void addNote (std::vector<LineNote>& notes, double at, double length, int pitch, int velocity)
        {
            LineNote n;
            n.beat = at;
            n.length = juce::jmax (0.05, length);
            n.pitch = pitch;
            n.velocity = GuitarVelocity::of (juce::jlimit (1, 127, velocity));
            notes.push_back (n);
        }
    }

    //==========================================================================
    const std::vector<SoloStyle>& soloStyles()
    {
        static const std::vector<SoloStyle> all { SoloStyle::automatic, SoloStyle::soaringBends, SoloStyle::rockSwagger,
                                                  SoloStyle::tapAndDive, SoloStyle::bluesHammer,
                                                  SoloStyle::screamAndDive, SoloStyle::thrashFire, SoloStyle::pinchSqueal,
                                                  SoloStyle::progPrecision, SoloStyle::glamFlash, SoloStyle::sleazeBlues,
                                                  SoloStyle::topHatMelody, SoloStyle::purpleScream, SoloStyle::gypsyShred,
                                                  SoloStyle::tensionRelease, SoloStyle::torturedBends, SoloStyle::grungeMelody,
                                                  SoloStyle::singingBends, SoloStyle::shuffleConversation, SoloStyle::slowBuildRakes,
                                                  SoloStyle::fallingMajorMelody, SoloStyle::sungInTheGaps, SoloStyle::rollingSequences,
                                                  SoloStyle::laidBackBends, SoloStyle::cycleTheme, SoloStyle::twoSpeeds, SoloStyle::lickTwice,
                                                  SoloStyle::repeatedBendClimb, SoloStyle::chordChasingArpeggios, SoloStyle::pedalDyadHook,
                                                  SoloStyle::hotStartCoolDown, SoloStyle::unisonBendAnthem, SoloStyle::octaveMelodyBuild,
                                                  SoloStyle::openStringPedalFire, SoloStyle::twinLeadHarmony, SoloStyle::easternTremoloStorm,
                                                  SoloStyle::whammyWail, SoloStyle::trillFallTag,
                                                  SoloStyle::cellWithTheChanges, SoloStyle::holdAndAccelerate, SoloStyle::whammyFire,
                                                  SoloStyle::legatoThrows, SoloStyle::squealsAndSextuplets, SoloStyle::chimingFourths,
                                                  SoloStyle::dottedBendChain, SoloStyle::hookThirdsHook, SoloStyle::bentRiffSolo,
                                                  SoloStyle::tremoloPedalOutro, SoloStyle::burstAndBend,
                                                  SoloStyle::burstBlocksSqueals, SoloStyle::bendsThenClimb, SoloStyle::bottomToTopClimb,
                                                  SoloStyle::burstBarBurst, SoloStyle::balladIntoFire, SoloStyle::halfSpeedBentCells,
                                                  SoloStyle::pedalClimbTremolo, SoloStyle::slowStaircaseBends, SoloStyle::loopedPullOffs,
                                                  SoloStyle::tritoneStaircase, SoloStyle::shuffleSnarl, SoloStyle::breakdownTrill,
                                                  SoloStyle::slowEasternTwins, SoloStyle::heldLineRisingCells, SoloStyle::statedRaisedDropped };
        return all;
    }

    juce::String soloStyleName (SoloStyle s)
    {
        switch (s)
        {
            case SoloStyle::automatic: return "BY STYLE";
            case SoloStyle::soaringBends: return "SOARING BENDS";
            case SoloStyle::rockSwagger: return "ROCK 'N' ROLL SWAGGER";
            case SoloStyle::tapAndDive: return "TAP & DIVE";
            case SoloStyle::bluesHammer: return "BLUES HAMMER";
            case SoloStyle::screamAndDive: return "SCREAM & DIVE";
            case SoloStyle::thrashFire: return "THRASH FIRE";
            case SoloStyle::pinchSqueal: return "PINCH SQUEAL";
            case SoloStyle::progPrecision: return "PROG PRECISION";
            case SoloStyle::glamFlash: return "GLAM FLASH";
            case SoloStyle::sleazeBlues: return "SLEAZE BLUES";
            case SoloStyle::topHatMelody: return "TOP HAT MELODY";
            case SoloStyle::purpleScream: return "PURPLE SCREAM";
            case SoloStyle::gypsyShred: return "GYPSY SHRED";
            case SoloStyle::tensionRelease: return "TENSION & RELEASE";
            case SoloStyle::torturedBends: return "TORTURED BENDS";
            case SoloStyle::grungeMelody: return "GRUNGE MELODY";
            case SoloStyle::singingBends: return "SINGING BENDS";
            case SoloStyle::shuffleConversation: return "SHUFFLE CONVERSATION";
            case SoloStyle::slowBuildRakes: return "SLOW BUILD WITH RAKES";
            case SoloStyle::fallingMajorMelody: return "FALLING MAJOR MELODY";
            case SoloStyle::sungInTheGaps: return "SUNG IN THE GAPS";
            case SoloStyle::rollingSequences: return "ROLLING SEQUENCES";
            case SoloStyle::laidBackBends: return "LAID-BACK SWEET BENDS";
            case SoloStyle::cycleTheme: return "THEME EVERY ROUND";
            case SoloStyle::twoSpeeds: return "TWO SPEEDS";
            case SoloStyle::lickTwice: return "LICK TWICE TO END";
            case SoloStyle::repeatedBendClimb: return "REPEATED BEND CLIMB";
            case SoloStyle::chordChasingArpeggios: return "CHORD-CHASING ARPEGGIOS";
            case SoloStyle::pedalDyadHook: return "PEDAL DYAD HOOK";
            case SoloStyle::hotStartCoolDown: return "HOT START, COOL DOWN";
            case SoloStyle::unisonBendAnthem: return "UNISON BEND ANTHEM";
            case SoloStyle::octaveMelodyBuild: return "OCTAVE MELODY BUILD";
            case SoloStyle::openStringPedalFire: return "OPEN-STRING PEDAL FIRE";
            case SoloStyle::twinLeadHarmony: return "TWIN-LEAD HARMONY";
            case SoloStyle::easternTremoloStorm: return "EASTERN TREMOLO STORM";
            case SoloStyle::whammyWail: return "WHAMMY WAIL";
            case SoloStyle::trillFallTag: return "TRILL & FALL TAG";
            case SoloStyle::cellWithTheChanges: return "CELL WITH THE CHANGES";
            case SoloStyle::holdAndAccelerate: return "HOLD & ACCELERATE";
            case SoloStyle::whammyFire: return "WHAMMY FIRE";
            case SoloStyle::legatoThrows: return "LEGATO THROWS";
            case SoloStyle::squealsAndSextuplets: return "SQUEALS & SEXTUPLETS";
            case SoloStyle::chimingFourths: return "CHIMING FOURTHS";
            case SoloStyle::dottedBendChain: return "DOTTED BEND CHAIN";
            case SoloStyle::hookThirdsHook: return "HOOK, THIRDS, HOOK";
            case SoloStyle::bentRiffSolo: return "BENT RIFF SOLO";
            case SoloStyle::tremoloPedalOutro: return "TREMOLO PEDAL OUTRO";
            case SoloStyle::burstAndBend: return "BURST AND BEND";
            case SoloStyle::burstBlocksSqueals: return "BURST BLOCKS & SQUEALS";
            case SoloStyle::bendsThenClimb: return "BENDS, THEN THE CLIMB";
            case SoloStyle::bottomToTopClimb: return "BOTTOM-TO-TOP CLIMB";
            case SoloStyle::burstBarBurst: return "BURST, BAR, BURST";
            case SoloStyle::balladIntoFire: return "BALLAD INTO FIRE";
            case SoloStyle::halfSpeedBentCells: return "HALF-SPEED BENT CELLS";
            case SoloStyle::pedalClimbTremolo: return "PEDAL CLIMB & TREMOLO DYADS";
            case SoloStyle::slowStaircaseBends: return "SLOW STAIRCASE OF BENDS";
            case SoloStyle::loopedPullOffs: return "LOOPED PULL-OFF CELLS";
            case SoloStyle::tritoneStaircase: return "TRITONE STAIRCASE";
            case SoloStyle::shuffleSnarl: return "SHUFFLE SNARL";
            case SoloStyle::breakdownTrill: return "BREAKDOWN TRILL";
            case SoloStyle::slowEasternTwins: return "SLOW EASTERN TWINS";
            case SoloStyle::heldLineRisingCells: return "HELD LINE & RISING CELLS";
            case SoloStyle::statedRaisedDropped: return "STATED, RAISED, DROPPED";
        }
        return "BY STYLE";
    }

    SoloStyle soloStyleNamed (const juce::String& name)
    {
        for (const auto s : soloStyles())
            if (soloStyleName (s).equalsIgnoreCase (name.trim()))
                return s;
        return SoloStyle::automatic;
    }

    juce::String soloStyleDescription (SoloStyle s)
    {
        if (s == SoloStyle::automatic)
            return "The guitar STYLE's own: rock and nu-metal SINGING BENDS, thrash and groove LICK TWICE TO END, "
                   "metalcore ROLLING SEQUENCES, shred TWO SPEEDS";
        return profileOf (s).description;
    }

    SoloStyle soloStyleForGuitarStyle (int guitarStyle)
    {
        switch (guitarStyle)
        {
            case 1: case 5: return SoloStyle::lickTwice;          // (thrash, groove)
            case 3: return SoloStyle::rollingSequences;           // (metalcore)
            case 4: return SoloStyle::twoSpeeds;                  // (shred)
            default: return SoloStyle::singingBends;              // (rock, nu-metal)
        }
    }

    //==========================================================================
    LineIdea composeSolo (SoloStyle style, const std::vector<ChordSpan>& spans, double beatsPerBar, const GuitarTuning& tuning,
                          const GuitarVelocities& vel, int variation, double shortest, double bpm)
    {
        const auto titled = [] (SoloStyle st) -> juce::String
        {
            switch (st)
            {
                case SoloStyle::soaringBends: return "Soaring Bends";
                case SoloStyle::rockSwagger: return "Rock 'n' Roll Swagger";
                case SoloStyle::tapAndDive: return "Tap & Dive";
                case SoloStyle::bluesHammer: return "Blues Hammer";
                case SoloStyle::screamAndDive: return "Scream & Dive";
                case SoloStyle::thrashFire: return "Thrash Fire";
                case SoloStyle::pinchSqueal: return "Pinch Squeal";
                case SoloStyle::progPrecision: return "Prog Precision";
                case SoloStyle::glamFlash: return "Glam Flash";
                case SoloStyle::sleazeBlues: return "Sleaze Blues";
                case SoloStyle::topHatMelody: return "Top Hat Melody";
                case SoloStyle::purpleScream: return "Purple Scream";
                case SoloStyle::gypsyShred: return "Gypsy Shred";
                case SoloStyle::tensionRelease: return "Tension & Release";
                case SoloStyle::torturedBends: return "Tortured Bends";
                case SoloStyle::grungeMelody: return "Grunge Melody";
                case SoloStyle::singingBends: return "Singing Bends";
                case SoloStyle::shuffleConversation: return "Shuffle Conversation";
                case SoloStyle::slowBuildRakes: return "Slow Build With Rakes";
                case SoloStyle::fallingMajorMelody: return "Falling Major Melody";
                case SoloStyle::sungInTheGaps: return "Sung In The Gaps";
                case SoloStyle::rollingSequences: return "Rolling Sequences";
                case SoloStyle::laidBackBends: return "Laid-Back Sweet Bends";
                case SoloStyle::cycleTheme: return "Theme Every Round";
                case SoloStyle::twoSpeeds: return "Two Speeds";
                case SoloStyle::lickTwice: return "Lick Twice To End";
                case SoloStyle::repeatedBendClimb: return "Repeated Bend Climb";
                case SoloStyle::chordChasingArpeggios: return "Chord-Chasing Arpeggios";
                case SoloStyle::pedalDyadHook: return "Pedal Dyad Hook";
                case SoloStyle::hotStartCoolDown: return "Hot Start, Cool Down";
                case SoloStyle::unisonBendAnthem: return "Unison Bend Anthem";
                case SoloStyle::octaveMelodyBuild: return "Octave Melody Build";
                case SoloStyle::openStringPedalFire: return "Open-String Pedal Fire";
                case SoloStyle::twinLeadHarmony: return "Twin-Lead Harmony";
                case SoloStyle::easternTremoloStorm: return "Eastern Tremolo Storm";
                case SoloStyle::whammyWail: return "Whammy Wail";
                case SoloStyle::trillFallTag: return "Trill & Fall Tag";
                case SoloStyle::cellWithTheChanges: return "Cell With The Changes";
                case SoloStyle::holdAndAccelerate: return "Hold & Accelerate";
                case SoloStyle::whammyFire: return "Whammy Fire";
                case SoloStyle::legatoThrows: return "Legato Throws";
                case SoloStyle::squealsAndSextuplets: return "Squeals & Sextuplets";
                case SoloStyle::chimingFourths: return "Chiming Fourths";
                case SoloStyle::dottedBendChain: return "Dotted Bend Chain";
                case SoloStyle::hookThirdsHook: return "Hook, Thirds, Hook";
                case SoloStyle::bentRiffSolo: return "Bent Riff Solo";
                case SoloStyle::tremoloPedalOutro: return "Tremolo Pedal Outro";
                case SoloStyle::burstAndBend: return "Burst And Bend";
                case SoloStyle::burstBlocksSqueals: return "Burst Blocks & Squeals";
                case SoloStyle::bendsThenClimb: return "Bends, Then The Climb";
                case SoloStyle::bottomToTopClimb: return "Bottom-To-Top Climb";
                case SoloStyle::burstBarBurst: return "Burst, Bar, Burst";
                case SoloStyle::balladIntoFire: return "Ballad Into Fire";
                case SoloStyle::halfSpeedBentCells: return "Half-Speed Bent Cells";
                case SoloStyle::pedalClimbTremolo: return "Pedal Climb & Tremolo Dyads";
                case SoloStyle::slowStaircaseBends: return "Slow Staircase Of Bends";
                case SoloStyle::loopedPullOffs: return "Looped Pull-Off Cells";
                case SoloStyle::tritoneStaircase: return "Tritone Staircase";
                case SoloStyle::shuffleSnarl: return "Shuffle Snarl";
                case SoloStyle::breakdownTrill: return "Breakdown Trill";
                case SoloStyle::slowEasternTwins: return "Slow Eastern Twins";
                case SoloStyle::heldLineRisingCells: return "Held Line & Rising Cells";
                case SoloStyle::statedRaisedDropped: return "Stated, Raised, Dropped";
                case SoloStyle::automatic: break;
            }
            return "By Style";
        };
        LineIdea solo { LineIdea::Part::lead, "Solo: " + titled (style), {}, {} };
        if (spans.empty() || beatsPerBar <= 0.0 || style == SoloStyle::automatic)
            return solo;
        auto p = profileOf (style);
        if (style == SoloStyle::grungeMelody && variation % 4 == 3)
            p.burstFrom = 0.4f;   // (0.69.0) one take in four noisy
        juce::Random r ((juce::int64) variation * 7919 + (int) style * 131 + 17);
        const auto origin = spans.front().start;
        const auto total = spans.back().end - origin;
        const auto low = juce::jmax (tuning.strings.size() > 1 ? tuning.strings[1] + 5 : tuning.lowest(), 45);
        const auto high = tuning.strings.back() + 22;
        const auto chordAt = [&] (double beat) -> const ChordSpan& { return spanAt (spans, beat + origin); };
        const auto chordTones = [&] (double beat) { return chordPcs (chordAt (beat).chord); };
        const auto scaleAt = [&] (double beat) { return scaleOver (chordAt (beat), p); };
        // (0.128.0) The notes a bend sings into over the chord there: its root and fifth, the minor key's fourth; over
        // a major chord its third and sixth too (the owner's Def Leppard solos: the 2nd up to the 3rd, the 5th to the 6th).
        const auto aimsAt = [&] (double beat)
        {
            const auto& span = chordAt (beat);
            const auto& c = span.chord;
            std::vector<int> out { pcOf (c.root), pcOf (c.root + (c.type == Chord::Type::diminished ? 6 : c.type == Chord::Type::augmented ? 8 : 7)) };
            if (c.type == Chord::Type::major)
            {
                out.push_back (pcOf (c.root + 4));
                out.push_back (pcOf (c.root + 9));
            }
            else
                out.push_back (pcOf ((span.key.minor ? span.key.pitchClass() : span.key.pitchClass() + 9) + 5));
            return out;
        };
        const auto aimAt = [&] (double beat, double around) { return nearestOf (aimsAt (beat), around, low + 2, high); };
        const auto phraseLength = p.phraseBars * beatsPerBar;
        const auto phrases = juce::jmax (1, (int) std::ceil (total / phraseLength - 1.0e-6));
        const auto shortestStep = shortest > 0.0 ? shortest : 0.0;
        const auto gridFor = [shortestStep] (double wanted) { return juce::jmax (wanted, shortestStep); };
        // (0.128.0) A fast passage's grid: as written when it runs at 7 to 13 notes a second at the song's tempo, else
        // the same kind (straight or triplet) nearest 10 a second (the owner's Metallica and Def Leppard solos: 8 to
        // 12 whatever the tempo); never faster than `shortest`. No tempo given: as written.
        const auto runGrid = [&gridFor, bpm] (double wanted)
        {
            if (bpm <= 0.0)
                return gridFor (wanted);
            const auto rate = [bpm] (double g) { return bpm / 60.0 / g; };
            if (rate (wanted) >= 7.0 && rate (wanted) <= 13.0)
                return gridFor (wanted);
            const auto triplet = std::abs (wanted * 3.0 - std::round (wanted * 3.0)) < 1.0e-6
                              && std::abs (wanted * 4.0 - std::round (wanted * 4.0)) > 1.0e-6;
            const std::vector<double> kinds = triplet ? std::vector<double> { 1.0 / 6.0, 1.0 / 3.0, 2.0 / 3.0 }
                                                      : std::vector<double> { 0.125, 0.25, 0.5 };
            auto best = wanted;
            for (const auto g : kinds)
                if (std::abs (rate (g) - 10.0) < std::abs (rate (best) - 10.0))
                    best = g;
            return gridFor (best);
        };
        auto& notes = solo.notes;

        std::vector<double> tensionTargets;
        const auto velocityAt = [&] (float t, bool accent)
        {
            const auto base = (float) vel.sustainSoft + ((float) vel.sustainHard - (float) vel.sustainSoft) * t;
            return vel.open (juce::roundToInt (base) + (accent ? 6 : 0));
        };

        std::vector<std::pair<double, double>> burstSpans;   // (from, to): no bends or vibrato added there
        // The style's signature passage from `burstFrom` to `landing` (0.116.0: placed as waves, not every other phrase).
        // (0.134.0) Three-note cells (up, up, back) played twice a beat, each beat a scale step higher, round again
        // two octaves up; and three-note cells in triplets, each beat a half step lower.
        const auto writeCellClimb = [&] (double from, double to, double startPitch, float t)
        {
            const auto g = runGrid (1.0 / 6.0);
            const int cell[] = { 0, 1, 2 };
            auto base = startPitch;
            auto k = 0;
            for (auto x = from; x < to - 1.0e-9; x += g, ++k)
            {
                const auto here = poolOf (scaleAt (x), low, high);
                if (here.empty())
                    continue;
                const auto beatIndex = (int) std::floor (x - from + 1.0e-6);
                auto at = nearestIndex (here, base) + beatIndex;
                if (at + 2 >= (int) here.size())
                {
                    base = startPitch;   // (round again)
                    at = nearestIndex (here, base);
                }
                addNote (notes, x, g, here[(size_t) juce::jlimit (0, (int) here.size() - 1, at + cell[k % 3])], velocityAt (t, k % 3 == 0));
            }
        };
        const auto writeChromaticDown = [&] (double from, double to, double startPitch, float t)
        {
            const auto g = gridFor (1.0 / 3.0);
            const int cell[] = { 0, -1, -3 };
            auto k = 0;
            for (auto x = from; x < to - 1.0e-9; x += g, ++k)
            {
                const auto beatIndex = (int) std::floor (x - from + 1.0e-6);
                addNote (notes, x, g, juce::jlimit (low, high, (int) startPitch - beatIndex + cell[k % 3]), velocityAt (t, k % 3 == 0));
            }
        };
        const auto writeBurst = [&] (double burstFrom, double landing, double centre, float t, int burstIndex, bool peak)
        {
            burstSpans.push_back ({ burstFrom, landing });
            auto pool = poolOf (scaleAt (burstFrom), low, high);
            if (pool.empty())
                return;
            auto idx = nearestIndex (pool, centre + 5.0);
            juce::ignoreUnused (burstIndex, peak);
            switch (p.climaxRepeatedBends && peak ? Burst::repeatedBend : p.burst)
            {
                case Burst::repeatedBend:
                {
                    // (0.128.0) The same bend struck again and again (three to six times), then the next group a scale
                    // step higher: written under its note, bent up into it each time.
                    const auto g = runGrid (0.25);
                    auto target = aimAt (burstFrom, centre + 3.0);
                    auto inGroup = 3 + r.nextInt (4);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        if (k == inGroup)
                        {
                            const auto here = poolOf (scaleAt (x), low, high);
                            const auto at = nearestIndex (here, target);
                            target = here[(size_t) juce::jmin ((int) here.size() - 1, at + 1)];
                            k = 0;
                            inGroup = 3 + r.nextInt (4);
                        }
                        const auto semis = target - 2 >= low ? 2 : 1;
                        addNote (notes, x, g, target - semis, velocityAt (t, k == 0));
                        notes.back().effect = LineNote::Effect::bend;
                        notes.back().amount = (float) semis;
                    }
                    break;
                }
                case Burst::chordArpeggio:
                {
                    // (0.128.0) The chord's notes up and back down (root, third, fifth, octave), starting again on
                    // each new chord from the note nearest where the last left off.
                    const auto g = runGrid (1.0 / 3.0);
                    const int shape[] = { 0, 1, 2, 3, 2, 1 };
                    auto around = centre;
                    auto k = 0;
                    const ChordSpan* was = nullptr;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto& span = chordAt (x);
                        if (&span != was)
                        {
                            was = &span;
                            k = 0;
                        }
                        const auto arp = poolOf (chordTones (x), low, high);
                        if (arp.empty())
                            continue;
                        const auto base = juce::jmin (nearestIndex (arp, around), juce::jmax (0, (int) arp.size() - 4));
                        const auto pitch = arp[(size_t) juce::jlimit (0, (int) arp.size() - 1, base + shape[k % 6])];
                        addNote (notes, x, g, pitch, velocityAt (t, k % 6 == 0));
                        if (k % 6 == 0)
                            around = pitch;
                    }
                    break;
                }
                case Burst::lowPedal:
                {
                    // (0.129.0) The low tonic (the open string's note) struck between the notes of a line climbing
                    // over it, five steps and back down.
                    const auto g = runGrid (0.25);
                    const auto& first = chordAt (burstFrom);
                    const auto tonic = first.key.minor ? first.key.pitchClass() : first.key.pitchClass() + 9;
                    const auto pedal = nearestOf ({ pcOf (tonic) }, (double) low, low, high);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        if (k % 2 == 0)
                            addNote (notes, x, g, pedal, velocityAt (t, true));
                        else
                        {
                            const auto here = poolOf (scaleAt (x), low, high);
                            const auto base = nearestIndex (here, pedal + 2.0);
                            const auto step = (k / 2) % 10;
                            const auto at = juce::jlimit (0, (int) here.size() - 1, base + 1 + (step < 5 ? step : 9 - step));
                            addNote (notes, x, g, here[(size_t) at], velocityAt (t, false));
                        }
                    }
                    break;
                }
                case Burst::tremoloRun:
                {
                    // (0.129.0) Tremolo picking moving along the scale, half a beat a note (up two, down one):
                    // written as fast repeats where the instrument plays them, else each note held with the
                    // instrument's tremolo articulation.
                    const auto fast = shortestStep <= 0.13;
                    // (picks nearest 13 a second: 32nds, 16th triplets or 16ths by the tempo)
                    auto pickGrid = 0.125;
                    if (bpm > 0.0)
                        for (const auto g : { 1.0 / 6.0, 0.25 })
                            if (std::abs (bpm / 60.0 / g - 13.0) < std::abs (bpm / 60.0 / pickGrid - 13.0))
                                pickGrid = g;
                    const auto pick = fast ? gridFor (pickGrid) : 0.5;
                    auto pool = poolOf (scaleAt (burstFrom), low, high);
                    auto at = nearestIndex (pool, centre + 2.0);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += 0.5, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        at = juce::jlimit (0, (int) here.size() - 1, at + (k % 3 == 2 ? -1 : 1));
                        if (at >= (int) here.size() - 1 || at <= 0)
                            at = nearestIndex (here, centre);
                        const auto pitch = here[(size_t) at];
                        const auto until = juce::jmin (landing, x + 0.5);
                        if (fast)
                            for (auto y = x; y < until - 1.0e-9; y += pick)
                                addNote (notes, y, pick, pitch, velocityAt (t, std::abs (y - x) < 1.0e-9));
                        else
                        {
                            addNote (notes, x, until - x, pitch, velocityAt (t, true));
                            notes.back().technique = LineNote::Technique::tremolo;
                        }
                    }
                    break;
                }
                case Burst::chordCell:
                {
                    // (0.133.0) A one-bar cell of sixteenths on the chord (root, third, fourth, fifth, octave), the
                    // third on its first note: started again on each new chord, so it moves with the changes and
                    // lands on each chord's third.
                    const auto g = runGrid (0.25);
                    const int shape[] = { 1, 2, 3, 2, 1, 0, 1, 3, 4, 3, 2, 1, 0, 1, 2, 1 };
                    const ChordSpan* was = nullptr;
                    auto k = 0;
                    auto rootNote = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto& span = chordAt (x);
                        if (&span != was)
                        {
                            was = &span;
                            k = 0;
                            rootNote = nearestOf ({ span.chord.root }, centre - 2.0, low, juce::jmax (low, high - 12));
                        }
                        const auto& c = span.chord;
                        const auto third = c.type == Chord::Type::minor || c.type == Chord::Type::diminished ? 3 : 4;
                        const auto fifth = c.type == Chord::Type::diminished ? 6 : c.type == Chord::Type::augmented ? 8 : 7;
                        const int degrees[] = { 0, third, 5, fifth, 12 };
                        addNote (notes, x, g, juce::jlimit (low, high, rootNote + degrees[shape[k % 16]]), velocityAt (t, k % 4 == 0));
                    }
                    break;
                }
                case Burst::accelerate:
                {
                    // (0.133.0) One note that sings over the chord held and shaken with the whammy (four beats at
                    // most, half the passage), then struck again and again, faster beat by beat: eighths, eighth
                    // triplets, sixteenths.
                    const auto target = aimAt (burstFrom, centre + 4.0);
                    const auto hold = juce::jmin (4.0, (landing - burstFrom) * 0.5);
                    addNote (notes, burstFrom, hold - 0.02, target, velocityAt (t, true));
                    notes.back().effect = LineNote::Effect::wobble;
                    notes.back().amount = 1.0f;
                    const auto from = burstFrom + hold;
                    const auto beats = juce::jmax (1.0, std::ceil (landing - from - 1.0e-6));
                    auto k = 0;
                    for (auto b = 0; from + b < landing - 1.0e-9; ++b)
                    {
                        const auto along = (double) b / beats;
                        const auto g = gridFor (along < 0.4 ? 0.5 : along < 0.65 ? 1.0 / 3.0 : 0.25);
                        const auto until = juce::jmin (landing, from + b + 1.0);
                        for (auto x = from + b; x + g <= until + 1.0e-9; x += g, ++k)   // (whole notes of the grid only)
                            addNote (notes, x, g, target, velocityAt (t, k % 2 == 0));
                    }
                    break;
                }
                case Burst::stairs:
                {
                    // (0.133.0) A staircase: each scale note struck three to six times in eighth triplets, then a
                    // step up (back down a few when it reaches the top).
                    const auto g = runGrid (1.0 / 3.0);
                    auto pitch = pool[(size_t) nearestIndex (pool, centre - 3.0)];
                    auto left = 3 + r.nextInt (4);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        if (here.empty())
                            continue;
                        auto at = nearestIndex (here, pitch);
                        if (left == 0)
                        {
                            at = at + 1 < (int) here.size() ? at + 1 : juce::jmax (0, at - 4);
                            left = 3 + r.nextInt (4);
                            k = 0;
                        }
                        pitch = here[(size_t) at];
                        addNote (notes, x, g, pitch, velocityAt (t, k == 0));
                        --left;
                    }
                    break;
                }
                case Burst::boxCells:
                {
                    // (0.133.0) Fast legato in the key's box (the chords not followed): a six-note cell (up, up, then
                    // down three) played three times in place, then three-note cells each starting two scale notes
                    // lower, round again at the bottom.
                    const auto g = runGrid (1.0 / 6.0);
                    const int six[] = { 0, 1, 2, 1, 0, -1 };
                    const int three[] = { 0, 2, 1 };
                    const auto base = juce::jlimit (1, juce::jmax (1, (int) pool.size() - 3), nearestIndex (pool, centre + 4.0));
                    const auto drops = juce::jmax (1, base / 2);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        auto at = 0;
                        if (k < 18)
                            at = base + six[k % 6];
                        else
                        {
                            const auto j = k - 18;
                            at = base - 2 * ((j / 3) % drops) + three[j % 3];
                        }
                        addNote (notes, x, g, pool[(size_t) juce::jlimit (0, (int) pool.size() - 1, at)], velocityAt (t, k % 6 == 0));
                    }
                    break;
                }
                case Burst::cycledCells:
                {
                    // (0.133.0) A three-note cell (down from its top) cycled in sextuplets two to four times, then
                    // moved a scale note down, following the chords' scale.
                    const auto g = runGrid (1.0 / 6.0);
                    auto topPitch = pool[(size_t) nearestIndex (pool, centre + 4.0)];
                    auto cycles = 2 + r.nextInt (3);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        if (here.empty())
                            continue;
                        const auto top = juce::jmax (2, nearestIndex (here, topPitch));
                        addNote (notes, x, g, here[(size_t) juce::jlimit (0, (int) here.size() - 1, top - k % 3)], velocityAt (t, k % 3 == 0));
                        if (k % 3 == 2 && --cycles == 0)
                        {
                            cycles = 2 + r.nextInt (3);
                            topPitch = here[(size_t) juce::jmax (2, top - 1)];
                            if (topPitch < low + 5)
                                topPitch = here[(size_t) nearestIndex (here, centre + 4.0)];
                        }
                    }
                    break;
                }
                case Burst::fourthCycle:
                {
                    // (0.133.0) Three sixteenths cycled (so three against four): a note bent a whole step up into one
                    // that sings over the chord, then two fourths struck as double stops (the chord's root on top),
                    // following the chord.
                    const auto g = runGrid (0.25);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        if (k % 3 == 0)
                        {
                            const auto target = aimAt (x, centre + 5.0);
                            addNote (notes, x, g, target - 2 >= low ? target - 2 : target, velocityAt (t, true));
                            if (target - 2 >= low)
                            {
                                notes.back().effect = LineNote::Effect::bend;
                                notes.back().amount = 2.0f;
                            }
                        }
                        else
                        {
                            const auto top = nearestOf ({ chordAt (x).chord.root }, centre + 3.0, juce::jmin (high, low + 5), high);
                            addNote (notes, x, g, top, velocityAt (t, false));
                            addNote (notes, x, g, top - 5, velocityAt (t, false));
                        }
                    }
                    break;
                }
                case Burst::dottedBends:
                {
                    // (0.133.0) A chain of whole-step bends struck every dotted quarter (three against four), each
                    // into a note that sings over the chord and held with vibrato, then a chromatic fall in eighth
                    // triplets (an octave at most) into the landing.
                    const auto fallFrom = burstFrom + 1.5 * juce::jmax (1.0, std::floor ((landing - burstFrom - 2.0) / 1.5 + 1.0e-9));
                    auto top = centre + 4.0;
                    for (auto x = burstFrom; x < juce::jmin (fallFrom, landing) - 1.0e-9; x += 1.5)
                    {
                        const auto target = aimAt (x, centre + 4.0);
                        top = juce::jmax (top, (double) target);
                        const auto held = juce::jmin (1.48, landing - x - 0.02);
                        addNote (notes, x, held, target - 2 >= low ? target - 2 : target, velocityAt (t, true));
                        if (target - 2 >= low)
                        {
                            notes.back().effect = LineNote::Effect::bend;
                            notes.back().amount = 2.0f;
                        }
                    }
                    const auto g = gridFor (1.0 / 3.0);
                    auto pitch = juce::jmin (high, (int) top + 1);
                    for (auto x = fallFrom; x + g <= landing + 1.0e-9 && pitch >= juce::jmax (low, (int) top - 12); x += g)
                        addNote (notes, x, g, pitch--, velocityAt (t, std::abs (x - std::round (x)) < 1.0e-6));
                    break;
                }
                case Burst::thirdDyads:
                {
                    // (0.133.0) Major thirds as double stops in eighths and sixteenths, the chord's third on top on each
                    // beat (moving with the chords), the notes between a scale step either side.
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; ++k)
                    {
                        const auto onBeat = std::abs (x - std::round (x)) < 1.0e-6;
                        const auto g = gridFor (k % 4 < 2 ? 0.5 : 0.25);
                        const auto& c = chordAt (x).chord;
                        const auto third = pcOf (c.root + (c.type == Chord::Type::minor || c.type == Chord::Type::diminished ? 3 : 4));
                        auto upper = nearestOf ({ third }, centre + 4.0, juce::jmin (high, low + 5), high);
                        if (! onBeat)
                        {
                            const auto here = poolOf (scaleAt (x), low, high);
                            upper = here[(size_t) juce::jlimit (0, (int) here.size() - 1, nearestIndex (here, upper) + (k % 2 == 0 ? 1 : -1))];
                        }
                        const auto len = juce::jmin (g, landing - x);
                        addNote (notes, x, len, upper, velocityAt (t, onBeat));
                        addNote (notes, x, len, juce::jmax (low, upper - 4), velocityAt (t, false));
                        x += g;
                    }
                    break;
                }
                case Burst::bentRiff:
                {
                    // (0.133.0) A riff in eighths: a note bent a whole step up into one that sings over the chord,
                    // then a fourth struck as a double stop (the chord's root on top), in turn; one fast roll inside
                    // a box (sextuplets) in its last beat.
                    const auto g = gridFor (0.5);
                    const auto rollFrom = juce::jmax (burstFrom + 1.0, landing - 1.0);
                    auto k = 0;
                    for (auto x = burstFrom; x < rollFrom - 1.0e-9; x += g, ++k)
                    {
                        if (k % 2 == 0)
                        {
                            const auto target = aimAt (x, centre + 4.0);
                            addNote (notes, x, g - 0.02, target - 2 >= low ? target - 2 : target, velocityAt (t, true));
                            if (target - 2 >= low)
                            {
                                notes.back().effect = LineNote::Effect::bend;
                                notes.back().amount = 2.0f;
                            }
                        }
                        else
                        {
                            const auto top = nearestOf ({ chordAt (x).chord.root }, centre + 2.0, juce::jmin (high, low + 5), high);
                            addNote (notes, x, g - 0.02, top, velocityAt (t, false));
                            addNote (notes, x, g - 0.02, top - 5, velocityAt (t, false));
                        }
                    }
                    const auto rg = runGrid (1.0 / 6.0);
                    const auto base = juce::jlimit (2, juce::jmax (2, (int) pool.size() - 2), nearestIndex (pool, centre + 3.0));
                    const int roll[] = { 0, 1, 0, -1, -2, -1 };
                    k = 0;
                    for (auto x = rollFrom; x < landing - 1.0e-9; x += rg, ++k)
                        addNote (notes, x, rg, pool[(size_t) juce::jlimit (0, (int) pool.size() - 1, base + roll[k % 6])], velocityAt (t, k % 6 == 0));
                    break;
                }
                case Burst::tremoloPedal:
                {
                    // (0.133.0) One note held through the changes, tremolo-picked (16ths where the instrument plays
                    // them, else its tremolo articulation): the key's fifth, which most chords share or colour.
                    const auto& first = chordAt (burstFrom);
                    const auto pc = pcOf ((first.key.minor ? first.key.pitchClass() : first.key.pitchClass()) + 7);
                    const auto pitch = nearestOf ({ pc }, centre + 5.0, low, high);
                    if (shortestStep <= 0.13)
                    {
                        const auto g = gridFor (bpm > 0.0 ? juce::jlimit (0.125, 0.25, bpm / 60.0 / 16.0) : 0.25);
                        auto k = 0;
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                            addNote (notes, x, juce::jmin (g, landing - x), pitch, velocityAt (t, k % 4 == 0));
                    }
                    else
                    {
                        addNote (notes, burstFrom, landing - burstFrom - 0.02, pitch, velocityAt (t, true));
                        notes.back().technique = LineNote::Technique::tremolo;
                    }
                    break;
                }
                case Burst::dyadBurst:
                {
                    // (0.133.0) Two-beat calls: the chord's root and fifth struck together again and again (an eighth,
                    // two sixteenths, then sixteenth triplets) answered by one note bent a whole step; past halfway,
                    // a flurry of three-note cells in sixteenth triplets under a fixed top note (the root), the two
                    // under it walking down a half step each beat.
                    const auto half = burstFrom + std::floor ((landing - burstFrom) * 0.5 / 2.0) * 2.0;
                    for (auto x = burstFrom; x < half - 1.0e-9; x += 2.0)
                    {
                        const auto root = nearestOf ({ chordAt (x).chord.root }, centre + 5.0, juce::jmin (high, low + 7), high);
                        std::vector<std::pair<double, double>> hits { { 0.0, 0.5 }, { 0.5, 0.25 }, { 0.75, 0.25 } };
                        const auto tg = gridFor (1.0 / 6.0);
                        for (auto y = 1.0; y < 1.5 - 1.0e-9; y += tg)
                            hits.push_back ({ y, tg });
                        for (const auto& [o, len] : hits)
                            if (x + o < juce::jmin (half, landing) - 1.0e-9 && o < 1.5 - 1.0e-9)
                            {
                                addNote (notes, x + o, len, root, velocityAt (t, o == 0.0));
                                addNote (notes, x + o, len, root - 5, velocityAt (t, false));
                            }
                        const auto target = aimAt (x + 1.5, centre + 4.0);
                        if (x + 1.5 < landing - 1.0e-9)
                        {
                            addNote (notes, x + 1.5, juce::jmin (0.48, landing - x - 1.5), target - 2 >= low ? target - 2 : target, velocityAt (t, true));
                            if (target - 2 >= low)
                            {
                                notes.back().effect = LineNote::Effect::bend;
                                notes.back().amount = 2.0f;
                            }
                        }
                    }
                    const auto g = runGrid (1.0 / 6.0);
                    auto k = 0;
                    for (auto x = half; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto top = nearestOf ({ chordAt (x).chord.root }, centre + 6.0, juce::jmin (high, low + 7), high);
                        const auto walk = (int) std::floor (x - half);
                        const int cell[] = { 0, -3, -5 };
                        addNote (notes, x, g, k % 3 == 0 ? top : juce::jmax (low, top + cell[k % 3] - walk % 6), velocityAt (t, k % 3 == 0));
                    }
                    break;
                }
                case Burst::tritoneCells:
                {
                    // (0.134.0) Three-note cells (a note, a half step over, a fourth over that) twice a beat, each beat a
                    // fourth higher, round again an octave and a half up.
                    const auto g = runGrid (1.0 / 6.0);
                    const auto start = nearestOf ({ chordAt (burstFrom).chord.root }, centre - 5.0 + 5.0 * r.nextInt (2), low, high - 12);
                    const int cell[] = { 0, 1, 5 };
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto beatIndex = (int) std::floor (x - burstFrom + 1.0e-6);
                        auto base = start + 5 * (beatIndex % 4);
                        if (base + 5 > high)
                            base -= 12;
                        addNote (notes, x, g, juce::jlimit (low, high, base + cell[k % 3]), velocityAt (t, k % 3 == 0));
                    }
                    break;
                }
                case Burst::dimTriplets:
                {
                    // (0.134.0) Diminished cells (a note and the minor thirds over it) in triplets, each beat a minor
                    // third higher, round again at the top.
                    const auto g = gridFor (1.0 / 3.0);
                    const auto start = nearestOf ({ chordAt (burstFrom).chord.root }, centre - 4.0, low, high - 9);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto beatIndex = (int) std::floor (x - burstFrom + 1.0e-6);
                        auto base = start + 3 * (beatIndex % 6);
                        if (base + 6 > high)
                            base -= 12;
                        addNote (notes, x, g, juce::jlimit (low, high, base + 3 * (k % 3)), velocityAt (t, k % 3 == 0));
                    }
                    break;
                }
                case Burst::thirdTrill:
                {
                    // (0.134.0) The minor and major third over the chord's root trilled, then a top note (the fifth, the
                    // flat sixth the next bar), in thirty-seconds; each note doubled a minor third under as a harmony
                    // (on a lane with two tracks, the second plays it).
                    const auto g = runGrid (0.125);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto bar = (int) std::floor (x / beatsPerBar + 1.0e-6);
                        const auto rootNote = nearestOf ({ chordAt (burstFrom).chord.root }, centre - 3.0, low + 3, high - 8);
                        const int cell[] = { 3, 4, bar % 2 == 0 ? 7 : 8 };
                        addNote (notes, x, g, rootNote + cell[k % 3], velocityAt (t, k % 3 == 0));
                        LineNote h = notes.back();
                        h.pitch -= 3;
                        h.harmony = true;
                        notes.push_back (h);
                    }
                    break;
                }
                case Burst::risingBentCells:
                {
                    // (0.134.0) Two-beat cells: an eighth, an eighth bent a half step up into the next scale note, that
                    // note held a beat with vibrato; each cell a scale step higher.
                    auto step = 0;
                    const auto first = (double) pool[(size_t) nearestIndex (pool, centre - 2.0)];
                    for (auto x = burstFrom; x + 2.0 <= landing + 1.0e-9; x += 2.0, ++step)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        if (here.size() < 2)
                            continue;
                        const auto at = juce::jlimit (0, (int) here.size() - 2, nearestIndex (here, first) + step);
                        const auto a = here[(size_t) at], b = here[(size_t) at + 1];
                        addNote (notes, x, 0.48, a, velocityAt (t, true));
                        addNote (notes, x + 0.5, 0.48, b - 1, velocityAt (t, false));
                        notes.back().effect = LineNote::Effect::bend;
                        notes.back().amount = (float) juce::jmax (1, b - (b - 1));
                        addNote (notes, x + 1.0, 0.98, b, velocityAt (t, false));
                        notes.back().effect = LineNote::Effect::vibrato;
                        notes.back().amount = p.vibrato;
                    }
                    break;
                }
                case Burst::dimCells:
                {
                    // (0.134.0) Diminished cells (a note and the minor thirds over it) twice a beat, each beat a fourth
                    // higher (round again an octave and a half up); then the top note struck four times and dropped
                    // down the shape, looped.
                    const auto g = runGrid (1.0 / 6.0);
                    const auto half = burstFrom + std::floor ((landing - burstFrom) * 0.5 + 1.0e-6);
                    const auto start = (double) nearestOf ({ chordAt (burstFrom).chord.root }, centre - 5.0, low, high - 12);
                    auto k = 0;
                    auto top = (int) start;
                    for (auto x = burstFrom; x < half - 1.0e-9; x += g, ++k)
                    {
                        const auto beatIndex = (int) std::floor (x - burstFrom + 1.0e-6);
                        auto base = (int) start + 5 * (beatIndex % 4);
                        if (base + 6 > high)
                            base -= 12;
                        const auto pitch = juce::jlimit (low, high, base + 3 * (k % 3));
                        top = juce::jmax (top, pitch);
                        addNote (notes, x, g, pitch, velocityAt (t, k % 3 == 0));
                    }
                    const int drop[] = { 0, 0, 0, 0, -3, -6 };
                    k = 0;
                    for (auto x = half; x < landing - 1.0e-9; x += g, ++k)
                        addNote (notes, x, g, juce::jlimit (low, high, top + drop[k % 6]), velocityAt (t, k % 6 == 0));
                    break;
                }
                case Burst::cellClimb:
                    writeCellClimb (burstFrom, landing, centre - 5.0, t);
                    break;
                case Burst::cellLoop:
                {
                    // (0.134.0) A one-beat cell: three notes pulled off down the scale in sixteenth triplets, then a
                    // note bent a whole step on the beat after, alternating between two notes that sing over the
                    // chord; looped eight times (a bar at a time), then moved a scale note up.
                    const auto g = gridFor (1.0 / 6.0);
                    auto k = 0;
                    auto lift = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += 1.0, ++k)
                    {
                        if (k > 0 && k % 8 == 0)
                            ++lift;
                        const auto here = poolOf (scaleAt (x), low, high);
                        if (here.empty())
                            continue;
                        const auto top = juce::jlimit (2, (int) here.size() - 1, nearestIndex (here, centre + 2.0) + lift);
                        for (auto j = 0; j < 3 && x + j * g < landing - 1.0e-9; ++j)
                            addNote (notes, x + j * g, g, here[(size_t) (top - j)], velocityAt (t, j == 0));
                        const auto bendAt = x + 3.0 * g;
                        if (bendAt + 0.25 <= landing + 1.0e-9)
                        {
                            const auto aims = aimsAt (x);
                            const auto target = nearestOf ({ aims[(size_t) (k % 2) % aims.size()] }, (double) here[(size_t) top], low + 2, high);
                            addNote (notes, bendAt, juce::jmin (1.0 - 3.0 * g, landing - bendAt) - 0.02, target - 2, velocityAt (t, true));
                            notes.back().effect = LineNote::Effect::bend;
                            notes.back().amount = 2.0f;
                        }
                    }
                    break;
                }
                case Burst::pedalClimb:
                {
                    // (0.134.0) A pedal note (the chord's root) under a line that climbs a scale step every other
                    // note, in sixteenths, on up (round again two octaves over the pedal).
                    const auto g = runGrid (0.25);
                    const auto pedal = nearestOf ({ chordAt (burstFrom).chord.root }, centre - 5.0, low, high - 12);
                    auto step = 1;
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        if (k % 2 == 0)
                            addNote (notes, x, g, pedal, velocityAt (t, true));
                        else
                        {
                            const auto here = poolOf (scaleAt (x), low, high);
                            const auto base = nearestIndex (here, pedal);
                            if (base + step >= (int) here.size() || here[(size_t) (base + step)] > pedal + 24)
                                step = 1;
                            addNote (notes, x, g, here[(size_t) juce::jmin ((int) here.size() - 1, base + step)], velocityAt (t, false));
                            ++step;
                        }
                    }
                    break;
                }
                case Burst::fire:
                {
                    // (0.134.0) The fiery half: cells cycled down from a top note, then the cell climb, then cells
                    // falling a half step each beat into the landing.
                    const auto length = landing - burstFrom;
                    const auto a = burstFrom + std::floor (length / 3.0 + 1.0e-6);
                    const auto b = burstFrom + std::floor (length * 2.0 / 3.0 + 1.0e-6);
                    const auto g = runGrid (1.0 / 6.0);
                    auto topPitch = pool[(size_t) nearestIndex (pool, centre + 4.0)];
                    auto k = 0;
                    for (auto x = burstFrom; x < a - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        const auto top = juce::jmax (2, nearestIndex (here, topPitch));
                        addNote (notes, x, g, here[(size_t) (top - k % 3)], velocityAt (t, k % 3 == 0));
                    }
                    writeCellClimb (a, b, (double) low + 5.0, t);
                    writeChromaticDown (b, landing, juce::jmin ((double) high, centre + 9.0), t);
                    break;
                }
                case Burst::bentCells:
                {
                    // (0.134.0) For slow songs, in thirty-seconds: a four-note cell (a pre-bend let down, the note
                    // under it pulled off, the one under that) looped, then one note struck again and again, bent a
                    // whole step and a minor third in turn.
                    const auto g = runGrid (0.125);
                    const auto half = burstFrom + std::floor ((landing - burstFrom) * 0.5 + 1.0e-6);
                    auto k = 0;
                    for (auto x = burstFrom; x < half - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        const auto top = juce::jlimit (2, (int) here.size() - 1, nearestIndex (here, centre + 4.0));
                        const int cell[] = { 0, 0, -1, -2 };
                        addNote (notes, x, g, here[(size_t) (top + cell[k % 4])], velocityAt (t, k % 4 == 0));
                        if (k % 4 == 0 && here[(size_t) top] + 2 <= high)
                        {
                            notes.back().effect = LineNote::Effect::release;
                            notes.back().amount = 2.0f;
                        }
                    }
                    const auto target = aimAt (half, centre + 4.0);
                    k = 0;
                    for (auto x = half; x < landing - 1.0e-9; x += juce::jmax (g, 0.25), ++k)
                    {
                        const auto semis = k % 2 == 0 ? 2 : 3;
                        addNote (notes, x, juce::jmax (g, 0.25) - 0.02, juce::jmax (low, target - semis), velocityAt (t, k % 2 == 0));
                        notes.back().effect = LineNote::Effect::bend;
                        notes.back().amount = (float) semis;
                    }
                    break;
                }
                case Burst::sameBend:
                {
                    // (0.134.0) One bend struck again and again at the top: a minor third up into the chord's fifth's
                    // octave, held a beat and a half each time.
                    const auto& c = chordAt (burstFrom).chord;
                    const auto target = nearestOf ({ pcOf (c.root + 7), pcOf (c.root + 10) }, (double) high - 4.0, low + 3, high);
                    for (auto x = burstFrom; x < landing - 0.5 - 1.0e-9; x += 1.5)
                    {
                        addNote (notes, x, juce::jmin (1.48, landing - x - 0.02), target - 3, velocityAt (t, true));
                        notes.back().effect = LineNote::Effect::bend;
                        notes.back().amount = 3.0f;
                    }
                    break;
                }
                case Burst::pedalHook:
                {
                    // (0.128.0) A pedal note above, struck between the notes of a line walking down the scale under
                    // it (back to the top every five steps), the pedal following the chord's root.
                    const auto g = runGrid (0.25);
                    auto k = 0;
                    auto step = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        const auto pedal = nearestOf ({ chordAt (x).chord.root }, centre + 7.0, low, high);
                        if (k % 2 == 0)
                            addNote (notes, x, g, pedal, velocityAt (t, true));
                        else
                        {
                            const auto top = nearestIndex (here, pedal - 1.0);
                            const auto at = juce::jlimit (0, (int) here.size() - 1, top - 1 - step);
                            addNote (notes, x, g, here[(size_t) at], velocityAt (t, false));
                            step = (step + 1) % 5;
                        }
                    }
                    break;
                }
                case Burst::run:
                {
                    // Down the scale in 16ths.
                    const auto g = runGrid (0.25);
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g)
                    {
                        addNote (notes, x, g, pool[(size_t) idx], velocityAt (t, false));
                        idx = juce::jmax (0, idx - 1);
                    }
                    break;
                }
                case Burst::cell:
                {
                    // A three-note pull-off cell in triplets, repeated (the scale under the chord as it changes).
                    const auto g = runGrid (1.0 / 3.0);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        const auto base = nearestIndex (here, centre + 3.0);
                        const auto at = juce::jlimit (0, (int) here.size() - 1, base + 2 - (k % 3));
                        addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 3 == 0));
                    }
                    break;
                }
                case Burst::tapping:
                {
                    // Tap, pull off, hammer on: the chord's triad, the tap an octave over its root, following the chords.
                    const auto g = runGrid (1.0 / 3.0);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto& span = chordAt (x);
                        const auto rootNote = nearestOf ({ span.chord.root }, centre, low, high - 12);
                        const auto third = nearestOf ({ pcOf (span.chord.root + (span.chord.type == Chord::Type::minor
                                                                                  || span.chord.type == Chord::Type::diminished ? 3 : 4)) },
                                                      rootNote + 4, low, high);
                        const int shape[] = { rootNote + 12, rootNote, third };
                        addNote (notes, x, g, juce::jlimit (low, high, shape[k % 3]), velocityAt (t, k % 3 == 0));
                        notes.back().technique = LineNote::Technique::tapping;
                    }
                    break;
                }
                case Burst::displaced:
                {
                    // A six-note cell in 16ths (a beat and a half), looped so it lands somewhere new in each bar.
                    const auto g = runGrid (0.25);
                    const int cell[] = { 2, 1, 0, 1, 0, -1 };
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        const auto base = nearestIndex (here, centre + 2.0);
                        const auto at = juce::jlimit (0, (int) here.size() - 1, base + cell[k % 6]);
                        addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 6 == 0));
                    }
                    break;
                }
                case Burst::chromaticShift:
                {
                    // A four-note cell moved up a fret every beat, outside the scale as it goes.
                    const auto g = runGrid (0.25);
                    const auto per = juce::jmax (1, juce::roundToInt (1.0 / g));
                    const auto here = poolOf (scaleAt (burstFrom), low, high);
                    const auto base = nearestIndex (here, centre);
                    int cell[4];
                    const int shape[] = { 0, 2, 1, 2 };
                    for (int c = 0; c < 4; ++c)
                        cell[c] = here[(size_t) juce::jlimit (0, (int) here.size() - 1, base + shape[c])];
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                        addNote (notes, x, g, juce::jlimit (low, high, cell[k % 4] + k / per), velocityAt (t, k % 4 == 0));
                    break;
                }
                case Burst::repeatLick:
                {
                    // A three-beat lick in picked triplets, repeated so it starts somewhere new in each bar.
                    const auto g = runGrid (1.0 / 3.0);
                    const int lick[] = { 2, 1, 0, 2, 1, 0, 1, 0, -1 };
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        const auto base = nearestIndex (here, centre + 3.0);
                        const auto at = juce::jlimit (0, (int) here.size() - 1, base + lick[k % 9]);
                        addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 9 == 0));
                    }
                    break;
                }
                case Burst::sixes:
                {
                    // Six-note groups down the scale in 16ths (each a step lower), drifting against the beat.
                    const auto g = runGrid (0.25);
                    const int six[] = { 0, -1, -2, -1, -2, -3 };
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        const auto top = nearestIndex (here, centre + 9.0);
                        const auto at = juce::jlimit (0, (int) here.size() - 1, top - k / 6 + six[k % 6]);
                        addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 6 == 0));
                    }
                    break;
                }
                case Burst::fourSequence:
                {
                    // A four-note run down from a scale note, each one starting a step higher, in 16ths.
                    const auto g = runGrid (0.25);
                    const int four[] = { 0, -1, -2, -3 };
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        const auto base = nearestIndex (here, centre);
                        const auto at = juce::jlimit (0, (int) here.size() - 1, base + k / 4 + four[k % 4]);
                        addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 4 == 0));
                    }
                    break;
                }
                case Burst::risingFlurry:
                {
                    const auto g = runGrid (0.25);
                    auto k = 0;
                    if (p.outburst && peak)
                    {
                        // The peak: an outburst outside the key, climbing by half steps.
                        const auto start = nearestOf (chordTones (burstFrom), centre + 2.0, low, high);
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                            addNote (notes, x, g, juce::jlimit (low, high, start + k), velocityAt (t, k % 4 == 0));
                        break;
                    }
                    // Up the scale, turning back down a step every fourth note.
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        const auto base = nearestIndex (here, centre - 2.0);
                        const auto at = juce::jlimit (0, (int) here.size() - 1, base + k - 2 * (k / 4));
                        addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 4 == 0));
                    }
                    break;
                }
                case Burst::sweep:
                {
                    // The chord's triad swept up and back down in triplets, following the chords.
                    const auto g = runGrid (1.0 / 3.0);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto arp = poolOf (chordTones (x), low, high);
                        const auto base = nearestIndex (arp, centre - 2.0);
                        const auto up = juce::jmin (4, (int) arp.size() - 1 - base);
                        const auto pos = k % juce::jmax (1, 2 * up);
                        const auto at = base + (pos <= up ? pos : 2 * up - pos);
                        addNote (notes, x, g, arp[(size_t) juce::jlimit (0, (int) arp.size() - 1, at)], velocityAt (t, pos == 0));
                    }
                    break;
                }
                case Burst::noise:
                {
                    // A noisy run: notes thrown around the scale and outside it, in 16ths.
                    const auto g = runGrid (0.25);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                        addNote (notes, x, g, juce::jlimit (low, high, juce::roundToInt (centre) + r.nextInt ({ -5, 8 })),
                                 velocityAt (t, k % 4 == 0));
                    break;
                }
                case Burst::turning:
                {
                    // (0.116.0) A three- to five-note cell turning round a note in 16ths, the cell kept while it moves a
                    // step at a time through the scale.
                    const auto g = runGrid (0.25);
                    const auto size = 3 + r.nextInt (3);
                    std::vector<int> cell;
                    for (int c = 0; c < size; ++c)
                        cell.push_back (c == 0 ? 0 : (c % 2 == 1 ? -1 : 1) * (1 + (c / 3)));
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        const auto base = nearestIndex (here, centre + 1.0) + (k / size) * (r.nextFloat() < 0.6f ? -1 : 1);
                        const auto at = juce::jlimit (0, (int) here.size() - 1, base + cell[(size_t) (k % size)]);
                        addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % size == 0));
                    }
                    break;
                }
                case Burst::flurry:
                {
                    // (0.116.0) Sextuplet flurries falling through a small box, a beat at a time, each from a little lower.
                    const auto g = runGrid (1.0 / 6.0);
                    auto k = 0;
                    for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                    {
                        const auto here = poolOf (scaleAt (x), low, high);
                        const auto beat = (int) std::floor ((x - burstFrom) + 1.0e-9);
                        const auto top = nearestIndex (here, centre + 6.0) - (beat % 3);
                        const auto in = (int) std::llround ((x - burstFrom - beat) / g);
                        const auto at = juce::jlimit (0, (int) here.size() - 1, top - in + (in >= 4 ? 2 : 0));
                        addNote (notes, x, g, here[(size_t) at], velocityAt (t, in == 0));
                    }
                    break;
                }
                case Burst::sequence:
                {
                    const auto g = runGrid (0.25);
                    auto k = 0;
                    if (t > 0.75f && burstIndex > 0)
                    {
                        // The climax: something outside, a chromatic climb.
                        const auto start = nearestOf (chordTones (burstFrom), centre, low, high);
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                            addNote (notes, x, g, juce::jlimit (low, high, start + k), velocityAt (t, k % 4 == 0));
                    }
                    else if (burstIndex % 2 == 0)
                    {
                        // Six-note scale cells, each starting two steps higher (turning round at the top).
                        const int cell[] = { 0, 1, 2, 3, 2, 1 };
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                        {
                            const auto here = poolOf (scaleAt (x), low, high);
                            const auto base = nearestIndex (here, centre - 4.0);
                            const auto span = juce::jmax (1, (int) here.size() - 4 - base);
                            auto climb = (k / 6) * 2;
                            climb = climb % (2 * span);
                            if (climb > span) climb = 2 * span - climb;
                            const auto at = juce::jlimit (0, (int) here.size() - 1, base + climb + cell[k % 6]);
                            addNote (notes, x, g, here[(size_t) at], velocityAt (t, k % 6 == 0));
                        }
                    }
                    else
                    {
                        // The chord's seventh arpeggio, one note each, up and back down.
                        for (auto x = burstFrom; x < landing - 1.0e-9; x += g, ++k)
                        {
                            const auto& span = chordAt (x);
                            auto tones = chordTones (x);
                            const auto seventh = pcOf (span.chord.root + (span.chord.type == Chord::Type::major
                                                                          && has (scaleAt (x), pcOf (span.chord.root + 11)) ? 11 : 10));
                            if (! has (tones, seventh))
                                tones.push_back (seventh);
                            const auto arp = poolOf (tones, low, high);
                            const auto base = nearestIndex (arp, centre - 3.0);
                            const auto up = juce::jmin (6, (int) arp.size() - 1 - base);
                            const auto pos = k % juce::jmax (1, 2 * up);
                            const auto at = base + (pos <= up ? pos : 2 * up - pos);
                            addNote (notes, x, g, arp[(size_t) juce::jlimit (0, (int) arp.size() - 1, at)], velocityAt (t, pos == 0));
                        }
                    }
                    break;
                }
            }
        };

        //======================================================================
        // (0.116.0, from the owner's Pink Floyd and Guns N' Roses solos, read for their shapes only) The phrase plan:
        //   - a phrase is a pickup (most start off beat 1: on an "and", beat 3, the last beat) running into its longest
        //     note, held 1.5-4 beats on a strong place; the held note is the breath, real rests come now and then;
        //   - busy passages come as waves (0-3, the biggest past halfway), each followed by a calm stretch, the last
        //     bars calm;
        //   - the register climbs to a peak past halfway (about 70 %) and comes back down to end on a held root;
        //   - chord tones where the phrase lands, not on every beat; pickups move by step, falling more than rising;
        //   - a call played again, the opening phrase back when the chords come round, rhythm cells kept with new notes.
        const auto bars = juce::jmax (1, (int) std::ceil (total / beatsPerBar - 1.0e-6));
        const auto archOf = [&p] (float t)
        {
            if (p.endHigh)
                return t;                                     // (the player styles that end high)
            if (p.falling)
                return 1.0f - 0.85f * t;                      // (each phrase lower)
            if (p.hotStart)
                return t < 0.15f ? 1.0f : 1.0f - 0.7f * (t - 0.15f) / 0.85f;   // (0.128.0: high at once, then down)
            return t <= p.peakAt ? t / juce::jmax (0.05f, p.peakAt) : 1.0f - 0.6f * (t - p.peakAt) / juce::jmax (0.05f, 1.0f - p.peakAt);
        };
        const auto centreAt = [&] (double beat)
        {
            const auto t = (float) juce::jlimit (0.0, 1.0, beat / juce::jmax (1.0e-6, total));
            auto c = (double) low + 7.0 + p.climb * archOf (t);
            if (p.octaveDownLate && t >= 0.5f && c - 12.0 >= low + 2)
                c -= 12.0;
            if (p.lowLastQuarter && t >= 0.75f)
                c = juce::jmax ((double) low + 4.0, c - 24.0);   // (0.134.0: the last quarter down low)
            return c;
        };

        // Where the waves go.
        struct Wave { double from, landing, until; bool peak; };
        std::vector<Wave> waves;
        {
            auto count = p.waves >= 0 ? p.waves : p.burstFrom > 1.0f ? 0 : p.burstEveryOther ? (bars >= 12 ? 3 : 2) : 1;
            if (bars < 4)
                count = juce::jmin (count, 1);
            std::vector<float> at;
            if (p.hotStart)
            {
                // (0.128.0) The fast passage first, then calm to the end.
                const auto startBars = bars >= 8 ? 2 : 1;
                const auto landing = startBars * beatsPerBar - juce::jmin (1.0, beatsPerBar * 0.25);
                waves.push_back ({ 0.0, landing, juce::jmin (total, landing + 2.0 + beatsPerBar * 0.5), true });
                count = 0;
            }
            if (! p.waveAts.empty())
                at = p.waveAts;   // (0.134.0: the style's own places)
            else if (p.middlePart)
                at = { 0.5f };   // (0.133.0: the ABA solo's middle part, a third of it long)
            else if (p.twoSpeed)
                at = { -1.0f };
            else if (count == 1) at = { 0.72f };
            else if (count == 2) at = { 0.4f, 0.74f };
            else if (count >= 3) at = { 0.3f, 0.56f, 0.8f };
            const auto burstBars = p.waveBars > 0 ? juce::jlimit (1, juce::jmax (1, bars / 4), p.waveBars)
                                 : p.middlePart ? juce::jmax (1, bars / 3) : bars >= 16 ? 2 : 1;
            for (size_t i = 0; i < at.size(); ++i)
            {
                if (p.twoSpeed)
                {
                    // The second half one continuous fast passage, the last bar calm.
                    const auto a = juce::jmax (1, bars / 2) * beatsPerBar;
                    const auto end = juce::jmax (a + beatsPerBar, (bars - 1) * beatsPerBar);
                    waves.push_back ({ a, end - beatsPerBar * 0.25, juce::jmin (total, end + beatsPerBar * 0.5), true });
                    break;
                }
                auto bar = juce::roundToInt (at[i] * (float) bars - (float) burstBars * 0.5f);
                bar = juce::jlimit (1, juce::jmax (1, bars - 2 - burstBars), bar);
                if (p.blocks && bars >= 8)
                    bar = juce::jlimit (1, juce::jmax (1, bars - 2 - burstBars), juce::jmax (4, juce::roundToInt ((float) bar / 4.0f) * 4));   // (0.134.0: on a four-bar line)
                const auto a = bar * beatsPerBar;
                if (! waves.empty() && a < waves.back().until - 1.0e-6)
                    continue;
                const auto landing = a + burstBars * beatsPerBar - juce::jmin (1.0, beatsPerBar * 0.25);
                const auto until = juce::jmin (total - (p.lickTwice && bars >= 6 ? 4.0 * beatsPerBar : 0.0), landing + 2.0 + beatsPerBar * 0.5);
                if (until <= landing + 0.5)
                    continue;
                waves.push_back ({ a, landing, until, i + 1 == at.size() });
            }
        }

        // The chords' cycle (where the progression comes round), for a theme brought back.
        double cycle = 0.0;
        {
            std::vector<std::pair<double, Chord>> seq;
            for (const auto& s : spans)
                seq.push_back ({ s.start - origin, s.chord });
            for (size_t n = 1; n <= seq.size() / 2 && cycle == 0.0; ++n)
            {
                auto same = seq.size() >= 2 * n;
                for (size_t i = n; i < seq.size() && same; ++i)
                    same = seq[i].second == seq[i - n].second;
                if (same && seq[n].first > beatsPerBar - 1.0e-6)
                    cycle = seq[n].first;
            }
        }

        struct Phrase { double start, end; std::vector<LineNote> notes; };
        std::set<long> copied;   // (onsets of notes played again: they take their original's bend or vibrato)
        struct CopyLink { long copyKey; int copyPitch; long sourceKey; int sourcePitch; };
        std::vector<CopyLink> copyLinks;
        std::vector<Phrase> history;
        const auto inScale = [&] (int pitch, double beat) { return has (scaleAt (beat), pcOf (pitch)); };
        const auto snapToScale = [&] (int pitch, double beat) { return inScale (pitch, beat) ? pitch : nearestOf (scaleAt (beat), pitch, low, high); };
        // The phrase's landing note: a chord tone (root, fifth, third; the style's own choice where it has one).
        const auto landingPitch = [&] (double beat, double around)
        {
            const auto& c = chordAt (beat).chord;
            const auto third = pcOf (c.root + (c.type == Chord::Type::minor || c.type == Chord::Type::diminished ? 3 : 4));
            const auto fifth = pcOf (c.root + (c.type == Chord::Type::diminished ? 6 : c.type == Chord::Type::augmented ? 8 : 7));
            if (p.thirdTargets)
                return nearestOf ({ third }, around, low, high);
            if (p.landThirdFifth)
                return nearestOf ({ third, fifth }, around, low, high);
            const auto pick = r.nextFloat();
            return nearestOf (pick < 0.45f ? std::vector<int> { c.root } : pick < 0.75f ? std::vector<int> { fifth } : std::vector<int> { third },
                              around, low, high);
        };

        auto cursor = 0.0;
        auto phraseNo = 0;
        auto previousLand = -1;
        auto repeatedLast = false;
        auto calmUntil = -1.0;
        auto lastLanding = -1;   // (the index of the last held note: it rings until the next phrase)
        size_t waveIndex = 0;
        auto burstCount = 0;
        const auto ringOn = [&] (double until)
        {
            if (lastLanding >= 0 && lastLanding < (int) notes.size())
            {
                auto& n = notes[(size_t) lastLanding];
                n.length = juce::jmax (n.length, until - n.beat - 0.02);
            }
        };
        // The opening: a held low note (where the style starts so).
        if (p.openHeld)
        {
            const auto hold = juce::jmin (total, beatsPerBar * (bars >= 8 ? 2.0 : 1.0)) - 0.25;
            addNote (notes, 0.0, hold, nearestOf ({ chordAt (0.0).chord.root }, (double) low + 2.0, low, high), velocityAt (0.0f, true));
            lastLanding = (int) notes.size() - 1;
            cursor = hold;
        }
        // (0.133.0) In on the downbeat with a high harmonic (the chord's root or fifth) held up to three beats,
        // scooped up into with the whammy from a fifth under.
        // (0.134.0) Four bars of held notes (from beat 2 to the bar's end, the band's beat 1 left alone) on the key's
        // root and its flat five in turn, each bar a tritone higher; sixteenths alternating the two on beat 1.
        if (p.tritoneOpening && bars >= 8)
        {
            const auto& first = chordAt (0.0);
            const auto tonic = first.key.minor ? first.key.pitchClass() : pcOf (first.key.pitchClass() + 9);
            const auto bottom = nearestOf ({ tonic }, (double) low + 5.0, low, juce::jmax (low, high - 18));
            for (auto b = 0; b < 4; ++b)
            {
                const auto o = b * beatsPerBar;
                const auto held = juce::jlimit (low, high, bottom + 6 * b);
                if (b > 0)
                    for (auto j = 0; j < 4; ++j)
                        addNote (notes, o + j * 0.25, 0.24, juce::jlimit (low, high, j % 2 == 0 ? held - 6 : held), velocityAt (0.3f, j == 0));
                addNote (notes, o + 1.0, beatsPerBar - 1.02, held, velocityAt (0.3f, true));
                notes.back().effect = LineNote::Effect::vibrato;
                notes.back().amount = p.vibrato;
            }
            burstSpans.push_back ({ 0.0, 4.0 * beatsPerBar });   // (the stair as written: no bends or squeals added)
            lastLanding = (int) notes.size() - 1;
            cursor = 4.0 * beatsPerBar;
        }
        // (0.134.0) The first half one long note a chord, each coming in an eighth before the bar line (tied over it),
        // raked into with two chord tones under it where the instrument plays such short notes.
        if (p.heldChordOpening && bars >= 4)
        {
            const auto upto = juce::jmax (2, bars / 2);
            auto line = centreAt (0.0) + 2.0;
            for (auto b = 0; b < upto; ++b)
            {
                const auto at = b == 0 ? 0.0 : b * beatsPerBar - 0.5;
                const auto& c = chordAt (b * beatsPerBar).chord;
                const auto third = pcOf (c.root + (c.type == Chord::Type::minor || c.type == Chord::Type::diminished ? 3 : 4));
                const auto pitch = nearestOf ({ third, pcOf (c.root) }, line, low + 9, high);
                line = pitch;
                if (b > 0 && shortestStep <= 0.1)
                    for (auto j = 2; j >= 1; --j)
                        addNote (notes, at - j / 24.0, 1.0 / 24.0, nearestOf (chordTones (b * beatsPerBar), (double) pitch - 4.0 * j, low, pitch - 1),
                                 velocityAt (0.3f, false));
                addNote (notes, at, (b + 1) * beatsPerBar - 0.5 - at - 0.02, pitch, velocityAt (0.3f, true));
                lastLanding = (int) notes.size() - 1;
            }
            cursor = upto * beatsPerBar - 0.5;
        }
        if (p.screamEntry && ! p.openHeld)
        {
            const auto hold = juce::jmin (total * 0.25, juce::jmin (3.0, beatsPerBar - 1.0));
            if (hold >= 1.0)
            {
                const auto& c = chordAt (0.0).chord;
                addNote (notes, 0.0, hold - 0.02, nearestOf ({ c.root, pcOf (c.root + 7) }, (double) high - 5.0, low, high), velocityAt (0.5f, true));
                notes.back().technique = LineNote::Technique::harmonic;
                notes.back().effect = LineNote::Effect::scoop;
                notes.back().amount = 7.0f;
                lastLanding = (int) notes.size() - 1;
                cursor = hold;
            }
        }
        const auto lickBars = p.lickTwice && bars >= 6 ? 4 : 0;
        // (0.128.0) The last note held long: the phrases stop that much before the end and the last landing rings on.
        const auto endHold = p.longEnds && lickBars == 0 && ! p.walkDown
                           ? (bars >= 16 ? 6.0 + 2.0 * r.nextDouble() : bars >= 8 ? 4.0 : 0.0) : 0.0;
        const auto phrasesEnd = total - lickBars * beatsPerBar - endHold;
        while (cursor < phrasesEnd - 0.5)
        {
            const auto t = (float) (cursor / juce::jmax (1.0e-6, total));
            // A wave here.
            if (waveIndex < waves.size() && cursor >= waves[waveIndex].from - beatsPerBar * 0.5 - 1.0e-6)
            {
                const auto& w = waves[waveIndex++];
                const auto from = juce::jmax (cursor, w.from);
                if (w.landing - from >= 0.75)
                {
                    ringOn (from);
                    const auto tw = (float) (from / total);
                    writeBurst (from, w.landing, centreAt (from) + 2.0, tw, burstCount++, w.peak);
                    const auto land = nearestOf (chordTones (w.landing), centreAt (w.landing) + 4.0, low, high);
                    addNote (notes, w.landing, w.until - w.landing - 0.02, land, velocityAt (tw, true));
                    lastLanding = (int) notes.size() - 1;
                    calmUntil = w.until + beatsPerBar;
                }
                cursor = juce::jmax (cursor, w.until);
                continue;
            }
            const auto limit = waveIndex < waves.size() ? juce::jmax (cursor, waves[waveIndex].from) : phrasesEnd;

            // Repetition: the call played again, the theme when the chords come round, the opening restated.
            const Phrase* copyFrom = nullptr;
            auto shift = 0.0;
            auto newNotes = false;
            if (! history.empty())
            {
                const auto& prev = history.back();
                const auto barsOf = [&] (double beats) { return std::ceil (beats / beatsPerBar - 1.0e-6) * beatsPerBar; };
                // A period: the opening phrase's length in whole bars, rounded up to 1, 2, 4 or 8 bars (the theme:
                // the chords' cycle), so the copies fall where the tune and the chords come round.
                const auto& head = history.front();
                auto period = beatsPerBar;
                while (period < head.end - head.start - 1.0e-6 && period < 8.0 * beatsPerBar)
                    period *= 2.0;
                if (p.restate)
                    period = juce::jmax (period, 2.0 * beatsPerBar);   // (room for the tune to ring before it comes back)
                if (p.cycleTheme && cycle > 0.0)
                    period = cycle;
                const auto nextCopy = std::ceil ((cursor - head.start) / period - 1.0e-6) * period;
                // (the tune comes back on its place: the held note before it rings on up to it)
                if (p.restate || ((p.cycleTheme && cycle > 0.0) && nextCopy - (cursor - head.start) <= period * 0.5 + 1.0e-6))
                {
                    copyFrom = &head;
                    shift = nextCopy;
                }
                else if (p.repeatPair && ! repeatedLast && r.nextFloat() < 0.35f)
                {
                    copyFrom = &prev;
                    shift = barsOf (cursor - prev.start + 1.0e-6);
                }
                else if (p.seqShare > 0.0f && r.nextFloat() < p.seqShare)
                {
                    copyFrom = &prev;
                    shift = barsOf (cursor - prev.start + 1.0e-6);
                    newNotes = true;
                }
                if (copyFrom != nullptr && copyFrom->end + shift > limit + 1.0e-6)
                {
                    if (p.restate)
                    {
                        // (the tune only: no room for it again, the held note rings on)
                        ringOn (limit);
                        cursor = limit;
                        continue;
                    }
                    copyFrom = nullptr;
                }
            }
            if (copyFrom != nullptr)
            {
                ringOn (copyFrom->start + shift);
                Phrase again { copyFrom->start + shift, copyFrom->end + shift, {} };
                const auto steps = newNotes ? (r.nextBool() ? 2 : -2) : 0;
                const auto late = (copyFrom->start + shift) >= total * 0.75 - 1.0e-6;
                const auto octave = p.restate && (p.restateEarly || (copyFrom->start + shift) >= total * 0.5 - 1.0e-6) && ! (p.restateEarly && late) ? 12 : 0;   // (where the copy lands; 0.134.0: from the first return, not in the last quarter)
                for (auto n : copyFrom->notes)
                {
                    const auto sourceKey = std::lround (n.beat * 960.0);
                    const auto sourcePitch = n.pitch;
                    n.beat += shift;
                    if (newNotes)
                    {
                        const auto pool = poolOf (scaleAt (n.beat), low, high);
                        if (! pool.empty())
                            n.pitch = pool[(size_t) juce::jlimit (0, (int) pool.size() - 1, nearestIndex (pool, n.pitch) + steps)];
                    }
                    else
                        n.pitch = snapToScale (n.pitch + (n.pitch + octave <= high && n.pitch + octave >= low ? octave : 0), n.beat);
                    n.effect = LineNote::Effect::none;
                    notes.push_back (n);
                    again.notes.push_back (n);
                    copied.insert (std::lround (n.beat * 960.0));
                    copyLinks.push_back ({ std::lround (n.beat * 960.0), n.pitch, sourceKey, sourcePitch });
                }
                lastLanding = (int) notes.size() - 1;
                history.push_back (again);
                repeatedLast = true;
                cursor = again.end + (r.nextFloat() < p.restChance ? p.restBeats : 0.0);
                ++phraseNo;
                continue;
            }
            repeatedLast = false;

            // The landing place: a bar line, beat 3, or the last eighth before a bar line (tied over it).
            auto pickup = 0.5 * (double) (1 + r.nextInt (juce::jmax (1, juce::roundToInt (p.pickupMax * 2.0))));
            if (p.upbeatEntry && phraseNo == 0)
                pickup = juce::jmax (pickup, 1.0);
            const auto earliest = cursor + pickup;
            struct Place { double at; float weight; };
            std::vector<Place> places;
            for (auto bar = (int) std::floor (earliest / beatsPerBar); bar * beatsPerBar < earliest + beatsPerBar * 1.5; ++bar)
            {
                const auto o = bar * beatsPerBar;
                places.push_back ({ o, p.landBeat1 });
                places.push_back ({ o + beatsPerBar * 0.5, (1.0f - p.landBeat1) * 0.6f });
                places.push_back ({ o - beatsPerBar * 0.125, (1.0f - p.landBeat1) * 0.4f });
            }
            auto weightSum = 0.0f;
            for (const auto& pl : places)
                if (pl.at >= earliest - 1.0e-6 && pl.at <= limit - 0.75)
                    weightSum += pl.weight;
            double landAt = -1.0;
            if (weightSum > 0.0f)
            {
                auto pick = r.nextFloat() * weightSum;
                for (const auto& pl : places)
                    if (pl.at >= earliest - 1.0e-6 && pl.at <= limit - 0.75)
                    {
                        if (pick < pl.weight) { landAt = pl.at; break; }
                        pick -= pl.weight;
                    }
            }
            if (landAt < 0.0)
            {
                // No room for a whole phrase: the held note rings on to the wave or the end.
                ringOn (limit);
                cursor = limit;
                continue;
            }
            auto hold = juce::jmin (p.holdMin + (p.holdMax - p.holdMin) * r.nextDouble(), limit - landAt);
            // (0.128.0) The first note the solo lands on held long (4 to 6 beats in 8 bars or more, 6 to 8 in 16).
            if (p.longEnds && history.empty() && bars >= 8)
                hold = juce::jmin (limit - landAt, juce::jmax (hold, (bars >= 16 ? 6.0 : 4.0) + 2.0 * r.nextDouble()));
            const auto pickStart = landAt - pickup;
            ringOn (pickStart);

            // How busy: the style's density, calmer after a wave, the two-speed solo slow first.
            auto perBeat = p.notesPerBeat;
            if (cursor < calmUntil)
                perBeat *= 0.5f;
            const auto grid = p.noSixteenths ? 0.5 : gridFor (perBeat > 1.6f ? 0.25 : 0.5);
            const auto count = juce::jlimit (1, juce::jmax (1, (int) std::floor (pickup / grid + 1.0e-6)),
                                             juce::roundToInt (perBeat * pickup * (0.7 + 0.6 * r.nextDouble())));
            auto at = rhythmOf (count, pickup, grid, r, false);
            if (p.upbeatEntry && phraseNo == 0)
                for (auto& x : at)
                    x = juce::jmax (x, 0.5 - pickStart);   // (in on an upbeat)
            at.erase (std::unique (at.begin(), at.end(), [] (double a, double b) { return std::abs (a - b) < 1.0e-6; }), at.end());

            Phrase phrase { pickStart, landAt + hold, {} };
            auto centre = centreAt (landAt);
            // (FALLING MAJOR MELODY: each phrase lands lower than the last, starting high again at the bottom)
            if (p.falling && previousLand > 0)
                centre = previousLand - 2 < low + 5 ? (double) low + p.climb : juce::jmin (centre, (double) previousLand - 2.0);
            auto land = landingPitch (landAt, centre);
            previousLand = land;
            // Pickups: by step into the landing, from above about two times in three.
            const auto falling = r.nextFloat() < 0.66f;
            {
                auto pool = poolOf (scaleAt (pickStart), low, high);
                if (! pool.empty())
                {
                    std::vector<int> pitches (at.size());
                    auto idx = nearestIndex (pool, land);
                    for (int i = (int) at.size() - 1; i >= 0; --i)
                    {
                        auto step = r.nextFloat() < 0.2f ? 2 : 1;
                        if (r.nextFloat() < 0.15f)
                            step = 0;   // (a repeated note)
                        idx = juce::jlimit (0, (int) pool.size() - 1, idx + (falling ? step : -step));
                        pitches[(size_t) i] = pool[(size_t) idx];
                    }
                    // (0.133.0) A two-note pickup thrown two octaves under the phrase (one under where there's no room).
                    if (p.throws && pitches.size() >= 3 && r.nextFloat() < 0.35f)
                        for (size_t i = 0; i < 2; ++i)
                            pitches[i] -= pitches[i] - 24 >= low ? 24 : pitches[i] - 12 >= low ? 12 : 0;
                    for (size_t i = 0; i < at.size(); ++i)
                    {
                        const auto x = pickStart + at[i];
                        auto pitch = snapToScale (pitches[i], x);
                        if (p.chromaticPassing && std::abs (at[i] - std::round (at[i])) > 1.0e-6 && r.nextFloat() < 0.25f)
                            pitch += r.nextBool() ? 1 : -1;
                        const auto next = i + 1 < at.size() ? pickStart + at[i + 1] : landAt;
                        addNote (notes, x, next - x, juce::jlimit (low, high, pitch), velocityAt (t, i == 0));
                        phrase.notes.push_back (notes.back());
                        if (p.harmonyThirds && phraseNo % 3 == 2)
                        {
                            const auto here = poolOf (scaleAt (x), low, high);
                            const auto k = nearestIndex (here, pitch);
                            if (k + 2 < (int) here.size())
                                addNote (notes, x, next - x, here[(size_t) k + 2], velocityAt (t, false));
                        }
                    }
                    // A grace note a whole step under the landing, just before it.
                    if (p.graceNotes && shortestStep <= 0.125 && r.nextFloat() < 0.5f && land - 2 >= low)
                    {
                        addNote (notes, landAt - 0.125, 0.1, land - 2, velocityAt (t, false));
                        phrase.notes.push_back (notes.back());
                    }
                    if (p.tension && ! phrase.notes.empty() && land + 1 <= high)
                        tensionTargets.push_back (landAt);
                }
            }
            addNote (notes, landAt, hold - 0.02, land, velocityAt (t, true));
            lastLanding = (int) notes.size() - 1;
            phrase.notes.push_back (notes.back());
            // A chord rake under the landing now and then (two notes struck with it).
            if (p.rakes && r.nextFloat() < 0.3f)
                for (const auto under : { 5, 9 })
                    if (land - under >= low && inScale (land - under, landAt))
                    {
                        addNote (notes, landAt, juce::jmin (hold, 1.0), land - under, velocityAt (t, false));
                        break;
                    }
            history.push_back (phrase);
            cursor = landAt + hold + (r.nextFloat() < p.restChance ? p.restBeats : 0.0);
            if (p.longFirstRest && phraseNo == 0)
            {
                cursor += 4.0;       // (0.134.0: silence after the first statement,
                lastLanding = -1;    //  the held note not ringing on through it)
            }
            ++phraseNo;
        }

        if (endHold > 0.0)
            ringOn (total);
        // (0.116.0) A two-bar lick played twice to finish.
        if (lickBars > 0)
        {
            ringOn (phrasesEnd);
            const auto from = phrasesEnd;
            burstSpans.push_back ({ from, total - beatsPerBar });   // (played the same both times: no bends added)
            const auto g = gridFor (0.25);
            auto pool = poolOf (scaleAt (from), low, high);
            if (! pool.empty())
            {
                std::vector<LineNote> lick;
                auto idx = nearestIndex (pool, centreAt (from) + 3.0);
                const auto length = 2.0 * beatsPerBar;
                for (auto x = 0.0; x < length - 1.0 - 1.0e-9; x += g)
                {
                    if (r.nextFloat() < 0.18f)
                        continue;
                    idx = juce::jlimit (0, (int) pool.size() - 1, idx + (r.nextFloat() < 0.6f ? -1 : 1));
                    LineNote n;
                    n.beat = x;
                    n.length = g;
                    n.pitch = pool[(size_t) idx];
                    n.velocity = GuitarVelocity::of (velocityAt (0.9f, std::abs (x - std::round (x)) < 1.0e-6));
                    lick.push_back (n);
                }
                LineNote held;
                held.beat = length - 1.0;
                held.length = 0.98;
                held.pitch = nearestOf (chordTones (from), (double) pool[(size_t) idx], low, high);
                held.velocity = GuitarVelocity::of (velocityAt (0.9f, true));
                lick.push_back (held);
                for (const auto copy : { 0.0, length })
                    for (auto n : lick)
                    {
                        n.beat += from + copy;
                        if (n.beat < total - 1.0e-6)
                            notes.push_back (n);
                    }
            }
        }
        // (0.116.0) Walking down to a long low root over the last two bars.
        if (p.walkDown && bars >= 4)
        {
            const auto from = total - 2.0 * beatsPerBar;
            notes.erase (std::remove_if (notes.begin(), notes.end(), [from] (const LineNote& n) { return n.beat >= from - 1.0e-6; }), notes.end());
            for (auto& n : notes)
                n.length = juce::jmin (n.length, juce::jmax (0.05, from - n.beat - 0.02));
            auto pool = poolOf (scaleAt (from), low, high);
            if (! pool.empty())
            {
                const auto rootLow = nearestOf ({ chordAt (total - 0.5).chord.root }, (double) low + 2.0, low, high);
                auto idx = nearestIndex (pool, rootLow + 9.0);
                for (auto x = from; x < total - beatsPerBar - 1.0e-9; x += 1.0)
                {
                    addNote (notes, x, 0.95, pool[(size_t) idx], velocityAt (0.6f, false));
                    idx = juce::jmax (0, idx - 1);
                }
                addNote (notes, total - beatsPerBar, beatsPerBar, rootLow, velocityAt (0.5f, true));
            }
        }

        // (0.134.0) The last quarter down low: everything from three quarters of the way an octave under (where
        // the whole passage fits).
        if (p.lowLastQuarter)
        {
            const auto from = total * 0.75 - 1.0e-6;
            auto lowest = high;
            for (const auto& n : notes)
                if (n.beat >= from)
                    lowest = juce::jmin (lowest, n.pitch);
            if (lowest - 12 >= low)
                for (auto& n : notes)
                    if (n.beat >= from)
                        n.pitch -= 12;
        }
        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        if (notes.empty())
            return solo;

        // The ending: the last note on the last chord's root, held to the end (one note: a rake or double stop
        // struck with it goes).
        {
            const auto lastBeat = notes.back().beat;
            auto top = notes.size() - 1;
            for (size_t i = 0; i < notes.size(); ++i)
                if (std::abs (notes[i].beat - lastBeat) < 1.0e-6 && notes[i].pitch > notes[top].pitch)
                    top = i;
            std::swap (notes[top], notes.back());
            notes.erase (std::remove_if (notes.begin(), notes.end() - 1, [lastBeat] (const LineNote& n) { return std::abs (n.beat - lastBeat) < 1.0e-6; }),
                         notes.end() - 1);
        }
        // (0.116.0) The last chord gets its own landing: a note held from before it lets go as it comes.
        if (const auto lastChord = spans.back().start - origin; notes.back().beat < lastChord - 1.0e-6 && total - lastChord >= 0.5)
        {
            for (auto& n : notes)
                n.length = juce::jmin (n.length, juce::jmax (0.05, lastChord - n.beat - 0.02));
            addNote (notes, lastChord, total - lastChord, notes.back().pitch, velocityAt (0.6f, true));
        }
        {
            auto& last = notes.back();
            const auto& c = chordAt (last.beat).chord;
            last.pitch = nearestOf ({ c.root }, p.endHigh ? (double) high - 4.0 : (double) last.pitch, low, high);   // (0.67.0: or high)
            last.length = juce::jmax (last.length, total - last.beat);
            last.technique = LineNote::Technique::normal;
        }

        // Expression: on long single notes outside the bursts, a bend into the note or vibrato.
        const auto inBurst = [&burstSpans] (double beat)
        {
            for (const auto& [a, b] : burstSpans)
                if (beat >= a - 1.0e-9 && beat < b - 1.0e-9)
                    return true;
            return false;
        };
        const auto single = [&notes] (size_t i)
        {
            for (size_t k = 0; k < notes.size(); ++k)
                if (k != i && std::abs (notes[k].beat - notes[i].beat) < HumanizeLimits::together)
                    return false;
            return true;
        };
        std::vector<LineNote> added;
        auto tremoloUsed = false;
        auto pinchBar = -2;
        auto dipUsed = false;
        auto overbendUsed = false;
        auto harmonicBar = -1;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            auto& n = notes[i];
            if (inBurst (n.beat) || n.technique != LineNote::Technique::normal || ! single (i) || copied.count (std::lround (n.beat * 960.0)) > 0)
                continue;
            const auto t = (float) (n.beat / juce::jmax (1.0e-6, total));
            const auto isLast = i + 1 == notes.size();
            const auto minorRoot = chordAt (n.beat).key.minor ? chordAt (n.beat).key.pitchClass() : chordAt (n.beat).key.pitchClass() + 9;
            // Blues curls: a flat third bent a little sharp.
            if (p.curls && ! isLast && pcOf (n.pitch) == pcOf (minorRoot + 3) && n.length < 0.75 && r.nextFloat() < 0.5f)
            {
                n.effect = LineNote::Effect::bend;
                n.amount = 0.5f;
                continue;
            }
            // (0.116.0) A short note bent now and then (about one in twenty), a whole step.
            if (! p.escalate && n.length >= 0.25 && n.length < 0.75 && ! isLast && n.pitch - 2 >= low && r.nextFloat() < 0.05f)
            {
                n.pitch -= 2;
                n.effect = LineNote::Effect::bend;
                n.amount = 2.0f;
                continue;
            }
            // (0.133.0) The whammy scooped up into a phrase's first note or a held note (2, 3 or now and then 12
            // semitones, in 0.06 s).
            if (p.scoopShare > 0.0f && ! isLast && n.length >= 0.25)
            {
                const auto opens = i == 0 || n.beat - (notes[i - 1].beat + notes[i - 1].length) > 0.4 || notes[i - 1].length >= 1.5;
                if ((opens || n.length >= 1.0) && r.nextFloat() < p.scoopShare)
                {
                    const auto pick = r.nextFloat();
                    n.effect = LineNote::Effect::scoop;
                    n.amount = p.scoopAmount > 0.0f ? p.scoopAmount : pick < 0.15f ? 12.0f : pick < 0.45f ? 3.0f : 2.0f;
                    continue;
                }
            }
            // (0.128.0) A slide up into a phrase's first note (after a rest or a long held note).
            if (! isLast && n.length >= 0.25 && n.pitch - 3 >= low)
            {
                const auto opens = i == 0 || n.beat - (notes[i - 1].beat + notes[i - 1].length) > 0.4 || notes[i - 1].length >= 1.5;
                if (opens && r.nextFloat() < p.slideShare)
                {
                    n.effect = LineNote::Effect::slideIn;
                    n.amount = 3.0f;
                    continue;
                }
            }
            if (n.length < 0.75 || isLast)
                continue;
            // (0.128.0) A fall-off out of a long held note now and then.
            if (n.length >= 1.5 && r.nextFloat() < p.fallShare)
            {
                n.effect = LineNote::Effect::fallOff;
                n.amount = 5.0f;
                continue;
            }
            const auto bar = (int) std::floor (n.beat / beatsPerBar);
            if (p.harmonics && bar % 4 == 0 && bar != harmonicBar && t > 0.15f)
            {
                harmonicBar = bar;
                n.technique = LineNote::Technique::harmonic;
                continue;
            }
            if (p.pinchSqueals && bar - pinchBar >= 2)
            {
                // A pinch-harmonic squeal shaken wide, every two bars at most.
                pinchBar = bar;
                n.velocity = GuitarVelocity::of (vel.pinch);
                n.effect = LineNote::Effect::vibrato;
                n.amount = p.vibrato;
                continue;
            }
            if (p.dip && ! dipUsed && t > 0.5f && n.length >= 1.0)
            {
                // (0.68.0) A whammy dip: down a semitone and back, like a voice.
                dipUsed = true;
                n.effect = LineNote::Effect::dive;
                n.amount = 1.0f;
                continue;
            }
            if (p.tremolo && ! tremoloUsed && t > 0.7f)
            {
                tremoloUsed = true;
                n.technique = LineNote::Technique::tremolo;
                continue;
            }
            // (0.129.0) The whammy bar: a held phrase end dived away (7 to 12 semitones, as far as the bend range
            // reaches), a held note shaken.
            if (p.diveShare > 0.0f && n.length >= 1.5 && r.nextFloat() < p.diveShare)
            {
                n.effect = LineNote::Effect::dive;
                n.amount = (float) (7 + r.nextInt (6));
                continue;
            }
            if (p.wobbleShare > 0.0f && n.length >= 1.0 && r.nextFloat() < p.wobbleShare)
            {
                n.effect = LineNote::Effect::wobble;
                n.amount = 1.0f;
                continue;
            }
            // (0.129.0) A trill: the note and the scale note above it in turn, fast, over its first two fifths,
            // then the note held.
            if (p.trillShare > 0.0f && n.length >= 2.0 && r.nextFloat() < p.trillShare)
            {
                const auto here = poolOf (scaleAt (n.beat), low, high);
                const auto at = nearestIndex (here, n.pitch);
                if (at + 1 < (int) here.size())
                {
                    const auto g = gridFor (bpm > 0.0 ? juce::jlimit (0.125, 0.25, bpm / 60.0 / 12.0) : 0.125);
                    const auto upper = here[(size_t) at + 1];
                    const auto until = n.beat + n.length * 0.4;
                    auto k = 0;
                    for (auto x = n.beat; x < until - g * 0.5; x += g, ++k)
                    {
                        LineNote m = n;
                        m.beat = x;
                        m.length = g;
                        m.pitch = k % 2 == 0 ? n.pitch : upper;
                        m.effect = LineNote::Effect::none;
                        added.push_back (m);
                    }
                    const auto held = n.beat + n.length - (n.beat + k * g);
                    n.beat += k * g;
                    n.length = held;
                    n.effect = LineNote::Effect::vibrato;
                    n.amount = p.vibrato;
                    continue;
                }
            }
            // (0.129.0) The anthem bend: the note struck, then bent slowly up into it from a whole step under.
            // (A real unison bend holds the note on another string while one bends up to it; a pitch bend moves the
            // whole channel, so here the bend comes after the strike.)
            if (p.unisonBends && n.length >= 2.0 && n.pitch - 2 >= low && r.nextFloat() < 0.5f)
            {
                const auto strike = juce::jmin (0.5, n.length / 3.0);
                LineNote bent = n;
                bent.beat = n.beat + strike;
                bent.length = n.length - strike;
                bent.pitch = n.pitch - 2;
                bent.effect = LineNote::Effect::slowBend;
                bent.amount = 2.0f;
                added.push_back (bent);
                n.length = strike - 0.02;
                n.effect = LineNote::Effect::ring;   // (no controller; kept off legato's overlap: the bend mustn't catch it)
                continue;
            }
            // (0.116.0) Bent before it's struck and let down onto its note (about a third of the bends in the
            // spacious solos), where the note a whole step up is in the scale.
            if (r.nextFloat() < p.releaseShare && n.pitch + 2 <= high && has (scaleAt (n.beat), pcOf (n.pitch + 2)))
            {
                n.effect = LineNote::Effect::release;
                n.amount = 2.0f;
                continue;
            }
            // (0.128.0) Bends aimed: into a note that sings over the chord, else bent only one time in three.
            const auto aimed = ! p.aimedBends || has (aimsAt (n.beat), pcOf (n.pitch)) || r.nextFloat() < 0.33f;
            if (aimed && r.nextFloat() < p.bendChance)
            {
                // A bend: written under its note, bent up into it.
                auto weightSum = 0.0f;
                // (0.116.0) Bends of a step and a half or more are rare in real solos (4 in 101): weighed down.
                const auto weightOf = [] (float size, float w) { return size >= 3.0f ? w * 0.35f : w; };
                for (const auto& [size, w] : p.bends)
                    if (size <= 2.0f || t >= p.bigFrom)
                        weightSum += weightOf (size, w);
                auto pick = r.nextFloat() * weightSum;
                auto size = 2.0f;
                for (const auto& [s, w] : p.bends)
                    if (s <= 2.0f || t >= p.bigFrom)
                    {
                        if (pick < weightOf (s, w)) { size = s; break; }
                        pick -= weightOf (s, w);
                    }
                if (p.escalate)
                    size = t < 0.34f ? 1.0f : t < 0.67f ? 2.0f : 3.0f;   // (0.68.0: half, whole, a step and a half)
                if (p.oneOverbend && ! overbendUsed && t >= 0.5f && n.length >= 1.5)
                {
                    size = 5.0f;   // (0.134.0: one big over-bend past halfway)
                    overbendUsed = true;
                }
                const auto semis = juce::roundToInt (size);
                if (n.pitch - semis >= low)
                {
                    n.pitch -= semis;
                    n.effect = LineNote::Effect::bend;
                    n.amount = size;
                    if (p.shortBends && size >= 2.0f && r.nextFloat() < 0.5f)
                        n.amount = size - 0.25f;   // (0.69.0) left a little short: it rubs against the chord
                    continue;
                }
            }
            if (p.doubleStops && r.nextFloat() < 0.3f)
            {
                // A 6th double stop: a note a sixth under it.
                LineNote under = n;
                under.pitch = p.fourthDoubleStops ? n.pitch - 5   // (0.116.0: fourths)
                            : p.thirdDoubleStops ? n.pitch - (has (scaleAt (n.beat), pcOf (n.pitch - 4)) ? 4 : 3)   // (0.68.0: thirds)
                                                 : n.pitch - (has (scaleAt (n.beat), pcOf (n.pitch - 9)) ? 9 : 8);
                if (under.pitch >= low)
                {
                    added.push_back (under);
                    continue;
                }
            }
            if (! p.vibratoLastOnly && (r.nextFloat() >= p.vibratoSkip || n.pitch < low + 12) && r.nextFloat() < p.vibratoHeld)
            {
                n.effect = LineNote::Effect::vibrato;
                n.amount = p.vibrato;
            }
        }
        notes.insert (notes.end(), added.begin(), added.end());

        // (0.134.0) Whammy squeals at phrase ends: held notes (not the last) taken down an octave where there's room
        // and pulled up 6 to 8 semitones with the bar (now and then an octave), spread over the solo.
        if (p.squeals > 0)
        {
            std::vector<size_t> candidates;
            for (size_t i = 0; i + 1 < notes.size(); ++i)
            {
                const auto& n = notes[i];
                if (n.length >= 0.75 && n.beat >= beatsPerBar - 1.0e-6 && ! inBurst (n.beat) && n.technique == LineNote::Technique::normal
                    && (n.effect == LineNote::Effect::none || n.effect == LineNote::Effect::vibrato) && single (i)
                    && copied.count (std::lround (n.beat * 960.0)) == 0)
                    candidates.push_back (i);
            }
            std::sort (candidates.begin(), candidates.end(), [&notes] (size_t a, size_t b) { return notes[a].beat < notes[b].beat; });
            const auto wanted = juce::jmin ((int) candidates.size(), juce::jmax (1, juce::roundToInt ((float) p.squeals * (float) bars / 16.0f)));
            for (auto k = 0; k < wanted; ++k)
            {
                auto& n = notes[candidates[(size_t) ((k * 2 + 1) * (int) candidates.size() / (2 * wanted))]];
                if (n.pitch - 12 >= low)
                    n.pitch -= 12;
                n.effect = LineNote::Effect::pullUp;
                n.amount = r.nextFloat() < 0.15f ? 12.0f : (float) (6 + r.nextInt (3));
            }
        }

        // (0.116.0) A phrase played again bends and shakes as it did the first time (its notes written as theirs).
        {
            const auto bentBy = [] (const LineNote& n) { return n.effect == LineNote::Effect::bend ? juce::roundToInt (n.amount) : 0; };
            for (const auto& link : copyLinks)
            {
                const LineNote* source = nullptr;
                for (const auto& n : notes)
                    if (std::lround (n.beat * 960.0) == link.sourceKey && n.pitch + bentBy (n) == link.sourcePitch)
                        source = &n;
                if (source == nullptr)
                    continue;
                for (auto& n : notes)
                    if (std::lround (n.beat * 960.0) == link.copyKey && n.pitch == link.copyPitch && n.effect == LineNote::Effect::none)
                    {
                        n.pitch = source->pitch + (link.copyPitch - link.sourcePitch);
                        n.effect = source->effect;
                        n.amount = source->amount;
                        break;
                    }
            }
        }

        // (0.69.0) Tension: the note before each phrase's last note a half step above it, resolving down onto it
        // (where the last note isn't bent and the note before is a plain one in the same phrase).
        for (const auto target : tensionTargets)
            for (size_t i = 1; i < notes.size(); ++i)
                if (std::abs (notes[i].beat - target) < 1.0e-6)
                {
                    auto& before = notes[i - 1];
                    if (notes[i].effect != LineNote::Effect::bend && before.effect == LineNote::Effect::none
                        && before.technique == LineNote::Technique::normal && target - before.beat < phraseLength && notes[i].pitch + 1 <= high)
                        before.pitch = notes[i].pitch + 1;
                    break;
                }

        // The very last note: a dive (a pinch harmonic first), or held with vibrato.
        {
            auto* last = &notes.front();
            for (auto& n : notes)
                if (n.beat >= last->beat)
                    last = &n;
            if (p.scream)
            {
                // (0.67.0) Before it, a harmonic screamed up with the bar: the last held note before the end.
                LineNote* held = nullptr;
                for (auto& n : notes)
                    if (&n != last && n.length >= 0.5 && n.beat < last->beat - 1.0e-6 && ! inBurst (n.beat) && n.pitch - 3 >= low)
                        held = &n;
                if (held != nullptr)
                {
                    held->technique = LineNote::Technique::harmonic;
                    // (written 3 under its note; a note already bent: from where its own bend began)
                    held->pitch += (held->effect == LineNote::Effect::bend ? juce::roundToInt (held->amount) : 0) - 3;
                    held->effect = LineNote::Effect::bend;
                    held->amount = 3.0f;
                }
            }
            // (0.128.0) The style's endings (none listed: a dive where the style dives, else held with vibrato).
            auto ending = p.dive ? Ending::dive : Ending::held;
            if (! p.endings.empty())
            {
                auto sum = 0.0f;
                for (const auto& e : p.endings)
                    sum += e.second;
                auto pick = r.nextFloat() * sum;
                for (const auto& e : p.endings)
                {
                    if (pick < e.second) { ending = e.first; break; }
                    pick -= e.second;
                }
            }
            const auto& lastChord = chordAt (last->beat).chord;
            const auto fifthOf = pcOf (lastChord.root + (lastChord.type == Chord::Type::diminished ? 6 : 7));
            // (the last bar written again: a passage running into a note on the last beat, held)
            const auto rewriteLastBar = [&] (bool repeated)
            {
                const auto from = juce::jmax (0.0, total - beatsPerBar);
                const auto top = nearestOf ({ lastChord.root }, (double) high - 4.0, low, high);
                notes.erase (std::remove_if (notes.begin(), notes.end(), [from] (const LineNote& n) { return n.beat >= from - 1.0e-6; }), notes.end());
                for (auto& n : notes)
                    n.length = juce::jmin (n.length, juce::jmax (0.05, from - n.beat - 0.02));
                const auto end = total - 1.0;
                const auto g = runGrid (repeated ? 0.25 : 1.0 / 3.0);
                auto pool = poolOf (scaleAt (from), low, high);
                if (pool.empty())
                    repeated = true;
                auto idx = pool.empty() ? 0 : nearestIndex (pool, top - 9.0);
                for (auto x = from; x < end - 1.0e-9; x += g)
                {
                    const auto pitch = repeated ? top : pool[(size_t) idx];
                    addNote (notes, x, g, pitch, velocityAt (0.9f, std::abs (x - std::round (x)) < 1.0e-6));
                    idx = juce::jmin ((int) pool.size() - 1, idx + 1);
                    if (! repeated && pool[(size_t) idx] >= top)
                        idx = juce::jmax (0, idx - 3);   // (round again, so the climb arrives on its top at the end)
                }
                addNote (notes, end, total - end, top, velocityAt (0.9f, true));
                last = &notes.back();
            };
            switch (ending)
            {
                case Ending::bendUp:
                {
                    // Bent up into the root or the fifth and held there.
                    const auto target = nearestOf (r.nextBool() ? std::vector<int> { lastChord.root } : std::vector<int> { fifthOf },
                                                   (double) last->pitch, low + 2, high);
                    last->pitch = target - 2;
                    last->effect = LineNote::Effect::bend;
                    last->amount = 2.0f;
                    break;
                }
                case Ending::fallOff:
                    last->effect = LineNote::Effect::fallOff;
                    last->amount = 5.0f;
                    break;
                case Ending::dive:
                    last->velocity = GuitarVelocity::of (vel.pinch);
                    last->effect = LineNote::Effect::dive;
                    last->amount = 12.0f;
                    break;
                case Ending::dropLow:
                    // Down to the low root, a beat, then the riff takes over.
                    last->pitch = nearestOf ({ lastChord.root }, (double) low, low, high);
                    last->length = juce::jmin (last->length, 1.0);
                    last->velocity = GuitarVelocity::of (velocityAt (1.0f, true));
                    last->effect = LineNote::Effect::none;
                    break;
                case Ending::repeatedTop:
                case Ending::tripletClimb:
                    if (total >= 2.0 * beatsPerBar)
                        rewriteLastBar (ending == Ending::repeatedTop);
                    last->effect = LineNote::Effect::vibrato;
                    last->amount = p.vibrato;
                    break;
                case Ending::trillFall:
                {
                    // (0.129.0) Trilled with the note above for half its length, then held and fallen off.
                    const auto here = poolOf (scaleAt (last->beat), low, high);
                    const auto at = nearestIndex (here, last->pitch);
                    const auto length = total - last->beat;
                    if (at + 1 < (int) here.size() && length >= 1.0)
                    {
                        const auto g = gridFor (bpm > 0.0 ? juce::jlimit (0.125, 0.25, bpm / 60.0 / 12.0) : 0.125);
                        const auto from = last->beat;
                        const auto pitch = last->pitch, upper = here[(size_t) at + 1];
                        auto k = 0;
                        std::vector<LineNote> trill;
                        for (auto x = from; x < from + length * 0.5 - g * 0.5; x += g, ++k)
                        {
                            LineNote m = *last;
                            m.beat = x;
                            m.length = g;
                            m.pitch = k % 2 == 0 ? pitch : upper;
                            m.effect = LineNote::Effect::none;
                            trill.push_back (m);
                        }
                        last->beat = from + k * g;
                        last->length = total - last->beat;
                        notes.insert (notes.end(), trill.begin(), trill.end());
                        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
                        last = &notes.back();
                    }
                    last->effect = LineNote::Effect::fallOff;
                    last->amount = 7.0f;
                    break;
                }
                case Ending::harmonicDives:
                {
                    // (0.133.0) The last bar: harmonics a beat each on the chord's notes stepping down, each dived a
                    // fifth away, then the low root dived an octave.
                    if (total < 2.0 * beatsPerBar)
                    {
                        last->effect = LineNote::Effect::dive;
                        last->amount = 12.0f;
                        break;
                    }
                    const auto from = total - beatsPerBar;
                    notes.erase (std::remove_if (notes.begin(), notes.end(), [from] (const LineNote& n) { return n.beat >= from - 1.0e-6; }), notes.end());
                    for (auto& n : notes)
                        n.length = juce::jmin (n.length, juce::jmax (0.05, from - n.beat - 0.02));
                    const auto tones = poolOf (chordTones (from), low, high);
                    auto at = tones.empty() ? 0 : nearestIndex (tones, (double) high - 3.0);
                    auto x = from;
                    for (; x < total - 1.0 - 1.0e-9 && ! tones.empty(); x += 1.0)
                    {
                        addNote (notes, x, 0.96, tones[(size_t) at], velocityAt (0.9f, true));
                        notes.back().technique = LineNote::Technique::harmonic;
                        notes.back().effect = LineNote::Effect::dive;
                        notes.back().amount = 7.0f;
                        at = juce::jmax (0, at - 1);
                    }
                    addNote (notes, x, total - x, nearestOf ({ lastChord.root }, (double) low, low, high), velocityAt (1.0f, true));
                    last = &notes.back();
                    last->effect = LineNote::Effect::dive;
                    last->amount = 12.0f;
                    break;
                }
                case Ending::divedDyad:
                {
                    // (0.133.0) Major thirds climbing a half step at a time in eighths into a double stop on the last
                    // chord's root (a third under it, in the scale), held two beats and dived an octave away.
                    const auto top = nearestOf ({ lastChord.root }, (double) last->pitch + 3.0, juce::jmin (high, low + 6), high);
                    const auto under = has (scaleAt (last->beat), pcOf (top - 4)) ? 4 : 3;
                    const auto held = juce::jmin (2.0, total - last->beat);
                    const auto climbFrom = total >= 2.0 * beatsPerBar ? total - beatsPerBar : total - held;
                    const auto heldFrom = total - held;
                    notes.erase (std::remove_if (notes.begin(), notes.end(), [climbFrom] (const LineNote& n) { return n.beat >= climbFrom - 1.0e-6; }), notes.end());
                    for (auto& n : notes)
                        n.length = juce::jmin (n.length, juce::jmax (0.05, climbFrom - n.beat - 0.02));
                    const auto steps = (int) std::floor ((heldFrom - climbFrom) / 0.5 + 1.0e-6);
                    for (auto i = 0; i < steps; ++i)
                    {
                        const auto upper = juce::jmax (low + 4, top - (steps - i));
                        addNote (notes, climbFrom + i * 0.5, 0.48, upper, velocityAt (0.8f, i % 2 == 0));
                        addNote (notes, climbFrom + i * 0.5, 0.48, upper - 4, velocityAt (0.8f, false));
                    }
                    addNote (notes, heldFrom, total - heldFrom, top - under, velocityAt (1.0f, true));
                    notes.back().effect = LineNote::Effect::dive;
                    notes.back().amount = 12.0f;
                    addNote (notes, heldFrom, total - heldFrom, top, velocityAt (1.0f, true));
                    last = &notes.back();
                    last->effect = LineNote::Effect::dive;
                    last->amount = 12.0f;
                    break;
                }
                case Ending::steppingDown:
                {
                    // (0.133.0) Notes held a bar each stepping down from the octave (b7, 5th, b3 in the scale there)
                    // to the last chord's root, each coming in a beat before the bar line and tied over it.
                    const auto m = juce::jlimit (0, 5, juce::jmin (bars - 1, bars / 3));
                    if (m < 2)
                    {
                        last->effect = LineNote::Effect::vibrato;
                        last->amount = p.vibrato;
                        break;
                    }
                    const auto from = total - m * beatsPerBar - 1.0;
                    notes.erase (std::remove_if (notes.begin(), notes.end(), [from] (const LineNote& n) { return n.beat >= from - 1.0e-6; }), notes.end());
                    for (auto& n : notes)
                        n.length = juce::jmin (n.length, juce::jmax (0.05, from - n.beat - 0.02));
                    const auto rootTop = nearestOf ({ lastChord.root }, (double) high - 6.0, juce::jmin (high, low + 12), high);
                    const int steps[] = { 12, 10, 7, 3, 0 };
                    for (auto k = 0; k < m; ++k)
                    {
                        const auto x = from + k * beatsPerBar;
                        const auto pitch = k == m - 1 ? rootTop - 12 : snapToScale (rootTop - 12 + steps[k + (5 - m)], x + 1.0);
                        addNote (notes, x, (k == m - 1 ? total - x : beatsPerBar - 0.02), juce::jlimit (low, high, pitch), velocityAt (0.8f, true));
                        if (k < m - 1)
                        {
                            notes.back().effect = LineNote::Effect::vibrato;
                            notes.back().amount = p.vibrato;
                        }
                    }
                    last = &notes.back();
                    last->effect = LineNote::Effect::vibrato;
                    last->amount = p.vibrato;
                    break;
                }
                case Ending::bendThenDive:
                    // (0.134.0) The root high up, bent into from a whole step under, held, then dived an octave.
                    last->pitch = nearestOf ({ lastChord.root }, (double) high - 4.0, low + 12, high);
                    last->effect = LineNote::Effect::bendDive;
                    last->amount = 12.0f;
                    break;
                case Ending::squealOut:
                {
                    // (0.134.0) A low fourth (the root and the fourth over it) squealed up with the bar and held.
                    const auto rootLow = nearestOf ({ lastChord.root }, (double) low + 2.0, low, high - 5);
                    const auto from = last->beat;
                    const auto length = total - from;
                    last->pitch = rootLow + 5;
                    last->effect = LineNote::Effect::pullUp;
                    last->amount = 8.0f;
                    last->length = length;
                    addNote (notes, from, length, rootLow, velocityAt (1.0f, true));
                    last = &notes.back();
                    last->effect = LineNote::Effect::pullUp;
                    last->amount = 8.0f;
                    break;
                }
                case Ending::tremoloDyads:
                {
                    // (0.134.0) The last two bars: a double stop tremolo-picked over the root (its upper note climbing
                    // the 2nd, b3, 4th and 5th a beat each), into the root and fifth held.
                    if (total < 3.0 * beatsPerBar)
                    {
                        last->effect = LineNote::Effect::vibrato;
                        last->amount = p.vibrato;
                        break;
                    }
                    const auto from = total - 2.0 * beatsPerBar;
                    notes.erase (std::remove_if (notes.begin(), notes.end(), [from] (const LineNote& n) { return n.beat >= from - 1.0e-6; }), notes.end());
                    for (auto& n : notes)
                        n.length = juce::jmin (n.length, juce::jmax (0.05, from - n.beat - 0.02));
                    const auto rootNote = nearestOf ({ lastChord.root }, (double) low + 7.0, low, high - 7);
                    const int uppers[] = { 2, 3, 5, 7 };
                    auto g = gridFor (0.125);
                    if (bpm > 0.0)
                        for (const auto c : { 1.0 / 6.0, 0.25 })
                            if (std::abs (bpm / 60.0 / c - 16.0) < std::abs (bpm / 60.0 / g - 16.0))
                                g = gridFor (c);
                    const auto heldFrom = total - beatsPerBar;
                    auto k = 0;
                    for (auto x = from; x + g <= heldFrom + 1.0e-9; x += g, ++k)
                    {
                        const auto beatIndex = juce::jmin (3, (int) std::floor ((x - from) / juce::jmax (1.0, beatsPerBar / 4.0) + 1.0e-6));
                        addNote (notes, x, g, rootNote + uppers[beatIndex], velocityAt (0.8f, k % 4 == 0));
                        addNote (notes, x, g, rootNote, velocityAt (0.8f, false));
                    }
                    addNote (notes, heldFrom, total - heldFrom, rootNote + 7, velocityAt (0.9f, true));
                    addNote (notes, heldFrom, total - heldFrom, rootNote, velocityAt (0.9f, true));
                    last = &notes.back();
                    last->effect = LineNote::Effect::vibrato;
                    last->amount = p.vibrato;
                    break;
                }
                case Ending::held:
                    last->effect = LineNote::Effect::vibrato;
                    last->amount = p.vibrato;
                    break;
            }
        }

        // (0.129.0) Every note doubled an octave up (down where there's no room), or a twin lead's harmony.
        if (p.octaveDouble)
        {
            std::vector<LineNote> octaves;
            for (const auto& n : notes)
                if (! n.keyswitch && n.technique != LineNote::Technique::tapping)
                {
                    LineNote m = n;
                    m.pitch = n.pitch + 12 <= high ? n.pitch + 12 : n.pitch - 12;
                    if (m.pitch >= low - 5)
                        octaves.push_back (m);
                }
            notes.insert (notes.end(), octaves.begin(), octaves.end());
        }
        if (p.twin)
        {
            // Harmonised from two fifths of the way to the last bar: thirds (two scale steps up), fourths (three)
            // or octaves by the take; bends there written out (a pitch bend would bend both voices on one track).
            const auto mode = p.twinTenth ? 3 : variation % 3;   // (0.134.0: 3: an octave and a third over)
            const auto from = total * 0.4, to = juce::jmax (from, total - beatsPerBar);
            std::vector<LineNote> voice;
            for (auto& n : notes)
            {
                if (n.keyswitch || n.beat < from - 1.0e-9 || n.beat >= to - 1.0e-9)
                    continue;
                if (n.effect == LineNote::Effect::bend || n.effect == LineNote::Effect::release || n.effect == LineNote::Effect::slowBend)
                {
                    if (n.effect != LineNote::Effect::release)
                        n.pitch += juce::roundToInt (n.amount);
                    n.effect = LineNote::Effect::vibrato;
                    n.amount = p.vibrato;
                }
                else if (n.effect != LineNote::Effect::vibrato)
                    n.effect = LineNote::Effect::none;
                LineNote h = n;
                h.harmony = true;
                if (mode == 2)
                    h.pitch = n.pitch + 12 <= high ? n.pitch + 12 : n.pitch - 12;
                else
                {
                    const auto here = poolOf (scaleAt (n.beat), low, high + 7);
                    const auto at = nearestIndex (here, n.pitch);
                    h.pitch = here[(size_t) juce::jmin ((int) here.size() - 1, at + (mode == 0 || mode == 3 ? 2 : 3))];
                    if (mode == 3)
                        h.pitch = h.pitch + 12 <= high ? h.pitch + 12 : h.pitch - 12;
                }
                voice.push_back (h);
            }
            notes.insert (notes.end(), voice.begin(), voice.end());
        }

        // Swing (16ths, or 8ths), and legato: a note running a hair into the next a step or two away
        // (a hammer-on or pull-off on a sampled guitar).
        if (p.swing > 0.5f)
        {
            // Only where the next note leaves room (nothing closer than the shortest note after it).
            std::vector<double> starts;
            for (const auto& n : notes)
                starts.push_back (n.beat);
            std::sort (starts.begin(), starts.end());
            const auto room = juce::jmax (0.05, shortestStep - 0.04);
            const auto unit = p.swing8 ? 0.5 : 0.25;
            const auto shift = unit * (p.swing - 0.5f) * 2.0;
            for (auto& n : notes)
            {
                const auto pos = n.beat / unit;
                const auto k = std::llround (pos);
                if (std::abs (pos - (double) k) > 1.0e-6 || k % 2 == 0)
                    continue;
                const auto next = std::upper_bound (starts.begin(), starts.end(), n.beat + 1.0e-6);
                if (next == starts.end() || *next - (n.beat + shift) >= room)
                    n.beat += shift;
            }
        }
        // (0.68.0) Laid back: the melody (not the bursts) a little behind the beat.
        if (p.lateBeats > 0.0)
            for (auto& n : notes)
                if (! inBurst (n.beat))
                    n.beat += p.lateBeats;
        std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.beat < b.beat; });
        if (p.legato)
            for (size_t i = 0; i + 1 < notes.size(); ++i)
            {
                auto& n = notes[i];
                const auto& next = notes[i + 1];
                const auto gap = next.beat - (n.beat + n.length);
                if (n.effect == LineNote::Effect::none && n.technique != LineNote::Technique::tremolo && single (i)
                    && std::abs (next.pitch - n.pitch) <= 2 && next.pitch != n.pitch && gap < 0.05 && gap > -0.01)
                    n.length = next.beat - n.beat + 0.02;
            }
        for (auto& n : notes)
        {
            n.pitch = juce::jlimit (low - 5, high, n.pitch);
            n.length = juce::jmin (n.length, juce::jmax (0.05, total - n.beat));
        }
        solo.description = juce::String (p.description) + " (" + juce::String (phraseNo) + " phrases, "
                         + juce::String (waves.size()) + (waves.size() == 1 ? " busy wave)" : " busy waves)");
        return solo;
    }
}
