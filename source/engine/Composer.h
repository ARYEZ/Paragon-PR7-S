#pragma once

#include "engine/Lines.h"
#include "engine/Progressions.h"
#include "engine/Melody.h"
#include "engine/Riffs.h"
#include "engine/RiffGenerator.h"
#include "engine/DrumGenerator.h"
#include "engine/DrumLearning.h"
#include "engine/Takes.h"
#include "engine/Instruments.h"
#include "engine/SoloEngine.h"

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
        // (0.86.0) DOUBLE THE SYNC TRACK (the bass and the drums, with SYNC on): every hit of the sync lane, not only
        // its accents. The bass doubles the riff's notes (an octave under where it can), the drums' kick every hit.
        bool doubleSync = false;
        // (0.94.0) The section type's approach for this lane (engine/SectionTypes.h: sectionApproaches); -1 automatic
        // (the style and the take choose, as before).
        int approach = -1;
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
        // (0.85.0) The guitar: riffs made by the riff generator (engine/RiffGenerator.h), every ROLL a new one,
        // leaning to FLAVOUR; or the written riffs of 0.53.0 (WRITTEN RIFFS in the guitar's style menu).
        int riffFlavour = 0;               // (riffFlavours()' index: 0 ANY)
        bool writtenRiffs = false;
        // (0.86.0, the owner) SYNC: the lane every other one follows (its rhythm); -1 off. It stays on until turned
        // off, so re-rolling the sync lane re-writes the others to its new rhythm. See syncRhythmOf below.
        int syncLane = -1;
        // (0.94.0) The block's TYPE (engine/SectionTypes.h; empty: STANDARD). It sets the guitar's flavour, and with it
        // the band's part (DRONING's band) and the approaches each lane offers.
        juce::String sectionType;
        std::optional<ComposerLayer> syncLayer() const noexcept
        {
            return syncLane >= 0 && syncLane < composerLayerCount ? std::optional<ComposerLayer> ((ComposerLayer) syncLane) : std::nullopt;
        }

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
        SoloStyle solo {};   // (0.66.0: the lead lane's seventh style, the whole solo)
        RiffSettings riff;
        DrumSettings drums;
        MelodySettings melody;
        std::optional<DrumProfile> drummer;   // DRUMMER (none: the style's rules)
        float humanize = 0.0f;
        bool feel = true;
        double bpm = 120.0;
        // (0.75.0) Your own riff (engine/RiffPaste.h), already repeated over the section: the guitar plays it
        // (shaped by RIFF SETTINGS: as written at their middle values) and the drums lock their kick to it.
        std::optional<LineIdea> myRiff;
    };

    // (0.86.0) SYNC: the rhythm a lane's idea gives the others. `all`: every hit (notes starting together are one
    // hit; the drums: every kick); `accents`: the hits that stand out (a pitched lane: open notes (velocity 60 and
    // over), anything held half a beat or more, and a bar's first hit; the drums: kicks on a beat). The followers:
    //   DRUMS    the kick on the accents (DOUBLE: every hit); the snare, cymbals and fills the section's own.
    //   BASS     the chords' roots on the accents (DOUBLE: the riff's own notes, each hit's lowest, an octave under
    //            where the bass reaches; a high stab over the chugs: the chord's root; the drums leading: every kick).
    //   CHORDS   a stab of the chord on each accent, held to the next (two beats at most).
    //   GUITAR   chugs on every hit: the root palm-muted, a power chord let ring on the accents.
    //   MELODY   its own line, a note within a sixteenth of an accent moved onto it (looser: the owner's choice).
    //   LEAD     the same as the melody.
    struct SyncRhythm { std::vector<double> all, accents; };
    SyncRhythm syncRhythmOf (ComposerLayer leader, const LineIdea&, double beatsPerBar, double originBeat);
    inline bool hasDoubleSync (ComposerLayer l) { return l == ComposerLayer::bass || l == ComposerLayer::drums; }

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
    // (0.97.1) True when the section type's band writes this lane (DRONING's bass, keys, lead and drums; STACCATO /
    // SPLIT CHUG's bass and drums; CLEAN VERSE's guitar, bass, drums and keys; SYNC off): the lane's styles don't
    // change it then, its approaches do, so composerChoices gives one idea per approach (in sectionApproaches' order).
    bool sectionTypeWritesLane (ComposerLayer, const ComposerState&, const ComposerContext&);
    // Every layer's idea, the lanes switched off too (shown dimmed), humanized as IDEAS' HUMANIZE.
    std::array<LineIdea, composerLayerCount> composeLayers (const ComposerState&, const std::vector<ChordSpan>&, const ComposerContext&);

    // (0.85.0) The guitar's riff generator recipe for the state (its section, flavour, BUSY, the take's seed), and
    // that riff's fingerprint (empty with written riffs or your riff): the composer tells repeats apart by it.
    RiffRecipe guitarRecipe (const ComposerState&, const ComposerContext&);
    juce::String guitarRiffFingerprint (const ComposerState&, const ComposerContext&);

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
