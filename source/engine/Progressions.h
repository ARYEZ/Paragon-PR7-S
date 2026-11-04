#pragma once

#include "engine/Chords.h"

#include <optional>

// The progression library (0.40.0, the owner asked: as their chord tool's, "progressions by
// genre and mood"): our own collection of chord progressions, each with a genre, one or two
// moods, and its chords written as numerals in a major or minor key ("I V vi IV", "i VI III VII").
// Played in any key: in its own mode on the key's tonic; in the other mode on the relative key
// (a major key's progression in A minor is played in C major), so every chord stays in the
// key's notes. The names, the sorting into genres and moods, and the choice are ours.
//
// Numerals: I..VII; upper case major, lower case minor; a leading b or # moves the root a half
// step from the mode's own degree; then o (diminished), + (augmented), sus2, sus4, h7 (half-
// diminished: m7b5), o7 (diminished seven), and the tensions 6, 7, maj7, add9, 9, maj9, 6/9 as
// "69", 11, 13; "/3" puts the mode's third degree in the bass ("/b7" a half step lower);
// ":2" makes the chord last two bars (":0.5" half a bar; one bar when not given).
namespace pr7s
{
    struct ProgressionStep
    {
        Chord chord;          // (in the key it was asked for)
        double bars = 1.0;
        int spelling = 0;     // written with a flat (-1) or sharp (1): named so ("bV" in A minor is Eb, not D#)
        juce::String numeral; // as written ("bVII7"), when the key's mode is the progression's own; else empty
        juce::String name (const Key&) const;       // the chord's name: by its written accidental, else the key's spelling
        juce::String numeralIn (const Key&) const;  // as written, else the key's numeral
    };

    struct LibraryProgression
    {
        juce::String name, genre;
        juce::StringArray moods;
        bool minor = false;           // written in a minor key
        juce::String numerals;        // as written ("I V vi IV")

        struct Step { int offset = 0; Chord::Type type = Chord::Type::major; Chord::Tension tension = Chord::Tension::none; int bassOffset = -1; double bars = 1.0;
                      int accidental = 0; juce::String numeral; };
        std::vector<Step> steps;      // (parsed from `numerals`; offsets from the mode's tonic)

        // Its chords in this key (the other mode's: on the relative key).
        std::vector<ProgressionStep> in (const Key&) const;
        int tonicIn (const Key&) const;   // the pitch class it's played from
        double totalBars() const;
    };

    // One numeral ("bVII7", "ii7/3", "IV:2") in a mode: false when it can't be read.
    bool parseNumeral (const juce::String&, bool minorMode, LibraryProgression::Step&);

    const std::vector<LibraryProgression>& progressionLibrary();
    const juce::StringArray& progressionGenres();   // "POP", "ROCK", "METAL"...
    const juce::StringArray& progressionMoods();    // "HAPPY", "SAD", "EPIC"...

    struct ProgressionFilter
    {
        juce::String genre, mood;     // empty: any
        juce::String text;            // words, all of them in the name, genre, moods, numerals or chords
        bool ownModeOnly = false;     // only those written in the key's own mode
        bool favoritesOnly = false;
        juce::StringArray favorites;  // names
    };

    // The library's indices that pass, those written in the key's own mode first.
    std::vector<int> findProgressions (const Key&, const ProgressionFilter&);
    int progressionNamed (const juce::String&);   // -1: none

    // A progression as text for a drag ("progression:" + this): "C*1;G/B*1;Am*2" (Chord::toText).
    juce::String progressionToText (const std::vector<ProgressionStep>&);
    std::vector<ProgressionStep> progressionFromText (const juce::String&);
}
