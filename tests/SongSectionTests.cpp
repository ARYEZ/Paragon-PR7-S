// The SECTIONS row (0.90.0, the owner's song engine): the song as blocks end to end from bar 1, each a section's
// name and a length in bars; rearranging the blocks moves the music in them (the owner's: "moving blocks moves the
// music in that part with it").

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include <set>
#include <functional>
#include "engine/DrumGenerator.h"
#include "engine/DrumIdeas.h"
#include "engine/RiffGenerator.h"
#include <tuple>
#include "engine/SectionTypes.h"
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
    const auto kept = song.pitchAt (80.0);   // (0.92.0: the chord's change moved its notes too, in both)
    CHECK (song.pitchAt (48.0) == kept);

    // Unlinked: its own from then on, its music kept.
    REQUIRE (studio.unlinkSongSection (3));
    CHECK_FALSE (studio.isSongSectionLinked (2));
    CHECK_FALSE (studio.isSongSectionLinked (3));
    index = partIndexAt (studio, song.track, 48.0);
    studio.setNotes (song.track, index, { { 0.0, 1.0, 60, 0.8f } });
    CHECK (song.pitchAt (48.0) == 60);
    CHECK (song.pitchAt (80.0) == kept);
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

namespace
{
    // The chorus's chord (index 2's) set to another one.
    void setChorusChord (Studio& studio, const Chord& chord)
    {
        const auto place = *studio.getSongSectionPlace (2);
        auto events = studio.getProject().chordTrack.events;
        for (auto& e : events)
            if (std::abs (e.beat - place.start) < 1.0e-6)
                e.chord = chord;
        studio.setChordEvents (events);
    }

    // The chorus's bass built by the composer (its default settings), noted on the block. Its track.
    int buildChorusBass (Studio& studio)
    {
        const auto place = *studio.getSongSectionPlace (2);
        std::vector<ChordSpan> spans { { place.start, place.end, Chord { 7 }, Key {} } };
        ComposerState state;
        const auto layers = composeLayers (state, spans, studio.getComposerContext (place.start));
        const auto& bass = layers[(size_t) ComposerLayer::bass];
        REQUIRE_FALSE (bass.notes.empty());
        const auto tracks = studio.buildSection ({ bass }, spans, "Chorus", {});
        REQUIRE (tracks.size() == 1);
        studio.noteSectionBuilt (studio.getSongSections().blocks[2].id, state.toString(), { { (int) ComposerLayer::bass, tracks[0] } });
        return tracks[0];
    }

    std::vector<int> pitchesIn (const Studio& studio, int track, const SectionPlace& place)
    {
        std::vector<int> out;
        for (const auto& p : studio.getProject().findTrack (track)->parts)
            for (const auto& n : p.notes)
                if (p.start + n.start >= place.start - 1.0e-9 && p.start + n.start < place.end - 1.0e-9)
                    out.push_back (n.pitch);
        return out;
    }
}

TEST_CASE ("0.92.0: a block's chord changed: the composer's lane built again, the rest follow the chord; one undo step", "[sections][rewrite]")
{
    Song song;
    auto& studio = song.studio;
    const auto bass = buildChorusBass (studio);
    const auto place = *studio.getSongSectionPlace (2);
    const auto before = pitchesIn (studio, bass, place);
    REQUIRE_FALSE (before.empty());
    for (const auto p : before)
        CHECK (((p % 12) + 12) % 12 == 7);   // (the bass on the kick: G's root)

    // G to A minor: the bass is built again on A; the lead's note follows the chord.
    setChorusChord (studio, Chord { 9, Chord::Type::minor });
    const auto after = pitchesIn (studio, bass, place);
    REQUIRE_FALSE (after.empty());
    for (const auto p : after)
        CHECK (((p % 12) + 12) % 12 == 9);
    // The lead's E (G's sixth, not a chord tone) moves along the scale with the root (G to A: a step up).
    const auto lead = song.pitchAt (place.start);
    CHECK (lead != 64);
    const auto scale = Key {}.scale();
    CHECK (std::find (scale.begin(), scale.end(), ((lead % 12) + 12) % 12) != scale.end());
    CHECK (song.pitchAt (0.0) == 60);    // (the intro untouched)
    CHECK (song.pitchAt (16.0) == 62);   // (the verse untouched)

    // One undo: the chord and the music back together.
    studio.undo();
    CHECK (song.chordRootAt (place.start) == 7);
    CHECK (pitchesIn (studio, bass, place) == before);
    CHECK (song.pitchAt (place.start) == 64);
}

TEST_CASE ("0.92.0: a lane edited by hand follows the chords instead of being built again; a move rewrites nothing", "[sections][rewrite]")
{
    Song song;
    auto& studio = song.studio;
    const auto bass = buildChorusBass (studio);
    const auto place = *studio.getSongSectionPlace (2);
    // The bass edited: two notes of its own (B and D over G: the chord's third and fifth).
    const auto& parts = studio.getProject().findTrack (bass)->parts;
    int index = -1;
    for (int i = 0; i < (int) parts.size(); ++i)
        if (parts[(size_t) i].start >= place.start - 1.0e-6)
            index = i;
    REQUIRE (index >= 0);
    studio.setNotes (bass, index, { { 0.0, 2.0, 35, 0.8f }, { 4.0, 2.0, 38, 0.8f } });
    // G to C: followed (two notes still, now C's third and fifth: E and G), not built again.
    setChorusChord (studio, Chord { 0 });
    const auto pitches = pitchesIn (studio, bass, place);
    REQUIRE (pitches.size() == 2);
    CHECK (((pitches[0] % 12) + 12) % 12 == 4);
    CHECK (((pitches[1] % 12) + 12) % 12 == 7);

    // Moving the chorus first changes no notes.
    const auto lead = song.pitchAt (place.start);
    REQUIRE (studio.moveSongSection (2, 0));
    CHECK (song.pitchAt (0.0) == lead);
    CHECK (pitchesIn (studio, bass, *studio.getSongSectionPlace (0)) == pitches);
}

TEST_CASE ("0.92.0: a linked chorus's chord changed: every copy rewritten", "[sections][rewrite][links]")
{
    Song song;
    auto& studio = song.studio;
    const auto bass = buildChorusBass (studio);
    REQUIRE (studio.duplicateSongSection (2));
    const auto copy = *studio.getSongSectionPlace (3);
    REQUIRE_FALSE (pitchesIn (studio, bass, copy).empty());
    setChorusChord (studio, Chord { 2 });   // G to D
    for (const auto p : pitchesIn (studio, bass, copy))
        CHECK (((p % 12) + 12) % 12 == 2);
    CHECK (song.chordRootAt (copy.start) == 2);
    // The recipe saved with the song.
    const auto again = Project::fromXml (*studio.getProject().toXml());
    REQUIRE (again.has_value());
    CHECK (again->songSections.blocks[2].recipe.isNotEmpty());
    CHECK (again->songSections.blocks[2].lanes.size() == 1);
}

