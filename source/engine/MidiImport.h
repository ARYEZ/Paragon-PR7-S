#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Project.h"

#include <optional>

namespace pr7s
{
    // A standard MIDI file, as the studio imports it: its first tempo and
    // time signature, and its notes, a track of them for each track in the
    // file (a type-0 file, one track holding everything: a track for each
    // MIDI channel in it, as the manual's "Auto Dissolve Format 0" does).
    // Notes are in beats (quarter notes) from the file's start.
    struct MidiImport
    {
        double bpm = 120.0;
        int numerator = 4, denominator = 4;
        struct Track
        {
            juce::String name;
            int channel = 1;              // the MIDI channel its notes are on (the first, if several)
            std::vector<Note> notes;      // start: beats from the file's start
        };
        std::vector<Track> tracks;        // only those with notes
    };

    // Nothing (with `error` set) if it isn't a MIDI file or has no notes.
    std::optional<MidiImport> readMidiFile (const juce::File&, juce::String& error);
    std::optional<MidiImport> readMidiFile (juce::InputStream&, juce::String& error);
}
