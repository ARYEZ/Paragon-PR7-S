// Insert Silence and Delete Time over every track (0.119.0, the owner: "add 4 bars before the song and have everything
// move 4 bars"): with every track in the range the whole song moves, not only the parts: the chord track, markers,
// tempo, signatures, automation and the SECTIONS row. A range over some tracks moves only their parts, as before.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    struct Song
    {
        Studio studio { false };
        int a = 0, b = 0;
        Song()
        {
            a = studio.getProject().tracks[0].id;
            b = studio.addTrack ({});
            studio.appendSongSection ("Intro", 4);
            studio.appendSongSection ("Verse", 8);
            for (const auto id : { a, b })
            {
                Part part;
                part.start = 0.0;
                part.length = 48.0;
                part.notes = { { 0.0, 1.0, id == a ? 60 : 40, 0.8f }, { 16.0, 1.0, id == a ? 62 : 42, 0.8f } };
                studio.addPart (id, part);
            }
            studio.setChordEvents ({ { 0.0, Chord { 0 }, 16.0 }, { 16.0, Chord { 5 }, 32.0 } });
            studio.addPositionMarker (16.0, "Verse");
            studio.addTempoTrack();
            studio.addTempoEvent (16.0, 140.0);
            studio.setSignatureAt (8, 3, 4);
            studio.setAutomationOpen (a, true);
            studio.setAutomationPoints (a, 0, { { 0.0, 0.2f, 0.0f }, { 16.0, 0.8f, 0.0f } });
        }
        Studio::Range all (double start, double end) const { return { start, end, studio.getShownTracks() }; }
        std::vector<double> noteStarts (int id) const
        {
            std::vector<double> starts;
            for (const auto& p : studio.getProject().findTrack (id)->parts)
                for (const auto& n : p.notes)
                    starts.push_back (p.start + n.start);
            std::sort (starts.begin(), starts.end());
            return starts;
        }
    };
}

TEST_CASE ("0.119.0: Insert Silence over every track moves the whole song", "[insertsilence]")
{
    Song song;
    auto& studio = song.studio;
    REQUIRE (studio.coversEveryTrack (song.all (0.0, 16.0)));
    studio.insertSilence (song.all (0.0, 16.0));   // four bars of 4/4 before the song
    const auto& p = studio.getProject();

    CHECK (song.noteStarts (song.a) == std::vector<double> { 16.0, 32.0 });
    CHECK (song.noteStarts (song.b) == std::vector<double> { 16.0, 32.0 });
    // The chords: nothing over the new bars, then C and F where their music went.
    REQUIRE (p.chordTrack.events.size() == 2);
    CHECK_THAT (p.chordTrack.events[0].beat, WithinAbs (16.0, 1e-9));
    CHECK (p.chordTrack.events[0].chord.root == 0);
    CHECK_THAT (p.chordTrack.events[1].beat, WithinAbs (32.0, 1e-9));
    // The marker, the tempo change, the signature change (bar 8 -> bar 12), the automation.
    CHECK_THAT (p.markerTrack.markers.front().start, WithinAbs (32.0, 1e-9));
    CHECK_THAT (p.tempoMap().bpmAt (8.0), WithinAbs (120.0, 1e-6));    // the new bars at the song's first tempo
    CHECK_THAT (p.tempoMap().bpmAt (24.0), WithinAbs (130.0, 1e-6));   // the ramp moved with its music
    CHECK_THAT (p.tempoMap().bpmAt (33.0), WithinAbs (140.0, 1e-6));
    REQUIRE (p.signatureChanges.size() == 1);
    CHECK (p.signatureChanges[0].bar == 12);
    const auto& lane = p.findTrack (song.a)->automation[0];
    CHECK_THAT (*lane.valueAt (16.0), WithinAbs (0.2, 1e-5));
    CHECK_THAT (*lane.valueAt (32.0), WithinAbs (0.8, 1e-5));
    // The SECTIONS row: a new empty 4-bar block in front, the others where their music is.
    const auto& blocks = p.songSections.blocks;
    REQUIRE (blocks.size() == 3);
    CHECK (blocks[0].bars == 4);
    CHECK (blocks[1].bars == 4);
    CHECK (blocks[2].bars == 8);
    CHECK_THAT (studio.getSongSectionPlaces()[1].start, WithinAbs (16.0, 1e-9));

    // One undo step puts it all back.
    studio.undo();
    CHECK (song.noteStarts (song.a) == std::vector<double> { 0.0, 16.0 });
    CHECK (studio.getProject().songSections.blocks.size() == 2);
    CHECK (studio.getProject().signatureChanges[0].bar == 8);

    // Delete Time takes it back out the same way.
    studio.redo();
    studio.deleteRange (song.all (0.0, 16.0), true);
    const auto& q = studio.getProject();
    CHECK (song.noteStarts (song.a) == std::vector<double> { 0.0, 16.0 });
    CHECK_THAT (q.chordTrack.events.front().beat, WithinAbs (0.0, 1e-9));
    CHECK_THAT (q.markerTrack.markers.front().start, WithinAbs (16.0, 1e-9));
    CHECK_THAT (q.tempoMap().bpmAt (17.0), WithinAbs (140.0, 1e-6));
    CHECK (q.signatureChanges[0].bar == 8);
    REQUIRE (q.songSections.blocks.size() == 2);
    CHECK (q.songSections.blocks[0].bars == 4);
}

TEST_CASE ("0.119.0: silence inside a section lengthens it; deleting shortens or removes blocks", "[insertsilence]")
{
    Song song;
    auto& studio = song.studio;
    studio.insertSilence (song.all (24.0, 32.0));   // two bars inside the verse
    auto blocks = studio.getProject().songSections.blocks;
    REQUIRE (blocks.size() == 2);
    CHECK (blocks[1].bars == 10);
    studio.deleteRange (song.all (8.0, 32.0), true);   // the intro's last two bars and four of the verse
    blocks = studio.getProject().songSections.blocks;
    REQUIRE (blocks.size() == 2);
    CHECK (blocks[0].bars == 2);
    CHECK (blocks[1].bars == 6);
    studio.deleteRange (song.all (0.0, 8.0), true);   // the intro gone
    blocks = studio.getProject().songSections.blocks;
    REQUIRE (blocks.size() == 1);
    CHECK (blocks[0].name == "Verse");
}

TEST_CASE ("0.119.0: a range over some tracks moves only their parts", "[insertsilence]")
{
    Song song;
    auto& studio = song.studio;
    const Studio::Range one { 0.0, 16.0, { song.a } };
    REQUIRE_FALSE (studio.coversEveryTrack (one));
    studio.insertSilence (one);
    CHECK (song.noteStarts (song.a) == std::vector<double> { 16.0, 32.0 });
    CHECK (song.noteStarts (song.b) == std::vector<double> { 0.0, 16.0 });
    CHECK_THAT (studio.getProject().chordTrack.events.front().beat, WithinAbs (0.0, 1e-9));
    CHECK (studio.getProject().songSections.blocks.size() == 2);
}

TEST_CASE ("0.119.0: a section inserted from the block menu is empty, not a linked copy", "[insertsilence]")
{
    Song song;
    auto& studio = song.studio;
    REQUIRE (studio.insertSongSection (0, "Intro", 4, false));
    const auto& blocks = studio.getProject().songSections.blocks;
    REQUIRE (blocks.size() == 3);
    CHECK (blocks[0].link == 0);
    CHECK (blocks[1].link == 0);
    CHECK (song.noteStarts (song.a) == std::vector<double> { 16.0, 32.0 });   // the music moved on, nothing copied
}