TEST_CASE ("0.93.0: the composer block by block: aimed at the first block, BUILD moves on, + ADD adds and composes, chips compose", "[sections][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    studio.appendSongSection ("Intro", 4);
    studio.appendSongSection ("Verse", 8);
    studio.appendSongSection ("Chorus", 8);
    MainView view (studio);
    view.setSize (1600, 900);
    view.openComposer();
    auto* window = view.getComposerWindow();
    REQUIRE (window != nullptr);
    auto& page = window->getPage();
    page.setSize (1700, 940);
    const auto& blocks = studio.getSongSections().blocks;
    REQUIRE (page.usesSectionsRow());
    CHECK (page.getTargetSection() == blocks[0].id);

    // BUILD into the intro: it keeps its recipe, the composer moves on to the verse.
    REQUIRE_FALSE (page.buildSection().empty());
    CHECK (blocks[0].recipe.isNotEmpty());
    CHECK (page.getTargetSection() == blocks[1].id);
    CHECK (page.getSection() == DrumSection::verse);

    // A chip on the SONG strip composes its block.
    const auto chip = page.getSongChipBounds (2);
    REQUIRE_FALSE (chip.isEmpty());
    const auto at = chip.getCentre().toFloat();
    page.mouseDown (mouseAt (page, at, at));
    CHECK (page.getTargetSection() == blocks[2].id);

    // + ADD: the suggestion's block added at the end and composed.
    REQUIRE (page.getNumNextKeys() > 0);
    const auto before = blocks.size();
    page.getNextKey (0)->onClick();
    REQUIRE (studio.getSongSections().blocks.size() == before + 1);
    CHECK (page.getTargetSection() == studio.getSongSections().blocks.back().id);
    CHECK (page.getNextKey (0)->getButtonText().startsWith ("+ ADD"));
    window->setVisible (false);
}

//==============================================================================
// Section types (0.94.0, the owner's: a chorus DRIVING or DRONING, each instrument with its own approaches within it).

TEST_CASE ("0.94.0: the types a section can have; STANDARD is none; a type sets the guitar's flavour", "[sections][types]")
{
    const auto chorus = sectionTypesFor ("Chorus");
    REQUIRE (chorus.size() == 2);
    CHECK (chorus[0].name == "DRIVING");
    CHECK (chorus[0].flavour == RiffFlavour::drivingEighths);
    CHECK (chorus[1].name == "DRONING");
    CHECK (chorus[1].flavour == RiffFlavour::droning);
    // A name plays as its kind: a Hook as a chorus, a Pre-Chorus as a verse.
    CHECK (sectionTypesFor ("Hook").size() == 2);
    CHECK (sectionTypesFor ("Pre-Chorus").front().name == sectionTypesFor ("Verse").front().name);
    for (const auto& name : songSectionNames())
        CHECK_FALSE (sectionTypesFor (name).empty());   // (every section seeded with suggestions)
    CHECK (isSectionTypeOf ("Chorus", {}));
    CHECK (isSectionTypeOf ("Chorus", "STANDARD"));
    CHECK (isSectionTypeOf ("Chorus", "droning"));
    CHECK_FALSE (isSectionTypeOf ("Chorus", "STACCATO CHUG"));
    CHECK (sectionTypesFor ("Verse").front().name == "STACCATO CHUG");

    ComposerState s;
    s.lane (ComposerLayer::drums).approach = 1;
    s.writtenRiffs = true;
    applySectionType (s, "Chorus", "DRONING");
    CHECK (s.sectionType == "DRONING");
    CHECK (riffFlavours()[(size_t) s.riffFlavour] == RiffFlavour::droning);
    CHECK_FALSE (s.writtenRiffs);
    CHECK (s.lane (ComposerLayer::drums).approach == -1);
    // Saved and read back with each lane's approach.
    s.lane (ComposerLayer::chords).approach = 2;
    const auto again = ComposerState::fromString (s.toString());
    CHECK (again == s);
    applySectionType (s, "Chorus", "STANDARD");
    CHECK (s.sectionType.isEmpty());
    CHECK (riffFlavours()[(size_t) s.riffFlavour] == RiffFlavour::any);
    CHECK (sectionApproaches (s, ComposerLayer::drums).isEmpty());   // (STANDARD: the lanes' usual styles)
}

TEST_CASE ("0.94.0: DRONING's approaches: each lane plays the one chosen", "[sections][types]")
{
    // The guitar: only the chosen approach's shapes, over many takes.
    for (juce::uint32 seed = 1; seed < 40; ++seed)
    {
        RiffRecipe r;
        r.flavour = RiffFlavour::droning;
        r.section = DrumSection::chorus;
        r.bars = 4;
        r.seed = seed;
        const auto has = [] (const std::vector<std::string>& bars, char c)
        {
            for (const auto& b : bars)
                if (b.find (c) != std::string::npos)
                    return true;
            return false;
        };
        r.approach = 0;
        const auto rung = generateRiffBars (r);
        CHECK_FALSE (has (rung, '~'));
        CHECK_FALSE (has (rung, '@'));
        r.approach = 1;
        CHECK (has (generateRiffBars (r), '~'));   // (pushed a beat early)
        r.approach = 2;
        CHECK (has (generateRiffBars (r), '@'));   // (the power chord held, a top note moving)
        r.approach = 4;
        CHECK ((has (generateRiffBars (r), '1') || has (generateRiffBars (r), '2') || has (generateRiffBars (r), '3')));   // (sliding down)
    }
    // The band: the approach's part, whatever the style or the take.
    ComposerState s;
    applySectionType (s, "Chorus", "DRONING");
    std::vector<ChordSpan> spans;
    for (int i = 0; i < 4; ++i)
        spans.push_back ({ i * 8.0, i * 8.0 + 8.0, Chord { (i * 5) % 12 }, Key {} });
    ComposerContext c;
    const auto check = [&] (ComposerLayer l, int approach, const juce::String& word)
    {
        for (const auto variation : { 0, 1, 2, 5 })
            for (const auto style : { 0, 1, 2 })
            {
                auto t = s;
                t.lane (l).on = true;
                t.lane (l).approach = approach;
                t.lane (l).variation = variation;
                t.lane (l).style = juce::jmin (style, composerStyleCount (l) - 1);
                const auto layers = composeLayers (t, spans, c);
                const auto& idea = layers[(size_t) l];
                INFO (composerLayerName (l) << " " << approach << ": " << idea.description << " / " << idea.style);
                CHECK ((idea.description + " " + idea.style).containsIgnoreCase (word));
            }
    };
    check (ComposerLayer::drums, 0, "open hats");
    check (ComposerLayer::drums, 1, "half time");
    check (ComposerLayer::drums, 2, "pushes");
    check (ComposerLayer::chords, 0, "pulse");
    check (ComposerLayer::chords, 1, "motif");
    check (ComposerLayer::lead, 0, "thirds");
    check (ComposerLayer::lead, 1, "roots");
    check (ComposerLayer::lead, 2, "pedal");
    check (ComposerLayer::bass, 1, "pulsed");
    CHECK (sectionApproaches (s, ComposerLayer::guitar).size() == 5);
    CHECK (approachName (s, ComposerLayer::drums) == "AUTOMATIC");
}

