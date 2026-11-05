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
        // Its tempo changes (0.10.0), from the file's start, as steps: the first is its tempo.
        // One or none: a steady tempo.
        std::vector<TempoEvent> tempoChanges;
        int numerator = 4, denominator = 4;
        std::vector<SignatureEvent> signatureChanges;   // (0.45.0) after the first, at their bars
        struct Track
        {
            juce::String name;
            int channel = 1;              // the MIDI channel its notes are on (the first, if several)
            std::vector<Note> notes;      // start: beats from the file's start
            std::vector<Controller> controllers;   // (0.25.0) CCs, pitchbend, aftertouch, poly pressure, program changes; start likewise
        };
        std::vector<Track> tracks;        // only those with notes
    };

    // Nothing (with `error` set) if it isn't a MIDI file or has no notes.
    std::optional<MidiImport> readMidiFile (const juce::File&, juce::String& error);
    std::optional<MidiImport> readMidiFile (juce::InputStream&, juce::String& error);
}
