// The SECTIONS row (0.90.0, the owner's song engine): the song as blocks end to end from bar 1, each a section's
// name and a length in bars; rearranging the blocks moves the music in them (the owner's: "moving blocks moves the
// music in that part with it").

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/DrumGenerator.h"
#include "engine/Studio.h"
#include "ui/ArrangeView.h"
#include "ui/ComposerPage.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, int clicks = 1)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 juce::ModifierKeys().withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, clicks, at != downAt);
    }

    // A part over each of the first blocks, its one note's pitch telling them apart (Intro 60, Verse 62, Chorus 64),
    // a chord a block (C, F, G) and a marker on the chorus.
    struct Song
    {
        Studio studio { false };
        int track = 0;
        Song()
        {
            track = studio.getProject().tracks[0].id;
            studio.appendSongSection ("Intro", 4);
            studio.appendSongSection ("Verse", 8);
            studio.appendSongSection ("Chorus", 8);
            const auto places = studio.getSongSectionPlaces();
            const int pitches[] { 60, 62, 64 };
            const Chord chords[] { Chord { 0 }, Chord { 5 }, Chord { 7 } };
            std::vector<ChordEvent> events;
            for (int i = 0; i < 3; ++i)
            {
                Part part;
                part.start = places[(size_t) i].start;
                part.length = places[(size_t) i].end - places[(size_t) i].start;
                part.notes = { { 0.0, 1.0, pitches[i], 0.8f } };
                studio.addPart (track, part);
                events.push_back ({ places[(size_t) i].start, chords[i], 0.0 });
            }
            studio.setChordEvents (events);
            studio.addPositionMarker (places[2].start + 4.0, "Hook");
        }
        // The pitch of the part starting at `beat` (-1: none).
        int pitchAt (double beat) const
        {
            for (const auto& p : studio.getProject().findTrack (track)->parts)
                if (std::abs (p.start - beat) < 1.0e-6 && ! p.notes.empty())
                    return p.notes.front().pitch;
            return -1;
        }
        int chordRootAt (double beat) const
        {
            for (const auto& e : studio.getProject().chordTrack.events)
                if (std::abs (e.beat - beat) < 1.0e-6)
                    return e.chord.root;
            return -1;
        }
        juce::StringArray names() const
        {
            juce::StringArray out;
            for (const auto& b : studio.getSongSections().blocks)
                out.add (b.name);
            return out;
        }
    };
}

TEST_CASE ("0.90.0: blocks end to end in bars, numbered names, saved with the song", "[sections]")
{
    Studio studio (false);
    CHECK_FALSE (studio.getSongSections().shown);
    CHECK (studio.appendSongSection ("Intro", 4) == 0);
    CHECK (studio.getSongSections().shown);
    studio.appendSongSection ("Chorus", 8);
    studio.appendSongSection ("Verse 3", 8);   // (a numbered name is its section)
    studio.appendSongSection ("Chorus", 2, false);   // (0.91.0: its own, not a linked copy)
    const auto& blocks = studio.getSongSections().blocks;
    REQUIRE (blocks.size() == 4);
    CHECK (blocks[2].name == "Verse");
    CHECK (sectionLabel (blocks, 1) == "Chorus 1");
    CHECK (sectionLabel (blocks, 3) == "Chorus 2");
    CHECK (sectionLabel (blocks, 2) == "Verse");

    const auto places = studio.getSongSectionPlaces();
    CHECK (places[1].firstBar == 4);
    CHECK_THAT (places[1].start, WithinAbs (16.0, 1.0e-9));
    CHECK_THAT (places[3].end, WithinAbs (88.0, 1.0e-9));
    CHECK (sectionAt (places, 17.0) == 1);
    CHECK (sectionAt (places, 100.0) == -1);

    // Saved and opened again.
    const auto again = Project::fromXml (*studio.getProject().toXml());
    REQUIRE (again.has_value());
    CHECK (again->songSections == studio.getSongSections());

    // The composer's sections by name.
    CHECK (drumSections()[(size_t) composerSectionFor ("Pre-Chorus")] == DrumSection::verse);
    CHECK (drumSections()[(size_t) composerSectionFor ("Post-Chorus")] == DrumSection::chorus);
    CHECK (drumSections()[(size_t) composerSectionFor ("Outro")] == DrumSection::ending);
    CHECK (drumSections()[(size_t) composerSectionFor ("Solo")] == DrumSection::solo);
    CHECK (drumSections()[(size_t) composerSectionFor ("Middle 8")] == DrumSection::bridge);
    CHECK (sectionBaseOf ("Middle 8") == "Middle 8");
    CHECK (sectionBaseOf ("Middle 8 2") == "Middle 8");
    CHECK (sectionBaseOf ("Verse 3") == "Verse");
}

