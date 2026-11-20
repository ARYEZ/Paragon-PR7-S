#pragma once

#include "engine/Riffs.h"
#include "engine/MediaBay.h"
#include "engine/Pr7dGrooves.h"

#include <array>
#include <memory>
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
        // (0.112.1) What libraryGroovesFor reads from its path, name and labels, worked out once when the groove is
        // found (the owner's 100,000 grooves were read word by word on every click): the section words in it (a bit
        // per section; -1: not worked out yet) and whether it's a fill.
        int sectionMask = -1;
        bool fill = false;
        // (0.137.0, the owner's rules) Its length in whole bars (0: not known), whether it's in the owner's Toontrack
        // folder (a folder with "toontrack" in its name on its path), whether a folder on its path (or its genre label)
        // names a style the owner leaves out (excludedGrooveWords: electronic, brushes, jazz, big band, R&B, reggae),
        // and its kick and snare (PR7-D's index; none: the MediaBay's files, not read for this).
        int bars = 0;
        bool toontrack = false;
        bool excluded = false;
        std::shared_ptr<const GrooveRhythm> rhythm;
        // (0.178.0) In one of the folders the owner marked as a favourite (worked out by the composer's pool, not
        // stored in the library's list: the favourites change without the library being read again).
        bool favourite = false;
    };
    // (0.178.0) A file inside one of `folders` (full paths, subfolders included; upper or lower case alike).
    bool isInFavouriteFolder (const juce::File&, const juce::StringArray& folders);
    // (0.137.0) The words that leave a groove out of the composer's drums (in a folder's name or a genre label).
    const juce::StringArray& excludedGrooveWords();
    // (0.112.1) Works out a groove's sectionMask and fill (findLibraryGrooves and findPr7dGrooves do it for theirs).
    void classifyGroove (LibraryGroove&);

    // (0.112.1) The composer's library grooves, shared (not copied) between every ComposerContext made from the same
    // library: a vector's reading surface over a shared list.
    struct GrooveList
    {
        GrooveList() = default;
        GrooveList (std::vector<LibraryGroove> grooves) : list (std::make_shared<const std::vector<LibraryGroove>> (std::move (grooves))) {}
        const std::vector<LibraryGroove>& get() const noexcept { static const std::vector<LibraryGroove> none; return list != nullptr ? *list : none; }
        operator const std::vector<LibraryGroove>&() const noexcept { return get(); }
        size_t size() const noexcept { return get().size(); }
        bool empty() const noexcept { return get().empty(); }
        const LibraryGroove& operator[] (size_t i) const noexcept { return get()[i]; }
        auto begin() const noexcept { return get().begin(); }
        auto end() const noexcept { return get().end(); }
        // (0.185.0) Held by a cache keyed on the list's address, so the address can't be reused while it's kept.
        std::shared_ptr<const void> keepAlive() const noexcept { return list; }
    private:
        std::shared_ptr<const std::vector<LibraryGroove>> list;
    };
    // The MediaBay's MIDI files that look like drum grooves, best first.
    std::vector<LibraryGroove> findLibraryGrooves (const std::vector<MediaItem>&, GuitarStyle, double songBpm);
    // (0.107.0) PR7-D's grooves, scored as findLibraryGrooves scores (its labels counted with the path), best first;
    // only those whose bar is the song's (`songBarBeats` quarters; 0: any).
    std::vector<LibraryGroove> findPr7dGrooves (const Pr7dLibrary&, GuitarStyle, double songBpm, double songBarBeats);
    // (0.176.0) What findPr7dGrooves works out from each groove's words, done once per library (on the thread that
    // reads it): each groove labelled (classifyGroove), its style words counted for every style, its name's place in
    // name order. Then a tempo, style or bar change only adds and sorts numbers: the same list, in the same order.
    struct PreparedPr7dGrooves
    {
        std::vector<LibraryGroove> grooves;            // classified, score 0
        std::vector<double> barBeats;
        std::vector<std::array<signed char, 8>> words; // per guitarStyles() index: 4 x its words found (+1 "metal" but rock)
        std::vector<bool> fillWord;                    // "fill" in its words (-3)
        std::vector<int> nameRank;                     // its place in compareNatural order (ties: as listed)
    };
    std::shared_ptr<const PreparedPr7dGrooves> preparePr7dGrooves (const Pr7dLibrary&);
    std::vector<LibraryGroove> findPr7dGrooves (const PreparedPr7dGrooves&, GuitarStyle, double songBpm, double songBarBeats);
    // A groove as an idea over the chords (its file read; nothing if it can't be).
    std::optional<LineIdea> grooveIdea (const LibraryGroove&, const std::vector<ChordSpan>&, double beatsPerBar, juce::String& error);

    // (0.106.0, the owner: "the composer reaches for my library grooves by default") The library's grooves for one
    // section (drumSections()' index: intro 0, verse 1, chorus 2, bridge 3, breakdown 4, ending 5, solo 6), best first:
    // the style score as found, +6 for the section's word in the file's path or name (folders such as "Verse" or
    // "Chorus" in groove packs), -3 for another section's word. `fills`: only files with "fill" in them; else none of
    // those. Our scoring.
    // (0.112.1) `limit`: only the best that many (0: all), in the same order a full list would give them.
    // (0.137.0) `ownersRules`: grooves in a left-out folder (excludedGrooveWords) skipped, and the Toontrack folder's
    // first (the composer's fills).
    std::vector<LibraryGroove> libraryGroovesFor (const std::vector<LibraryGroove>& found, int section, bool fills, size_t limit = 0,
                                                  bool ownersRules = false);
    // The notes of a library file's last bar, from its first note in that bar (beats from that note's quarter; one bar
    // at most). Empty if the file can't be read. Files are read once and kept (by path and date).
    std::vector<LineNote> libraryFillBar (const LibraryGroove&, double beatsPerBar);
}
