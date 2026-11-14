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
                           turning, flurry };   // (0.116.0: turning cells, the rhythm kept; sextuplet flurries falling)
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
                    p.vibrato = 0.9f; p.climb = 12;
                    p.description = "Bluesy and loose: major and minor mixed, flat thirds curled sharp, licks repeated so they "
                                    "shift against the bar, crammed bursts, big over-bends and a climax of repeated bends";
                    break;
                case SoloStyle::screamAndDive:
                    p.sixth = p.flatFive = p.majorThird = true;
                    p.startDensity = 4.0f; p.peakDensity = 14.0f; p.restBeats = 1.0; p.phraseBars = 1;
                    p.burst = Burst::chromaticShift; p.burstFrom = 0.3f;
                    p.bendChance = 0.6f; p.bends = { { 1.0f, 1.0f }, { 2.0f, 0.8f }, { 3.0f, 0.3f }, { 4.0f, 0.2f } }; p.bigFrom = 0.5f;
                    p.vibrato = 1.3f; p.climb = 12; p.dive = p.scream = true;
                    p.description = "Aggressive groove-metal lead: the blues scale with the major 3rd and 6th, slow half-step bends "
                                    "from below, licks shifted up a fret at a time, a harmonic screamed up with the bar and a dive bomb to end";
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
                    p.bendChance = 0.4f; p.bends = { { 2.0f, 1.0f }, { 1.0f, 0.5f } };
                    p.releaseShare = 0.3f; p.vibrato = 0.8f;
                    p.description = "A major-key melody sung from the top down: phrases from the and of three into a held third "
                                    "or fifth, each lower than the last, quick grace notes into the held notes";
                    break;
                case SoloStyle::sungInTheGaps:
                    p.majorOnMajor = p.graceNotes = p.cycleTheme = p.octaveDownLate = p.noSixteenths = true;
                    p.notesPerBeat = 1.3f; p.holdMin = 2.0; p.holdMax = 3.0; p.pickupMax = 2.0; p.landBeat1 = 0.3f;
                    p.waves = 0; p.climb = 7; p.restChance = 0.15f;
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
                                                  SoloStyle::laidBackBends, SoloStyle::cycleTheme, SoloStyle::twoSpeeds, SoloStyle::lickTwice };
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
                          const GuitarVelocities& vel, int variation, double shortest)
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
        const auto phraseLength = p.phraseBars * beatsPerBar;
        const auto phrases = juce::jmax (1, (int) std::ceil (total / phraseLength - 1.0e-6));
        const auto shortestStep = shortest > 0.0 ? shortest : 0.0;
        const auto gridFor = [shortestStep] (double wanted) { return juce::jmax (wanted, shortestStep); };
        auto& notes = solo.notes;

        std::vector<double> tensionTargets;
        const auto velocityAt = [&] (float t, bool accent)
        {
            const auto base = (float) vel.sustainSoft + ((float) vel.sustainHard - (float) vel.sustainSoft) * t;
            return vel.open (juce::roundToInt (base) + (accent ? 6 : 0));
        };

        std::vector<std::pair<double, double>> burstSpans;   // (from, to): no bends or vibrato added there
        // The style's signature passage from `burstFrom` to `landing` (0.116.0: placed as waves, not every other phrase).
        const auto writeBurst = [&] (double burstFrom, double landing, double centre, float t, int burstIndex, bool peak)
        {
            burstSpans.push_back ({ burstFrom, landing });
            auto pool = poolOf (scaleAt (burstFrom), low, high);
            if (pool.empty())
                return;
            auto idx = nearestIndex (pool, centre + 5.0);
            juce::ignoreUnused (burstIndex, peak);
            switch (p.burst)
            {
                case Burst::run:
                {
                    // Down the scale in 16ths.
                    const auto g = gridFor (0.25);
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
                    const auto g = gridFor (1.0 / 3.0);
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
                    const auto g = gridFor (1.0 / 3.0);
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
                    const auto g = gridFor (0.25);
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
                    const auto g = gridFor (0.25);
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
                    const auto g = gridFor (1.0 / 3.0);
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
                    const auto g = gridFor (0.25);
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
                    const auto g = gridFor (0.25);
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
                    const auto g = gridFor (0.25);
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
                    const auto g = gridFor (1.0 / 3.0);
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
                    const auto g = gridFor (0.25);
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
                    const auto g = gridFor (0.25);
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
                    const auto g = gridFor (1.0 / 6.0);
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
                    const auto g = gridFor (0.25);
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
            return t <= p.peakAt ? t / juce::jmax (0.05f, p.peakAt) : 1.0f - 0.6f * (t - p.peakAt) / juce::jmax (0.05f, 1.0f - p.peakAt);
        };
        const auto centreAt = [&] (double beat)
        {
            const auto t = (float) juce::jlimit (0.0, 1.0, beat / juce::jmax (1.0e-6, total));
            auto c = (double) low + 7.0 + p.climb * archOf (t);
            if (p.octaveDownLate && t >= 0.5f && c - 12.0 >= low + 2)
                c -= 12.0;
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
            if (p.twoSpeed)
                at = { -1.0f };
            else if (count == 1) at = { 0.72f };
            else if (count == 2) at = { 0.4f, 0.74f };
            else if (count >= 3) at = { 0.3f, 0.56f, 0.8f };
            const auto burstBars = bars >= 16 ? 2 : 1;
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
                return nearestOf (r.nextBool() ? std::vector<int> { third } : std::vector<int> { fifth }, around, low, high);
            const auto pick = r.nextFloat();
            return nearestOf (pick < 0.45f ? std::vector<int> { c.root } : pick < 0.75f ? std::vector<int> { fifth } : std::vector<int> { third },
                              around, low, high);
        };

        auto cursor = 0.0;
        auto phraseNo = 0;
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
        const auto lickBars = p.lickTwice && bars >= 6 ? 4 : 0;
        const auto phrasesEnd = total - lickBars * beatsPerBar;
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
                if (p.cycleTheme && cycle > 0.0)
                    period = cycle;
                const auto nextCopy = std::ceil ((cursor - head.start) / period - 1.0e-6) * period;
                if ((p.restate || (p.cycleTheme && cycle > 0.0)) && nextCopy - (cursor - head.start) < beatsPerBar - 1.0e-6)
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
                    copyFrom = nullptr;
            }
            if (copyFrom != nullptr)
            {
                ringOn (copyFrom->start + shift);
                Phrase again { copyFrom->start + shift, copyFrom->end + shift, {} };
                const auto steps = newNotes ? (r.nextBool() ? 2 : -2) : 0;
                const auto octave = p.restate && t >= 0.5f ? 12 : 0;
                for (auto n : copyFrom->notes)
                {
                    n.beat += shift;
                    if (newNotes)
                    {
                        const auto pool = poolOf (scaleAt (n.beat), low, high);
                        if (! pool.empty())
                            n.pitch = pool[(size_t) juce::jlimit (0, (int) pool.size() - 1, nearestIndex (pool, n.pitch) + steps)];
                    }
                    else
                        n.pitch = snapToScale (n.pitch + (n.pitch + octave <= high ? octave : 0), n.beat);
                    n.effect = LineNote::Effect::none;
                    notes.push_back (n);
                    again.notes.push_back (n);
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
            const auto hold = juce::jmin (p.holdMin + (p.holdMax - p.holdMin) * r.nextDouble(), limit - landAt);
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
            const auto centre = centreAt (landAt);
            auto land = landingPitch (landAt, centre);
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
            ++phraseNo;
        }

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
        auto harmonicBar = -1;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            auto& n = notes[i];
            if (inBurst (n.beat) || n.technique != LineNote::Technique::normal || ! single (i))
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
            if (n.length >= 0.25 && n.length < 0.75 && ! isLast && n.pitch - 2 >= low && r.nextFloat() < 0.05f)
            {
                n.pitch -= 2;
                n.effect = LineNote::Effect::bend;
                n.amount = 2.0f;
                continue;
            }
            if (n.length < 0.75 || isLast)
                continue;
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
            // (0.116.0) Bent before it's struck and let down onto its note (about a third of the bends in the
            // spacious solos), where the note a whole step up is in the scale.
            if (r.nextFloat() < p.releaseShare && n.pitch + 2 <= high && has (scaleAt (n.beat), pcOf (n.pitch + 2)))
            {
                n.effect = LineNote::Effect::release;
                n.amount = 2.0f;
                continue;
            }
            if (r.nextFloat() < p.bendChance)
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
            if (p.dive)
            {
                last->velocity = GuitarVelocity::of (vel.pinch);
                last->effect = LineNote::Effect::dive;
                last->amount = 12.0f;
            }
            else
            {
                last->effect = LineNote::Effect::vibrato;
                last->amount = p.vibrato;
            }
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