TEST_CASE ("0.90.0: moving a block moves its music, chords and markers; one undo step puts it back", "[sections]")
{
    Song song;
    auto& studio = song.studio;
    // Chorus to the front: Chorus 0-32, Intro 32-48, Verse 48-80.
    REQUIRE (studio.moveSongSection (2, 0));
    CHECK (song.names() == juce::StringArray { "Chorus", "Intro", "Verse" });
    CHECK (song.pitchAt (0.0) == 64);
    CHECK (song.pitchAt (32.0) == 60);
    CHECK (song.pitchAt (48.0) == 62);
    CHECK (song.chordRootAt (0.0) == 7);
    CHECK (song.chordRootAt (32.0) == 0);
    CHECK (song.chordRootAt (48.0) == 5);
    const auto& markers = studio.getMarkerTrack().markers;
    REQUIRE (markers.size() == 1);
    CHECK_THAT (markers[0].start, WithinAbs (4.0, 1.0e-9));
    CHECK (studio.getProject().findTrack (song.track)->parts.size() == 3);

    studio.undo();
    CHECK (song.names() == juce::StringArray { "Intro", "Verse", "Chorus" });
    CHECK (song.pitchAt (0.0) == 60);
    CHECK (song.pitchAt (48.0) == 64);
    CHECK_THAT (studio.getMarkerTrack().markers[0].start, WithinAbs (52.0, 1.0e-9));
}

TEST_CASE ("0.90.0: a part across a block's edge is cut there, each piece goes with its block", "[sections]")
{
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    studio.appendSongSection ("Verse", 4);
    studio.appendSongSection ("Chorus", 4);
    Part part;
    part.start = 8.0;
    part.length = 16.0;   // (beats 8-24: across the edge at 16)
    part.notes = { { 0.0, 1.0, 60, 0.8f }, { 12.0, 1.0, 67, 0.8f } };
    studio.addPart (track, part);
    REQUIRE (studio.moveSongSection (1, 0));
    const auto& parts = studio.getProject().findTrack (track)->parts;
    REQUIRE (parts.size() == 2);
    for (const auto& p : parts)
    {
        REQUIRE (p.notes.size() == 1);
        if (p.notes.front().pitch == 67)
            CHECK_THAT (p.start + p.notes.front().start, WithinAbs (4.0, 1.0e-9));   // (was at 20: the chorus's 5th beat)
        else
            CHECK_THAT (p.start + p.notes.front().start, WithinAbs (24.0, 1.0e-9));  // (was at 8: the verse's 9th beat)
    }
}

