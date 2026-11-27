#pragma once

#include "engine/Chords.h"

#include <juce_core/juce_core.h>

#include <optional>
#include <vector>

// BLUEPRINTS (0.152.0, the owner: "choose specific songs and we create their section layout as a template so that way
// people can have presets to start with, like we do with expansion rigs"): a song's SECTIONS row as a preset. Each
// block's name, length, linked copies, section type and transition, and its chords as steps from the key's tonic (so a
// blueprint fits any key), with a tempo. Ours read from the owner's songs for their form only (shapes: no notes),
// named in our own words (the owner's choice: never the songs' names in the app); the owner's own saved from a song's
// row (.pr7blueprint files in Documents/Paragon PR7-S/Blueprints). Using one lays the row out; the chords and the tempo
// are each a choice (the owner: "carry chords but give the option to turn the chords off").
namespace pr7s
{
    struct BlueprintChord
    {
        Chord chord;          // from a tonic of C (pitch class 0): moved onto the key's tonic when used
        double bars = 1.0;
        bool operator== (const BlueprintChord&) const = default;
    };

    struct BlueprintBlock
    {
        juce::String name { "Verse" };   // one of songSectionNames()
        int bars = 8;
        int link = 0;                    // blocks with the same number above 0 are linked copies
        juce::String type;               // a section type of that name (empty: STANDARD)
        juce::String transition;         // into the next (empty: NONE)
        std::vector<BlueprintChord> chords;   // end to end from the block's start (none: no chords there)
        // (0.262.0, the owner: "when a blue print is loaded each section should be fully populated with riffs and beats
        // that are inspired by the song") What plays there, measured from the song's section (never its notes): the
        // lanes that play and the option each takes, "drums=NAME|guitar=|bass=NAME" (a lane named: on; empty after
        // `=`: AUTOMATIC; a lane left out: off; empty text: the type's own lanes), and the song's kick and snare on the
        // sixteenths of a bar (a bit a sixteenth: where they hit in most bars), which the drums lane's library groove
        // is chosen by.
        juce::String parts;
        int kick = 0, snare = 0;
        bool operator== (const BlueprintBlock&) const = default;
    };

    struct Blueprint
    {
        juce::String name, description;
        juce::String group;              // (0.262.0) the kind of song it's from (the picker's groups); empty: MY BLUEPRINTS
        double bpm = 0.0;                // 0: no tempo of its own
        bool minor = true;               // the mode its chords are written in
        int keyIndex = -1;               // a key it suits (an index into Key::all()); -1: none
        std::vector<BlueprintBlock> blocks;
        bool factory = false;            // one of ours (not a file: can't be deleted)
        juce::File file;                 // the owner's: where it's kept

        int totalBars() const noexcept;
        bool hasChords() const noexcept;
        // The tonic its chords are played from in `key`: the key's own, or its relative's when the modes differ (a
        // minor blueprint in C major plays from A).
        int tonicIn (const Key&) const noexcept;
        // A block's chords in `key`.
        std::vector<std::pair<Chord, double>> chordsIn (const Key&, int block) const;
        std::unique_ptr<juce::XmlElement> toXml() const;   // "Blueprint"
        static std::optional<Blueprint> fromXml (const juce::XmlElement&);
        bool operator== (const Blueprint&) const;
    };

    // Ours (read from the owner's songs for their form; our names).
    const std::vector<Blueprint>& factoryBlueprints();
    // A block for a table: chords as numerals in the blueprint's mode ("i:2 VI:2 III VII"; parseNumeral's language).
    BlueprintBlock blueprintBlock (const juce::String& name, int bars, int link, const juce::String& type,
                                   const juce::String& transition, const juce::String& numerals, bool minor);
    // (0.262.0) A block's `parts` read: layer -> its option (empty: AUTOMATIC). None (empty text): nothing said.
    std::vector<std::pair<int, juce::String>> blueprintParts (const juce::String&);
    juce::String blueprintPartName (int layer);   // "chords", "melody", "bass", "guitar", "lead", "drums", "clean", "acoustic"

    // The owner's: Documents/Paragon PR7-S/Blueprints, .pr7blueprint files.
    juce::File defaultBlueprintFolder();
    std::vector<Blueprint> userBlueprints (const juce::File& folder);
    bool saveBlueprint (const Blueprint&, const juce::File&);
    std::optional<Blueprint> loadBlueprint (const juce::File&);
    inline constexpr const char* blueprintExtension = ".pr7blueprint";
}