TEST_CASE ("0.94.0: a block's type: STANDARD to start, saved, linked copies share it; a built block is built again for it", "[sections][types]")
{
    Song song;
    auto& studio = song.studio;
    for (const auto& b : studio.getSongSections().blocks)
        CHECK (b.type.isEmpty());   // (every block starts STANDARD)
    CHECK_FALSE (studio.getAskBeforeTypeChange());   // (HUB SETTINGS' question off by default)

    // Not one of the block's: refused. The label shows it.
    CHECK_FALSE (studio.setSongSectionType (2, "STACCATO CHUG"));
    const auto bass = buildChorusBass (studio);
    const auto place = *studio.getSongSectionPlace (2);
    const auto partName = [&]
    {
        for (const auto& p : studio.getProject().findTrack (bass)->parts)
            if (p.start >= place.start - 1.0e-6 && p.start < place.end)
                return p.name;
        return juce::String();
    };
    const auto before = pitchesIn (studio, bass, place);
    CHECK_FALSE (partName().containsIgnoreCase ("droning"));

    // DRONING: the bass built again as the droning band's (doubling the ringing guitar).
    REQUIRE (studio.setSongSectionType (2, "droning"));
    CHECK (studio.getSongSections().blocks[2].type == "DRONING");
    CHECK (sectionLabelWithType (studio.getSongSections().blocks, 2) == "Chorus - DRONING");
    CHECK (partName().containsIgnoreCase ("droning"));
    CHECK (ComposerState::fromString (studio.getSongSections().blocks[2].recipe).sectionType == "DRONING");
    CHECK (pitchesIn (studio, bass, place) != before);
    CHECK (song.pitchAt (0.0) == 60);   // (the intro untouched)

    // One undo: STANDARD and the bass as it was.
    studio.undo();
    CHECK (studio.getSongSections().blocks[2].type.isEmpty());
    CHECK (pitchesIn (studio, bass, place) == before);

    // A bass edited by hand stays as it is through a type change.
    REQUIRE (studio.setSongSectionType (2, "DRIVING"));
    const auto& parts = studio.getProject().findTrack (bass)->parts;
    int index = -1;
    for (int i = 0; i < (int) parts.size(); ++i)
        if (parts[(size_t) i].start >= place.start - 1.0e-6)
            index = i;
    REQUIRE (index >= 0);
    studio.setNotes (bass, index, { { 0.0, 2.0, 31, 0.8f } });
    REQUIRE (studio.setSongSectionType (2, "DRONING"));
    CHECK (pitchesIn (studio, bass, place) == std::vector<int> { 31 });

    // A linked copy shares the type; saved and opened; a name that hasn't the type goes back to STANDARD.
    REQUIRE (studio.duplicateSongSection (2));
    CHECK (studio.getSongSections().blocks[3].type == "DRONING");
    const auto xml = studio.getSongSections().toXml();
    CHECK (SongSections::fromXml (*xml).blocks[3].type == "DRONING");
    REQUIRE (studio.renameSongSection (3, "Breakdown"));   // (no DRONING there)
    CHECK (studio.getSongSections().blocks[3].type.isEmpty());
    CHECK (studio.getSongSections().blocks[2].type.isEmpty());   // (linked: one name, so both)
}

TEST_CASE ("0.94.0: the composer's TYPE: the block's, the approaches in the lanes, the question when HUB SETTINGS asks", "[sections][types][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    studio.appendSongSection ("Intro", 4);
    studio.appendSongSection ("Chorus", 8);
    REQUIRE (studio.setSongSectionType (1, "DRONING"));
    MainView view (studio);
    view.setSize (1600, 900);
    view.openComposer();
    auto* window = view.getComposerWindow();
    REQUIRE (window != nullptr);
    auto& page = window->getPage();
    page.setSize (1700, 940);
    const auto chorusId = studio.getSongSections().blocks[1].id;
    page.targetSection (chorusId);
    CHECK (page.getSectionType() == "DRONING");
    page.setApproach (ComposerLayer::drums, 1);
    CHECK (page.getApproach (ComposerLayer::drums) == 1);

    // Built, then the type changed with HUB SETTINGS asking: nothing until the answer.
    REQUIRE_FALSE (page.buildSection().empty());
    page.targetSection (chorusId);
    CHECK (ComposerState::fromString (studio.getSongSections().blocks[1].recipe).lane (ComposerLayer::drums).approach == 1);
    studio.setAskBeforeTypeChange (true);
    std::function<void()> answer;
    page.askToRebuild = [&answer] (const juce::String&, std::function<void()> yes) { answer = std::move (yes); };
    page.setSectionType ("DRIVING");
    CHECK (studio.getSongSections().blocks[1].type == "DRONING");
    REQUIRE (answer != nullptr);
    answer();
    CHECK (studio.getSongSections().blocks[1].type == "DRIVING");
    CHECK (page.getSectionType() == "DRIVING");
    CHECK (page.getApproach (ComposerLayer::drums) == -1);   // (DRIVING has no band approaches yet)

    // The intro is STANDARD: the composer goes back to no type (the flavour ANY).
    page.targetSection (studio.getSongSections().blocks[0].id);
    CHECK (page.getSectionType().isEmpty());
    window->setVisible (false);
}

TEST_CASE ("0.94.0: STACCATO CHUG: one verse type, the rhythm guitar's approaches its chug kinds", "[sections][types]")
{
    ComposerState s;
    applySectionType (s, "Verse", "STACCATO CHUG");
    const auto names = sectionApproaches (s, ComposerLayer::guitar);
    REQUIRE (names.size() == 6);   // (0.96.0: BURST & SILENCE moved to SPLIT CHUG)
    CHECK (names[0] == "PEDAL & STABS");
    CHECK (names[2] == "TRITONE CHUG");
    CHECK (names[4] == "GALLOP");
    ComposerContext c;
    s.lane (ComposerLayer::guitar).approach = 4;
    CHECK (guitarRecipe (s, c).flavour == RiffFlavour::gallop);
    s.lane (ComposerLayer::guitar).approach = 1;
    CHECK (guitarRecipe (s, c).flavour == RiffFlavour::chugThrees);
    // AUTOMATIC: each take another of the type's kinds, never one outside it.
    s.lane (ComposerLayer::guitar).approach = -1;
    std::set<RiffFlavour> seen;
    for (int v = 0; v < 14; ++v)
    {
        s.lane (ComposerLayer::guitar).variation = v;
        seen.insert (guitarRecipe (s, c).flavour);
    }
    CHECK (seen == std::set<RiffFlavour> { RiffFlavour::pedalStabs, RiffFlavour::chugThrees,
                                           RiffFlavour::tritone, RiffFlavour::chugs, RiffFlavour::gallop, RiffFlavour::stopStart });
    // A Pre-Chorus has it too; a chorus doesn't.
    CHECK (isSectionTypeOf ("Pre-Chorus", "STACCATO CHUG"));
    CHECK (sectionApproaches (s, ComposerLayer::bass).size() == 3);   // (0.97.2: LOCKED WITH THE KICK)
    CHECK (sectionApproaches (s, ComposerLayer::chords).isEmpty());  // (the keys: their usual styles)
}