TEST_CASE ("0.90.0: a block's length, inserting, copying and removing move what follows", "[sections]")
{
    {
        Song song;
        // The verse cut to 4 bars: the chorus comes 16 beats sooner; the verse's part ends at its new end.
        REQUIRE (song.studio.setSongSectionBars (1, 4));
        CHECK (song.pitchAt (32.0) == 64);
        CHECK (song.pitchAt (16.0) == 62);
        for (const auto& p : song.studio.getProject().findTrack (song.track)->parts)
            if (p.notes.front().pitch == 62)
                CHECK_THAT (p.end(), WithinAbs (32.0, 1.0e-9));
        // Lengthened again: nothing comes back (it was cut), the chorus moves on.
        REQUIRE (song.studio.setSongSectionBars (1, 12));
        CHECK (song.pitchAt (64.0) == 64);
    }
    {
        Song song;
        // A new block before the verse: the verse and chorus move 2 bars later, the new block is empty.
        REQUIRE (song.studio.insertSongSection (1, "Pre-Chorus", 2));
        CHECK (song.names() == juce::StringArray { "Intro", "Pre-Chorus", "Verse", "Chorus" });
        CHECK (song.pitchAt (24.0) == 62);
        CHECK (song.pitchAt (56.0) == 64);
        CHECK (song.pitchAt (16.0) == -1);
        CHECK_THAT (song.studio.getMarkerTrack().markers[0].start, WithinAbs (60.0, 1.0e-9));
    }
    {
        Song song;
        // A copy of the chorus after it, its music copied.
        REQUIRE (song.studio.duplicateSongSection (2));
        CHECK (song.names() == juce::StringArray { "Intro", "Verse", "Chorus", "Chorus" });
        CHECK (song.pitchAt (48.0) == 64);
        CHECK (song.pitchAt (80.0) == 64);
        CHECK (song.chordRootAt (80.0) == 7);
        CHECK (song.studio.getMarkerTrack().markers.size() == 2);
        CHECK (sectionLabel (song.studio.getSongSections().blocks, 3) == "Chorus 2");
    }
    {
        Song song;
        // The verse removed with its music: the chorus moves up to beat 16.
        REQUIRE (song.studio.removeSongSection (1));
        CHECK (song.names() == juce::StringArray { "Intro", "Chorus" });
        CHECK (song.pitchAt (16.0) == 64);
        CHECK (song.studio.getProject().findTrack (song.track)->parts.size() == 2);
        CHECK (song.chordRootAt (16.0) == 7);
    }
}

TEST_CASE ("0.90.0: a block in 3/4 keeps its bars, tempo changes and automation where it lands", "[sections]")
{
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    studio.appendSongSection ("Verse", 4);    // 4/4: 0-16
    studio.appendSongSection ("Bridge", 4);   // 3/4: 16-28
    studio.appendSongSection ("Chorus", 4);   // 4/4 again: 28-44
    REQUIRE (studio.setSignatureAt (4, 3, 4));
    REQUIRE (studio.setSignatureAt (8, 4, 4));
    auto places = studio.getSongSectionPlaces();
    REQUIRE_THAT (places[1].end, WithinAbs (28.0, 1.0e-9));
    // A tempo change inside the bridge and a volume curve.
    studio.addTempoTrack();
    studio.setTempoEvents ({ { 0.0, 120.0, false }, { 20.0, 90.0, false }, { 28.0, 120.0, false } });
    studio.setAutomationOpen (track, true);
    studio.setAutomationPoints (track, 0, { { 0.0, 0.2f }, { 22.0, 0.9f }, { 40.0, 0.2f } });

    // The bridge first: bars 0-3 in 3/4 (beats 0-12), then the verse and chorus in 4/4.
    REQUIRE (studio.moveSongSection (1, 0));
    places = studio.getSongSectionPlaces();
    CHECK_THAT (places[0].end, WithinAbs (12.0, 1.0e-9));
    CHECK_THAT (places[1].end, WithinAbs (28.0, 1.0e-9));
    CHECK (studio.getProject().signatures().segmentOfBar (0).event.numerator == 3);
    CHECK (studio.getProject().signatures().segmentOfBar (4).event.numerator == 4);
    CHECK (studio.getProject().signatures().segmentOfBar (11).event.numerator == 4);
    // The bridge's 90 BPM (4 beats in) came with it; the verse starts at the tempo it had.
    const auto tempo = studio.getProject().tempoMap();
    CHECK_THAT (tempo.bpmAt (5.0), WithinAbs (90.0, 1.0e-9));
    CHECK_THAT (tempo.bpmAt (13.0), WithinAbs (120.0, 1.0e-9));
    // The bridge's point (6 beats in) is now at 6.
    const auto& lane = studio.getProject().findTrack (track)->automation[0];
    CHECK (std::any_of (lane.points.begin(), lane.points.end(), [] (const AutomationPoint& p) { return std::abs (p.beat - 6.0) < 1.0e-6 && std::abs (p.value - 0.9f) < 1.0e-6f; }));
}

TEST_CASE ("0.90.0: renaming and the row's shown state; nothing else moves", "[sections]")
{
    Song song;
    auto& studio = song.studio;
    REQUIRE (studio.renameSongSection (1, "Pre-Chorus"));
    CHECK (song.names()[1] == "Pre-Chorus");
    CHECK_FALSE (studio.renameSongSection (1, "Not a section"));
    CHECK (song.pitchAt (16.0) == 62);
    studio.removeSongSectionTrack();
    CHECK_FALSE (studio.getSongSections().shown);
    CHECK (studio.getSongSections().blocks.size() == 3);
    studio.addSongSectionTrack();
    CHECK (studio.getSongSections().shown);
    CHECK_FALSE (studio.moveSongSection (5, 0));
}

