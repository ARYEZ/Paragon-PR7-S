#pragma once

#include "engine/Lines.h"
#include <optional>

namespace pr7s
{
    // (0.219.0, the owner: "a metal song with the normal options for the sections but for the keyboards ... more of a
    // cyberpunk feel, keep the other instruments in the rock and metal world") A lane's WORLD: the world its parts come
    // from. The section's own (0: the section type's options and the lane's styles, as always) or another genre's, whose
    // parts the lane plays whatever the section type: the rest of the band stays on the type. The parts still read the
    // section's chords and key, and lock to the kick and the guitar's hits where they say so.
    //
    // CYBERPUNK (the keys lane) is our own design from the genre's well-known habits, not from any one song: sixteenth
    // arpeggios whose filter opens, gated chord stabs in 3+3+2, pumping pads ducked on the kick (written as CC 11 dips),
    // octave lead hooks, risers and filter sweeps (CC 74) into the chorus, glitch stutters, sub drops under the chugs,
    // sus2 / add9 colours and the minor key's flat sixth. CC 74 is the usual filter-cutoff controller and CC 11 the
    // expression (level): a synth that doesn't answer them plays the notes without the moves (learn them to its filter
    // and level). Everything in 4/4 grids, scaled to the bar.
    enum class LaneWorld { section = 0, cyberpunk = 1 };
    const juce::StringArray& laneWorldNames();          // "ROCK / METAL (THE SECTION'S)", "CYBERPUNK"
    juce::String laneWorldDescription (int world);

    // Where a world's part is being written: chooses AUTOMATIC's option.
    enum class WorldSection { intro, verse, preChorus, chorus, bridge, breakdown, outro, solo };

    const juce::StringArray& cyberpunkKeysOptions();    // the keys lane's cyberpunk parts (only append: saved by number)
    juce::String cyberpunkKeysDescription (int option);
    int cyberpunkKeysAutomatic (WorldSection, int variation);   // AUTOMATIC: one of the section's own by the take

    struct CyberpunkRequest
    {
        WorldSection section = WorldSection::verse;
        double beatsPerBar = 4.0;
        int variation = 0;
        float busy = 0.5f;
        std::vector<double> kicks;        // the drums' kick hits (beats from the section's start; empty: none)
        std::vector<double> guitarHits;   // the guitar's hits likewise
        std::optional<Chord> nextChord;   // the chord after the section (risers aim at it)
    };

    // The keys lane's cyberpunk part `option` over these chords (LineIdea::Part::chords; notes and CC moves from the
    // first span's start).
    LineIdea cyberpunkKeys (int option, const std::vector<ChordSpan>& spans, const CyberpunkRequest&);
}
