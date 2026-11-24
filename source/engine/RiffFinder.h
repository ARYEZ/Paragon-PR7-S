#pragma once

#include "engine/MediaBay.h"
#include "engine/MidiImport.h"
#include "engine/RiffPaste.h"

#include <atomic>
#include <functional>

// FIND A RIFF IN THE MEDIABAY (0.227.0, the owner: "do we have the ability to scan the media Bay for riffs that are
// in key for the song that we are working on and would fit for the part"). Ours, all of it:
//
//   - which files: the MediaBay's MIDI files (any location it scans), not drums (a drum word in the name or a
//     folder, the DRUMS category, or only channel-10 notes), 1 to 16 bars long, with at least three notes;
//   - each read as + MY RIFF reads a file (every track but channel 10's, from the bar it starts in), its key from
//     the MediaBay's (yours, the name's or the file's) else guessed from its notes (`guessKey`);
//   - moved into the song's key the short way (a major riff into a minor key: to its relative major, the same notes),
//     then by whole octaves into a guitar's range (A0 to E5: 33 to 88);
//   - scored: how much of it is in the key after moving (the most), how much of it a guitar can play, its tempo
//     against the song's (twice or half as fast counts, less), the section's word in its name or folders (another
//     section's word counts against), and riff words (riff, guitar, gtr, chug).
//
// The score is a guide: it can't hear whether a riff is any good.
namespace pr7s
{
    struct RiffFindRequest
    {
        Key key;                          // the song's (the riff is moved into it)
        double bpm = 120.0;
        double beatsPerBar = 4.0;
        DrumSection section = DrumSection::verse;
        int most = 100;                   // how many come back (the best)
        int mostRead = 4000;              // how many files are read at most (the likeliest by name and tempo first)
    };

    struct RiffFind
    {
        juce::File file;
        juce::String name, folder;        // the file's name, its folder's
        ComposerRiff riff;                // moved into the song's key and a guitar's range, ready to paste
        Key from;                         // the key it was in (known or guessed)
        bool keyKnown = false;            // from the MediaBay (else guessed)
        int shift = 0;                    // semitones its tonic moved (the octaves not counted)
        double inKey = 0.0;               // 0-1: its notes in the song's key after moving
        double playable = 0.0;            // 0-1: in a guitar's range
        double tempo = 0.0;               // 0: not known
        double score = 0.0;               // 0-1
        juce::String reason;              // "in key 96%, MOVED +3, 2 bars, 120 BPM, says verse"
    };

    // A drum file by its name, folders or category (the notes are checked as it's read).
    bool looksLikeDrums (const MediaItem&);
    // A key written as the MediaBay writes it ("Am", "F#", "Bbm"); none when empty or not one.
    std::optional<Key> keyFromText (const juce::String&);
    // One file's notes as a found riff (none: no notes left once the drums are out, or longer than 16 bars).
    std::optional<RiffFind> scoreRiff (const MediaItem&, const MidiImport&, const RiffFindRequest&);
    // Every likely MIDI file read and scored, the best first (`stop`: true ends it early, with what it has).
    std::vector<RiffFind> findRiffs (const std::vector<MediaItem>&, const RiffFindRequest&,
                                     const std::function<bool()>& stop = {}, std::atomic<int>* read = nullptr);
}
