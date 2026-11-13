#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <vector>

// PR7-D'S GROOVE LIBRARY (0.107.0, the owner: "you can't leverage the groove library that is scanned into the PR7-D?
// which is my drum plugin"). PR7-D, the family's drum plug-in, keeps the groove folders its GROOVES page links and
// what it read from them in two files every PR7-D shares, in its presets folder (Windows %APPDATA%\Paragon\Paragon
// PR7-D\Rigs, Linux ~/.config/Paragon/Paragon PR7-D/Rigs, macOS ~/Library/Application Support/Paragon/Paragon PR7-D/Rigs):
//   - Grooves.xml: <Grooves> <Folder path map user excluded/> ... (map: the folder's note layout, PR7-D's
//     notemap::Layout: 0 root, 1 groove library, 2 general MIDI, 3 custom);
//   - Groove Index.xml: <G path stamp bpm bpmKnown num den bars hand intensity hits feel rhythm tags prio/> per groove
//     (tags "Group:Value|..." from groove packs: "Type:Fill", "Genre:Metal"...).
// We only read them (PR7-D owns them), and leave out grooves inside groove databases (the owner uses .mid files only)
// and excluded folders (left out of PR7-D's own searches too). Read from PR7-D's code (its GrooveLibrary.cpp); keep in
// step with it.
//
// THE NOTES: groove packs are written for different kits, so each folder has a layout; a groove's notes are turned
// into the drum track's: General MIDI, or the PR7-D Map (PR7-D's GROOVE LIBRARY layout, its default), through PR7-D's
// own drums ("slots") and its tables (NoteMap.h's libraryNotes, generalMidi, homeNote). ROOT folders (PR7-D's 36 keys
// from a root each instance sets) are read from C1 (36); CUSTOM ones (a map only PR7-D's settings know) as they are.
namespace pr7s
{
    struct Pr7dGroove
    {
        juce::File file;
        juce::String name, folder;   // the file's name, its folder's name
        double bpm = 0.0;            // 0: not known
        int numerator = 4, denominator = 4, bars = 1;
        float intensity = 0.0f;
        juce::StringArray tags;      // "Group:Value"
        int layout = 1;              // its linked folder's (the values above)
        double barBeats() const noexcept { return numerator * 4.0 / juce::jmax (1, denominator); }
    };

    struct Pr7dLibrary
    {
        juce::File folder;           // where the two files were
        std::vector<Pr7dGroove> grooves;
        juce::Time indexDate;        // Groove Index.xml's
        bool found = false;          // Grooves.xml was there
    };

    // PR7-D's presets folder on this computer.
    juce::File pr7dPresetsFolder();
    // Its library as listed in that folder (`folder` for tests). Nothing found: `found` false, no grooves.
    Pr7dLibrary readPr7dLibrary (const juce::File& folder);

    // The drum notes a groove goes onto.
    enum class DrumNotes { asWritten, generalMidi, pr7d };
    // A note of a folder's `layout` as `target`'s note (-1: nothing plays it there: left out).
    int convertDrumNote (int note, int layout, DrumNotes target);
}