TEST_CASE ("0.95.0: STACCATO CHUG's riff kinds: Black's stabs in threes, Stricken's chugs in threes, Waffle's burst and silence", "[sections][types]")
{
    for (juce::uint32 seed = 1; seed <= 30; ++seed)
    {
        RiffRecipe r;
        r.seed = seed;
        r.bars = 4;
        r.section = DrumSection::verse;
        // PEDAL & STABS: chugs on the low string, a power chord (a digit) every third sixteenth.
        r.flavour = RiffFlavour::pedalStabs;
        const auto black = generateRiffBars (r);
        REQUIRE (black.size() == 4);
        for (int i = 0; i < 13; ++i)
        {
            const auto ch = black[0][(size_t) i];
            if (i > 0 && i % 3 == 0)
                CHECK ((ch >= '1' && ch <= '9'));
            else
                CHECK (ch == 'x');
        }
        // CHUGS IN THREES: a rest and two muted power chords over and over, a walk up at the bar's end; the fourth bar climbs.
        r.flavour = RiffFlavour::chugThrees;
        const auto stricken = generateRiffBars (r);
        REQUIRE (stricken.size() == 4);
        for (int i = 0; i < 13; ++i)
            CHECK (stricken[0][(size_t) i] == (i % 3 == 0 ? '-' : '*'));
        CHECK ((stricken[0][13] >= '1' && stricken[0][13] <= '9'));
        CHECK (stricken[0][15] == '*');
        CHECK ((stricken[3][1] >= '1' && stricken[3][1] <= '9'));
        // BURST & SILENCE: a hit on the bar line, the bar's last quarter silent (a stab there every other bar).
        r.flavour = RiffFlavour::burstSilence;
        const auto waffle = generateRiffBars (r);
        REQUIRE (waffle.size() == 4);
        CHECK (waffle[0][0] == 'x');
        CHECK (waffle[0].substr (13) == "---");
        CHECK (waffle[1][12] != '-');
        CHECK (waffle[1][12] != 'x');
    }
}

TEST_CASE ("0.95.0: STACCATO CHUG's band: the bass doubles the guitar's chugs and the kick plays every one (the accents when asked)", "[sections][types]")
{
    ComposerState s;
    applySectionType (s, "Verse", "STACCATO CHUG");
    s.lane (ComposerLayer::guitar).approach = 1;   // (Stricken's chugs in threes)
    std::vector<ChordSpan> spans;
    for (int i = 0; i < 4; ++i)
        spans.push_back ({ i * 4.0, i * 4.0 + 4.0, Chord { i % 2 == 0 ? 2 : 5, i % 2 == 0 ? Chord::Type::minor : Chord::Type::major }, Key {} });
    ComposerContext c;
    const auto layers = composeLayers (s, spans, c);
    const auto starts = [] (const LineIdea& idea, std::function<bool (const LineNote&)> keep)
    {
        std::set<int> at;
        for (const auto& n : idea.notes)
            if (! n.keyswitch && keep (n))
                at.insert (juce::roundToInt (n.beat * 4.0));
        return at;
    };
    const auto all = [] (const LineNote&) { return true; };
    const auto guitar = starts (layers[(size_t) ComposerLayer::guitar], all);
    REQUIRE (guitar.size() > 20);
    const auto bass = starts (layers[(size_t) ComposerLayer::bass], all);
    const auto kicks = starts (layers[(size_t) ComposerLayer::drums], [] (const LineNote& n) { return n.pitch == 35 || n.pitch == 36; });
    // Every chug has the bass and a kick on it (16ths: rounded to the sixteenth).
    auto bassOn = 0, kickOn = 0;
    for (const auto g : guitar)
    {
        bassOn += bass.count (g) > 0 ? 1 : 0;
        kickOn += kicks.count (g) > 0 ? 1 : 0;
    }
    CHECK (bassOn >= (int) guitar.size() * 9 / 10);
    CHECK (kickOn >= (int) guitar.size() * 8 / 10);
    // The accents only: fewer.
    auto loose = s;
    loose.lane (ComposerLayer::drums).approach = 1;
    loose.lane (ComposerLayer::bass).approach = 1;
    const auto looser = composeLayers (loose, spans, c);
    CHECK (starts (looser[(size_t) ComposerLayer::drums], [] (const LineNote& n) { return n.pitch == 35 || n.pitch == 36; }).size() < kicks.size());
    CHECK (starts (looser[(size_t) ComposerLayer::bass], all).size() < bass.size());
    // STANDARD: neither follows the guitar.
    ComposerState plain;
    plain.riffFlavour = s.riffFlavour;
    const auto standard = composeLayers (plain, spans, c);
    CHECK (starts (standard[(size_t) ComposerLayer::bass], all) != bass);
}

TEST_CASE ("0.96.0: SPLIT CHUG: a verse type of its own, its riff kinds the owner's three and Waffle's", "[sections][types]")
{
    REQUIRE (isSectionTypeOf ("Verse", "SPLIT CHUG"));
    CHECK (isSectionTypeOf ("Pre-Chorus", "SPLIT CHUG"));
    CHECK_FALSE (isSectionTypeOf ("Chorus", "SPLIT CHUG"));
    ComposerState s;
    applySectionType (s, "Verse", "SPLIT CHUG");
    const auto names = sectionApproaches (s, ComposerLayer::guitar);
    REQUIRE (names.size() == 4);
    CHECK (names[0] == "SECOND-HALF STABS");
    CHECK (names[1] == "FIRST-HALF BURST");
    CHECK (names[2] == "HIT & PUSH");
    CHECK (names[3] == "BURST & SILENCE");
    CHECK (sectionApproaches (s, ComposerLayer::bass)[1] == "HOLDS THROUGH THE GAPS");
    CHECK (sectionApproaches (s, ComposerLayer::drums)[1] == "PLAIN BACKBEAT");

    for (juce::uint32 seed = 1; seed <= 30; ++seed)
    {
        RiffRecipe r;
        r.seed = seed;
        r.bars = 4;
        r.section = DrumSection::verse;
        const auto quiet = [] (const std::string& bar, int from, int to)
        {
            for (int i = from; i < to; ++i)
                if (bar[(size_t) i] != '-')
                    return false;
            return true;
        };
        // Going Under: a chord on 1, the first half otherwise silent, stabs in the second half; the 4th bar a breath.
        r.flavour = RiffFlavour::secondHalfStabs;
        const auto stabs = generateRiffBars (r);
        REQUIRE (stabs.size() == 4);
        CHECK (stabs[0][0] == '#');
        CHECK (quiet (stabs[0], 1, 7));
        auto second = 0;
        for (int i = 8; i < 16; ++i)
            second += stabs[0][(size_t) i] == '#' ? 1 : 0;
        CHECK (second >= 3);
        CHECK (quiet (stabs[3], 1, 16));
        // Papercut: the burst in the first half, the second half silent (or the intro's held chord), a pickup in the 4th bar.
        r.flavour = RiffFlavour::firstHalfBurst;
        const auto burst = generateRiffBars (r);
        CHECK (burst[0][0] == '*');
        CHECK ((quiet (burst[0], 11, 16) || burst[0].substr (8) == "========"));
        CHECK (burst[3].substr (13) == "223");
        // Call Me: a chord on 1 (or three quick chugs), the bar open, the push on the last three sixteenths.
        r.flavour = RiffFlavour::hitPush;
        const auto push = generateRiffBars (r);
        for (const auto& bar : push)
        {
            CHECK ((bar[0] == '#' || bar[0] == '*'));
            CHECK (quiet (bar, 3, 13));
            CHECK (bar.substr (13) == "[==");
        }
    }
}

