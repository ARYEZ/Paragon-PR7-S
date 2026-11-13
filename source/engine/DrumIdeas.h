#pragma once

#include "engine/Riffs.h"
#include "engine/MediaBay.h"
#include "engine/Pr7dGrooves.h"

#include <optional>

// IDEAS' DRUMS tab (0.54.0, the owner asked: "completely new and generated drum beats and also
// grooves that are in my library").
//
// GENERATED: six beats for the guitar tab's STYLE, written by us as rows of steps (a character per
// sixteenth, or per triplet with 12 or 24 a bar; X accent, x normal, g ghost, - none; bars A|B|C
// played A, A B A B or A B A C over four bars), a crash on each four-bar phrase's first beat and a
// fill on its last beat, in the style. The sixth of each style is "Locked to the riff": the kick on
// the guitar riff's hits (the metal way: kick and chugs together), the snare on the backbeat (half
// time for nu-metal and metalcore), the hats or china on the beats. General MIDI drum notes, which
// most drum libraries' maps follow.
//
// FROM MY LIBRARY: the MediaBay's scanned MIDI files that look like drum grooves (a drum word in
// their name, folder or category), best first: the style's words in them, a tempo near the song's
// (or half or double), fills last. A groove is laid over the chords from the first bar and repeated.
//
// BASS: two more bass ideas, on the chords' roots: "Locked to the kick" and "Following the riff".
namespace pr7s
{
    namespace GmDrum
    {
        constexpr int kick = 36, sideStick = 37, snare = 38, closedHat = 42, pedalHat = 44, openHat = 46,
                      crash = 49, ride = 51, china = 52, rideBell = 53, crash2 = 57,
                      tomHigh = 48, tomMid = 45, tomLow = 43, tomFloor = 41;
    }

    // The six beats (the sixth locked to `riff`, a guitar rhythm idea; none: kicks on one and three).
    std::vector<LineIdea> drumIdeas (GuitarStyle, const std::vector<ChordSpan>&, double beatsPerBar, int variation, const LineIdea* riff);

    // Where a part's hits are (beats from its start): a drum idea's kicks; a riff's attacks (notes
    // starting together count once).
    std::vector<double> kickHits (const LineIdea& drums);
    std::vector<double> riffHits (const LineIdea& riff);
    // A bass line on those hits: each chord's root (or slash bass) in the bass range near the one
    // before, held to the next hit (no longer than its chord).
    LineIdea bassOnHits (const std::vector<ChordSpan>&, const std::vector<double>& hits, const juce::String& name, const juce::String& description);

    struct LibraryGroove
    {
        juce::File file;
        juce::String name, folder;
        double bpm = 0.0;      // 0: not known
        int score = 0;
        // (0.107.0) From PR7-D's library: its labels ("Type:Fill|Genre:Metal"), its folder's note layout (-1: the
        // MediaBay's, notes as written) and the drum notes it goes onto (convertDrumNote).
        juce::String tags;
        int layout = -1;
        DrumNotes notes = DrumNotes::asWritten;
    };
    // The MediaBay's MIDI files that look like drum grooves, best first.
    std::vector<LibraryGroove> findLibraryGrooves (const std::vector<MediaItem>&, GuitarStyle, double songBpm);
    // (0.107.0) PR7-D's grooves, scored as findLibraryGrooves scores (its labels counted with the path), best first;
    // only those whose bar is the song's (`songBarBeats` quarters; 0: any).
    std::vector<LibraryGroove> findPr7dGrooves (const Pr7dLibrary&, GuitarStyle, double songBpm, double songBarBeats);
    // A groove as an idea over the chords (its file read; nothing if it can't be).
    std::optional<LineIdea> grooveIdea (const LibraryGroove&, const std::vector<ChordSpan>&, double beatsPerBar, juce::String& error);

    // (0.106.0, the owner: "the composer reaches for my library grooves by default") The library's grooves for one
    // section (drumSections()' index: intro 0, verse 1, chorus 2, bridge 3, breakdown 4, ending 5, solo 6), best first:
    // the style score as found, +6 for the section's word in the file's path or name (folders such as "Verse" or
    // "Chorus" in groove packs), -3 for another section's word. `fills`: only files with "fill" in them; else none of
    // those. Our scoring.
    std::vector<LibraryGroove> libraryGroovesFor (const std::vector<LibraryGroove>& found, int section, bool fills);
    // The notes of a library file's last bar, from its first note in that bar (beats from that note's quarter; one bar
    // at most). Empty if the file can't be read. Files are read once and kept (by path and date).
    std::vector<LineNote> libraryFillBar (const LibraryGroove&, double beatsPerBar);
}