TEST_CASE ("0.90.0: the SECTIONS row on screen: a block dragged to the front, a double-click opens the composer on it", "[sections][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Song song;
    auto& studio = song.studio;
    MainView view (studio);
    view.setSize (1600, 900);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    REQUIRE_FALSE (arrange.getSectionArea().isEmpty());
    const auto chorus = arrange.getSectionBounds (2);
    REQUIRE_FALSE (chorus.isEmpty());
    CHECK (arrange.sectionBlockAt (chorus.getCentre()) == 2);

    // The chorus dragged left of the intro's middle: it goes first, its music with it.
    const auto from = chorus.getCentre();
    const auto to = juce::Point<float> (arrange.getSectionBounds (0).getX() + 4.0f, from.y);
    arrange.mouseDown (mouseAt (arrange, from, from));
    arrange.mouseDrag (mouseAt (arrange, to, from));
    CHECK (arrange.getSectionDragTarget() == 0);
    arrange.mouseUp (mouseAt (arrange, to, from));
    CHECK (song.names() == juce::StringArray { "Chorus", "Intro", "Verse" });
    CHECK (song.pitchAt (0.0) == 64);
    CHECK (arrange.getSelectedSection() == 0);

    // A double-click on the verse (now third) opens the composer working there.
    const auto verse = arrange.getSectionBounds (2).getCentre();
    arrange.mouseDoubleClick (mouseAt (arrange, verse, verse, 2));
    auto* window = view.getComposerWindow();
    REQUIRE (window != nullptr);
    auto& page = window->getPage();
    CHECK (page.getTargetSection() == studio.getSongSections().blocks[2].id);
    const auto place = page.getTargetPlace();
    REQUIRE (place.has_value());
    CHECK_THAT (place->start, WithinAbs (48.0, 1.0e-9));
    CHECK (page.buildStart() == place->start);
    CHECK (page.getSection() == DrumSection::verse);
    // Its chords end at the block's end (8 bars), wherever the progression would run on.
    const auto spans = page.spansAt (page.buildStart());
    REQUIRE_FALSE (spans.empty());
    CHECK_THAT (spans.front().start, WithinAbs (48.0, 1.0e-9));
    CHECK (spans.back().end <= place->end + 1.0e-9);
    CHECK (spans.back().end > place->end - 4.0);
    window->setVisible (false);
}

namespace
{
    int partIndexAt (const Studio& studio, int track, double beat)
    {
        const auto& parts = studio.getProject().findTrack (track)->parts;
        for (int i = 0; i < (int) parts.size(); ++i)
            if (std::abs (parts[(size_t) i].start - beat) < 1.0e-6)
                return i;
        return -1;
    }
}

