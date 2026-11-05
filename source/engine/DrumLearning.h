#pragma once

#include "engine/DrumGenerator.h"
#include "engine/Signature.h"
#include "engine/Project.h"

#include <array>
#include <optional>

// Learning a drummer (0.56.0, the owner asked: beats trained on real drummers' playing, from a
// folder of whole songs of drum MIDI, one song a file, with no section names in them).
//
// 1. Each song is read into bars (by the file's own signatures). Any kit's notes are read by what
//    they are (kick, snare, ghost, hats, open hats, ride, bell, crash, china, toms), and written back
//    as General MIDI's.
// 2. Its sections are found from the drums alone: where the playing changes (the kick and snare's
//    places, the cymbal, how busy, half time), helped by what drummers do at a change (a fill in
//    the bar before, a crash on the first beat). Sections that play alike are grouped, then named:
//    half time is a breakdown (a quiet one a bridge), the first short one an intro, the last short
//    one an ending, the repeated groups verse and chorus (the bigger one the chorus), a group
//    played once later on a bridge. This is a guess from the drums: the review page lets the owner
//    change a section's name, move its start, split or join it, or leave it out.
// 3. A profile is learned per section name: the grooves played most (as played: the notes, their
//    places on a 48-to-the-bar grid, their velocities), the fills (the bar's end where the cymbal
//    stops and the toms or snare take over), the chance of a kick, snare and ghost note on each
//    sixteenth, the cymbal, how often a phrase starts on a crash, half time, and the feel (how far
//    ahead of or behind the grid the kick, snare and cymbal sit).
// 4. The generator plays from a profile: a groove of that drummer's for the section, kept and
//    repeated, the third bar of a phrase another of theirs (or a kick moved as they would), their
//    fills, their crashes, their feel. DRUM SETTINGS still shape it (at 50%: as learned).
namespace pr7s
{
    constexpr int learnSteps = 48;   // places in a bar (12 a beat in 4/4: sixteenths and triplets)

    enum class DrumRole { kick, snare, ghost, hats, openHat, ride, bell, crash, china, tom, other };
    DrumRole drumRoleOf (int pitch, float velocity);
    int gmNoteOf (int pitch);   // a kit's note as General MIDI's (-1: not a drum we write)

    struct LearnedHit
    {
        int step = 0;        // 0..learnSteps-1 from the bar's start (a fill's: from the fill's start)
        int note = 36;       // General MIDI
        int velocity = 100;  // 1..127
        bool operator== (const LearnedHit&) const = default;
    };

    struct DrumBar
    {
        double start = 0.0, length = 4.0;   // beats in the song
        std::vector<LearnedHit> hits;        // in step order
        // What the analysis reads.
        std::array<bool, 16> kick {}, snare {};   // (loud snare)
        int ghosts = 0, toms = 0, handHits = 0, notes = 0;
        int cymbal = -1;          // the hand's cymbal (the note played most of hats, ride, bell, china, crash), -1: none
        bool crashAtStart = false, fill = false, twoBeatFill = false, halfTime = false;
        float velocity = 0.0f;    // the mean, 0..1
        bool silent() const noexcept { return notes == 0; }
    };

    struct DrumSongSection
    {
        int firstBar = 0, bars = 1;
        DrumSection kind = DrumSection::verse;
        bool used = true;    // learned from (the review page's LEAVE OUT; silent sections start left out)
        int group = 0;       // sections that play alike share it
        bool operator== (const DrumSongSection&) const = default;
    };

    struct DrumSong
    {
        juce::File file;
        juce::String name;
        double bpm = 120.0;
        std::vector<DrumBar> bars;
        std::vector<DrumSongSection> sections;   // in order, covering every bar
        bool use = true;                          // (the review page's tick)
        // The feel: how far from the grid (beats; + late) the kick, the snare and the cymbal were played.
        std::array<double, 3> pushSum {};
        std::array<int, 3> pushCount {};
    };