TEST_CASE ("0.96.0: SPLIT CHUG's band: the bass on the hits or held through the gaps, the kick with the hits or a plain backbeat", "[sections][types]")
{
    ComposerState s;
    applySectionType (s, "Verse", "SPLIT CHUG");
    s.lane (ComposerLayer::guitar).approach = 2;   // (Call Me's hit and push)
    std::vector<ChordSpan> spans;
    for (int i = 0; i < 4; ++i)
        spans.push_back ({ i * 4.0, i * 4.0 + 4.0, Chord { i % 2 == 0 ? 4 : 2, i % 2 == 0 ? Chord::Type::minor : Chord::Type::major }, Key {} });
    ComposerContext c;
    const auto starts = [] (const LineIdea& idea, std::function<bool (const LineNote&)> keep)
    {
        std::set<int> at;
        for (const auto& n : idea.notes)
            if (! n.keyswitch && keep (n))
                at.insert (juce::roundToInt (n.beat * 4.0));
        return at;
    };
    const auto all = [] (const LineNote&) { return true; };
    const auto kick = [] (const LineNote& n) { return n.pitch == 35 || n.pitch == 36; };
    const auto layers = composeLayers (s, spans, c);
    const auto guitar = starts (layers[(size_t) ComposerLayer::guitar], all);
    REQUIRE (guitar.size() >= 4);
    // Automatic: the bass doubles the hits, the kick catches them.
    const auto bass = starts (layers[(size_t) ComposerLayer::bass], all);
    const auto kicks = starts (layers[(size_t) ComposerLayer::drums], kick);
    auto bassOn = 0, kickOn = 0;
    for (const auto g : guitar)
    {
        bassOn += bass.count (g) > 0 ? 1 : 0;
        kickOn += kicks.count (g) > 0 ? 1 : 0;
    }
    CHECK (bassOn >= (int) guitar.size() * 9 / 10);
    CHECK (kickOn >= (int) guitar.size() * 8 / 10);
    // HOLDS THROUGH THE GAPS: the same starts, each note held to the next (a beat or more here).
    auto held = s;
    held.lane (ComposerLayer::bass).approach = 1;
    held.lane (ComposerLayer::drums).approach = 1;
    const auto heldLayers = composeLayers (held, spans, c);
    const auto& heldBass = heldLayers[(size_t) ComposerLayer::bass];
    REQUIRE_FALSE (heldBass.notes.empty());
    auto longest = 0.0;
    for (const auto& n : heldBass.notes)
        longest = juce::jmax (longest, n.length);
    CHECK (longest >= 2.0);
    // PLAIN BACKBEAT: kick on 1 and 3 of every bar, snare on 2 and 4, whatever the guitar does.
    const auto beatKicks = starts (heldLayers[(size_t) ComposerLayer::drums], kick);
    const auto snares = starts (heldLayers[(size_t) ComposerLayer::drums], [] (const LineNote& n) { return n.pitch == 38; });
    for (int bar = 0; bar < 4; ++bar)
    {
        CHECK (beatKicks.count (bar * 16) == 1);
        CHECK (beatKicks.count (bar * 16 + 8) == 1);
        CHECK (snares.count (bar * 16 + 4) == 1);
        CHECK (snares.count (bar * 16 + 12) == 1);
    }
    CHECK (beatKicks.size() == 8);
}

namespace
{
    // (0.97.0) Four bars of A minor, F, C, G (a chord a bar) in A minor, and a state typed CLEAN VERSE.
    std::vector<ChordSpan> cleanSpans (double start = 0.0)
    {
        Key aMinor;
        for (const auto& k : Key::all())
            if (k.minor && k.pitchClass() == 9)
                aMinor = k;
        const std::array<Chord, 4> chords { Chord { 9, Chord::Type::minor }, Chord { 5, Chord::Type::major },
                                            Chord { 0, Chord::Type::major }, Chord { 7, Chord::Type::major } };
        std::vector<ChordSpan> spans;
        for (int bar = 0; bar < 8; ++bar)
            spans.push_back ({ start + bar * 4.0, start + bar * 4.0 + 4.0, chords[(size_t) (bar % 4)], aMinor });
        return spans;
    }

    ComposerState cleanState (int guitarApproach)
    {
        ComposerState s;
        applySectionType (s, "Verse", "CLEAN VERSE");
        s.lane (ComposerLayer::guitar).approach = guitarApproach;
        return s;
    }

    bool inChord (const std::vector<ChordSpan>& spans, const LineNote& n)
    {
        const auto& span = spans[(size_t) juce::jlimit (0, (int) spans.size() - 1, (int) std::floor (n.beat / 4.0 + 1.0e-6))];
        const auto pcs = span.chord.pitchClasses();
        return std::find (pcs.begin(), pcs.end(), ((n.pitch % 12) + 12) % 12) != pcs.end();
    }
}

TEST_CASE ("0.97.0: CLEAN VERSE: a verse type with five clean guitar parts and the band's options", "[sections][types]")
{
    REQUIRE (isSectionTypeOf ("Verse", "CLEAN VERSE"));
    CHECK (sectionTypesFor ("Verse")[2].name == "CLEAN VERSE");
    CHECK_FALSE (isSectionTypeOf ("Chorus", "CLEAN VERSE"));
    const auto s = cleanState (-1);
    CHECK (sectionBand (s) == SectionType::Band::clean);
    const auto guitar = sectionApproaches (s, ComposerLayer::guitar);
    REQUIRE (guitar.size() == 5);
    CHECK (guitar[0] == "RINGING ARPEGGIO");
    CHECK (guitar[1] == "PICKED CHORD");
    CHECK (guitar[2] == "SPARSE CLEAN NOTES");
    CHECK (guitar[3] == "GUITARS OUT");
    CHECK (guitar[4] == "HELD CHORD & PULSE");
    CHECK (sectionApproaches (s, ComposerLayer::bass).size() == 5);   // (0.97.2: LOCKED WITH THE KICK)
    CHECK (sectionApproaches (s, ComposerLayer::drums).size() == 3);
    CHECK (sectionApproaches (s, ComposerLayer::chords).size() == 4);
    CHECK (sectionApproaches (s, ComposerLayer::lead).isEmpty());
    CHECK_FALSE (typedGuitarFlavour (s).has_value());   // (clean parts, not riff kinds)
}