TEST_CASE ("0.91.0: linked copies: an edit to one changes the others, in the same undo step", "[sections][links]")
{
    Song song;
    auto& studio = song.studio;
    REQUIRE (studio.duplicateSongSection (2));   // the chorus at 48, its linked copy at 80
    CHECK (studio.isSongSectionLinked (2));
    CHECK (studio.isSongSectionLinked (3));
    CHECK_FALSE (studio.isSongSectionLinked (1));
    REQUIRE (song.pitchAt (80.0) == 64);

    // Chorus 1's notes edited: chorus 2's follow.
    auto index = partIndexAt (studio, song.track, 48.0);
    REQUIRE (index >= 0);
    studio.setNotes (song.track, index, { { 0.0, 1.0, 64, 0.8f }, { 2.0, 1.0, 67, 0.8f } });
    auto copy = partIndexAt (studio, song.track, 80.0);
    REQUIRE (copy >= 0);
    REQUIRE (studio.getProject().findTrack (song.track)->parts[(size_t) copy].notes.size() == 2);
    CHECK (studio.getProject().findTrack (song.track)->parts[(size_t) copy].notes[1].pitch == 67);
    // One undo: both back.
    studio.undo();
    copy = partIndexAt (studio, song.track, 80.0);
    CHECK (studio.getProject().findTrack (song.track)->parts[(size_t) copy].notes.size() == 1);
    index = partIndexAt (studio, song.track, 48.0);
    CHECK (studio.getProject().findTrack (song.track)->parts[(size_t) index].notes.size() == 1);

    // Chorus 2 edited: chorus 1 follows.
    copy = partIndexAt (studio, song.track, 80.0);
    studio.setNotes (song.track, copy, { { 0.0, 1.0, 70, 0.8f } });
    CHECK (song.pitchAt (48.0) == 70);

    // A part added inside chorus 2 (on another track too) is in chorus 1.
    const auto other = studio.addTrack ({});
    Part part;
    part.start = 88.0;
    part.length = 4.0;
    part.notes = { { 0.0, 1.0, 72, 0.8f } };
    studio.addPart (other, part);
    REQUIRE (partIndexAt (studio, other, 56.0) >= 0);
    CHECK (studio.getProject().findTrack (other)->parts[(size_t) partIndexAt (studio, other, 56.0)].notes.front().pitch == 72);

    // Chorus 1's chord changed: chorus 2's too.
    auto events = studio.getProject().chordTrack.events;
    for (auto& e : events)
        if (std::abs (e.beat - 48.0) < 1.0e-6)
            e.chord = Chord { 9 };
    studio.setChordEvents (events);
    CHECK (song.chordRootAt (80.0) == 9);
    CHECK (song.chordRootAt (16.0) == 5);   // (the verse untouched)

    // Unlinked: its own from then on, its music kept.
    REQUIRE (studio.unlinkSongSection (3));
    CHECK_FALSE (studio.isSongSectionLinked (2));
    CHECK_FALSE (studio.isSongSectionLinked (3));
    index = partIndexAt (studio, song.track, 48.0);
    studio.setNotes (song.track, index, { { 0.0, 1.0, 60, 0.8f } });
    CHECK (song.pitchAt (48.0) == 60);
    CHECK (song.pitchAt (80.0) == 70);
}

TEST_CASE ("0.91.0: a name already in the song adds a linked copy; linked blocks share length and name; saved and opened", "[sections][links]")
{
    Song song;
    auto& studio = song.studio;
    // Another verse: a linked copy of the verse (its 8 bars and its music) after the chorus.
    const auto added = studio.appendSongSection ("Verse", 4);
    REQUIRE (added == 3);
    const auto& blocks = studio.getSongSections().blocks;
    CHECK (blocks[3].bars == 8);
    CHECK (blocks[3].link > 0);
    CHECK (blocks[3].link == blocks[1].link);
    CHECK (song.pitchAt (80.0) == 62);

    // One length and one name for both.
    REQUIRE (studio.setSongSectionBars (3, 4));
    CHECK (blocks[1].bars == 4);
    CHECK (blocks[3].bars == 4);
    REQUIRE (studio.renameSongSection (3, "Pre-Chorus"));
    CHECK (blocks[1].name == "Pre-Chorus");

    // Its own when asked: a new empty block, not linked.
    REQUIRE (studio.insertSongSection (0, "Chorus", 2, false));
    CHECK (studio.getSongSections().blocks[0].link == 0);
    CHECK (song.pitchAt (0.0) == -1);

    // Saved and opened again: still linked, edits still copied.
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("linked", ".pr7s");
    REQUIRE (studio.save (file));
    Studio opened (false);
    juce::String problems;
    REQUIRE (opened.open (file, problems));
    const auto& o = opened.getSongSections().blocks;
    REQUIRE (o.size() == 5);
    CHECK (o[2].link > 0);
    CHECK (o[2].link == o[4].link);
    const auto places = opened.getSongSectionPlaces();
    const auto track = opened.getProject().tracks[0].id;
    const auto i = partIndexAt (opened, track, places[2].start);
    REQUIRE (i >= 0);
    opened.setNotes (track, i, { { 0.0, 1.0, 65, 0.8f } });
    const auto j = partIndexAt (opened, track, places[4].start);
    REQUIRE (j >= 0);
    CHECK (opened.getProject().findTrack (track)->parts[(size_t) j].notes.front().pitch == 65);
    file.deleteFile();
}
