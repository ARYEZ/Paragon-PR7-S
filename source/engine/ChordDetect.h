#pragma once

#include "engine/Chords.h"
#include "engine/TempoMap.h"

#include <juce_audio_basics/juce_audio_basics.h>

// Chord detection (0.42.0, the owner asked: as their chord tool's "detect chords" from MIDI
// and audio). One detector for both: the notes heard in each stretch of the song (a beat, half a
// bar or a bar: `DetectSettings::stepBeats`) as twelve weights, one per pitch class, and the bass;
// every chord scored against them (its notes heard, notes outside it, its root in the bass); then
// one pass over the whole stretch that changes chord only where the notes clearly say so (a
// passing melody note doesn't flicker it): the chord sequence with the best total score less a
// cost per change.
//
// MIDI: the weights are the notes' lengths and velocities, exactly. Audio: the sound's spectrum
// folded into the twelve pitch classes (a "chromagram") between A1 and C7, with each note's first
// overtones partly taken away (a C's fifth and third overtones would otherwise read as G and E),
// and the bass from 40-260 Hz. Audio is a best guess: clear chordal parts (a guitar, a piano)
// read well; a full mix with vocals and drums less so; tuning far from A = 440 Hz not at all.
// Our own method and weights.
namespace pr7s
{
    struct DetectSettings
    {
        double stepBeats = 2.0;    // the shortest a chord may last (and how often it's asked)
        bool plainOnly = true;     // majors and minors (and diminished): the owner's chords
        double changeCost = 0.12;  // how sure a change must be (part of a step's own score)

        juce::String toString() const;
        static DetectSettings fromString (const juce::String&);
        bool operator== (const DetectSettings& o) const noexcept { return toString() == o.toString(); }
    };

    // One stretch's evidence: twelve pitch-class weights and the bass's (-1: none heard).
    struct ChromaStep
    {
        std::array<double, 12> weight {};
        std::array<double, 12> bass {};
        double total() const;
    };

    // The best chord for each step, changing only where it pays; events where the chord changes
    // (beats from `from` by `step`). A silent step continues the chord before.
    std::vector<ChordEvent> chordsFromChroma (const std::vector<ChromaStep>&, double from, double step, const DetectSettings&);

    // Notes into steps (velocity and overlap weighted; the bass: the lowest held over a quarter of the step).
    std::vector<ChromaStep> chromaFromNotes (const std::vector<HeardNote>&, double from, double to, double step);
    std::vector<ChordEvent> detectChordsInNotes (const std::vector<HeardNote>&, double from, double to, const DetectSettings&);

    // A sound (mono, `rate`) that starts at song beat `startBeat`, into steps over [from, to) by the
    // tempo map; the steps' chroma from the frames inside them.
    std::vector<ChromaStep> chromaFromAudio (const float* samples, int numSamples, double rate, double startBeat,
                                             const TempoMap&, double from, double to, double step);
    std::vector<ChordEvent> detectChordsInAudio (const float* samples, int numSamples, double rate, double startBeat,
                                                 const TempoMap&, double from, double to, const DetectSettings&);

    // The chord these notes make together (the MIDI editor's selection): exact first (recogniseChord),
    // else the best scored reading; none for fewer than two pitch classes.
    std::optional<Chord> chordOfNotes (const std::vector<int>& pitches, bool plainOnly = false);
}