TEST_CASE ("0.97.0: CLEAN VERSE's guitar parts", "[sections][types]")
{
    const ComposerContext c;
    for (const auto start : { 0.0, 32.0 })   // (a section after the song's start: the chords looked up by the idea's beats)
    {
        const auto spans = cleanSpans (start);
        // RINGING ARPEGGIO (Masterpiece): the low note on each bar's root, the top two the key's B and C over every chord.
        {
            const auto idea = composeLayers (cleanState (0), spans, c)[(size_t) ComposerLayer::guitar];
            REQUIRE (idea.notes.size() >= 8 * 8);
            std::set<int> tops;
            for (const auto& n : idea.notes)
            {
                if (n.pitch >= 55)
                    tops.insert (n.pitch % 12);
                if (std::abs (n.beat - std::round (n.beat / 4.0) * 4.0) < 1.0e-6)   // (each bar's first note: the root)
                    CHECK (n.pitch % 12 == spans[(size_t) std::round (n.beat / 4.0)].chord.root);
            }
            CHECK (tops.count (11) == 1);   // B: A minor's 2nd
            CHECK (tops.count (0) == 1);    // C: its 3rd
        }
        // PICKED CHORD (Circle With Me): one note every eighth, each in the chord sounding.
        {
            const auto idea = composeLayers (cleanState (1), spans, c)[(size_t) ComposerLayer::guitar];
            REQUIRE (idea.notes.size() == 8 * 8);
            for (size_t i = 0; i < idea.notes.size(); ++i)
            {
                CHECK (idea.notes[i].beat == Catch::Approx (i * 0.5));
                CHECK (inChord (spans, idea.notes[i]));
            }
        }
        // SPARSE CLEAN NOTES: two strikes a bar of one high chord tone; the eighth bar picked in eighths.
        {
            const auto idea = composeLayers (cleanState (2), spans, c)[(size_t) ComposerLayer::guitar];
            std::array<int, 8> perBar {};
            for (const auto& n : idea.notes)
            {
                ++perBar[(size_t) std::floor (n.beat / 4.0 + 1.0e-6)];
                CHECK (inChord (spans, n));
                CHECK ((n.pitch >= 64 && n.pitch <= 79));
            }
            for (int bar = 0; bar < 7; ++bar)
                CHECK (perBar[(size_t) bar] == 2);
            CHECK (perBar[7] == 8);
        }
        // GUITARS OUT: nothing.
        CHECK (composeLayers (cleanState (3), spans, c)[(size_t) ComposerLayer::guitar].notes.empty());
        // HELD CHORD & PULSE: a power chord (root, fifth, octave) struck on each chord, held to the next.
        {
            const auto idea = composeLayers (cleanState (4), spans, c)[(size_t) ComposerLayer::guitar];
            REQUIRE (idea.notes.size() == 8 * 3);
            for (size_t i = 0; i < idea.notes.size(); i += 3)
            {
                CHECK (idea.notes[i].beat == Catch::Approx ((double) (i / 3) * 4.0));
                std::vector<int> chord { idea.notes[i].pitch, idea.notes[i + 1].pitch, idea.notes[i + 2].pitch };
                std::sort (chord.begin(), chord.end());
                CHECK (chord[1] - chord[0] == 7);
                CHECK (chord[2] - chord[0] == 12);
                CHECK (chord[0] % 12 == spans[i / 3].chord.root);
                CHECK (idea.notes[i].length >= 3.5);
            }
        }
    }
}

TEST_CASE ("0.97.0: CLEAN VERSE's band follows the guitar's part when left automatic", "[sections][types]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();
    const auto layer = [] (const std::array<LineIdea, composerLayerCount>& all, ComposerLayer l) -> const LineIdea& { return all[(size_t) l]; };
    // Circle With Me: no drums, no keys, the bass in eighths changing an eighth after the chord.
    {
        const auto all = composeLayers (cleanState (1), spans, c);
        CHECK (layer (all, ComposerLayer::drums).notes.empty());
        CHECK (layer (all, ComposerLayer::chords).notes.empty());
        const auto& bass = layer (all, ComposerLayer::bass).notes;
        REQUIRE (bass.size() == 8 * 8);
        CHECK (bass[8].pitch % 12 == 9);    // (bar 2's first eighth: still A)
        CHECK (bass[9].pitch % 12 == 5);    // (then F)
    }
    // Crawling / In The End: the guitars out, the keys carrying it, half-time drums (kick on 1 and 3, the snare ghosted).
    {
        const auto all = composeLayers (cleanState (3), spans, c);
        CHECK (layer (all, ComposerLayer::guitar).notes.empty());
        CHECK_FALSE (layer (all, ComposerLayer::chords).notes.empty());
        std::set<int> kicks;
        for (const auto& n : layer (all, ComposerLayer::drums).notes)
        {
            if (n.pitch == 36 && n.beat < 4.0)
                kicks.insert (juce::roundToInt (n.beat * 4.0));
            if (n.pitch == 38)
                CHECK (n.velocity < 0.4f);
        }
        CHECK (kicks == std::set<int> { 0, 8 });
    }
    // Masterpiece: syncopated roots, the light groove, held pads.
    {
        const auto all = composeLayers (cleanState (0), spans, c);
        CHECK (layer (all, ComposerLayer::bass).style == "Clean: syncopated roots");
        CHECK (layer (all, ComposerLayer::drums).style == "Clean: light groove");
        CHECK (layer (all, ComposerLayer::chords).style == "Clean: held pads");
        for (const auto& n : layer (all, ComposerLayer::chords).notes)
            CHECK (inChord (spans, n));
    }
    // A choice wins over the automatic one; the melody plays as it does without the type.
    {
        auto s = cleanState (1);
        s.lane (ComposerLayer::drums).approach = 0;
        s.lane (ComposerLayer::chords).approach = 2;
        s.lane (ComposerLayer::bass).approach = 3;
        const auto all = composeLayers (s, spans, c);
        CHECK (layer (all, ComposerLayer::drums).style == "Clean: light groove");
        CHECK (layer (all, ComposerLayer::chords).style == "Clean: piano line");
        CHECK (layer (all, ComposerLayer::bass).style == "Clean: octave eighths");
        ComposerState plain;
        CHECK (layer (all, ComposerLayer::melody).notes == layer (composeLayers (plain, spans, c), ComposerLayer::melody).notes);
    }
}

