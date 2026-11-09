#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <optional>
#include <vector>

// Drum maps (0.26.0, the manual's Drum Maps): a list of 128 drum sounds, one per pitch, each
// with a name, the note that plays it from a keyboard (I-note), the note sent to the
// instrument (O-note), a channel (or the track's), and muted or not; plus the order the drum
// editor lists them in. A track with a map: its notes are named after the drums they play
// (in the drum editor and the piano roll), played through the map (pitch -> O-note, muted
// sounds silent) and played in through it (I-note -> pitch).
//
// Built in: the GM Map (the General MIDI drum kit: names from 35 Acoustic Bass Drum to 81
// Open Triangle, and the GM2 additions 27-34 and 82-87) and, since 0.77.2, the PR7-D Map (PR7-D's
// GROOVE LIBRARY layout, the one it starts on). Others are made in the Drum Map
// Setup, or loaded from .drm files (the other DAW's drum map files, which drum instruments
// such as Superior Drummer, EZdrummer or Addictive Drums can export).
namespace pr7s
{
    struct DrumSound
    {
        juce::String name;
        int inNote = 0, outNote = 0;   // 0-127
        int channel = -1;              // 0-15, or -1: the track's
        bool mute = false;

        bool operator== (const DrumSound&) const = default;
    };

    struct DrumMap
    {
        juce::String name;
        std::array<DrumSound, 128> sounds;   // by pitch
        std::vector<int> order;               // the pitches as the list shows them, top to bottom (all 128)

        bool operator== (const DrumMap&) const = default;

        static DrumMap gm();                   // the built-in GM Map
        static DrumMap empty (const juce::String& name);   // "Sound 1"..., every note to itself (the manual's New Map)
        static juce::String gmName (int pitch);            // "" where GM names none
        // (0.77.2) PR7-D's GROOVE LIBRARY note layout (its default), note for note from PR7-D's own
        // table (`synth::drums::notemap::libraryNotes`): notes 6-127 named as PR7-D names them.
        // Every note plays as it is (names only).
        static DrumMap pr7d();
        static juce::String pr7dName (int pitch);          // "" where PR7-D names none
        static std::optional<DrumMap> builtIn (const juce::String& name);   // the GM Map or the PR7-D Map

        bool isIdentity() const noexcept;      // plays every note as it is (no O-note, channel or mute changes)
        int pitchForInput (int inNote) const noexcept;     // the sound whose I-note it is (else the note itself)

        std::unique_ptr<juce::XmlElement> toXml() const;   // ours, in the song
        static std::optional<DrumMap> fromXml (const juce::XmlElement&);

        // The .drm format (XML: a "Map" list of 128 items with INote, ONote, Channel, Mute, Name;
        // an "Order" list). `readDrm` takes what it finds and leaves the rest as the GM map has it.
        static std::optional<DrumMap> readDrm (const juce::File&, juce::String& error);
        static std::optional<DrumMap> readDrm (const juce::String& text, juce::String& error);
        juce::String toDrm() const;
        bool writeDrm (const juce::File&) const;
    };
}
