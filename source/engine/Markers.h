#pragma once

#include <juce_core/juce_core.h>

#include <optional>
#include <vector>

// Markers (0.49.0, the manual's Markers chapter): saved positions on the song's marker track.
// A position marker keeps one place; a cycle marker keeps a range (a pair of locators), to
// recall and play round, zoom to or select. Positions are in beats, as everything placed in
// the song. Each kind has its own IDs from 1 (cycle markers' are shown in brackets): the keys
// that go to "marker 3" go to the position marker whose ID is 3, wherever it is.
//
// Our choice: one marker track (the manual allows ten). The rest follows the manual.
namespace pr7s
{
    struct Marker
    {
        int id = 1;
        bool cycle = false;
        double start = 0.0;         // beats
        double end = 0.0;           // a cycle marker's end (beats); a position marker's is its start
        juce::String name;          // the manual's Description
        double length() const noexcept { return cycle ? end - start : 0.0; }
        bool operator== (const Marker&) const = default;
    };

    struct MarkerTrack
    {
        bool shown = false;         // in the track list (added once; removed: hidden, the markers kept)
        bool locked = false;        // the track's Lock: no edits (but naming the track and locating)
        juce::String name { "Markers" };
        std::vector<Marker> markers;   // by start (`sort`); a cycle marker before a position marker at the same beat
        // The Markers window's settings (the manual's Marker Settings panel) and the track's Show Marker Lines.
        bool showLines = true;      // lines across the arrangement at each marker
        bool showIds = true;        // IDs on the track's events
        bool cycleFollows = false;  // Cycle Follows When Locating to Markers

        void sort();
        int nextId (bool cycle) const;   // the first free ID of that kind
        const Marker* find (int id, bool cycle) const;
        int indexOf (int id, bool cycle) const;   // -1: none
        // The marker starts in time order (positions and cycle starts), for Locate Next/Previous.
        std::optional<double> nextStart (double beat, bool forward) const;
        // The marker at or nearest before `beat` (the Markers window's locate arrow), -1: none before.
        int nearestBefore (double beat) const;
        // IDs given again in time order (the manual's Reassign Position / Cycle Marker IDs).
        void reassignIds (bool cycle);

        std::unique_ptr<juce::XmlElement> toXml() const;   // "MarkerTrack"
        static MarkerTrack fromXml (const juce::XmlElement&);
        bool isEmpty() const noexcept { return markers.empty(); }
        bool operator== (const MarkerTrack&) const = default;
    };

    // "1", or "(2)" for a cycle marker (the manual shows those in brackets).
    juce::String markerIdText (const Marker&);

    // (0.82.3, the owner's list) The names a song's sections go by, for naming cycle markers: Intro, Verse,
    // Pre-Chorus, Chorus, Post-Chorus, Bridge, Breakdown, Solo, Interlude... (ours, in a song's usual order).
    const juce::StringArray& songSectionNames();
    // A name's section without its number ("Verse 3" -> "Verse"; "Chorus" -> "Chorus"); not one of the list: empty.
    juce::String sectionBaseOf (const juce::String& name);
    // Section names numbered in time order where a section comes more than once ("Verse 1", "Verse 2", "Verse 3"),
    // plain where it comes once. Other names are left alone.
    void numberSections (std::vector<Marker>&);
}