TEST_CASE ("0.97.1: a lane the section type writes offers the type's options, each a different part", "[sections][types]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();
    const auto distinct = [] (const std::vector<LineIdea>& ideas)
    {
        std::set<juce::String> seen;
        for (const auto& idea : ideas)
        {
            juce::String key;
            for (const auto& n : idea.notes)
                key << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
            seen.insert (key);
        }
        return (int) seen.size();
    };
    // The owner's report: DRONING's bass listed one bassline seven times.
    ComposerState droning;
    applySectionType (droning, "Chorus", "DRONING");
    REQUIRE (sectionTypeWritesLane (ComposerLayer::bass, droning, c));
    CHECK_FALSE (sectionTypeWritesLane (ComposerLayer::guitar, droning, c));
    CHECK_FALSE (sectionTypeWritesLane (ComposerLayer::melody, droning, c));
    const auto bass = composerChoices (ComposerLayer::bass, droning, spans, c);
    REQUIRE (bass.size() == 7);   // (0.97.2: LOCKED WITH THE KICK last)
    CHECK (distinct (bass) == 7);
    for (const auto l : { ComposerLayer::chords, ComposerLayer::drums })   // (0.97.2: seven each)
    {
        const auto ideas = composerChoices (l, droning, spans, c);
        CHECK (ideas.size() == 7);
        CHECK (distinct (ideas) == 7);   // (the keys' SILENT is the one with no notes)
    }
    CHECK (distinct (composerChoices (ComposerLayer::lead, droning, spans, c)) == 3);
    // WALKING INTO THE CHANGES: the root held, then two eighths stepping into the next chord's root.
    {
        auto s = droning;
        s.lane (ComposerLayer::bass).approach = 4;
        const auto idea = composeLayers (s, spans, c)[(size_t) ComposerLayer::bass];
        CHECK (idea.style == "Droning: walking into the changes");
        auto walks = 0;
        for (size_t i = 0; i + 2 < idea.notes.size(); ++i)
            if (idea.notes[i + 1].length < 0.6 && idea.notes[i + 2].length < 0.6 && i + 3 < idea.notes.size())
            {
                const auto target = idea.notes[i + 3].pitch;
                CHECK (std::abs (idea.notes[i + 2].pitch - target) <= 2);   // (a scale step into it)
                CHECK (std::abs (idea.notes[i + 1].pitch - target) <= 4);
                ++walks;
            }
        CHECK (walks >= 2);
    }
    // The other types: SPLIT CHUG's and STACCATO CHUG's bass and drums, CLEAN VERSE's guitar.
    ComposerState split;
    applySectionType (split, "Verse", "SPLIT CHUG");
    CHECK (composerChoices (ComposerLayer::drums, split, spans, c).size() == 2);
    CHECK (distinct (composerChoices (ComposerLayer::drums, split, spans, c)) == 2);
    CHECK_FALSE (sectionTypeWritesLane (ComposerLayer::guitar, split, c));   // (its guitar has its riff kinds and styles)
    const auto clean = cleanState (-1);
    CHECK (distinct (composerChoices (ComposerLayer::guitar, clean, spans, c)) == 5);
    CHECK (distinct (composerChoices (ComposerLayer::bass, clean, spans, c)) == 5);
    // SYNC on: the lanes follow the sync lane, the styles back.
    auto synced = droning;
    synced.syncLane = (int) ComposerLayer::guitar;
    CHECK_FALSE (sectionTypeWritesLane (ComposerLayer::bass, synced, c));
    // STANDARD: the styles, as before.
    ComposerState plain;
    CHECK (composerChoices (ComposerLayer::bass, plain, spans, c).size() == (size_t) composerStyleCount (ComposerLayer::bass));
}


TEST_CASE ("0.97.2: LOCKED WITH THE KICK on the bass under every type that writes it", "[sections][types]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();
    const auto starts = [] (const LineIdea& idea, bool kicksOnly)
    {
        std::set<int> at;
        for (const auto& n : idea.notes)
            if (! n.keyswitch && (! kicksOnly || n.pitch == 35 || n.pitch == 36))
                at.insert (juce::roundToInt (n.beat * 8.0));
        return at;
    };
    for (const auto& [name, type] : { std::pair<const char*, const char*> { "Chorus", "DRONING" }, { "Verse", "STACCATO CHUG" },
                                      { "Verse", "SPLIT CHUG" }, { "Verse", "CLEAN VERSE" }, { "Breakdown", "HEAVY CHUG" },
                                      { "Chorus", "DRIVING" } })   // (0.98.0)
    {
        INFO (type);
        ComposerState s;
        applySectionType (s, name, type);
        const auto names = sectionApproaches (s, ComposerLayer::bass);
        REQUIRE_FALSE (names.isEmpty());
        CHECK (names[names.size() - 1] == "LOCKED WITH THE KICK");
        s.lane (ComposerLayer::bass).approach = names.size() - 1;
        if (juce::String (type) == "CLEAN VERSE")
            s.lane (ComposerLayer::drums).approach = 0;   // (the light groove: kicks to lock to)
        const auto all = composeLayers (s, spans, c);
        const auto& bass = all[(size_t) ComposerLayer::bass];
        CHECK (bass.style == "Locked with the kick");
        const auto kicks = starts (all[(size_t) ComposerLayer::drums], true);
        REQUIRE_FALSE (kicks.empty());
        CHECK (starts (bass, false) == kicks);
    }
    // No drums in the part (CLEAN VERSE's NO DRUMS): the roots on every beat.
    ComposerState quiet;
    applySectionType (quiet, "Verse", "CLEAN VERSE");
    quiet.lane (ComposerLayer::drums).approach = 2;
    quiet.lane (ComposerLayer::bass).approach = 4;
    CHECK (composeLayers (quiet, spans, c)[(size_t) ComposerLayer::bass].notes.size() == 8 * 4);
}

TEST_CASE ("0.98.0: the DRIVING chorus's options for every lane", "[sections][types]")
{
    ComposerState s;
    applySectionType (s, "Chorus", "DRIVING");
    const ComposerContext c;
    CHECK (sectionTypeWritesLane (ComposerLayer::bass, s, c));
    CHECK (sectionTypeWritesLane (ComposerLayer::drums, s, c));
    CHECK (sectionTypeWritesLane (ComposerLayer::chords, s, c));
    CHECK (sectionTypeWritesLane (ComposerLayer::lead, s, c));
    CHECK_FALSE (sectionTypeWritesLane (ComposerLayer::melody, s, c));
    CHECK (sectionApproaches (s, ComposerLayer::guitar).size() == 6);
    CHECK (sectionApproaches (s, ComposerLayer::bass).size() == 5);
    CHECK (sectionApproaches (s, ComposerLayer::drums).size() == 5);
    CHECK (sectionApproaches (s, ComposerLayer::chords).size() == 4);
    CHECK (sectionApproaches (s, ComposerLayer::lead).size() == 3);
    // Every option of every lane a different part.
    s.lane (ComposerLayer::lead).on = true;
    const auto spans = cleanSpans();
    for (const auto l : { ComposerLayer::guitar, ComposerLayer::bass, ComposerLayer::drums, ComposerLayer::chords, ComposerLayer::lead })
    {
        INFO (composerLayerName (l));
        const auto names = sectionApproaches (s, l);
        std::set<std::vector<std::tuple<int, int, int>>> parts;
        for (int i = 0; i < names.size(); ++i)
        {
            auto each = s;
            each.lane (l).approach = i;
            std::vector<std::tuple<int, int, int>> notes;
            for (const auto& n : composeLayers (each, spans, c)[(size_t) l].notes)
                notes.emplace_back (juce::roundToInt (n.beat * 96.0), n.pitch, juce::roundToInt (n.length * 96.0));
            parts.insert (notes);
        }
        CHECK ((int) parts.size() == names.size());
    }
}

