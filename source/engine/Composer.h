#pragma once

#include "engine/Lines.h"
#include "engine/Progressions.h"
#include "engine/Melody.h"
#include "engine/Riffs.h"
#include "engine/DrumGenerator.h"
#include "engine/DrumLearning.h"
#include "engine/Takes.h"
#include "engine/Instruments.h"

#include <array>
#include <optional>

// THE COMPOSER (0.58.0, the owner asked: "a combined page, all layers stacked, each with a lock and
// a re-roll, and one generate button", after another idea generator's way of working). Our own page
// and rules, built from IDEAS' own makers: nothing new is invented here about how a bass line or a
// beat goes, only how the layers are chosen together.
//
//   the chords   a progression from our library (in the KEY, filtered by GENRE and MOOD), repeated to
//                LENGTH bars; or the chord track's chords (FROM CHORD TRACK). Heard as a piano part
//                (one of IDEAS' six piano styles).
//   the layers   CHORDS, MELODY, BASS, GUITAR, LEAD, DRUMS: each one of IDEAS' ideas for those chords
//                (its `style`: which of them; its `variation`: IDEAS' MORE IDEAS count). The bass has a
//                seventh style, locked to the drums' kick; the drums' six styles are the generator's
//                six sections (intro to ending), by the guitar STYLE (or the learned DRUMMER).
//   ROLL         a new take of the layer's style (0.61.0; before: another style and progression).
//   BUSY         per lane, sparse to busy (0.61.0); LOW STRING on bass and guitar.
//   LOCK         a locked layer keeps its style and variation through GENERATE and ROLL. With the chords
//                locked too its notes stay exactly as they are; when the chords change, a locked layer
//                plays the same idea over the new chords (it can't keep notes that no longer fit).
//   GENERATE     every unlocked layer rolled at once.
// The same state always gives the same notes (what you hear is what USE writes).
namespace pr7s
{
    enum class ComposerLayer { chords, melody, bass, guitar, lead, drums };
    constexpr int composerLayerCount = 6;
    const std::array<ComposerLayer, composerLayerCount>& composerLayers();
    juce::String composerLayerName (ComposerLayer);    // "CHORDS", "MELODY"...
    LineIdea::Part composerPartOf (ComposerLayer);      // the IDEAS part it's made as (melody: the MELODY tab's)

    struct ComposerLane
    {
        bool on = true, locked = false;
        int style = 0;        // which of the layer's ideas
        int variation = 0;    // the take (0.61.0: ROLL draws another; engine/Takes.h)
        float busy = 0.5f;    // BUSY (0.61.0): 0 sparse .. 0.5 the approach as usual .. 1 busy
        bool lowString = false;   // LOW STRING (0.61.0: bass and guitar)
        int onlyString = 0;       // ONLY STRING n (0.62.0; 1 the lowest; 0: off): engine/Instruments.h's playOnString
        bool operator== (const ComposerLane&) const = default;
    };

    struct ComposerState
    {
        Key key;                           // (C major)
        juce::String genre, mood;          // the library's filter (empty: any)
        int bars = 8;                      // LENGTH: 4, 8 or 16 (the progression repeated to fill it)
        juce::String progression;          // the library's name (empty or unknown: the first that passes)
        bool fromChordTrack = false;       // the chord track's chords instead of the library's
        bool toChordTrack = true;          // USE also writes the chords onto the chord track (library chords)
        std::array<ComposerLane, composerLayerCount> lanes;

        ComposerState();                   // (the lead off: the melody sings the top line)
        ComposerLane& lane (ComposerLayer l) noexcept { return lanes[(size_t) l]; }
        const ComposerLane& lane (ComposerLayer l) const noexcept { return lanes[(size_t) l]; }
        juce::String toString() const;     // (settings key "composer")
        static ComposerState fromString (const juce::String&);   // (unreadable parts: the defaults)
        bool operator== (const ComposerState&) const = default;
    };

    // What the layers are made with: IDEAS' settings, as the studio keeps them.
    struct ComposerContext
    {
        double beatsPerBar = 4.0;
        GuitarTuning tuning = GuitarTuning::all().front();
        GuitarTuning bassTuning = bassTunings().front();   // (0.61.0: LOW STRING and the bass's range)
        // (0.62.0) The Hub's INSTRUMENTS profiles used for these lanes (none: the tunings above).
        std::optional<InstrumentProfile> bassProfile, guitarProfile, leadProfile;
        GuitarVelocities velocities;
        GuitarStyle style {};
        RiffSettings riff;
        DrumSettings drums;
        MelodySettings melody;
        std::optional<DrumProfile> drummer;   // DRUMMER (none: the style's rules)
        float humanize = 0.0f;
        bool feel = true;
        double bpm = 120.0;
    };

    // The library progressions that pass the state's filter in its key (the key's own mode first).
    std::vector<int> composerProgressions (const ComposerState&);
    // The state's progression (by name; else the first that passes; -1: none pass).
    int composerProgressionIndex (const ComposerState&);
    // Its chords repeated to fill LENGTH (whole repeats; at least once).
    std::vector<ProgressionStep> composerSteps (const ComposerState&);
    // Those as spans from `startBeat`.
    std::vector<ChordSpan> composerSpans (const ComposerState&, double startBeat, double beatsPerBar);

    // How many styles a layer has (the bass: 7, the last locked to the drums' kick).
    int composerStyleCount (ComposerLayer);
    // Every style of a layer for these chords, at the lane's variation (not humanized): the style menu.
    std::vector<LineIdea> composerChoices (ComposerLayer, const ComposerState&, const std::vector<ChordSpan>&, const ComposerContext&);
    // Every layer's idea, the lanes switched off too (shown dimmed), humanized as IDEAS' HUMANIZE.
    std::array<LineIdea, composerLayerCount> composeLayers (const ComposerState&, const std::vector<ChordSpan>&, const ComposerContext&);

    // ROLL (0.61.0, the owner's rule: the approach is chosen only in the style menu): a new take of the
    // same style (another variation); the chords keep their progression. Locked or not: the caller decides.
    void rollLane (ComposerState&, ComposerLayer, juce::Random&);
    // The strings, frets and the profile (if any) a lane plays on (0.62.0; the bass and the guitar).
    struct LaneStrings { std::vector<int> strings; int frets = 24; const InstrumentProfile* profile = nullptr; };
    LaneStrings composerStrings (ComposerLayer, const ComposerContext&);
    // Whether a layer has LOW STRING (the bass and the guitar).
    inline bool hasLowString (ComposerLayer l) { return l == ComposerLayer::bass || l == ComposerLayer::guitar; }
    // GENERATE: every unlocked lane rolled (new takes, the same styles). Returns how many were.
    int generateComposer (ComposerState&, juce::Random&);
}
