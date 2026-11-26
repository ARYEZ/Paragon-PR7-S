#pragma once

#include "engine/Chords.h"
#include "engine/DrumGenerator.h"
#include "engine/Lines.h"
#include "engine/Project.h"

// YOUR OWN RIFF in the composer (0.75.0, the owner: "let's say I have an idea for a song, maybe I've got a
// verse riff I like but that's all I've got... paste that in, select that it's a verse riff, and the verse
// parts suggested would be around that riff"). Ours, all of it:
//
//   - the riff: a MIDI part's notes (or a MIDI file's), from the start of the bar it starts in, as long as
//     its whole bars (at most 16);
//   - its key, guessed (`guessKey`): the notes in each key's scale (each counted by a quarter beat plus its
//     length, and its velocity), the notes outside it counted against, and the lowest notes (a riff's pedal
//     and roots) counted again towards the key on them; minor unless the major third is heard more;
//   - its chords (`riffChords`), a chord every half bar (a bar in odd signatures), the same ones merged: the
//     root is the low notes' pitch class most played there; the third if the riff plays one, else the key's
//     own (a root in the key: its triad in the key; outside it, the flat second or fifth of a metal riff: a
//     major (power) chord, kept as played and shown as outside the key: the owner's choice);
//   - repeated to fill a section (`tileRiff`, `riffSpans`).
namespace pr7s
{
    struct ComposerRiff
    {
        std::vector<LineNote> notes;   // beats from the riff's first bar line
        int bars = 1;
        DrumSection section = DrumSection::verse;   // what the owner said it is
        juce::String name;                           // where it came from ("Guitar - Verse riff")
        bool operator== (const ComposerRiff&) const = default;
    };

    struct RiffChord
    {
        double start = 0.0, length = 0.0;   // beats from the riff's start
        Chord chord;
        bool operator== (const RiffChord&) const = default;
    };

    // A part's notes as a riff: `startBeat` is where the part starts in the song (its notes are from it), the
    // riff from the bar line at or before it.
    ComposerRiff riffFromNotes (const std::vector<Note>&, double startBeat, double beatsPerBar, const juce::String& name, DrumSection);
    Key guessKey (const std::vector<LineNote>&);
    std::vector<RiffChord> riffChords (const ComposerRiff&, const Key&, double beatsPerBar);
    // The riff over a section of `totalBeats` (repeated, cut at the end), as the guitar's idea.
    LineIdea tileRiff (const ComposerRiff&, double totalBeats, double beatsPerBar);
    // Its chords over a section from `start` (repeated, cut at the end; `overrides`: by span index).
    std::vector<ChordSpan> riffSpans (const std::vector<RiffChord>&, const Key&, double start, double totalBeats, double riffBeats);

    // (0.226.0, the owner: the verse's chords set on the chord track, a riff of their own used on it) FIT MY RIFF
    // TO THE CHORDS: the tiled riff (its beat 0 = spans.front().start) with each note moved from the riff's own
    // chord there (riffChords, the riff's guessed key) onto the chord under it in `spans`, as Follow Chord
    // Track's Chords & Scales moves a note: the same rhythm, lengths, velocities and effects; keyswitches, and
    // notes where both say the same chord, untouched.
    LineIdea fitRiffToChords (const LineIdea& tiled, const ComposerRiff&, const std::vector<ChordSpan>& spans, double beatsPerBar);
    // Where the riff's own chords (repeated as it's tiled) and the chords under it disagree: every half bar
    // checked by root and kind (major, minor...), each disagreeing stretch once (beats from spans.front().start).
    struct RiffClash { double beat = 0.0; Chord riff, track; };
    std::vector<RiffClash> riffClashes (const ComposerRiff&, const std::vector<ChordSpan>& spans, double beatsPerBar);
}