    // A whole song of drums: a MIDI file's channel-10 tracks (none: every track). Sections found.
    std::optional<DrumSong> readDrumSong (const juce::File&, juce::String& error);
    // The same from notes (beats from the song's start): tests, and anything already read.
    DrumSong drumSongFromNotes (const juce::String& name, const std::vector<Note>&, double bpm, const SignatureMap&);
    // The song's MIDI files in a folder and those under it (sorted by name).
    std::vector<juce::File> findDrumSongs (const juce::File& folder);

    std::vector<DrumSongSection> findSections (const std::vector<DrumBar>&);
    void nameSections (const std::vector<DrumBar>&, std::vector<DrumSongSection>&);   // (groups and kinds)

    // The review page's edits (false: not possible there). Each keeps the sections covering the song.
    void setSectionKind (DrumSong&, int index, DrumSection);
    void setSectionUsed (DrumSong&, int index, bool);
    bool splitSection (DrumSong&, int index, int atBar);        // atBar inside it (not its first)
    bool joinSections (DrumSong&, int index);                   // with the next (its kind kept)
    bool moveSectionStart (DrumSong&, int index, int firstBar); // index > 0; every section keeps a bar
    juce::String sectionsToText (const std::vector<DrumSongSection>&);   // "0:intro:4:1;4:verse:8:1"
    std::optional<std::vector<DrumSongSection>> sectionsFromText (const juce::String&, int totalBars);
    std::optional<DrumSection> drumSectionNamed (const juce::String&);   // "verse", "VERSE"

    // A section's notes as an idea (to hear it on the review page), from its first bar.
    LineIdea sectionIdea (const DrumSong&, int index);

    //==========================================================================
    struct SectionHabits
    {
        int bars = 0;      // bars learned (0: nothing for this section)
        int sections = 0;
        struct Pattern
        {
            std::vector<LearnedHit> hits;
            int count = 0;
            int beats = 0;   // (a fill's length)
            bool operator== (const Pattern&) const = default;
        };
        std::vector<Pattern> grooves, fills;   // most played first (at most 12 each)
        std::array<float, 16> kick {}, snare {}, ghost {};   // chances on each sixteenth
        float fillChance = 0.0f;      // fills per bar
        float crashChance = 0.0f;     // phrases (every 4 bars from a section's start) starting on a crash
        float halfTime = 0.0f;        // bars in half time
        int cymbal = -1;
        std::array<double, 3> push {};   // beats (+: late) for the kick, the snare, the cymbal
    };

    struct DrumProfile
    {
        juce::String name;
        std::array<SectionHabits, 6> sections;   // by DrumSection
        struct Song { juce::String path, sections; bool use = true; };
        std::vector<Song> songs;                  // what it learned from (the review page opens them again)

        const SectionHabits& of (DrumSection s) const { return sections[(size_t) s]; }
        bool has (DrumSection s) const { return of (s).bars > 0; }
        int barsLearned() const;
        std::unique_ptr<juce::XmlElement> toXml() const;
        static std::optional<DrumProfile> fromXml (const juce::XmlElement&);
        bool save (const juce::File&) const;
        static std::optional<DrumProfile> load (const juce::File&);
    };

    DrumProfile learnProfile (const juce::String& name, const std::vector<DrumSong>&);

    // Profiles live in a folder, a ".pr7drummer" file each (Documents/Paragon PR7-S/Drummer Profiles).
    juce::File defaultDrummerFolder();
    std::vector<juce::String> drummerNames (const juce::File& folder);
    juce::File drummerFile (const juce::File& folder, const juce::String& name);

    // A beat for the section from what the drummer played. A section the profile lacks takes the
    // nearest it has (chorus <-> verse, bridge -> verse, breakdown -> chorus, intro/ending -> verse);
    // nothing learned at all: no notes.
    LineIdea generateBeat (const DrumProfile&, DrumSection, const std::vector<ChordSpan>&, double beatsPerBar,
                           const DrumSettings&, juce::uint32 seed, const std::vector<double>& riffHits = {});
    // The six, as `generatedBeats` (the same seeds).
    std::vector<LineIdea> generatedBeats (const DrumProfile&, const std::vector<ChordSpan>&, double beatsPerBar,
                                          const DrumSettings&, int variation, const std::vector<double>& riffHits = {});
}
