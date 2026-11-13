#pragma once

#include "engine/Signature.h"

#include <juce_core/juce_core.h>

#include <memory>
#include <vector>

// The SECTIONS row (0.90.0, the owner's song engine: "each box can represent any piece of a song intro outro verse
// chorus pre-chorus bridge breakdown solo post chorus ... we arrange them in the order that the song will be and then
// if we double click the block it opens up the composer to that section of the song"). The song as blocks, end to end
// from bar 1: each a section's name (songSectionNames(), the drop-down) and a length in bars (the owner's: in bars).
// Where a block lies comes from the blocks before it, so the row never has gaps or overlaps. Rearranging the blocks
// moves the music inside them with them (Studio's song-section actions: one undo step each).
//
// Our choices: one row, at the top of the song's own rows (above the chord track); a block 1 to 64 bars; names from the
// list only (numbered on screen where a name comes back: "Chorus 2"). Linked copies (0.91.0, the owner's: repeats are
// linked): `SongSection::link`; Studio keeps their music the same.
namespace pr7s
{
    struct SongSection
    {
        int id = 0;                       // per song, from 1 (kept through moves: the composer finds its block again)
        juce::String name { "Verse" };    // one of songSectionNames()
        int bars = 8;
        // (0.91.0, the owner's: "repeated parts should be a linked copy") Blocks with the same link number (above 0)
        // are linked copies: an edit to the music in one (its parts on every track, its chords) is copied into the
        // others. 0: its own.
        int link = 0;
        // (0.92.0) How the composer built it (BUILD into the block): the composer's settings as text, which lane went
        // onto which track, and a mark of what it wrote on each. When the block's chords change, a lane whose track
        // still holds what was written is built again over the new chords; everything else follows them.
        juce::String recipe;
        // (0.94.0) Its TYPE (engine/SectionTypes.h: DRIVING, DRONING...; empty: STANDARD). Linked copies share it.
        juce::String type;
        struct Lane { int layer = 0, track = 0; juce::int64 mark = 0; bool operator== (const Lane&) const = default; };
        std::vector<Lane> lanes;
        bool operator== (const SongSection&) const = default;
    };

    constexpr int leastSectionBars = 1, mostSectionBars = 64;

    struct SongSections
    {
        bool shown = true;                // the row in the track list (0.103.0: there by default, the owner's; hidden: the blocks kept)
        std::vector<SongSection> blocks;  // in the song's order, from bar 1

        int nextId() const noexcept;
        int totalBars() const noexcept;
        int indexOf (int id) const noexcept;   // -1: none
        int nextLink() const noexcept;         // a link number not used yet
        std::vector<int> linkedWith (int index) const;   // the blocks linked with it, itself included (alone: just it)
        bool isEmpty() const noexcept { return blocks.empty(); }
        bool operator== (const SongSections&) const = default;
        std::unique_ptr<juce::XmlElement> toXml() const;   // "SongSections"
        static SongSections fromXml (const juce::XmlElement&);
    };

    // Where each block lies: its first bar (from 0) and its beats by the song's signatures.
    struct SectionPlace
    {
        int firstBar = 0, bars = 0;
        double start = 0.0, end = 0.0;    // beats
        bool contains (double beat) const noexcept { return beat >= start && beat < end; }
    };
    std::vector<SectionPlace> sectionPlaces (const SongSections&, const SignatureMap&);
    // The block holding a beat (-1: none: before the first or after the last).
    int sectionAt (const std::vector<SectionPlace>&, double beat) noexcept;

    // What the block shows: its name, numbered by its place among the blocks of that name when the name comes more
    // than once ("Chorus 2"), plain when once.
    juce::String sectionLabel (const std::vector<SongSection>&, int index);
    // (0.94.0) With its type when it has one: "Chorus 1 - DRONING".
    juce::String sectionLabelWithType (const std::vector<SongSection>&, int index);

    // The composer's SECTION for a block's name (an index into drumSections(): intro, verse, chorus, bridge,
    // breakdown, ending, solo): Pre-Chorus and Riff play as a verse, Post-Chorus / Hook / Refrain / Drop as a
    // chorus, Middle 8 / Interlude / Instrumental / Break as a bridge, Build-Up as a breakdown, Outro / Tag / Coda
    // as an ending. Our mapping.
    int composerSectionFor (const juce::String& name);
}