TEST_CASE ("0.98.0: the DRIVING chorus's guitar shapes", "[sections][types]")
{
    for (juce::uint32 seed = 1; seed < 30; ++seed)
    {
        RiffRecipe r;
        r.flavour = RiffFlavour::drivingEighths;
        r.section = DrumSection::chorus;
        r.bars = 4;
        r.seed = seed;
        const auto barsOf = [&r] (int approach) { r.approach = approach; return generateRiffBars (r, 16); };
        const auto struck = [] (const std::string& bar) { int n = 0; for (int i = 0; i < 16; i += 2) n += bar[(size_t) i] != '=' && bar[(size_t) i] != '-'; return n; };
        // STRAIGHT EIGHTHS: every eighth struck.
        for (const auto& bar : barsOf (0))
            CHECK (struck (bar) == 8);
        // SYNCOPATED THIRD BAR: the third bar rung across the beats (fewer hits), the others in eighths.
        {
            const auto bars = barsOf (1);
            REQUIRE (bars.size() == 4);
            CHECK (struck (bars[0]) == 8);
            CHECK (struck (bars[2]) <= 4);
            CHECK (bars[2].find ('=') != std::string::npos);
        }
        // OPEN AND MUTED IN TURN: open bars, muted bars.
        {
            const auto bars = barsOf (2);
            CHECK (std::string ("#o").find (bars[0][0]) != std::string::npos);
            CHECK (std::string ("*x").find (bars[1][0]) != std::string::npos);
            CHECK (std::string ("#o").find (bars[2][0]) != std::string::npos);
            CHECK (std::string ("*x").find (bars[3][0]) != std::string::npos);
        }
        // SLIDES INTO THE CHANGES: the second and fourth bars end in two chords stepping up into the next.
        {
            const auto bars = barsOf (3);
            CHECK (bars[1].substr (14) == "[]");
            CHECK (bars[3].substr (14) == "[]");
        }
        // STRUMMED FULL CHORDS, MOVING POWER CHORDS.
        for (const auto& bar : barsOf (4))
            CHECK (bar.find ('%') != std::string::npos);
        auto moved = false;
        for (const auto& bar : barsOf (5))
            moved = moved || bar.find_first_of ("123456789") != std::string::npos;
        CHECK (moved);
    }
}

TEST_CASE ("0.98.0: the DRIVING chorus's band", "[sections][types]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();
    ComposerState s;
    applySectionType (s, "Chorus", "DRIVING");
    s.lane (ComposerLayer::lead).on = true;
    const auto with = [&s] (ComposerLayer l, int approach) { auto each = s; each.lane (l).approach = approach; return each; };
    const auto at = [] (const LineIdea& idea, double beat, int pitch)
    {
        return std::any_of (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return n.pitch == pitch && std::abs (n.beat - beat) < 1.0e-6; });
    };
    // ROOT EIGHTHS: eight a bar, each the bar's root.
    {
        const auto bass = composeLayers (with (ComposerLayer::bass, 0), spans, c)[(size_t) ComposerLayer::bass];
        REQUIRE (bass.notes.size() == 8 * 8);
        for (const auto& n : bass.notes)
            CHECK (n.pitch % 12 == spans[(size_t) std::floor (n.beat / 4.0 + 1.0e-6)].chord.root);
    }
    // WALKING INTO THE CHANGES: the last eighth before each change a scale step under the next root.
    {
        const auto bass = composeLayers (with (ComposerLayer::bass, 2), spans, c)[(size_t) ComposerLayer::bass];
        REQUIRE (bass.notes.size() == 8 * 8);
        auto walked = 0;
        for (const auto& n : bass.notes)
            if (std::abs (n.beat - (std::floor (n.beat / 4.0) * 4.0 + 3.5)) < 1.0e-6)
            {
                const auto next = spans[(size_t) ((int) std::floor (n.beat / 4.0) + 1) % spans.size()].chord.root;
                const auto gap = ((next - n.pitch % 12) + 12) % 12;
                CHECK ((gap == 1 || gap == 2));
                ++walked;
            }
        CHECK (walked == 8);
    }
    // HALF TIME, KICK ON THE EIGHTHS: a kick on every eighth of the first bar, the snare on 3.
    {
        const auto drums = composeLayers (with (ComposerLayer::drums, 1), spans, c)[(size_t) ComposerLayer::drums];
        for (int e = 0; e < 8; ++e)
            CHECK (at (drums, e * 0.5, GmDrum::kick));
        CHECK (at (drums, 2.0, GmDrum::snare));
        CHECK_FALSE (at (drums, 1.0, GmDrum::snare));
    }
    // CYMBALS ON THE BEATS, KICKS BETWEEN.
    {
        const auto drums = composeLayers (with (ComposerLayer::drums, 2), spans, c)[(size_t) ComposerLayer::drums];
        for (const auto beat : { 4.0, 5.0, 6.0, 7.0 })
            CHECK (at (drums, beat, GmDrum::crash2));
        for (const auto beat : { 4.5, 5.5, 6.5, 7.5 })
            CHECK (at (drums, beat, GmDrum::kick));
        CHECK_FALSE (at (drums, 5.0, GmDrum::kick));
    }
    // A fill on the last beat of every four bars.
    {
        const auto drums = composeLayers (with (ComposerLayer::drums, 0), spans, c)[(size_t) ComposerLayer::drums];
        CHECK (at (drums, 15.5, GmDrum::tomHigh));
        CHECK (at (drums, 31.75, GmDrum::tomFloor));
        CHECK (at (drums, 16.0, GmDrum::crash));
    }
    // The keys: answering in bars 3 and 4 only; silent.
    {
        const auto keys = composeLayers (with (ComposerLayer::chords, 1), spans, c)[(size_t) ComposerLayer::chords];
        REQUIRE_FALSE (keys.notes.empty());
        for (const auto& n : keys.notes)
            CHECK ((int) std::floor (n.beat / 4.0 + 1.0e-6) % 4 >= 2);
        CHECK (composeLayers (with (ComposerLayer::chords, 3), spans, c)[(size_t) ComposerLayer::chords].notes.empty());
    }
    // The lead's squeal: a pinch harmonic on the last beat of every four bars, bent.
    {
        const auto lead = composeLayers (with (ComposerLayer::lead, 1), spans, c)[(size_t) ComposerLayer::lead];
        auto squeals = 0;
        for (const auto& n : lead.notes)
            if (GuitarVelocity::midiOf (n.velocity) >= 127)
            {
                CHECK (std::fmod (n.beat, 16.0) == Catch::Approx (15.0));
                CHECK (n.effect == LineNote::Effect::bend);
                ++squeals;
            }
        CHECK (squeals == 2);
    }
    // AUTOMATIC: the band still writes its part (not the lanes' usual styles).
    {
        const auto all = composeLayers (s, spans, c);
        CHECK (all[(size_t) ComposerLayer::bass].style.startsWith ("Driving"));
        CHECK (all[(size_t) ComposerLayer::drums].style.startsWith ("Driving"));
    }
}
