#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Project.h"

// MIDI file export (0.51.0, the manual's Exporting MIDI Tracks as Standard MIDI Files and its Export
// Options): the instrument tracks' parts as a standard MIDI file, with the tempo (the tempo track's
// events, or the fixed tempo), the time signatures and, if asked, the markers. Notes as they're heard:
// a part's Transpose and Velocity (0.47.0) applied, muted parts left out, each track on its MIDI
// channel. Type 1 (a track each, the first the tempo, signatures and markers) or type 0 (all on one).
namespace pr7s
{
    struct MidiExportOptions
    {
        bool markers = true;            // Export Markers
        bool type0 = false;             // Export as Type 0
        int resolution = 480;           // Export Resolution: ticks per quarter note, 24-960
        bool locatorsOnly = false;      // Export Locator Range (from the left locator: it becomes 0)
        bool volumePan = false;         // Export Inspector Volume/Pan: the tracks' as CC 7 and CC 10 at the start
        std::vector<int> trackIds;      // which (empty: every instrument track with parts)
    };

    juce::MidiFile makeMidiFile (const Project&, const MidiExportOptions&);
    bool writeMidiFile (const Project&, const MidiExportOptions&, const juce::File&, juce::String& error);
}
