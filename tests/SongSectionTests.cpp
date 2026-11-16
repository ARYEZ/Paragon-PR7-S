// The SECTIONS row (0.90.0, the owner's song engine): the song as blocks end to end from bar 1, each a section's
// name and a length in bars; rearranging the blocks moves the music in them (the owner's: "moving blocks moves the
// music in that part with it").

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include <set>
#include <map>
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
#include "ui/SectionEditor.h"

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
    CHECK (studio.getSongSections().shown);   // (0.103.0: there by default)
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
    REQUIRE (chorus.size() == 11 + 23 + 11);   // (0.140.0: 23 more after them) (0.99.0: SYNCOPATED DRIVE between them; 0.115.0 and 0.116.0: four more each after them)
    CHECK (chorus[0].name == "DRIVING");
    CHECK (chorus[0].flavour == RiffFlavour::drivingEighths);
    CHECK (chorus[2].name == "DRONING");
    CHECK (chorus[2].flavour == RiffFlavour::droning);
    // A name plays as its kind: a Hook as a chorus, a Pre-Chorus as a verse (0.102.0: after its own types).
    CHECK (sectionTypesFor ("Hook").size() == sectionTypesFor ("Chorus").size());
    CHECK (sectionTypesFor ("Pre-Chorus").back().name == sectionTypesFor ("Verse").back().name);
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
    REQUIRE (names.size() == 7);   // (0.96.0: BURST & SILENCE moved to SPLIT CHUG; 0.100.0: GALLOP & SQUEAL)
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
    REQUIRE (guitar.size() == 10);   // (0.100.0: PEDAL HOOK, MUTED DYAD WALK; 0.101.0: the still shapes)
    CHECK (guitar[0] == "RINGING ARPEGGIO");
    CHECK (guitar[1] == "PICKED CHORD");
    CHECK (guitar[2] == "SPARSE CLEAN NOTES");
    CHECK (guitar[3] == "GUITARS OUT");
    CHECK (guitar[4] == "HELD CHORD & PULSE");
    CHECK (sectionApproaches (s, ComposerLayer::bass).size() == 6);   // (0.97.2: LOCKED WITH THE KICK; 0.100.0: OUT, THEN IN HALFWAY)
    CHECK (baseSectionApproaches (s, ComposerLayer::drums).size() == 6);   // (0.100.0: OUT, THEN IN HALFWAY; 0.101.0: HALF-TIME HATS, TOM GROOVE)
    CHECK (baseSectionApproaches (s, ComposerLayer::chords).size() == 5);   // (0.100.0: OCTAVE DRONE)
    CHECK (sectionApproaches (s, ComposerLayer::lead).size() == 2);   // (0.101.0: the swells; automatic: the lead's own style)
    CHECK (sectionApproaches (s, ComposerLayer::melody).isEmpty());
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
        const auto n = (l == ComposerLayer::drums ? 8u : 7u) + (size_t) extraApproaches (l).size();   // (0.116.0: THE GROOVE CARRIES ON; 0.145.0: the extras)
        CHECK (ideas.size() == n);
        CHECK (distinct (ideas) == n);   // (the keys' SILENT is the one with no notes)
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
    const auto splitDrums = 2u + (size_t) extraApproaches (ComposerLayer::drums).size();   // (0.145.0: 2 + the extras)
    CHECK (composerChoices (ComposerLayer::drums, split, spans, c).size() == splitDrums);
    CHECK (distinct (composerChoices (ComposerLayer::drums, split, spans, c)) == (int) splitDrums);
    CHECK_FALSE (sectionTypeWritesLane (ComposerLayer::guitar, split, c));   // (its guitar has its riff kinds and styles)
    const auto clean = cleanState (-1);
    CHECK (distinct (composerChoices (ComposerLayer::guitar, clean, spans, c)) == 10);   // (0.100.0: seven; 0.101.0: ten)
    CHECK (distinct (composerChoices (ComposerLayer::bass, clean, spans, c)) == 6);     // (0.100.0: six)
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
                                      { "Chorus", "DRIVING" }, { "Chorus", "SYNCOPATED DRIVE" }, { "Verse", "MUTED DRIVE" } })   // (0.98.0, 0.99.0, 0.100.0)
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
    quiet.lane (ComposerLayer::bass).approach = 5;   // (0.100.0: LOCKED WITH THE KICK moved to 6th)
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
    CHECK (sectionApproaches (s, ComposerLayer::guitar).size() == 7);   // (0.117.0: the turnaround)
    CHECK (sectionApproaches (s, ComposerLayer::bass).size() == 6);   // (0.117.0: the dominant pedal)
    CHECK (baseSectionApproaches (s, ComposerLayer::drums).size() == 7);   // (0.116.0: two more)
    CHECK (baseSectionApproaches (s, ComposerLayer::chords).size() == 4);
    CHECK (sectionApproaches (s, ComposerLayer::lead).size() == 4);   // (0.117.0: the falling octaves)
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
            const auto all = composeLayers (each, spans, c);   // (kept: a loop over a temporary's member dangles; MSVC read it empty)
            for (const auto& n : all[(size_t) l].notes)
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

TEST_CASE ("0.99.0: SYNCOPATED DRIVE: a chorus type with its options", "[sections][types]")
{
    REQUIRE (isSectionTypeOf ("Chorus", "SYNCOPATED DRIVE"));
    CHECK (sectionTypesFor ("Chorus")[1].name == "SYNCOPATED DRIVE");
    CHECK_FALSE (isSectionTypeOf ("Verse", "SYNCOPATED DRIVE"));
    ComposerState s;
    applySectionType (s, "Chorus", "SYNCOPATED DRIVE");
    const ComposerContext c;
    for (const auto l : { ComposerLayer::bass, ComposerLayer::drums, ComposerLayer::chords, ComposerLayer::lead })
        CHECK (sectionTypeWritesLane (l, s, c));
    CHECK_FALSE (sectionTypeWritesLane (ComposerLayer::melody, s, c));
    CHECK (sectionApproaches (s, ComposerLayer::guitar).size() == 5);   // (0.117.0: the turnaround)
    CHECK (sectionApproaches (s, ComposerLayer::bass).size() == 5);   // (0.117.0: the dominant pedal)
    CHECK (baseSectionApproaches (s, ComposerLayer::drums).size() == 4);
    CHECK (baseSectionApproaches (s, ComposerLayer::chords).size() == 3);
    CHECK (sectionApproaches (s, ComposerLayer::lead).size() == 3);
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
            const auto all = composeLayers (each, spans, c);   // (kept: a loop over a temporary's member dangles; MSVC read it empty)
            for (const auto& n : all[(size_t) l].notes)
                notes.emplace_back (juce::roundToInt (n.beat * 96.0), n.pitch, juce::roundToInt (n.length * 96.0));
            parts.insert (notes);
        }
        CHECK ((int) parts.size() == names.size());
    }
}

TEST_CASE ("0.99.0: SYNCOPATED DRIVE's guitar shapes", "[sections][types]")
{
    const auto struck = [] (const std::string& bar) { int n = 0; for (int i = 0; i < 16; i += 2) n += bar[(size_t) i] != '=' && bar[(size_t) i] != '-'; return n; };
    std::set<std::string> automatic;
    for (juce::uint32 seed = 1; seed < 40; ++seed)
    {
        RiffRecipe r;
        r.flavour = RiffFlavour::syncopatedDrive;
        r.section = DrumSection::chorus;
        r.bars = 4;
        r.seed = seed;
        {
            const auto bars = generateRiffBars (r, 16);
            REQUIRE (bars.size() == 4);
            automatic.insert (bars[0].substr (0, 1) + (bars[0].find ('-') != std::string::npos ? "K" : "") + bars[3].substr (14));
        }
        const auto barsOf = [&r] (int approach) { r.approach = approach; return generateRiffBars (r, 16); };
        // STABS, THEN EIGHTHS: two bars of stabs with silence, then eighths.
        {
            const auto bars = barsOf (0);
            for (int b = 0; b < 2; ++b)
            {
                CHECK (bars[(size_t) b].find ('-') != std::string::npos);
                CHECK (struck (bars[(size_t) b]) <= 3);
            }
            CHECK (struck (bars[2]) == 8);
            CHECK (struck (bars[3]) == 8);
        }
        // SYNCOPATED THIRD BAR.
        CHECK (struck (barsOf (1)[2]) <= 4);
        // PUSHED CHANGES: every bar's last eighth the next chord.
        for (const auto& bar : barsOf (2))
            CHECK (bar[14] == '_');
        // STABS, THEN PUSHES.
        {
            const auto bars = barsOf (3);
            CHECK (bars[0].find ('-') != std::string::npos);
            CHECK (bars[3][14] == '_');
        }
    }
    CHECK (automatic.size() >= 3);   // (AUTOMATIC draws the shapes in turn)
    // The push plays the next chord's power chord on the bar's last eighth.
    {
        RiffRecipe r;
        r.flavour = RiffFlavour::syncopatedDrive;
        r.section = DrumSection::chorus;
        r.bars = 4;
        r.approach = 2;
        const auto spans = cleanSpans();
        const auto idea = generateRiff (r, spans, 4.0, GuitarTuning::all().front(), GuitarVelocities {});
        std::set<int> at35;
        for (const auto& n : idea.notes)
            if (! n.keyswitch && std::abs (n.beat - 3.5) < 1.0e-6)
                at35.insert (n.pitch % 12);
        CHECK (at35.count (5) == 1);   // F, the second bar's chord
        CHECK (at35.count (0) == 1);   // its fifth
        CHECK (at35.count (9) == 0);   // not A, the first bar's
    }
}

TEST_CASE ("0.99.0: SYNCOPATED DRIVE's band locks the bass and kick in the figure", "[sections][types]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();
    ComposerState s;
    applySectionType (s, "Chorus", "SYNCOPATED DRIVE");
    s.lane (ComposerLayer::lead).on = true;
    const auto with = [&s] (ComposerLayer l, int approach) { auto each = s; each.lane (l).approach = approach; return each; };
    const std::vector<double> figure { 0.0, 1.5, 3.0, 4.5, 5.5, 6.5, 7.5 };
    // The bass: 1, the and of 2, 4 held over the bar line, then the off-beats; each the chord's root there.
    const auto bass = composeLayers (with (ComposerLayer::bass, 0), spans, c)[(size_t) ComposerLayer::bass];
    REQUIRE (bass.notes.size() == 7 * 4);
    for (size_t i = 0; i < figure.size(); ++i)
    {
        CHECK (bass.notes[i].beat == Catch::Approx (figure[i]));
        CHECK (bass.notes[i].pitch % 12 == spans[(size_t) std::floor (figure[i] / 4.0)].chord.root);
    }
    CHECK (bass.notes[2].beat + bass.notes[2].length > 4.0);   // (tied over the bar line)
    // The drums: the kick on the same figure (the fill's last beat aside), the snare on 3 (half time) or 2 and 4.
    for (int approach = 0; approach < 2; ++approach)
    {
        const auto drums = composeLayers (with (ComposerLayer::drums, approach), spans, c)[(size_t) ComposerLayer::drums];
        std::set<int> kicks, snares;
        for (const auto& n : drums.notes)
            if (n.beat < 8.0 - 1.0e-6)
            {
                if (n.pitch == GmDrum::kick)
                    kicks.insert (juce::roundToInt (n.beat * 8.0));
                if (n.pitch == GmDrum::snare)
                    snares.insert (juce::roundToInt (n.beat * 8.0));
            }
        std::set<int> expected;
        for (const auto f : figure)
            expected.insert (juce::roundToInt (f * 8.0));
        CHECK (kicks == expected);
        if (approach == 0)
            CHECK (snares == std::set<int> { 16, 48 });
        else
            CHECK (snares == std::set<int> { 8, 24, 40, 56 });
    }
    // The keys' stabs on the figure.
    {
        const auto keys = composeLayers (with (ComposerLayer::chords, 1), spans, c)[(size_t) ComposerLayer::chords];
        std::set<int> starts;
        for (const auto& n : keys.notes)
            if (n.beat < 8.0 - 1.0e-6)
                starts.insert (juce::roundToInt (n.beat * 8.0));
        std::set<int> expected;
        for (const auto f : figure)
            expected.insert (juce::roundToInt (f * 8.0));
        CHECK (starts == expected);
    }
    // The lead: three notes going round in eighths (the fourth repeats the first) within a chord.
    {
        const auto lead = composeLayers (with (ComposerLayer::lead, 0), spans, c)[(size_t) ComposerLayer::lead];
        REQUIRE (lead.notes.size() >= 8);
        CHECK (lead.notes[0].pitch == lead.notes[3].pitch);
        CHECK (lead.notes[1].pitch == lead.notes[4].pitch);
        CHECK (lead.notes[0].pitch != lead.notes[1].pitch);
        CHECK (lead.notes[1].pitch != lead.notes[2].pitch);
    }
    // AUTOMATIC writes the band's own parts.
    const auto all = composeLayers (s, spans, c);
    CHECK ((all[(size_t) ComposerLayer::bass].style.startsWith ("Syncopated") || all[(size_t) ComposerLayer::bass].style.startsWith ("Driving")));
    CHECK (all[(size_t) ComposerLayer::drums].style.startsWith ("Syncopated"));
}

TEST_CASE ("0.100.0: MUTED DRIVE: a verse type of palm-muted eighths with its band", "[sections][types]")
{
    REQUIRE (isSectionTypeOf ("Verse", "MUTED DRIVE"));
    CHECK (sectionTypesFor ("Verse")[3].name == "MUTED DRIVE");
    ComposerState s;
    applySectionType (s, "Verse", "MUTED DRIVE");
    const ComposerContext c;
    for (const auto l : { ComposerLayer::bass, ComposerLayer::drums, ComposerLayer::chords, ComposerLayer::lead })
        CHECK (sectionTypeWritesLane (l, s, c));
    CHECK (sectionApproaches (s, ComposerLayer::guitar).size() == 3);
    CHECK (sectionApproaches (s, ComposerLayer::bass).size() == 4);
    CHECK (baseSectionApproaches (s, ComposerLayer::drums).size() == 4);   // (0.118.0: FOUR ON THE FLOOR)
    const auto struck = [] (const std::string& bar) { int n = 0; for (int i = 0; i < 16; i += 2) n += bar[(size_t) i] != '=' && bar[(size_t) i] != '-'; return n; };
    for (juce::uint32 seed = 1; seed < 30; ++seed)
    {
        RiffRecipe r;
        r.flavour = RiffFlavour::mutedDrive;
        r.section = DrumSection::verse;
        r.bars = 4;
        r.seed = seed;
        // AUTOMATIC: every bar in eighths, never an open power chord (the verse holds back; the walk aside).
        for (const auto& bar : generateRiffBars (r, 16))
        {
            CHECK (struck (bar) == 8);
            CHECK (bar.find ('#') == std::string::npos);
        }
        // SINGLE NOTES, THEN CHORDS: two bars of muted root notes, then muted power chords, the last rocking.
        r.approach = 0;
        const auto bars = generateRiffBars (r, 16);
        CHECK (bars[0].substr (0, 8) == "x=x=x=x=");
        CHECK (bars[2][0] == '*');   // (muted power chords, never the single notes again)
        CHECK (bars[3].find_first_of ("123456789") != std::string::npos);
    }
    // The band: closed hats in eighths and a plain backbeat; the bass on the roots in eighths.
    const auto spans = cleanSpans();
    auto plain = s;
    plain.lane (ComposerLayer::drums).approach = 0;
    plain.lane (ComposerLayer::bass).approach = 0;
    const auto all = composeLayers (plain, spans, c);
    std::set<int> snares;
    auto hats = 0;
    for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
    {
        if (n.beat < 4.0 - 1.0e-6 && n.pitch == GmDrum::snare)
            snares.insert (juce::roundToInt (n.beat * 2.0));
        hats += n.beat < 4.0 - 1.0e-6 && n.pitch == GmDrum::closedHat;
    }
    CHECK (snares == std::set<int> { 2, 6 });
    CHECK (hats == 8);
    CHECK (all[(size_t) ComposerLayer::bass].notes.size() == 8 * 8);
    CHECK (all[(size_t) ComposerLayer::chords].notes.empty());   // (the keys out)
}

TEST_CASE ("0.100.0: STACCATO CHUG's GALLOP & SQUEAL and the double-kick carpet", "[sections][types]")
{
    ComposerState s;
    applySectionType (s, "Verse", "STACCATO CHUG");
    const auto guitar = sectionApproaches (s, ComposerLayer::guitar);
    REQUIRE (guitar.size() == 7);
    CHECK (guitar[6] == "GALLOP & SQUEAL");
    const auto drums = baseSectionApproaches (s, ComposerLayer::drums);
    REQUIRE (drums.size() == 3);
    CHECK (drums[2] == "DOUBLE-KICK CARPET");
    RiffRecipe r;
    r.flavour = RiffFlavour::gallopSqueal;
    r.section = DrumSection::verse;
    r.bars = 4;
    for (juce::uint32 seed = 1; seed < 20; ++seed)
    {
        r.seed = seed;
        const auto bars = generateRiffBars (r, 16);
        REQUIRE (bars.size() == 4);
        CHECK (bars[0] == bars[2]);
        CHECK (bars[0].substr (0, 4) == "x-xx");   // (the gallop)
        CHECK (bars[1].find ('p') != std::string::npos);   // (the squeal)
        CHECK (bars[3].find ('K') != std::string::npos);   // (the turn on the seventh)
    }
    // The carpet: sixteenth kicks and a snare on every beat, the fourth bar's kicks with the guitar.
    const ComposerContext c;
    const auto spans = cleanSpans();
    s.lane (ComposerLayer::guitar).approach = 6;
    s.lane (ComposerLayer::drums).approach = 2;
    const auto all = composeLayers (s, spans, c);
    const auto& beat = all[(size_t) ComposerLayer::drums];
    std::set<int> kicks0, snares0, kicks3;
    for (const auto& n : beat.notes)
    {
        const auto bar = (int) std::floor (n.beat / 4.0 + 1.0e-6);
        if (bar == 0 && n.pitch == GmDrum::kick) kicks0.insert (juce::roundToInt (n.beat * 4.0));
        if (bar == 0 && n.pitch == GmDrum::snare) snares0.insert (juce::roundToInt (n.beat));
        if (bar == 3 && n.pitch == GmDrum::kick) kicks3.insert (juce::roundToInt (n.beat * 4.0));
    }
    CHECK (kicks0.size() == 16);
    CHECK (snares0 == std::set<int> { 0, 1, 2, 3 });
    CHECK (kicks3.size() < 16);
    CHECK_FALSE (kicks3.empty());
}

TEST_CASE ("0.100.0: CLEAN VERSE's PEDAL HOOK, MUTED DYAD WALK, OCTAVE DRONE and the band in halfway", "[sections][types]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    // PEDAL HOOK: the key's A in eighths under every chord; the hook on E, F and D the same over every chord.
    {
        auto s = cleanState (5);
        const auto all = composeLayers (s, spans, c);
        const auto& idea = all[(size_t) ComposerLayer::guitar];
        std::set<int> low, high;
        for (const auto& n : idea.notes)
            (n.pitch < 60 ? low : high).insert (n.pitch % 12);
        CHECK (low == std::set<int> { 9 });
        CHECK (high == std::set<int> { 2, 4, 5 });
    }
    // MUTED DYAD WALK: two muted notes every eighth, the lower the chord's root.
    {
        auto s = cleanState (6);
        const auto all = composeLayers (s, spans, c);
        const auto& idea = all[(size_t) ComposerLayer::guitar];
        REQUIRE (idea.notes.size() == 8 * 8 * 2);
        for (size_t i = 0; i < idea.notes.size(); i += 2)
        {
            const auto lowNote = juce::jmin (idea.notes[i].pitch, idea.notes[i + 1].pitch);
            CHECK (lowNote % 12 == spans[(size_t) std::floor (idea.notes[i].beat / 4.0 + 1.0e-6)].chord.root);
            CHECK (GuitarVelocity::midiOf (idea.notes[i].velocity) < 60);
        }
    }
    // OCTAVE DRONE: A and its octave held two bars at a time.
    {
        auto s = cleanState (0);
        s.lane (ComposerLayer::chords).approach = 4;
        const auto all = composeLayers (s, spans, c);
        const auto& keys = all[(size_t) ComposerLayer::chords];
        REQUIRE (keys.notes.size() == 4 * 2);
        for (const auto& n : keys.notes)
        {
            CHECK (n.pitch % 12 == 9);
            CHECK (n.length > 7.5);
        }
    }
    // OUT, THEN IN HALFWAY: nothing in the first four bars, the parts from bar 5.
    {
        auto s = cleanState (0);
        s.lane (ComposerLayer::bass).approach = 4;
        s.lane (ComposerLayer::drums).approach = 3;
        const auto all = composeLayers (s, spans, c);
        for (const auto l : { ComposerLayer::bass, ComposerLayer::drums })
        {
            const auto& idea = all[(size_t) l];
            REQUIRE_FALSE (idea.notes.empty());
            for (const auto& n : idea.notes)
                CHECK (n.beat >= 16.0 - 1.0e-6);
        }
    }
}


TEST_CASE ("0.101.0: CLEAN VERSE's still shape, swell melody and drums (Circle With Me)", "[sections][types]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    // STILL SHAPE IN THREES: A, C, E picked in threes over every eighth, whatever the chord; D (the step under E)
    // only on the seventh eighth of every fourth bar.
    {
        auto s = cleanState (7);
        const auto all = composeLayers (s, spans, c);
        const auto& idea = all[(size_t) ComposerLayer::guitar];
        REQUIRE (idea.notes.size() == 8 * 8);
        std::set<int> pitches;
        for (size_t i = 0; i < idea.notes.size(); ++i)
        {
            const auto& n = idea.notes[i];
            CHECK (n.beat == Catch::Approx (0.5 * (double) i));
            CHECK (GuitarVelocity::midiOf (n.velocity) < 60);
            pitches.insert (n.pitch);
            const auto bar = (int) i / 8, inBar = (int) i % 8;
            if (bar % 4 == 3 && inBar == 6)
                CHECK (n.pitch == 62);
            else
                CHECK (n.pitch == std::array<int, 3> { 57, 60, 64 }[(i % 16) % 3]);
        }
        CHECK (pitches == std::set<int> { 57, 60, 62, 64 });
    }
    // SIXTEENTHS: the same notes, twice as many, starting again every bar.
    {
        auto s = cleanState (8);
        const auto all = composeLayers (s, spans, c);
        const auto& idea = all[(size_t) ComposerLayer::guitar];
        REQUIRE (idea.notes.size() == 8 * 16);
        for (int bar = 0; bar < 8; ++bar)
            CHECK (idea.notes[(size_t) bar * 16].pitch == 57);
    }
    // STILL CLUSTER: E, F, C (A minor's fifth, sixth, third) picked middle, top, low; the cycle's last eighth A under.
    {
        auto s = cleanState (9);
        const auto all = composeLayers (s, spans, c);
        const auto& idea = all[(size_t) ComposerLayer::guitar];
        REQUIRE (idea.notes.size() == 8 * 8);
        for (size_t i = 0; i < idea.notes.size(); ++i)
        {
            if (i % 16 == 15)
                CHECK (idea.notes[i].pitch == 57);
            else
                CHECK (idea.notes[i].pitch == std::array<int, 3> { 64, 65, 60 }[(i % 16) % 3]);
        }
    }
    // Left on AUTOMATIC under a still shape, the bass changes late in eighths, the drums take the half-time hats,
    // the keys stay out (as the song).
    {
        auto s = cleanState (7);
        const auto all = composeLayers (s, spans, c);
        CHECK (all[(size_t) ComposerLayer::drums].style == "Clean: half-time hats");
        CHECK (all[(size_t) ComposerLayer::bass].style == "Clean: eighths, changing late");
        CHECK (all[(size_t) ComposerLayer::chords].notes.empty());
    }
    // HALF-TIME HATS: the snare on beat 3 of every bar; TOM GROOVE: the hat's foot on every beat, toms, the snare on 4.
    {
        auto s = cleanState (0);
        s.lane (ComposerLayer::drums).approach = 4;
        auto all = composeLayers (s, spans, c);
        int snares = 0;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.pitch == 38)
            {
                ++snares;
                CHECK (std::fmod (n.beat, 4.0) == Catch::Approx (2.0));
            }
        CHECK (snares == 8);
        s.lane (ComposerLayer::drums).approach = 5;
        all = composeLayers (s, spans, c);
        int feet = 0, toms = 0, loud = 0;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
        {
            feet += n.pitch == 44 ? 1 : 0;
            toms += n.pitch == 43 || n.pitch == 45 ? 1 : 0;
            if (n.pitch == 38 && n.velocity > 0.8f)
            {
                ++loud;
                CHECK (std::fmod (n.beat, 4.0) == Catch::Approx (3.0));
            }
        }
        CHECK (feet == 8 * 4);
        CHECK (toms >= 8 * 3);
        CHECK (loud == 8);
    }
    // The lead: AUTOMATIC plays its own style as before; SWELL MELODY steps down the scale from the third, every
    // note a swell, longer at first; SWELL, THEN WARBLE picks the long ones again with quarter-tone bends.
    {
        auto plainState = cleanState (7);
        const auto automatic = composeLayers (plainState, spans, c)[(size_t) ComposerLayer::lead];
        CHECK_FALSE (automatic.style.startsWith ("Clean"));
        auto s = cleanState (7);
        s.lane (ComposerLayer::lead).approach = 0;
        const auto all = composeLayers (s, spans, c);
        const auto& lead = all[(size_t) ComposerLayer::lead];
        REQUIRE (lead.notes.size() == 9);
        CHECK (lead.notes.front().pitch % 12 == 0);   // (C, A minor's third)
        CHECK (lead.notes.front().length > lead.notes.back().length * 1.5);
        for (size_t i = 1; i < lead.notes.size(); ++i)
            if (i != 3)   // (the second time round starts on the third again)
                CHECK (lead.notes[i].pitch < lead.notes[i - 1].pitch);
        for (const auto& n : lead.notes)
        {
            CHECK (n.effect == LineNote::Effect::swell);
            const auto pc = n.pitch % 12;
            CHECK ((pc == 9 || pc == 11 || pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7));
        }
        // The swells become expression ramps from nothing.
        const auto controllers = Studio::effectControllers (lead.notes, 2);
        int fromNothing = 0;
        for (const auto& k : controllers)
            if (k.type == Controller::Type::cc && k.number == 11 && k.value == 0)
                ++fromNothing;
        CHECK (fromNothing == 9);
        s.lane (ComposerLayer::lead).approach = 1;
        const auto warbled = composeLayers (s, spans, c);
        int bends = 0;
        for (const auto& n : warbled[(size_t) ComposerLayer::lead].notes)
            if (n.effect == LineNote::Effect::bend)
            {
                ++bends;
                CHECK (n.amount == Catch::Approx (0.5f));
            }
        CHECK (bends >= 4);
    }
}


TEST_CASE ("0.102.0: a pre-chorus's own types, every lane's options a different part", "[sections][types]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> names { "BUILD & STOP", "GUITARS OUT, BASS CLIMBS", "CYCLE IN QUARTERS",
                                            "CHUGS UNDER THE SHAPE", "STABS & GAPS", "RISING BASS" };
    // A pre-chorus lists them first, then the verse's types (a block typed as a verse keeps its type); a verse hasn't them.
    const auto pre = sectionTypesFor ("Pre-Chorus 2");
    REQUIRE (pre.size() == names.size() + 6 + 7 + 6 + sectionTypesFor ("Verse").size());   // (0.123.0: six more after them)
    for (size_t i = 0; i < names.size(); ++i)
        CHECK (pre[i].name == names[i]);
    CHECK (sectionTypeNamed ("Pre-Chorus", "STACCATO CHUG").has_value());
    CHECK_FALSE (sectionTypeNamed ("Verse", "BUILD & STOP").has_value());

    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (const auto& name : names)
    {
        ComposerState s;
        applySectionType (s, "Pre-Chorus", name);
        CHECK (s.sectionType == name);
        CHECK (sectionBand (s) == SectionType::Band::preChorus);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
                seen.insert (key (idea));
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
    }
}

TEST_CASE ("0.102.0: the pre-chorus types' shapes", "[sections][types]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (8 bars of 4/4: 32 beats)
    const auto typed = [] (const juce::String& name)
    {
        ComposerState s;
        applySectionType (s, "Pre-Chorus", name);
        return s;
    };
    // BUILD & STOP: the guitar, bass and keys silent on the last beat; the drums' roll carries on through it.
    {
        auto s = typed ("BUILD & STOP");
        auto all = composeLayers (s, spans, c);
        for (const auto l : { ComposerLayer::guitar, ComposerLayer::bass, ComposerLayer::chords, ComposerLayer::drums })
        {
            const auto& idea = all[(size_t) l];
            REQUIRE_FALSE (idea.notes.empty());
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 31.0 + 1.0e-6);
        }
        // the keys hold the key's fifth (E) over every chord
        for (const auto& n : all[(size_t) ComposerLayer::chords].notes)
            CHECK (n.pitch % 12 == 4);
        s.lane (ComposerLayer::drums).approach = 1;
        all = composeLayers (s, spans, c);
        auto late = 0;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            late += n.beat > 31.0 ? 1 : 0;
        CHECK (late >= 3);
        // ROOTS, DYADS, CHORDS: single notes in the first half, two at once after, then three; half a bar of silence.
        s.lane (ComposerLayer::guitar).approach = 1;
        all = composeLayers (s, spans, c);
        std::map<int, int> together;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
        {
            ++together[juce::roundToInt (n.beat * 96.0)];
            CHECK (n.beat < 30.0);
        }
        CHECK (together[0] == 1);
        CHECK (together[juce::roundToInt (20.0 * 96.0)] == 2);
        CHECK (together[juce::roundToInt (28.0 * 96.0)] == 3);
    }
    // CYCLE IN QUARTERS: the lead cycles C, E, F (A minor's third, fifth, sixth) in quarters; the last two bars a stop:
    // one hit from the band at beat 24, the lead's one held note, nothing else.
    {
        auto s = typed ("CYCLE IN QUARTERS");
        s.lane (ComposerLayer::lead).on = true;
        const auto all = composeLayers (s, spans, c);
        const auto& lead = all[(size_t) ComposerLayer::lead];
        REQUIRE (lead.notes.size() >= 3);
        CHECK (lead.notes[0].pitch % 12 == 0);
        CHECK (lead.notes[1].pitch % 12 == 4);
        CHECK (lead.notes[2].pitch % 12 == 5);
        CHECK (lead.notes[1].beat == Catch::Approx (1.0));
        int heldInStop = 0;
        for (const auto& n : lead.notes)
            if (n.beat >= 24.0 - 1.0e-6)
            {
                ++heldInStop;
                CHECK (n.beat == Catch::Approx (24.0));
                CHECK (n.length > 7.0);
            }
        CHECK (heldInStop == 1);
        for (const auto l : { ComposerLayer::guitar, ComposerLayer::bass, ComposerLayer::drums })
        {
            bool hitAtStop = false;
            for (const auto& n : all[(size_t) l].notes)
            {
                if (n.beat >= 24.0 - 1.0e-6)
                    CHECK (n.beat == Catch::Approx (24.0));
                else
                    CHECK (n.beat + n.length <= 24.0 + 1.0e-6);
                hitAtStop = hitAtStop || std::abs (n.beat - 24.0) < 1.0e-6;
            }
            CHECK (hitAtStop);
        }
        // the guitar's still shape: A, C, E over every chord before the stop
        std::set<int> pcs;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat < 24.0 - 1.0e-6)
                pcs.insert (n.pitch % 12);
        CHECK (pcs == std::set<int> { 9, 0, 4, 2 });
    }
    // RISING BASS: the bass climbs A, B, C, D... a scale step a bar, in sixteenths, whatever the chords.
    {
        const auto s = typed ("RISING BASS");
        const auto all = composeLayers (s, spans, c);
        const auto& bass = all[(size_t) ComposerLayer::bass];
        REQUIRE (bass.notes.size() == 8 * 16);
        const std::array<int, 8> climb { 9, 11, 0, 2, 4, 5, 7, 9 };
        for (int bar = 0; bar < 8; ++bar)
            CHECK (bass.notes[(size_t) bar * 16].pitch % 12 == climb[(size_t) bar]);
    }
    // STABS & GAPS: the guitar's hits on sixteenths 0-3 and 14-15 of the first bar (6-11 too in the second); the
    // toms on every hit; the bass doubles them.
    {
        const auto s = typed ("STABS & GAPS");
        const auto all = composeLayers (s, spans, c);
        std::set<int> places;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat < 8.0 - 1.0e-6)
                places.insert (juce::roundToInt (n.beat * 4.0));
        CHECK (places == std::set<int> { 0, 1, 2, 3, 14, 15, 16, 17, 18, 19, 22, 23, 24, 25, 26, 27, 30, 31 });
        int toms = 0;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            toms += n.pitch == 45 || n.pitch == 43 ? 1 : 0;
        CHECK (toms == 8 * 6 + 4 * 6);
        std::set<int> bassPlaces;
        for (const auto& n : all[(size_t) ComposerLayer::bass].notes)
            if (n.beat < 8.0 - 1.0e-6)
                bassPlaces.insert (juce::roundToInt (n.beat * 4.0));
        CHECK (bassPlaces == places);
    }
    // CHUGS UNDER THE SHAPE: the kick on every chug; the lead the verse's still cluster an octave up.
    {
        auto s = typed ("CHUGS UNDER THE SHAPE");
        const auto all = composeLayers (s, spans, c);
        std::set<int> chugs, kicks;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            chugs.insert (juce::roundToInt (n.beat * 4.0));
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.pitch == 36)
                kicks.insert (juce::roundToInt (n.beat * 4.0));
        CHECK (kicks == chugs);
        std::set<int> lead;
        for (const auto& n : all[(size_t) ComposerLayer::lead].notes)
            lead.insert (n.pitch);
        CHECK (lead == std::set<int> { 69, 72, 76, 77 });   // (C, E, F and the A under them, an octave up)
    }
}


TEST_CASE ("0.103.0: the SECTIONS row there by default; hidden stays hidden in its song", "[sections]")
{
    Studio studio (false);
    CHECK (studio.getSongSections().shown);
    // A song from before (no row saved) opens with it.
    auto xml = studio.getProject().toXml();
    if (auto* old = xml->getChildByName ("SongSections"))
        xml->removeChildElement (old, true);
    CHECK (Project::fromXml (*xml)->songSections.shown);
    // Hidden: saved so, opens hidden.
    studio.removeSongSectionTrack();
    CHECK_FALSE (Project::fromXml (*studio.getProject().toXml())->songSections.shown);
}

TEST_CASE ("0.103.0: a block's chords read and written; the chords either side kept", "[sections]")
{
    Studio studio (false);
    studio.appendSongSection ("Verse", 4);    // beats 0-16
    studio.appendSongSection ("Chorus", 4);   // beats 16-32
    const Chord am { 9, Chord::Type::minor }, f { 5, Chord::Type::major }, c { 0, Chord::Type::major }, g { 7, Chord::Type::major };
    // A chord over the verse's last bar running into the chorus, one after it.
    studio.setChordEvents ({ { 12.0, am, 8.0 }, { 24.0, g, 8.0 } });
    auto chorus = studio.getSectionChords (1);
    REQUIRE (chorus.size() == 2);
    CHECK (chorus[0].chord == am);
    CHECK (chorus[0].beats == Catch::Approx (4.0));
    CHECK (chorus[1].beats == Catch::Approx (8.0));
    // The chorus: F C F G, a bar each... and a fifth that doesn't fit, left out.
    REQUIRE (studio.setSectionChords (1, { { f, 4.0 }, { c, 4.0 }, { f, 4.0 }, { g, 4.0 }, { am, 4.0 } }));
    chorus = studio.getSectionChords (1);
    REQUIRE (chorus.size() == 4);
    CHECK (chorus[1].chord == c);
    CHECK (chorus[3].chord == g);
    // The verse keeps its Am, now ending at the chorus.
    const auto verse = studio.getSectionChords (0);
    REQUIRE (verse.size() == 1);
    CHECK (verse[0].chord == am);
    CHECK (verse[0].beats == Catch::Approx (4.0));
    CHECK (studio.getProject().chordTrack.shown);
    // One undo step back.
    studio.undo();
    CHECK (studio.getSectionChords (1).size() == 2);
}

TEST_CASE ("0.103.0: the block editor: name, length, the song's key, its chords added, changed, moved, removed", "[sections][ui]")
{
    Studio studio (false);
    studio.appendSongSection ("Verse", 4);
    studio.appendSongSection ("Chorus", 4);
    Key aMinor;
    for (const auto& k : Key::all())
        if (k.minor && k.pitchClass() == 9)
            aMinor = k;
    SectionEditor editor (studio, studio.getSongSections().blocks[1].id);
    editor.setSongKey (aMinor);
    CHECK (editor.getKey() == aMinor);
    CHECK (studio.chordTrackKeyAt (0.0) == aMinor);
    // + CHORD: the key's own first, then what the one before suggests; each a bar, end to end.
    for (int i = 0; i < 4; ++i)
        editor.addChord();
    auto chords = editor.getChords();
    REQUIRE (chords.size() == 4);
    CHECK (chords[0].chord == keyChords (aMinor).front());
    for (const auto& ch : chords)
    {
        CHECK (ch.beats == Catch::Approx (4.0));
        CHECK (ch.chord.isInKey (aMinor));
    }
    // Full: another takes nothing (every chord a bar long).
    editor.addChord();
    CHECK (editor.getChords().size() == 4);
    // Changed, made two bars (what no longer fits drops), moved, removed: the studio's chords follow.
    const Chord f { 5, Chord::Type::major }, g { 7, Chord::Type::major };
    editor.changeChord (1, f);
    editor.changeChord (2, g);
    editor.moveChord (2, 0);
    chords = studio.getSectionChords (1);
    REQUIRE (chords.size() == 4);
    CHECK (chords[0].chord == g);
    CHECK (chords[2].chord == f);
    editor.setChordBars (0, 2.0);
    chords = studio.getSectionChords (1);
    REQUIRE (chords.size() == 3);
    CHECK (chords[0].beats == Catch::Approx (8.0));
    editor.removeChord (0);
    CHECK (studio.getSectionChords (1).size() == 2);
    // The verse untouched.
    CHECK (studio.getSectionChords (0).empty());
    // Name and length.
    editor.setName ("Pre-Chorus");
    CHECK (studio.getSongSections().blocks[1].name == "Pre-Chorus");
    editor.setBars (8);
    CHECK (studio.getSongSections().blocks[1].bars == 8);
    CHECK (editor.getIndex() == 1);
    // The strip's cells: one per chord, left to right.
    editor.setSize (SectionEditor::width, SectionEditor::height);
    CHECK (editor.getChordBounds (0).getX() < editor.getChordBounds (1).getX());
    editor.clearChords();
    CHECK (studio.getSectionChords (1).empty());
}

TEST_CASE ("0.103.0: the block editor drawn", "[snapshot-sectioneditor]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", {});
    if (path.isEmpty())
        return;
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    Studio studio (false);
    studio.appendSongSection ("Verse", 8);
    studio.appendSongSection ("Chorus", 8);
    studio.setSongSectionType (1, "DRIVING");
    Key aMinor;
    for (const auto& k : Key::all())
        if (k.minor && k.pitchClass() == 9)
            aMinor = k;
    studio.setChordTrackKey (0.0, aMinor);
    studio.setSectionChords (1, { { { 5, Chord::Type::major }, 8.0 }, { { 0, Chord::Type::major }, 8.0 },
                                  { { 7, Chord::Type::major }, 8.0 }, { { 9, Chord::Type::minor }, 8.0 } });
    studio.setSongSectionTransition (1, "WALK DOWN");   // (0.104.0)
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    juce::Image image;
    {
        SectionEditor editor (studio, studio.getSongSections().blocks[1].id);
        editor.setSize (SectionEditor::width, SectionEditor::height);
        image = editor.createComponentSnapshot (editor.getLocalBounds(), true, 2.0f);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    juce::File file (path);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (image, out);
}

namespace
{
    // (0.104.0) Every lane composed over cleanSpans (A minor, 8 bars) with a transition.
    std::array<LineIdea, composerLayerCount> withTransition (const juce::String& name, ComposerContext c = {})
    {
        ComposerState s;
        s.transition = name;
        return composeLayers (s, cleanSpans(), c);
    }

    bool sameNotes (const LineIdea& a, const LineIdea& b)
    {
        if (a.notes.size() != b.notes.size())
            return false;
        for (size_t i = 0; i < a.notes.size(); ++i)
            if (a.notes[i].beat != b.notes[i].beat || a.notes[i].pitch != b.notes[i].pitch || a.notes[i].length != b.notes[i].length)
                return false;
        return true;
    }

    // Nothing in the lane starts at or after `from` but at `allowedAt` (-1: none), and nothing rings past `ringTo`.
    void checkQuietFrom (const LineIdea& idea, double from, double allowedAt, double ringTo)
    {
        for (const auto& n : idea.notes)
        {
            if (n.keyswitch)
                continue;
            if (n.beat >= from - 1.0e-6)
                CHECK (std::abs (n.beat - allowedAt) < 1.0e-6);
            CHECK (n.beat + n.length <= ringTo + 1.0e-6);
        }
    }

    const std::array<ComposerLayer, 4> bandLanes { ComposerLayer::chords, ComposerLayer::bass, ComposerLayer::guitar, ComposerLayer::drums };
}

TEST_CASE ("0.104.0: transitions: the last bar written for each, the melody untouched", "[sections][transitions]")
{
    const auto& list = sectionTransitions();
    REQUIRE (list.size() == 14 + 21 + 27);   // (0.144.0: 21 more; 0.154.0: 27 more)
    CHECK (list.front().name == noTransition);
    CHECK_FALSE (sectionTransitionNamed ("NONE").has_value());
    CHECK_FALSE (sectionTransitionNamed ("").has_value());
    CHECK (sectionTransitionNamed ("stop")->name == "STOP");
    CHECK (sectionTransitionNamed ("SHORT BAR: 2 BEATS")->shortBeats == 2);
    {   // The composer's settings keep it.
        ComposerState s;
        s.transition = "WALK DOWN";
        CHECK (ComposerState::fromString (s.toString()).transition == "WALK DOWN");
        CHECK (ComposerState::fromString (s.toString()) == s);
    }
    const auto none = withTransition ({});
    for (const auto l : bandLanes)
        REQUIRE_FALSE (none[(size_t) l].notes.empty());
    // Every transition changes the band's last bar and never the melody.
    for (size_t t = 1; t < list.size(); ++t)
    {
        INFO (list[t].name);
        const auto all = withTransition (list[t].name);
        CHECK (sameNotes (all[(size_t) ComposerLayer::melody], none[(size_t) ComposerLayer::melody]));
        auto changed = false;
        for (const auto l : bandLanes)
            changed = changed || ! sameNotes (all[(size_t) l], none[(size_t) l]);
        CHECK (changed);
        for (const auto l : bandLanes)   // (nothing written before the last two bars)
            for (const auto& n : all[(size_t) l].notes)
                if (n.beat < 24.0 - 1.0e-6)
                    CHECK (std::find_if (none[(size_t) l].notes.begin(), none[(size_t) l].notes.end(),
                                         [&n] (const LineNote& o) { return o.beat == n.beat && o.pitch == n.pitch; })
                           != none[(size_t) l].notes.end());
    }
    // STOP: one hit together on the last bar's first beat, held a beat, then nothing.
    {
        const auto all = withTransition ("STOP");
        for (const auto l : bandLanes)
            checkQuietFrom (all[(size_t) l], 28.0, 28.0, 32.0);
        for (const auto& n : all[(size_t) ComposerLayer::bass].notes)
            if (n.beat >= 28.0 - 1.0e-6)
                CHECK (n.pitch % 12 == 7);   // (G, the last chord's root)
    }
    // BREATH: the last beat empty.
    for (const auto l : bandLanes)
        checkQuietFrom (withTransition ("BREATH")[(size_t) l], 31.0, -1.0, 31.0);
    // SILENT BAR / BASS ALONE: the last bar empty (but the bass, as it was).
    {
        const auto silent = withTransition ("SILENT BAR"), alone = withTransition ("BASS ALONE");
        for (const auto l : bandLanes)
        {
            checkQuietFrom (silent[(size_t) l], 28.0, -1.0, 28.0);
            if (l == ComposerLayer::bass)
                CHECK (sameNotes (alone[(size_t) l], none[(size_t) l]));
            else
                checkQuietFrom (alone[(size_t) l], 28.0, -1.0, 28.0);
        }
    }
    // SNARE ROLL: sixteenth snares from the and of three, louder each time; only the drums changed.
    {
        const auto all = withTransition ("SNARE ROLL");
        std::vector<const LineNote*> snares;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.beat >= 30.5 - 1.0e-6)
            {
                CHECK ((n.pitch == 38 || n.pitch == 36));
                if (n.pitch == 38)
                    snares.push_back (&n);
            }
        REQUIRE (snares.size() == 6);
        for (size_t i = 0; i < snares.size(); ++i)
        {
            CHECK (snares[i]->beat == Catch::Approx (30.5 + 0.25 * (double) i));
            if (i > 0)
                CHECK (snares[i]->velocity > snares[i - 1]->velocity);
        }
        for (const auto l : { ComposerLayer::chords, ComposerLayer::bass, ComposerLayer::guitar })
            CHECK (sameNotes (all[(size_t) l], none[(size_t) l]));
    }
    // DRUMS BUILD: the drums alone in the last bar, eighths then sixteenths.
    {
        const auto all = withTransition ("DRUMS BUILD");
        for (const auto l : { ComposerLayer::chords, ComposerLayer::bass, ComposerLayer::guitar })
            checkQuietFrom (all[(size_t) l], 28.0, -1.0, 28.0);
        auto snares = 0;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.beat >= 28.0 - 1.0e-6 && n.pitch == 38)
                ++snares;
        CHECK (snares == 4 + 8);
    }
    // WALK DOWN / UP: four eighths a half step apart into the next chord (C here), bass and guitar together.
    {
        ComposerContext c;
        c.nextChord = Chord { 0 };
        for (const auto up : { false, true })
        {
            const auto all = withTransition (up ? "WALK UP" : "WALK DOWN", c);
            std::vector<int> bass, guitar;
            for (const auto& n : all[(size_t) ComposerLayer::bass].notes)
                if (n.beat >= 30.0 - 1.0e-6)
                    bass.push_back (n.pitch);
            for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
                if (n.beat >= 30.0 - 1.0e-6 && ! n.keyswitch)
                    guitar.push_back (n.pitch);
            REQUIRE (bass.size() == 4);
            REQUIRE (guitar.size() == 8);
            for (int k = 0; k < 4; ++k)
            {
                CHECK (((bass[(size_t) k] % 12) + 12) % 12 == (up ? 8 + k : 4 - k));
                CHECK (guitar[(size_t) k * 2 + 1] - guitar[(size_t) k * 2] == 7);
                if (k > 0)
                    CHECK (bass[(size_t) k] - bass[(size_t) k - 1] == (up ? 1 : -1));
            }
        }
    }
    // PINCH SLIDE: the guitar's last two beats one pinch harmonic dived down.
    {
        const auto all = withTransition ("PINCH SLIDE");
        const auto& last = all[(size_t) ComposerLayer::guitar].notes.back();
        CHECK (last.beat == Catch::Approx (30.0));
        CHECK (last.effect == LineNote::Effect::dive);
        CHECK (GuitarVelocity::midiOf (last.velocity) == 127);
    }
    // MUTED LINK: the last two bars palm-muted eighths on the guitar, the keys out.
    {
        const auto all = withTransition ("MUTED LINK");
        auto count = 0;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat >= 24.0 - 1.0e-6 && ! n.keyswitch)
            {
                CHECK (GuitarVelocity::midiOf (n.velocity) < 60);
                ++count;
            }
        CHECK (count == 16);
        checkQuietFrom (all[(size_t) ComposerLayer::chords], 24.0, -1.0, 24.0);
    }
    // SHORT BAR: a last bar of two beats (the studio's signature), the band hitting each beat.
    {
        ComposerContext c;
        c.lastBarBeats = 2.0;
        auto spans = cleanSpans();
        spans.back().end = 30.0;
        ComposerState s;
        s.transition = "SHORT BAR: 2 BEATS";
        const auto all = composeLayers (s, spans, c);
        std::set<double> starts;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat >= 28.0 - 1.0e-6)
                starts.insert (n.beat);
        CHECK (starts == std::set<double> { 28.0, 29.0 });
        auto crash = 0;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.beat >= 28.0 - 1.0e-6 && n.pitch == 49)
                ++crash;
        CHECK (crash == 1);
    }
}

TEST_CASE ("0.104.0: a block's transition: saved, shared by linked copies; a SHORT BAR moves what follows; one undo step", "[sections][transitions]")
{
    Song song;
    auto& studio = song.studio;
    CHECK_FALSE (studio.setSongSectionTransition (1, "NO SUCH THING"));
    REQUIRE (studio.setSongSectionTransition (1, "STOP"));
    CHECK (studio.getSongSections().blocks[1].transition == "STOP");
    CHECK (studio.getProject().signatureChanges.empty());
    {
        const auto again = Project::fromXml (*studio.getProject().toXml());
        REQUIRE (again.has_value());
        CHECK (again->songSections.blocks[1].transition == "STOP");
    }
    // The verse's last bar two beats: the chorus (and its part, its chord, its marker) two beats earlier.
    REQUIRE (studio.setSongSectionTransition (1, "SHORT BAR: 2 BEATS"));
    const auto& changes = studio.getProject().signatureChanges;
    REQUIRE (changes.size() == 2);
    CHECK (changes[0].bar == 11);
    CHECK (changes[0].numerator == 2);
    CHECK (changes[0].denominator == 4);
    CHECK (changes[1].bar == 12);
    CHECK (changes[1].numerator == 4);
    const auto chorus = *studio.getSongSectionPlace (2);
    CHECK_THAT (chorus.start, WithinAbs (46.0, 1.0e-9));
    CHECK (song.pitchAt (46.0) == 64);
    CHECK (song.chordRootAt (46.0) == 7);
    CHECK (song.pitchAt (16.0) == 62);   // (the verse where it was)
    CHECK_THAT (studio.getSongSectionPlace (1)->end - studio.getSongSectionPlace (1)->start, WithinAbs (30.0, 1.0e-9));
    {   // The composer gets the short last bar and the next chord for the verse.
        const auto c = studio.getComposerContext (16.0);
        CHECK_THAT (c.lastBarBeats, WithinAbs (2.0, 1.0e-9));
        REQUIRE (c.nextChord.has_value());
        CHECK (c.nextChord->root == 7);
    }
    // Undo: the stop back, the chorus at 48.
    studio.undo();
    CHECK (studio.getSongSections().blocks[1].transition == "STOP");
    CHECK (studio.getProject().signatureChanges.empty());
    CHECK (song.pitchAt (48.0) == 64);
    // Back to a short bar, then none: the bar whole again.
    REQUIRE (studio.setSongSectionTransition (1, "SHORT BAR: 3 BEATS"));
    CHECK_THAT (studio.getSongSectionPlace (2)->start, WithinAbs (47.0, 1.0e-9));
    REQUIRE (studio.setSongSectionTransition (1, noTransition));
    CHECK (studio.getSongSections().blocks[1].transition.isEmpty());
    CHECK (studio.getProject().signatureChanges.empty());
    CHECK (song.pitchAt (48.0) == 64);
    // A linked copy shares it.
    REQUIRE (studio.duplicateSongSection (2));
    REQUIRE (studio.setSongSectionTransition (3, "SNARE ROLL"));
    CHECK (studio.getSongSections().blocks[2].transition == "SNARE ROLL");
}

TEST_CASE ("0.104.0: a built block's transition writes its last bar again", "[sections][transitions][rewrite]")
{
    Song song;
    auto& studio = song.studio;
    {   // (the chorus's chord through the whole block: the song's last chord otherwise lasts a bar)
        auto events = studio.getProject().chordTrack.events;
        events.back().length = studio.getSongSectionPlace (2)->end - events.back().beat;
        studio.setChordEvents (events);
    }
    const auto bass = buildChorusBass (studio);
    const auto place = *studio.getSongSectionPlace (2);
    const auto lastBar = place.end - 4.0;
    const auto startsInLastBar = [&]
    {
        std::vector<double> out;
        for (const auto& p : studio.getProject().findTrack (bass)->parts)
            for (const auto& n : p.notes)
                if (p.start + n.start >= lastBar - 1.0e-9 && p.start + n.start < place.end - 1.0e-9)
                    out.push_back (p.start + n.start - lastBar);
        return out;
    };
    const auto before = startsInLastBar();
    REQUIRE (before.size() > 1);
    REQUIRE (studio.setSongSectionTransition (2, "STOP"));
    const auto stopped = startsInLastBar();
    REQUIRE (stopped.size() == 1);
    CHECK (stopped.front() == Catch::Approx (0.0));
    studio.undo();
    CHECK (startsInLastBar() == before);
    // A short bar: still the composer's lane (built again in the shorter bar, not left as "edited by hand").
    REQUIRE (studio.setSongSectionTransition (2, "SHORT BAR: 1 BEAT"));
    const auto shortPlace = *studio.getSongSectionPlace (2);
    CHECK_THAT (shortPlace.end - shortPlace.start, WithinAbs (29.0, 1.0e-9));
    auto inShortBar = 0;
    for (const auto& p : studio.getProject().findTrack (bass)->parts)
        for (const auto& n : p.notes)
            if (p.start + n.start >= shortPlace.end - 1.0 - 1.0e-9 && p.start + n.start < shortPlace.end)
            {
                CHECK (p.start + n.start == Catch::Approx (shortPlace.end - 1.0));
                ++inShortBar;
            }
    CHECK (inShortBar == 1);
}

TEST_CASE ("0.105.0: a bridge's own types, every lane's options a different part", "[sections][types][bridge]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> names { "HALF-TIME MELODIC", "HEAVY BUILD", "LATE-CHANGE CLIMB", "CLEAN BRIDGE",
                                            "SLIDING CHORDS", "GUITARS OUT, THEN BACK", "SLIDE OVER A HELD ROOT" };
    const auto bridge = sectionTypesFor ("Bridge");
    REQUIRE (bridge.size() == names.size() + 9 + 11 + 14 + 3);   // (0.142.0: eleven more)   // (0.122.0: nine more after them)
    for (size_t i = 0; i < names.size(); ++i)
        CHECK (bridge[i].name == names[i]);
    CHECK (bridge[names.size() + 20 + 14].name == "CREEP");   // (the old three kept after them)
    CHECK (sectionTypeNamed ("Middle 8", "CLEAN BRIDGE").has_value());
    CHECK_FALSE (sectionTypeNamed ("Verse", "HEAVY BUILD").has_value());
    CHECK (sectionTypeCalled ("SLIDING CHORDS")->band == SectionType::Band::bridge);

    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (const auto& name : names)
    {
        ComposerState s;
        applySectionType (s, "Bridge", name);
        CHECK (sectionBand (s) == SectionType::Band::bridge);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
                seen.insert (key (idea));
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        // The composed band stays inside the section.
        const auto all = composeLayers (s, spans, c);
        for (const auto& idea : all)
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.105.0: the bridge types' shapes", "[sections][types][bridge]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (Am F C G a bar each, 8 bars: 32 beats)
    const auto typed = [] (const juce::String& name, int guitar = -1, int drums = -1)
    {
        ComposerState s;
        applySectionType (s, "Bridge", name);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::drums).approach = drums;
        return s;
    };
    const auto pc = [] (int p) { return ((p % 12) + 12) % 12; };
    // LATE-CHANGE CLIMB: the chord changes a sixteenth late (Am to F at beat 4: A still on 4, F from 4.25).
    {
        const auto all = composeLayers (typed ("LATE-CHANGE CLIMB", 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        std::map<int, int> lowest;   // (sixteenth -> lowest pitch)
        for (const auto& n : g.notes)
        {
            const auto k = juce::roundToInt (n.beat * 4.0);
            lowest[k] = lowest.count (k) > 0 ? juce::jmin (lowest[k], n.pitch) : n.pitch;
        }
        CHECK (lowest.size() == 128);
        CHECK (pc (lowest[15]) == 9);
        CHECK (pc (lowest[16]) == 9);
        CHECK (pc (lowest[17]) == 5);
        auto kicks = 0;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            kicks += n.pitch == 36 ? 1 : 0;
        CHECK (kicks == 128);
    }
    // GUITARS OUT, THEN BACK: no guitar before halfway, then eighths; the last bar an octave up; the drums crash there.
    {
        const auto all = composeLayers (typed ("GUITARS OUT, THEN BACK", 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        REQUIRE_FALSE (g.notes.empty());
        for (const auto& n : g.notes)
            CHECK (n.beat >= 16.0 - 1.0e-6);
        int lowBefore = 128, lowLast = 128;
        for (const auto& n : g.notes)
        {
            if (n.beat >= 28.0 - 1.0e-6)
                lowLast = juce::jmin (lowLast, n.pitch);
            else if (n.beat >= 24.0 - 1.0e-6)
                lowBefore = juce::jmin (lowBefore, n.pitch);
        }
        CHECK (lowLast > lowBefore);
        auto crashAtBack = false;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            crashAtBack = crashAtBack || (n.pitch == 49 && std::abs (n.beat - 16.0) < 1.0e-6);
        CHECK (crashAtBack);
        const auto quarter = composeLayers (typed ("GUITARS OUT, THEN BACK", 1), spans, c);
        for (const auto& n : quarter[(size_t) ComposerLayer::guitar].notes)
            CHECK (n.beat >= 24.0 - 1.0e-6);
    }
    // CLEAN BRIDGE's pair: the tonic below, the top a scale step lower every two bars, struck in 3+3+2.
    {
        const auto all = composeLayers (typed ("CLEAN BRIDGE", 0), spans, c);
        std::map<int, int> top;   // bar -> highest
        std::set<int> places;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
        {
            const auto bar = (int) std::floor (n.beat / 4.0 + 1.0e-6);
            top[bar] = juce::jmax (top.count (bar) > 0 ? top[bar] : 0, n.pitch);
            places.insert (juce::roundToInt (std::fmod (n.beat, 4.0) * 2.0));
        }
        CHECK (places == std::set<int> { 0, 3, 6 });
        CHECK (top[0] == top[1]);
        CHECK (top[2] < top[1]);
        CHECK (top[4] < top[2]);
        // The bass and the snare march come in halfway.
        for (const auto& n : all[(size_t) ComposerLayer::bass].notes)
            CHECK (n.beat >= 16.0 - 1.0e-6);
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            CHECK (n.beat >= 16.0 - 1.0e-6);
    }
    // HEAVY BUILD's falling eighths: octave, 5th, b3, b3, 2, 2 over the bar's root, then a harmonic.
    {
        const auto all = composeLayers (typed ("HEAVY BUILD", 3), spans, c);
        std::vector<const LineNote*> bar0;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat < 4.0 - 1.0e-6)
                bar0.push_back (&n);
        REQUIRE (bar0.size() == 7);
        const int steps[] { 12, 7, 3, 3, 2, 2 };
        const auto root = bar0[0]->pitch - 12;
        for (int i = 0; i < 6; ++i)
            CHECK (bar0[(size_t) i]->pitch - root == steps[i]);
        CHECK (bar0[6]->technique == LineNote::Technique::harmonic);
        // The lead bends whole steps on 2 and 4.
        const auto& lead = all[(size_t) ComposerLayer::lead].notes;
        REQUIRE (lead.size() == 16);
        for (const auto& n : lead)
        {
            CHECK (n.effect == LineNote::Effect::bend);
            CHECK ((std::abs (std::fmod (n.beat, 4.0) - 1.0) < 1.0e-6 || std::abs (std::fmod (n.beat, 4.0) - 3.0) < 1.0e-6));
        }
    }
    // HALF-TIME MELODIC's drums: the last two bars a triplet fill.
    {
        const auto all = composeLayers (typed ("HALF-TIME MELODIC", 0, 0), spans, c);
        auto triplets = 0;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.beat >= 24.0 - 1.0e-6 && n.pitch != 36)
            {
                CHECK (std::abs (n.beat * 3.0 - std::round (n.beat * 3.0)) < 1.0e-6);
                ++triplets;
            }
        CHECK (triplets == 24);
    }
    // SLIDING CHORDS: the fourth bar's chord slides down a half step halfway.
    {
        const auto all = composeLayers (typed ("SLIDING CHORDS", 0), spans, c);
        int first = 128, second = 128;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat >= 12.0 - 1.0e-6 && n.beat < 16.0 - 1.0e-6)
            {
                if (n.beat < 14.0)
                    first = juce::jmin (first, n.pitch);
                else
                    second = juce::jmin (second, n.pitch);
            }
        CHECK (second == first - 1);
    }
}

TEST_CASE ("0.109.0: a breakdown's own types, every lane's options a different part", "[sections][types][breakdown]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> names { "CHROMATIC CRAWL", "HIT & HOLD, THEN CARPET", "CHUG & SQUEAL", "DOTTED TRITONE PUSH", "TRIPLET CLUSTER",
                                            "GAPPED SIXTEENTHS", "THREES & TWOS", "CALL & CHUG", "HALF-TIME OPEN CHORDS", "OFF-BEAT GALLOPS" };
    const auto breakdown = sectionTypesFor ("Breakdown");
    REQUIRE (breakdown.size() == names.size() + 4 + 5 + 4 + 2);   // (0.142.0: five more)   // (0.122.0: four more after them)
    for (size_t i = 0; i < names.size(); ++i)
        CHECK (breakdown[i].name == names[i]);
    CHECK (breakdown[names.size() + 9 + 4].name == "HEAVY CHUG");   // (the old two kept after them)
    CHECK (breakdown[names.size() + 10 + 4].name == "STACCATO CHUG");
    CHECK_FALSE (sectionTypeNamed ("Verse", "CALL & CHUG").has_value());
    for (const auto& name : names)
        CHECK (sectionTypeCalled (name)->band == SectionType::Band::breakdown);

    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (const auto& name : names)
    {
        ComposerState s;
        applySectionType (s, "Breakdown", name);
        REQUIRE (sectionBand (s) == SectionType::Band::breakdown);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        const auto all = composeLayers (s, spans, c);
        for (const auto& idea : all)
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.109.0: the breakdown types' shapes", "[sections][types][breakdown]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (Am F C G a bar each, 8 bars: 32 beats; the low string A0 = 33)
    const auto typed = [] (const juce::String& name, int guitar = -1, int drums = -1)
    {
        ComposerState s;
        applySectionType (s, "Breakdown", name);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::drums).approach = drums;
        return s;
    };
    const auto onsets = [] (const LineIdea& idea, double from, double to)
    {
        std::set<int> out;   // (in 48ths of a beat)
        for (const auto& n : idea.notes)
            if (n.beat >= from - 1.0e-6 && n.beat < to - 1.0e-6)
                out.insert (juce::roundToInt ((n.beat - from) * 48.0));
        return out;
    };
    const auto lowestAt = [] (const LineIdea& idea, double at)
    {
        auto low = 999;
        for (const auto& n : idea.notes)
            if (std::abs (n.beat - at) < 1.0e-6)
                low = juce::jmin (low, n.pitch);
        return low;
    };
    // CHROMATIC CRAWL: quarters a minor third and a whole step up, harmonics on 3, the root on 4; the snare once in two bars.
    {
        const auto all = composeLayers (typed ("CHROMATIC CRAWL", 0, 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (lowestAt (g, 0.0) == 33 + 3);
        CHECK (lowestAt (g, 1.0) == 33 + 2);
        CHECK (lowestAt (g, 3.0) == 33);
        auto harmonics = 0;
        for (const auto& n : g.notes)
            harmonics += n.technique == LineNote::Technique::harmonic && n.beat >= 2.0 - 1.0e-6 && n.beat < 3.0 - 1.0e-6 ? 1 : 0;
        CHECK (harmonics > 0);
        auto snares = 0;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            snares += n.pitch == GmDrum::snare ? 1 : 0;
        CHECK (snares == 4);
        // The bass doubles the low notes, not the harmonics: nothing on beat 3 of the first bar.
        for (const auto& n : all[(size_t) ComposerLayer::bass].notes)
            CHECK_FALSE (std::abs (n.beat - 2.0) < 1.0e-6);
    }
    // HIT & HOLD, THEN CARPET: the second half's drums a sixteenth carpet, the first half's not.
    {
        const auto all = composeLayers (typed ("HIT & HOLD, THEN CARPET", 0, 0), spans, c);
        std::set<int> kicks;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.pitch == GmDrum::kick)
                kicks.insert (juce::roundToInt (n.beat * 4.0));
        auto firstHalf = 0, secondHalf = 0;
        for (const auto k : kicks)
            (k < 64 ? firstHalf : secondHalf)++;
        CHECK (secondHalf == 64);
        CHECK (firstHalf < 32);
        // The first half's bar: a stab, then the held chord on the and of 1.
        CHECK (onsets (all[(size_t) ComposerLayer::guitar], 0.0, 4.0) == std::set<int> { 0, 24, 96, 120, 144, 168 });
    }
    // CHUG & SQUEAL: chugs, the squeal a minor ninth over the root on beat 2 and the and of 3.
    {
        const auto g = composeLayers (typed ("CHUG & SQUEAL", 0), spans, c)[(size_t) ComposerLayer::guitar];
        auto high = 0;
        for (const auto& n : g.notes)
            if (std::abs (n.beat - 1.0) < 1.0e-6)
                high = juce::jmax (high, n.pitch);
        CHECK (high == 33 + 25);
        CHECK (lowestAt (g, 1.0) == 33 + 13);
    }
    // DOTTED TRITONE PUSH (the push only): 1, the and of 2, 4: a whole step, a minor third, the tritone.
    {
        const auto g = composeLayers (typed ("DOTTED TRITONE PUSH", 1), spans, c)[(size_t) ComposerLayer::guitar];
        CHECK (onsets (g, 0.0, 4.0) == std::set<int> { 0, 72, 144 });
        CHECK (lowestAt (g, 0.0) == 35);
        CHECK (lowestAt (g, 1.5) == 36);
        CHECK (lowestAt (g, 3.0) == 39);
    }
    // TRIPLET CLUSTER: triplets, the last four bars in quarter-note triplets.
    {
        const auto g = composeLayers (typed ("TRIPLET CLUSTER", 0), spans, c)[(size_t) ComposerLayer::guitar];
        for (const auto& n : g.notes)
        {
            const auto grid = n.beat >= 16.0 - 1.0e-6 ? 2.0 / 3.0 : 1.0 / 3.0;
            CHECK (std::abs (n.beat / grid - std::round (n.beat / grid)) < 1.0e-6);
        }
        CHECK (onsets (g, 16.0, 20.0).size() == 6);
        CHECK (lowestAt (g, 0.0) == 34);   // (a half step over the root)
    }
    // GAPPED SIXTEENTHS: the first bar's gaps after three, three, four.
    {
        const auto g = composeLayers (typed ("GAPPED SIXTEENTHS", 0), spans, c)[(size_t) ComposerLayer::guitar];
        std::set<int> steps;
        for (const auto k : onsets (g, 0.0, 4.0))
            steps.insert (k / 12);
        CHECK (steps == std::set<int> { 0, 1, 2, 4, 5, 6, 8, 9, 10, 11, 13, 14, 15 });
    }
    // OFF-BEAT GALLOPS: nothing on the first beat; gallops from beat 2; the kick on every gallop note.
    {
        const auto all = composeLayers (typed ("OFF-BEAT GALLOPS", 0, 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        for (const auto& n : g.notes)
            CHECK_FALSE ((n.beat < 1.0 - 1.0e-6));
        CHECK (onsets (g, 0.0, 4.0) == std::set<int> { 48, 60, 72, 96, 108, 120, 144, 156, 168 });
        std::set<int> kicks;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.pitch == GmDrum::kick && n.beat < 4.0 - 1.0e-6)
                kicks.insert (juce::roundToInt (n.beat * 48.0));
        CHECK (kicks == onsets (g, 0.0, 4.0));
        // The squeal: a pinch harmonic in the fourth bar.
        auto pinch = false;
        for (const auto& n : g.notes)
            pinch = pinch || (n.beat >= 12.0 && n.beat < 16.0 && n.velocity >= GuitarVelocity::of (GuitarVelocity::pinch) - 1.0e-6);
        CHECK (pinch);
    }
    // HALF-TIME OPEN CHORDS with the build: the last bars whole notes, then quarters, then eighths; the lead on top.
    {
        auto s = typed ("HALF-TIME OPEN CHORDS", 1, 1);
        s.lane (ComposerLayer::lead).on = true;
        s.lane (ComposerLayer::lead).approach = 0;
        const auto all = composeLayers (s, spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (onsets (g, 16.0, 20.0).size() == 1);
        CHECK (onsets (g, 20.0, 24.0).size() == 4);
        CHECK (onsets (g, 24.0, 28.0).size() == 8);
        const auto& lead = all[(size_t) ComposerLayer::lead];
        REQUIRE (lead.notes.size() == 16);
        for (const auto& n : lead.notes)
            CHECK (n.pitch >= 64);
    }
}

TEST_CASE ("0.110.0: interlude and post-chorus types, every lane's options a different part", "[sections][types][interlude]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> names { "HOOK RIFF", "CHORUS CARRIES ON", "PEDAL IN THREES", "HIGH TRILL HOOK", "PEDAL & HIGH MELODY", "BURST & ANSWER",
                                            "STAB & ANSWER", "TREMOLO OVER CHUGS", "CLEAN BREATH", "TRITONE AIR & BUILD", "CHROMATIC WOBBLE" };
    // A post-chorus: these, then the chorus's; an interlude or instrumental: these, then the bridge's.
    const auto post = sectionTypesFor ("Post-Chorus");
    REQUIRE (post.size() == names.size() + 6 + 3 + sectionTypesFor ("Chorus").size());
    for (size_t i = 0; i < names.size(); ++i)
        CHECK (post[i].name == names[i]);
    CHECK (post[names.size() + 6 + 3].name == "DRIVING");   // (0.141.0: six more first)
    const auto interlude = sectionTypesFor ("Interlude 2");
    REQUIRE (interlude.size() == names.size() + 6 + 3 + sectionTypesFor ("Bridge").size());
    CHECK (interlude[names.size() + 6 + 3].name == "HALF-TIME MELODIC");
    CHECK (sectionTypeNamed ("Instrumental", "HOOK RIFF").has_value());
    CHECK_FALSE (sectionTypeNamed ("Chorus", "HOOK RIFF").has_value());
    CHECK_FALSE (sectionTypeNamed ("Hook", "HOOK RIFF").has_value());
    for (const auto& name : names)
        CHECK (sectionTypeCalled (name)->band == SectionType::Band::interlude);

    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (const auto& name : names)
    {
        ComposerState s;
        applySectionType (s, "Post-Chorus", name);
        REQUIRE (sectionBand (s) == SectionType::Band::interlude);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        const auto all = composeLayers (s, spans, c);
        for (const auto& idea : all)
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.110.0: the interlude types' shapes", "[sections][types][interlude]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (Am F C G a bar each, 8 bars; the low string A0 = 33: Am 33, F 41, C 36, G 43)
    const auto typed = [] (const juce::String& name, int guitar = -1, int drums = -1)
    {
        ComposerState s;
        applySectionType (s, "Interlude", name);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::drums).approach = drums;
        s.lane (ComposerLayer::lead).on = true;
        return s;
    };
    const auto lowestAt = [] (const LineIdea& idea, double at)
    {
        auto low = 999;
        for (const auto& n : idea.notes)
            if (std::abs (n.beat - at) < 1.0e-6)
                low = juce::jmin (low, n.pitch);
        return low;
    };
    // HOOK RIFF: the bar's chord on its eighths, the next chord struck on the last eighth (F at 3.5, C at 7.5).
    {
        const auto all = composeLayers (typed ("HOOK RIFF", 0, 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (lowestAt (g, 0.0) == 33);
        CHECK (lowestAt (g, 3.0) == 33);
        CHECK (lowestAt (g, 3.5) == 41);
        CHECK (lowestAt (g, 4.0) == 999);   // (held over the bar line, not struck again)
        CHECK (lowestAt (g, 4.5) == 41);
        CHECK (lowestAt (g, 7.5) == 36);
        // The hook: eighths, the fourth bar resolving onto the high root with vibrato.
        const auto& lead = all[(size_t) ComposerLayer::lead].notes;
        REQUIRE_FALSE (lead.empty());
        for (const auto& n : lead)
            CHECK (std::abs (n.beat * 2.0 - std::round (n.beat * 2.0)) < 1.0e-6);
        auto resolved = false;
        for (const auto& n : lead)
            resolved = resolved || (std::abs (n.beat - 15.5) < 1.0e-6 && n.pitch % 12 == 9 && n.effect == LineNote::Effect::vibrato);
        CHECK (resolved);
        // The kick pushes on the and of 4.
        auto push = false;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            push = push || (n.pitch == GmDrum::kick && std::abs (n.beat - 3.5) < 1.0e-6);
        CHECK (push);
    }
    // CHORUS CARRIES ON: the last two bars stabs on 1 and 2 only.
    {
        const auto all = composeLayers (typed ("CHORUS CARRIES ON", 0), spans, c);
        std::set<int> starts;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat >= 24.0 - 1.0e-6)
                starts.insert (juce::roundToInt ((n.beat - 24.0) * 4.0));
        CHECK (starts == std::set<int> { 0, 4, 16, 20 });
        // Frozen: every bar on the tonic.
        const auto frozen = composeLayers (typed ("CHORUS CARRIES ON", 1), spans, c)[(size_t) ComposerLayer::guitar];
        CHECK (lowestAt (frozen, 4.0) == 33);
        CHECK (lowestAt (frozen, 12.0) == 33);
    }
    // PEDAL IN THREES: the low root on every third sixteenth.
    {
        const auto g = composeLayers (typed ("PEDAL IN THREES", 0), spans, c)[(size_t) ComposerLayer::guitar];
        for (int i = 0; i < 16; i += 3)
            CHECK (lowestAt (g, i * 0.25) == 33);
        CHECK (lowestAt (g, 0.25) > 33);
    }
    // CLEAN BREATH: the bass out until halfway (its first option).
    {
        const auto all = composeLayers (typed ("CLEAN BREATH", 0), spans, c);
        const auto& b = all[(size_t) ComposerLayer::bass].notes;
        REQUIRE_FALSE (b.empty());
        for (const auto& n : b)
            CHECK (n.beat >= 16.0 - 1.0e-6);
    }
    // TRITONE AIR & BUILD: the guitar out in the third quarter, scratches in the last.
    {
        const auto g = composeLayers (typed ("TRITONE AIR & BUILD", 0), spans, c)[(size_t) ComposerLayer::guitar];
        auto inGap = 0, scratches = 0;
        for (const auto& n : g.notes)
        {
            inGap += n.beat >= 16.0 - 1.0e-6 && n.beat < 24.0 - 1.0e-6 ? 1 : 0;
            scratches += n.beat >= 24.0 - 1.0e-6 ? 1 : 0;
        }
        CHECK (inGap == 0);
        CHECK (scratches > 0);
    }
}

TEST_CASE ("0.111.0: intro and outro types, every lane's options a different part", "[sections][types][bookend]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> intros { "RIFF ALONE, THEN THE BAND", "FULL BAND FROM THE DOWNBEAT", "ARPEGGIO BUILD", "KEYS HOOK OVER A LOOP", "SWELL AND DRONE",
                                             "STAB CALLS", "TREMOLO SWELL", "HOLD AND ANSWER", "DOWNBEAT HITS UNDER A LEAD", "RIFF WITH DRUMS BUILDING",
                                             // (0.124.0)
                                             "BASS RIFF WITH TOMS", "PEDAL SEESAW, DOUBLE KICK", "TWO CELLS IN THREES", "KIT HITS IN PAIRS",
                                             "GAPPED PEDAL, BUILD INTO THE VERSE" };
    const std::vector<juce::String> outros { "INTRO RETURNS, LAST CHORD RINGS", "WINDING DOWN", "DEAD STOP", "STABS, THEN RING", "FILL AND FINAL HIT",
                                             "HOOK ALONE TO THE END", "SPEEDING HITS", "SPACED STABS", "TRIPLE STAB STOP",
                                             "FALLING OCTAVE PEDAL",   // (0.117.0)
                                             "EVERY OTHER BAR EMPTY", "SINGLE-NOTE BAR, CHORD BAR" };   // (0.124.0)
    const auto intro = sectionTypesFor ("Intro");
    REQUIRE (intro.size() == intros.size() + 8 + 8 + 2);   // (0.143.0: eight more)
    for (size_t i = 0; i < intros.size(); ++i)
        CHECK (intro[i].name == intros[i]);
    CHECK (intro[intros.size() + 8 + 8].name == "DRONING");
    for (const auto* name : { "Outro", "Ending", "Coda", "Tag" })
    {
        const auto outro = sectionTypesFor (name);
        REQUIRE (outro.size() == outros.size() + 8 + 4 + 2);   // (0.143.0: eight more)
        for (size_t i = 0; i < outros.size(); ++i)
            CHECK (outro[i].name == outros[i]);
    }
    CHECK_FALSE (sectionTypeNamed ("Intro", "DEAD STOP").has_value());
    CHECK_FALSE (sectionTypeNamed ("Outro", "STAB CALLS").has_value());

    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    std::vector<std::pair<juce::String, juce::String>> all;
    for (const auto& n : intros)
        all.emplace_back ("Intro", n);
    for (const auto& n : outros)
        all.emplace_back ("Outro", n);
    for (const auto& [section, name] : all)
    {
        ComposerState s;
        applySectionType (s, section, name);
        REQUIRE (sectionBand (s) == SectionType::Band::bookend);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        const auto composed = composeLayers (s, spans, c);
        for (const auto& idea : composed)
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.111.0: the intro and outro types' shapes", "[sections][types][bookend]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (Am F C G a bar each, 8 bars: 32 beats; the low string A0 = 33)
    const auto typed = [] (const juce::String& section, const juce::String& name, int guitar = -1, int drums = -1)
    {
        ComposerState s;
        applySectionType (s, section, name);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::drums).approach = drums;
        return s;
    };
    // RIFF ALONE, THEN THE BAND: no drums or bass before halfway; the guitar muted until then.
    {
        const auto all = composeLayers (typed ("Intro", "RIFF ALONE, THEN THE BAND", 0, 0), spans, c);
        for (const auto& n : all[(size_t) ComposerLayer::bass].notes)
            CHECK (n.beat >= 16.0 - 1.0e-6);
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            CHECK ((n.beat >= 16.0 - 1.0e-6 || n.beat >= 15.0 - 1.0e-6));   // (a fill on the last beat before)
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat < 16.0 - 1.0e-6)
                CHECK (n.velocity < GuitarVelocity::of (60));
    }
    // DEAD STOP: nothing at or after beat 3 of the last bar, and the last notes short.
    {
        const auto all = composeLayers (typed ("Outro", "DEAD STOP", 0, 0), spans, c);
        for (const auto l : { ComposerLayer::guitar, ComposerLayer::bass, ComposerLayer::chords, ComposerLayer::drums })   // (the type's lanes; the melody is its own)
            for (const auto& n : all[(size_t) l].notes)
            {
                CHECK (n.beat < 30.0 + 1.0e-6);
                CHECK (n.beat + n.length <= 30.25 + 1.0e-6);
            }
    }
    // TRIPLE STAB STOP: the last bar is three sixteenths and silence.
    {
        const auto g = composeLayers (typed ("Outro", "TRIPLE STAB STOP", 0), spans, c)[(size_t) ComposerLayer::guitar];
        std::set<int> last;
        for (const auto& n : g.notes)
            if (n.beat >= 28.0 - 1.0e-6)
                last.insert (juce::roundToInt ((n.beat - 28.0) * 4.0));
        CHECK (last == std::set<int> { 0, 1, 2 });
    }
    // SPEEDING HITS: halves, then quarters, then eighths.
    {
        const auto g = composeLayers (typed ("Outro", "SPEEDING HITS", 0), spans, c)[(size_t) ComposerLayer::guitar];
        auto starts = [&] (double from, double to)
        {
            std::set<int> s;
            for (const auto& n : g.notes)
                if (n.beat >= from - 1.0e-6 && n.beat < to - 1.0e-6)
                    s.insert (juce::roundToInt (n.beat * 8.0));
            return (int) s.size();
        };
        CHECK (starts (0.0, 4.0) == 2);
        CHECK (starts (12.0, 16.0) == 4);
        CHECK (starts (28.0, 32.0) == 8);
    }
    // FILL AND FINAL HIT: the last bar one chord, the drums one kick and crash on it.
    {
        const auto all = composeLayers (typed ("Outro", "FILL AND FINAL HIT", 0, 0), spans, c);
        std::set<int> lastDrums;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.beat >= 28.0 - 1.0e-6)
                lastDrums.insert (n.pitch * 1000 + juce::roundToInt ((n.beat - 28.0) * 4.0));
        CHECK (lastDrums == std::set<int> { GmDrum::kick * 1000, GmDrum::crash * 1000 });
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat >= 28.0 - 1.0e-6)
                CHECK (n.beat == Catch::Approx (28.0));
    }
    // ARPEGGIO BUILD: the drums silent until the wall (the last quarter), but for soft crashes.
    {
        const auto all = composeLayers (typed ("Intro", "ARPEGGIO BUILD", 0, 0), spans, c);
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.beat < 24.0 - 1.0e-6)
                CHECK (n.pitch == GmDrum::crash);
    }
}

TEST_CASE ("0.116.0: the other four chorus types, every lane's options a different part", "[sections][types][chorus116]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> names { "TOM GROOVE RIFF", "RING, THEN ANSWER", "HALF-TIME CHORDS WITH AN OCTAVE PEDAL", "CHUG BAR, CHORD BAR" };
    const auto chorus = sectionTypesFor ("Chorus");
    REQUIRE (chorus.size() == 7 + names.size() + 23 + 11);   // (0.140.0: 23 more after them)
    CHECK (chorus[3].name == "PUSHED CHORDS UNDER A FIXED HOOK");   // (0.115.0's kept in their places)
    for (size_t i = 0; i < names.size(); ++i)
    {
        CHECK (chorus[7 + i].name == names[i]);
        CHECK (sectionTypeCalled (names[i])->band == SectionType::Band::chorus);
    }
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (const auto& name : names)
    {
        ComposerState s;
        applySectionType (s, "Chorus", name);
        REQUIRE (sectionBand (s) == SectionType::Band::chorus);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        const auto all = composeLayers (s, spans, c);
        for (const auto& idea : all)
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.115.0: the four chorus types from the owner's songs, every lane's options a different part", "[sections][types][chorus115]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> names { "PUSHED CHORDS UNDER A FIXED HOOK", "TWIN HARMONY, HALF TIME", "SIXTEENTH WALL, HALF-TIME KIT", "GROUPED STREAM" };
    const auto chorus = sectionTypesFor ("Chorus");
    REQUIRE (chorus.size() == 3 + names.size() + 4 + 23 + 11);   // (0.116.0: four more after them; 0.140.0: 23 more)
    CHECK (chorus[0].name == "DRIVING");   // (the first three kept in their places)
    for (size_t i = 0; i < names.size(); ++i)
    {
        CHECK (chorus[3 + i].name == names[i]);
        CHECK (sectionTypeCalled (names[i])->band == SectionType::Band::chorus);
    }
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (const auto& name : names)
    {
        ComposerState s;
        applySectionType (s, "Chorus", name);
        REQUIRE (sectionBand (s) == SectionType::Band::chorus);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        const auto all = composeLayers (s, spans, c);
        for (const auto& idea : all)
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.116.0: the new chorus types' shapes; DRIVING's and DRONING's new drums", "[sections][types][chorus116]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (Am F C G a bar each, 8 bars)
    const auto typed = [] (const juce::String& name, int guitar = -1, int bass = -1, int drums = -1, int lead = -1)
    {
        ComposerState s;
        applySectionType (s, "Chorus", name);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::bass).approach = bass;
        s.lane (ComposerLayer::drums).approach = drums;
        s.lane (ComposerLayer::lead).approach = lead;
        s.lane (ComposerLayer::lead).on = true;
        return s;
    };
    const auto has = [] (const LineIdea& idea, double beat, int pitch)
    {
        for (const auto& n : idea.notes)
            if (std::abs (n.beat - beat) < 1.0e-6 && n.pitch == pitch)
                return true;
        return false;
    };
    // TOM GROOVE RIFF: toms where the hats would be; the march kick and floor tom on every sixteenth.
    {
        const auto all = composeLayers (typed ("TOM GROOVE RIFF", 0, 0, 0, 0), spans, c);
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (has (d, 0.25, 41));
        CHECK (has (d, 1.0, 38));
        for (const auto& n : d.notes)
            CHECK ((n.pitch != 42 && n.pitch != 46));
        const auto march = composeLayers (typed ("TOM GROOVE RIFF", 0, 0, 1, 0), spans, c)[(size_t) ComposerLayer::drums];
        for (int k = 0; k < 16; ++k)
        {
            CHECK (has (march, k * 0.25, 36));
            CHECK (has (march, k * 0.25, 41));
        }
        const auto toms = composeLayers (typed ("TOM GROOVE RIFF", 0, 0, 2, 0), spans, c)[(size_t) ComposerLayer::drums];
        for (const auto& n : toms.notes)
            CHECK ((n.pitch == 41 || n.pitch == 48));
    }
    // HALF-TIME CHORDS WITH AN OCTAVE PEDAL: the lead's eighths in octave pairs, one note for four bars.
    {
        const auto all = composeLayers (typed ("HALF-TIME CHORDS WITH AN OCTAVE PEDAL", 0, 0, 0, 0), spans, c);
        const auto& lead = all[(size_t) ComposerLayer::lead];
        REQUIRE (! lead.notes.empty());
        int first = -1;
        for (const auto& n : lead.notes)
            if (n.beat < 16.0 - 1.0e-6)
            {
                if (first < 0) first = n.pitch % 12;
                CHECK (n.pitch % 12 == first);
            }
        for (int k = 0; k < 32; ++k)
        {
            int count = 0;
            for (const auto& n : lead.notes)
                if (std::abs (n.beat - k * 0.5) < 1.0e-6) ++count;
            CHECK (count == 2);
        }
    }
    // DRIVING's two new drums and DRONING's new one: their own parts.
    {
        ComposerState s;
        applySectionType (s, "Chorus", "DRIVING");
        CHECK (baseSectionApproaches (s, ComposerLayer::drums).size() == 7);
        ComposerState d;
        applySectionType (d, "Chorus", "DRONING");
        CHECK (baseSectionApproaches (d, ComposerLayer::drums).size() == 8);
    }
}

TEST_CASE ("0.115.0: the chorus types' shapes", "[sections][types][chorus115]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (Am F C G a bar each; the low string A0 = 33: Am 33, F 41, C 36, G 43)
    const auto typed = [] (const juce::String& name, int guitar = -1, int bass = -1, int drums = -1, int lead = -1)
    {
        ComposerState s;
        applySectionType (s, "Chorus", name);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::bass).approach = bass;
        s.lane (ComposerLayer::drums).approach = drums;
        s.lane (ComposerLayer::lead).approach = lead;
        s.lane (ComposerLayer::lead).on = true;
        return s;
    };
    const auto at = [] (const LineIdea& idea, double beat)
    {
        std::vector<int> pitches;
        for (const auto& n : idea.notes)
            if (std::abs (n.beat - beat) < 1.0e-6)
                pitches.push_back (n.pitch);
        std::sort (pitches.begin(), pitches.end());
        return pitches;
    };
    // PUSHED CHORDS: each change struck a beat early and held over the bar line.
    {
        const auto all = composeLayers (typed ("PUSHED CHORDS UNDER A FIXED HOOK", 0, 0, 0, 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        REQUIRE (! at (g, 0.0).empty());
        CHECK (at (g, 0.0).front() == 33);
        CHECK (at (g, 3.0).front() == 41);   // (F pushed onto bar 1's fourth beat)
        CHECK (at (g, 4.0).empty());
        CHECK (at (g, 7.0).front() == 36);
        // The bass doubles the pushes.
        const auto& b = all[(size_t) ComposerLayer::bass];
        CHECK (! at (b, 3.0).empty());
        CHECK (at (b, 4.0).empty());
        // The hook: only the key's 1, 2, 3 and 5 (A minor: A B C E), the same two bars every time.
        const auto& lead = all[(size_t) ComposerLayer::lead];
        REQUIRE (! lead.notes.empty());
        for (const auto& n : lead.notes)
        {
            const auto pc = n.pitch % 12;
            CHECK ((pc == 9 || pc == 11 || pc == 0 || pc == 4));
        }
        for (const auto& n : lead.notes)
            if (n.beat < 8.0 - 1.0e-6)
                CHECK (at (lead, n.beat + 8.0) == std::vector<int> { n.pitch });
    }
    // TWIN HARMONY: two voices a third apart, the snare on 3 (half time).
    {
        const auto all = composeLayers (typed ("TWIN HARMONY, HALF TIME", 0, 0, 0, 0), spans, c);
        const auto& lead = all[(size_t) ComposerLayer::lead];
        for (const auto beat : { 0.0, 0.75, 1.5, 2.0 })
        {
            const auto p = at (lead, beat);
            REQUIRE (p.size() == 2);
            CHECK ((p[1] - p[0] == 3 || p[1] - p[0] == 4));
        }
        CHECK (at (lead, 12.0).size() == 2);   // (the fourth bar one held pair)
        CHECK (at (lead, 12.75).empty());
        const auto& d = all[(size_t) ComposerLayer::drums];
        const auto snareAt = [&] (double beat) { const auto p = at (d, beat); return std::find (p.begin(), p.end(), 38) != p.end(); };
        CHECK (snareAt (2.0));
        CHECK_FALSE (snareAt (1.0));
        CHECK_FALSE (snareAt (3.0));
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (at (g, 0.0).size() == 3);
        CHECK (at (g, 0.75).empty());   // (rung on the one)
        const auto locked = composeLayers (typed ("TWIN HARMONY, HALF TIME", 2), spans, c)[(size_t) ComposerLayer::guitar];
        CHECK (at (locked, 0.75).size() == 3);   // (on the line's 3+3+2)
    }
    // SIXTEENTH WALL: every sixteenth, the turnaround's three chords in bar 4 (G, G a step under A, then A).
    {
        const auto all = composeLayers (typed ("SIXTEENTH WALL, HALF-TIME KIT", 0, 0, 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        for (int k = 0; k < 16; ++k)
            CHECK (at (g, k * 0.25).size() == 3);
        CHECK (at (g, 12.0).front() == 43);
        CHECK (at (g, 15.0).front() == 33);
        const auto& b = all[(size_t) ComposerLayer::bass];
        CHECK (at (b, 15.0).front() % 12 == 9);
        CHECK (at (b, 0.25).size() == 1);
    }
    // GROUPED STREAM: grouped fives leave the sixth and the twelfth eighth of every two bars empty.
    {
        const auto all = composeLayers (typed ("GROUPED STREAM", 1, 0, 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (! at (g, 2.0).empty());
        CHECK (at (g, 2.5).empty());
        CHECK (at (g, 5.5).empty());
        CHECK (! at (g, 6.0).empty());
        const auto unbroken = composeLayers (typed ("GROUPED STREAM", 0), spans, c)[(size_t) ComposerLayer::guitar];
        for (int k = 0; k < 64; ++k)
            CHECK (! at (unbroken, k * 0.5).empty());
        const auto& b = all[(size_t) ComposerLayer::bass];
        CHECK (! at (b, 0.0).empty());
        CHECK (at (b, 2.5).empty());
    }
}

TEST_CASE ("0.117.0: the pushed turnaround, the dominant pedal ending and the falling octave pedal", "[sections][types][chorus117]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G a bar each, 8 bars; the low string A0 = 33)
    const auto at = [] (const LineIdea& idea, double beat)
    {
        std::vector<int> pitches;
        for (const auto& n : idea.notes)
            if (std::abs (n.beat - beat) < 1.0e-6)
                pitches.push_back (n.pitch);
        std::sort (pitches.begin(), pitches.end());
        return pitches;
    };
    const auto startsIn = [] (const LineIdea& idea, double from, double to)
    {
        int count = 0;
        for (const auto& n : idea.notes)
            if (n.beat >= from - 1.0e-6 && n.beat < to - 1.0e-6)
                ++count;
        return count;
    };
    for (const auto* type : { "DRIVING", "SYNCOPATED DRIVE" })
    {
        INFO (type);
        ComposerState s;
        applySectionType (s, "Chorus", type);
        const auto names = sectionApproaches (s, ComposerLayer::guitar);
        s.lane (ComposerLayer::guitar).approach = names.indexOf ("PUSHED FOUR-CHORD TURNAROUND");
        REQUIRE (s.lane (ComposerLayer::guitar).approach >= 0);
        const auto all = composeLayers (s, spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        // The fourth bar (beats 12-16): G A C D (bVII i III iv) as power chords on the pushes: mmx-x-mmx---mmx-.
        const double hits[] { 12.5, 13.0, 14.0, 15.5 };
        const int roots[] { 43, 33, 36, 38 };
        for (int i = 0; i < 4; ++i)
        {
            const auto p = at (g, hits[i]);
            REQUIRE (p.size() == 3);
            CHECK (p == std::vector<int> { roots[i], roots[i] + 7, roots[i] + 12 });
        }
        CHECK (at (g, 12.0) == std::vector<int> { 43 });   // (the muted grace into the first)
        CHECK (at (g, 13.25).empty());
        // The bass on the pushes, the keys out of the bar, the drums' toms on the pushes and a crash on the last.
        const auto& b = all[(size_t) ComposerLayer::bass];
        for (int i = 0; i < 4; ++i)
            CHECK (! at (b, hits[i]).empty());
        CHECK (startsIn (all[(size_t) ComposerLayer::chords], 12.0, 16.0) == 0);
        const auto& d = all[(size_t) ComposerLayer::drums];
        const auto has = [&] (double beat, int note) { const auto p = at (d, beat); return std::find (p.begin(), p.end(), note) != p.end(); };
        CHECK (has (12.5, GmDrum::tomHigh));
        CHECK (has (15.5, GmDrum::tomFloor));
        CHECK (has (15.5, GmDrum::crash));
        // The other bars as before: the same first shape without the turnaround.
        auto plain = s;
        plain.lane (ComposerLayer::guitar).approach = 0;
        const auto before = composeLayers (plain, spans, c)[(size_t) ComposerLayer::guitar];
        CHECK (at (before, 4.0) == at (g, 4.0));
        CHECK (at (before, 13.0) != at (g, 13.0));

        // DOMINANT PEDAL ENDING: the last four bars on E (the key's fifth) on every eighth.
        auto pedal = s;
        pedal.lane (ComposerLayer::guitar).approach = 0;
        pedal.lane (ComposerLayer::bass).approach = sectionApproaches (s, ComposerLayer::bass).indexOf ("DOMINANT PEDAL ENDING");
        REQUIRE (pedal.lane (ComposerLayer::bass).approach >= 0);
        const auto bass = composeLayers (pedal, spans, c)[(size_t) ComposerLayer::bass];
        for (int k = 0; k < 32; ++k)
        {
            const auto p = at (bass, 16.0 + k * 0.5);
            REQUIRE (p.size() == 1);
            CHECK (p.front() % 12 == 4);
        }
        CHECK (startsIn (bass, 0.0, 16.0) > 0);   // (the line as it was before the pedal)
        if (juce::String (type) == "DRIVING")
        {
            const auto f = at (bass, 4.0);
            REQUIRE (f.size() == 1);
            CHECK (f.front() % 12 == 5);   // (F's root before the pedal)
        }
    }
    // FALLING OCTAVE PEDAL under DRIVING: A and its octave on every eighth for four bars, then G.
    {
        ComposerState s;
        applySectionType (s, "Chorus", "DRIVING");
        s.lane (ComposerLayer::lead).on = true;
        s.lane (ComposerLayer::lead).approach = sectionApproaches (s, ComposerLayer::lead).indexOf ("FALLING OCTAVE PEDAL");
        REQUIRE (s.lane (ComposerLayer::lead).approach >= 0);
        const auto lead = composeLayers (s, spans, c)[(size_t) ComposerLayer::lead];
        for (int k = 0; k < 64; ++k)
        {
            const auto p = at (lead, k * 0.5);
            REQUIRE (p.size() == 2);
            CHECK (p[1] - p[0] == 12);
            CHECK (p[0] % 12 == (k < 32 ? 9 : 7));
        }
    }
    // The outro: the falling octaves, the riff under them, the last bar three stabs with the drums.
    {
        ComposerState s;
        applySectionType (s, "Outro", "FALLING OCTAVE PEDAL");
        REQUIRE (sectionBand (s) == SectionType::Band::bookend);
        s.lane (ComposerLayer::lead).on = true;
        const auto all = composeLayers (s, spans, c);
        const auto& lead = all[(size_t) ComposerLayer::lead];
        CHECK (at (lead, 0.5).size() == 2);
        CHECK (at (lead, 16.0).front() % 12 == 7);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        for (const auto beat : { 28.0, 29.5, 31.0 })
            CHECK (at (g, beat).size() == 3);
        CHECK (startsIn (g, 28.01, 29.49) == 0);
        const auto& d = all[(size_t) ComposerLayer::drums];
        for (int k = 0; k < 8; ++k)
        {
            const auto p = at (d, k * 0.5);
            CHECK (std::find (p.begin(), p.end(), GmDrum::kick) != p.end());
        }
        const auto last = at (d, 31.0);
        CHECK (std::find (last.begin(), last.end(), GmDrum::crash) != last.end());
    }
}

TEST_CASE ("0.118.0: the verse types, every lane's options a different part", "[sections][types][verse118]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> names { "PICKED LINE, KICK IN THREES", "PICKED OSTINATO", "SKANK LINE", "LINE OVER HITS", "THREE BARS AND A STOP",
                                            "SIDESTICK PULSE", "OFF-BEAT SNARE", "CHUGS, THEN DOTTED HITS", "PEDAL RIFF IN SIXTEENTHS", "TRESILLO RIFF" };
    const auto verse = sectionTypesFor ("Verse");
    REQUIRE (verse.size() == 4 + names.size() + 12 + 5 + 11 + 2);   // (0.139.0: twelve more after them; 0.146.0: five more)
    CHECK (verse[3].name == "MUTED DRIVE");
    for (size_t i = 0; i < names.size(); ++i)
    {
        CHECK (verse[4 + i].name == names[i]);
        CHECK (sectionTypeCalled (names[i])->band == SectionType::Band::verse);
        CHECK (isSectionTypeOf ("Pre-Chorus", names[i]));   // (a pre-chorus lists the verse's types after its own)
    }
    CHECK (verse.back().name == "SPARSE");
    CHECK (verse[4 + names.size()].name == "BASS RIFF, HIGH NOTES RUNG");
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (const auto& name : names)
    {
        ComposerState s;
        applySectionType (s, "Verse", name);
        REQUIRE (sectionBand (s) == SectionType::Band::verse);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        const auto all = composeLayers (s, spans, c);
        for (const auto& idea : all)
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.118.0: the verse types' shapes", "[sections][types][verse118]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();
    const auto typed = [] (const juce::String& name, int guitar = -1, int drums = -1)
    {
        ComposerState s;
        applySectionType (s, "Verse", name);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::drums).approach = drums;
        return s;
    };
    const auto has = [] (const LineIdea& idea, double beat, int pitch)
    {
        return std::any_of (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return std::abs (n.beat - beat) < 1.0e-6 && n.pitch == pitch; });
    };
    // PICKED LINE, KICK IN THREES: the kick on 1, the and of 2 and 4 (3+3+2), the snare between; one note an eighth.
    {
        const auto all = composeLayers (typed ("PICKED LINE, KICK IN THREES", 0, 0), spans, c);
        const auto& d = all[(size_t) ComposerLayer::drums];
        for (int k = 0; k < 8; ++k)
        {
            const auto kick = k == 0 || k == 3 || k == 6;
            CHECK (has (d, k * 0.5, GmDrum::kick) == kick);
            CHECK (has (d, k * 0.5, GmDrum::snare) == ! kick);
        }
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (g.notes.size() == 64u);   // (eight bars of eighths, single notes)
        const auto running = composeLayers (typed ("PICKED LINE, KICK IN THREES", 0, 1), spans, c)[(size_t) ComposerLayer::drums];
        CHECK (has (running, 4.5, GmDrum::kick));   // (the ninth eighth, the third kick of the run, past the bar line)
        CHECK_FALSE (has (running, 4.0, GmDrum::kick));
    }
    // THREE BARS AND A STOP: the fourth bar of every four one hit on its one, then silence in every lane.
    {
        const auto all = composeLayers (typed ("THREE BARS AND A STOP", 0, 0), spans, c);
        for (const auto l : { ComposerLayer::guitar, ComposerLayer::bass, ComposerLayer::drums })
        {
            const auto& idea = all[(size_t) l];
            INFO (composerLayerName (l));
            CHECK (std::any_of (idea.notes.begin(), idea.notes.end(), [] (const LineNote& n) { return std::abs (n.beat - 12.0) < 1.0e-6; }));
            for (const auto& n : idea.notes)
                for (const auto stop : { 12.0, 28.0 })
                    if (n.beat >= stop - 1.0e-6 && n.beat < stop + 4.0 - 1.0e-6)
                        CHECK (n.beat < stop + 0.25);
        }
        const auto& d = all[(size_t) ComposerLayer::drums];
        for (int b = 0; b < 4; ++b)
            CHECK (has (d, b, GmDrum::kick));   // (four on the floor)
    }
    // SKANK LINE: the snare on every beat, the kick on every off-beat eighth; the guitar's pedal every other eighth.
    {
        const auto all = composeLayers (typed ("SKANK LINE", 0, 0), spans, c);
        const auto& d = all[(size_t) ComposerLayer::drums];
        for (int b = 0; b < 4; ++b)
        {
            CHECK (has (d, b, GmDrum::snare));
            CHECK (has (d, b + 0.5, GmDrum::kick));
            CHECK_FALSE (has (d, b, GmDrum::kick));
        }
        const auto& g = all[(size_t) ComposerLayer::guitar];
        for (int k = 0; k < 8; k += 2)
            CHECK (has (g, k * 0.5, 33));   // (A on the low string)
    }
    // MUTED DRIVE's new FOUR ON THE FLOOR.
    {
        ComposerState s;
        applySectionType (s, "Verse", "MUTED DRIVE");
        s.lane (ComposerLayer::drums).approach = 3;
        const auto d = composeLayers (s, spans, c)[(size_t) ComposerLayer::drums];
        for (int b = 0; b < 4; ++b)
            CHECK (has (d, b, GmDrum::kick));
        CHECK (has (d, 0.5, GmDrum::snare));
    }
}

TEST_CASE ("0.122.0: the rock songs' bridges and breakdowns, every lane's options a different part", "[sections][types][rock]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> bridges { "ONE GUITAR RUNS, THE BAND HITS", "TWIN TREMOLO RUSH", "RING & CHUG SPLIT", "DRUM BURSTS & PULSE",
                                              "LAYERED FLOOR BUILD", "UPPER-FIFTH CHORDS OVER A MOVING BASS", "CHROMATIC CREEP, NO BACKBEAT",
                                              "CLIMB UNDER A TRILL", "KEYS CARRY THE CHORUS ALONE" };
    const std::vector<juce::String> breakdowns { "ONE BAR OF CHUGS, THREE OF AIR", "CHUGS UNDER SPARSE HITS", "DOUBLE-HIT UNISON RIFF",
                                                 "ARPEGGIO, THEN THE RIFF UNDER IT" };
    const auto bridge = sectionTypesFor ("Bridge");
    const auto breakdown = sectionTypesFor ("Breakdown");
    for (size_t i = 0; i < bridges.size(); ++i)
        CHECK (bridge[7 + i].name == bridges[i]);   // (after 0.105.0's seven)
    for (size_t i = 0; i < breakdowns.size(); ++i)
        CHECK (breakdown[10 + i].name == breakdowns[i]);   // (after 0.109.0's ten)

    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    const auto check = [&] (const juce::String& section, const juce::String& name, SectionType::Band band)
    {
        ComposerState s;
        applySectionType (s, section, name);
        REQUIRE (sectionBand (s) == band);
        CHECK (sectionTypeCalled (name)->band == band);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && l != ComposerLayer::clean && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        const auto all = composeLayers (s, spans, c);
        for (const auto& idea : all)
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    };
    for (const auto& name : bridges)
        check ("Bridge", name, SectionType::Band::bridge);
    for (const auto& name : breakdowns)
        check ("Breakdown", name, SectionType::Band::breakdown);
}

TEST_CASE ("0.122.0: the rock songs' bridges and breakdowns, their shapes", "[sections][types][rock]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (Am F C G a bar each, 8 bars: 32 beats; the low string A0 = 33)
    const auto typed = [] (const juce::String& section, const juce::String& name, int guitar = -1, int drums = -1)
    {
        ComposerState s;
        applySectionType (s, section, name);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::drums).approach = drums;
        return s;
    };
    const auto starts = [] (const LineIdea& idea, int pitch, double from, double to)
    {
        std::set<int> out;   // (sixteenths from `from`)
        for (const auto& n : idea.notes)
            if (n.pitch == pitch && n.beat >= from - 1.0e-6 && n.beat < to - 1.0e-6)
                out.insert (juce::roundToInt ((n.beat - from) * 4.0));
        return out;
    };
    // ONE GUITAR RUNS, THE BAND HITS: the dyads never stop; the kick only on the hits, an empty bar in every four.
    {
        const auto all = composeLayers (typed ("Bridge", "ONE GUITAR RUNS, THE BAND HITS", 0, 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (starts (g, 33, 0.0, 4.0) == std::set<int> { 0, 2, 4, 6, 8, 10, 12, 14 });   // (every eighth on the low A)
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::kick, 0.0, 4.0) == std::set<int> { 0, 2 });
        CHECK (starts (d, GmDrum::kick, 4.0, 8.0) == std::set<int> { 14 });
        CHECK (starts (d, GmDrum::kick, 12.0, 16.0).empty());
        CHECK (starts (d, GmDrum::kick, 28.0, 32.0) == std::set<int> { 0, 10, 12, 14 });
        CHECK (starts (d, GmDrum::ride, 0.0, 32.0).empty());
    }
    // TWIN TREMOLO RUSH: two voices a third apart in sixteenths; the kick and snare together on the gapped grid; the
    // last bar cut at its half.
    {
        const auto all = composeLayers (typed ("Bridge", "TWIN TREMOLO RUSH", 0, 0), spans, c);
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::kick, 0.0, 4.0) == std::set<int> { 0, 1, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15 });
        CHECK (starts (d, GmDrum::snare, 0.0, 4.0) == starts (d, GmDrum::kick, 0.0, 4.0));
        auto last = 0.0;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            last = juce::jmax (last, n.beat + n.length);
        CHECK (last <= 30.0 + 1.0e-6);
    }
    // UPPER-FIFTH CHORDS: over Am the power chord sits on C (the third), held across the F bar: the bass moves under it.
    {
        const auto all = composeLayers (typed ("Bridge", "UPPER-FIFTH CHORDS OVER A MOVING BASS", 1), spans, c);
        int lowest = 128;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat < 1.0e-6)
                lowest = juce::jmin (lowest, n.pitch);
        CHECK (lowest % 12 == 0);   // C
        std::set<int> bassClasses;
        for (const auto& n : all[(size_t) ComposerLayer::bass].notes)
            if (n.beat < 8.0 - 1.0e-6)
                bassClasses.insert (n.pitch % 12);
        CHECK (bassClasses == std::set<int> { 9, 5 });   // A, then F under the same held chord
    }
    // CHROMATIC CREEP: the root then its two half steps above; no snare on 2 and 4, one every two bars.
    {
        const auto all = composeLayers (typed ("Bridge", "CHROMATIC CREEP, NO BACKBEAT", 1, 0), spans, c);
        std::set<int> bar0;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat < 4.0 - 1.0e-6)
                bar0.insert (n.pitch - 33);
        CHECK (bar0 == std::set<int> { 0, 1, 2 });
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::snare, 0.0, 32.0).size() == 4);
    }
    // KEYS CARRY THE CHORUS ALONE: guitars, bass and drums out; the keys' changes pushed an eighth early.
    {
        const auto all = composeLayers (typed ("Bridge", "KEYS CARRY THE CHORUS ALONE"), spans, c);
        CHECK (all[(size_t) ComposerLayer::guitar].notes.empty());
        CHECK (all[(size_t) ComposerLayer::drums].notes.empty());
        CHECK (all[(size_t) ComposerLayer::bass].notes.empty());
        auto pushed = false;
        for (const auto& n : all[(size_t) ComposerLayer::chords].notes)
            pushed = pushed || std::abs (n.beat - 3.5) < 1.0e-6;
        CHECK (pushed);
    }
    // ONE BAR OF CHUGS, THREE OF AIR: sixteen chugs, two hits, two silent bars where only the ride plays.
    {
        const auto all = composeLayers (typed ("Breakdown", "ONE BAR OF CHUGS, THREE OF AIR", 0, 1), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (starts (g, 33, 0.0, 4.0).size() == 16);
        auto air = 0;
        for (const auto& n : g.notes)
            if (n.beat >= 8.0 - 1.0e-6 && n.beat < 16.0 - 1.0e-6)
                ++air;
        CHECK (air == 0);
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::ride, 8.0, 16.0).size() == 8);
        CHECK (starts (d, GmDrum::kick, 0.0, 4.0).size() == 16);
        CHECK (starts (d, GmDrum::kick, 8.0, 16.0).empty());
    }
    // DOUBLE-HIT UNISON RIFF: the kick on every hit of the guitar, in pairs.
    {
        const auto all = composeLayers (typed ("Breakdown", "DOUBLE-HIT UNISON RIFF", 1, 0), spans, c);
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::kick, 4.0, 8.0) == std::set<int> { 0, 1, 8, 9, 12, 13 });
    }
    // ARPEGGIO, THEN THE RIFF UNDER IT: guitars and drums (but one crash) out for the first half; the last bar stops on 1-and.
    {
        const auto all = composeLayers (typed ("Breakdown", "ARPEGGIO, THEN THE RIFF UNDER IT", 0, 0), spans, c);
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            CHECK (n.beat >= 16.0 - 1.0e-6);
        auto drumsEarly = 0;
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.beat < 16.0 - 1.0e-6)
                ++drumsEarly;
        CHECK (drumsEarly == 1);
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat >= 28.0 - 1.0e-6)
                CHECK (n.beat <= 28.5 + 1.0e-6);
        CHECK (! all[(size_t) ComposerLayer::chords].notes.empty());
    }
}

TEST_CASE ("0.123.0: the rock songs' pre-choruses, every lane's options a different part", "[sections][types][rock]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> names { "SHIFTING BURSTS, KICK JOINS", "SNARE CARPET BUILD", "PEDAL WITH OPEN-HAT BEATS",
                                            "CHROMATIC CHUG, TREMOLO TRIAD", "OFF-BEAT SNARE, LOW BURSTS", "THREES ACROSS THE BAR" };
    const auto pre = sectionTypesFor ("Pre-Chorus");
    for (size_t i = 0; i < names.size(); ++i)
        CHECK (pre[6 + i].name == names[i]);   // (after 0.102.0's six)
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (const auto& name : names)
    {
        ComposerState s;
        applySectionType (s, "Pre-Chorus", name);
        REQUIRE (sectionBand (s) == SectionType::Band::preChorus);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && l != ComposerLayer::clean && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        for (const auto& idea : composeLayers (s, spans, c))
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.123.0: the rock songs' pre-choruses, their shapes", "[sections][types][rock]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (Am F C G a bar each, 8 bars: 32 beats; the low string A0 = 33)
    const auto typed = [] (const juce::String& name, int guitar = -1, int drums = -1)
    {
        ComposerState s;
        applySectionType (s, "Pre-Chorus", name);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::drums).approach = drums;
        return s;
    };
    const auto starts = [] (const LineIdea& idea, int pitch, double from, double to)
    {
        std::set<int> out;   // (sixteenths from `from`)
        for (const auto& n : idea.notes)
            if (n.pitch == pitch && n.beat >= from - 1.0e-6 && n.beat < to - 1.0e-6)
                out.insert (juce::roundToInt ((n.beat - from) * 4.0));
        return out;
    };
    // SHIFTING BURSTS: bursts of four, the next bar a beat later; half time first, then the kick on every burst.
    {
        const auto all = composeLayers (typed ("SHIFTING BURSTS, KICK JOINS", 0, 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (starts (g, 33, 0.0, 4.0) == std::set<int> { 0, 1, 2, 3, 8, 9, 10, 11 });
        CHECK (starts (g, 41, 4.0, 8.0) == std::set<int> { 4, 5, 6, 7, 12, 13, 14, 15 });   // (F's low root a beat later)
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::kick, 0.0, 4.0) == std::set<int> { 0 });
        CHECK (starts (d, GmDrum::snare, 0.0, 4.0) == std::set<int> { 8 });
        CHECK (starts (d, GmDrum::kick, 16.0, 20.0) == std::set<int> { 0, 1, 2, 3, 8, 9, 10, 11 });   // (halfway: with the bursts)
    }
    // SNARE CARPET: no kick for the first four bars, the snare on every sixteenth but one; then the groove with a shaker.
    {
        const auto all = composeLayers (typed ("SNARE CARPET BUILD", 0, 0), spans, c);
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::kick, 0.0, 16.0).empty());
        CHECK (starts (d, GmDrum::snare, 0.0, 4.0).size() == 15);
        CHECK (starts (d, 82, 16.0, 20.0).size() == 8);
        CHECK_FALSE (starts (d, GmDrum::kick, 16.0, 20.0).empty());
    }
    // PEDAL WITH OPEN-HAT BEATS: every sixteenth but the downbeat; the second bar climbs at its end; open hats on the beats.
    {
        const auto all = composeLayers (typed ("PEDAL WITH OPEN-HAT BEATS", 0, 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (starts (g, 33, 0.0, 4.0).size() == 15);
        CHECK (starts (g, 33, 0.0, 4.0).count (0) == 0);
        CHECK (starts (g, 43, 4.0, 8.0) == std::set<int> { 14 });   // (the low F + 2 near the end of bar 2)
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::openHat, 0.0, 4.0) == std::set<int> { 0, 4, 8, 12 });
        CHECK (starts (d, GmDrum::snare, 0.0, 4.0) == std::set<int> { 1, 4, 12 });
    }
    // CHROMATIC CHUG: x-xxx-xxxxx-xxx- with the half step on the late ones; the high triad over it; a roll at the end.
    {
        auto s = typed ("CHROMATIC CHUG, TREMOLO TRIAD", 0, 0);
        s.lane (ComposerLayer::lead).on = true;
        s.lane (ComposerLayer::lead).approach = 0;
        const auto all = composeLayers (s, spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (starts (g, 34, 0.0, 4.0) == std::set<int> { 4, 10, 14 });
        CHECK (starts (g, 33, 0.0, 4.0) == std::set<int> { 0, 2, 3, 6, 7, 8, 9, 12, 13 });
        std::set<int> triad;
        for (const auto& n : all[(size_t) ComposerLayer::lead].notes)
            triad.insert (n.pitch % 12);
        CHECK (triad == std::set<int> { 0, 4, 7 });   // (A minor: the relative major's, C E G)
        CHECK (starts (all[(size_t) ComposerLayer::drums], GmDrum::snare, 28.0, 32.0).size() == 16);
    }
    // OFF-BEAT SNARE: the snare only on the off eighths, never 2 and 4.
    {
        const auto all = composeLayers (typed ("OFF-BEAT SNARE, LOW BURSTS", 0, 0), spans, c);
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::snare, 0.0, 4.0) == std::set<int> { 2, 6, 10, 14 });
        CHECK (starts (d, GmDrum::kick, 0.0, 4.0) == std::set<int> { 0, 1, 3, 7, 9, 12, 13 });
    }
    // THREES ACROSS THE BAR: power chords every three beats across the bar lines in the first half (the 4/4 kept).
    {
        const auto all = composeLayers (typed ("THREES ACROSS THE BAR", 0, 0), spans, c);
        std::set<int> strikes;   // (beats of the power chords' fifths in the first half)
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            if (n.beat < 16.0 - 1.0e-6 && n.length > 0.8)
                strikes.insert (juce::roundToInt (n.beat));
        CHECK (strikes == std::set<int> { 0, 3, 6, 9, 12, 15 });
        CHECK (starts (all[(size_t) ComposerLayer::drums], GmDrum::crash, 0.0, 32.0).count (0) == 1);
    }
}

TEST_CASE ("0.124.0: the rock songs' intros and outros, their shapes", "[sections][types][rock]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (Am F C G a bar each, 8 bars: 32 beats; the low string A0 = 33)
    const auto typed = [] (const juce::String& section, const juce::String& name, int guitar = -1, int drums = -1)
    {
        ComposerState s;
        applySectionType (s, section, name);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::drums).approach = drums;
        return s;
    };
    const auto starts = [] (const LineIdea& idea, int pitch, double from, double to)
    {
        std::set<int> out;   // (sixteenths from `from`)
        for (const auto& n : idea.notes)
            if (n.pitch == pitch && n.beat >= from - 1.0e-6 && n.beat < to - 1.0e-6)
                out.insert (juce::roundToInt ((n.beat - from) * 4.0));
        return out;
    };
    // The style says intro or outro.
    CHECK (composeLayers (typed ("Intro", "KIT HITS IN PAIRS", 0, 0), spans, c)[(size_t) ComposerLayer::drums].style.startsWith ("Intro: "));
    CHECK (composeLayers (typed ("Outro", "EVERY OTHER BAR EMPTY", 0, 0), spans, c)[(size_t) ComposerLayer::drums].style.startsWith ("Outro: "));
    // BASS RIFF WITH TOMS: the floor tom on the bass's riff; no kick until the last quarter.
    {
        const auto all = composeLayers (typed ("Intro", "BASS RIFF WITH TOMS", 0, 0), spans, c);
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::tomFloor, 0.0, 4.0) == std::set<int> { 0, 1, 3, 4, 7, 9 });
        CHECK (starts (d, GmDrum::kick, 0.0, 24.0).empty());
        CHECK (starts (d, GmDrum::kick, 24.0, 28.0) == std::set<int> { 0, 1, 3, 4, 7, 9 });
        CHECK (starts (d, GmDrum::snare, 24.0, 28.0) == std::set<int> { 4, 12 });
    }
    // PEDAL SEESAW: eighths from the low tonic A, every other one above it; the fourth bar a burst a step under (G).
    {
        const auto all = composeLayers (typed ("Intro", "PEDAL SEESAW, DOUBLE KICK", 0, 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (starts (g, 33, 0.0, 4.0) == std::set<int> { 0, 4, 8, 12 });
        CHECK (starts (g, 43, 12.0, 16.0).size() == 16);
        CHECK (starts (all[(size_t) ComposerLayer::drums], GmDrum::kick, 0.0, 4.0).size() == 16);
    }
    // GAPPED PEDAL: the low root in sixteenths but on 2 and 4, the octave on 3; the last two bars the build.
    {
        const auto all = composeLayers (typed ("Intro", "GAPPED PEDAL, BUILD INTO THE VERSE", 0, 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (starts (g, 33, 0.0, 4.0) == std::set<int> { 0, 1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15 });
        CHECK (starts (g, 45, 0.0, 4.0) == std::set<int> { 8 });
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::snare, 0.0, 4.0) == std::set<int> { 8 });
        CHECK (starts (d, GmDrum::kick, 24.0, 28.0).size() == 16);
    }
    // KIT HITS IN PAIRS: the pairs on the dotted grid, then a bar of one crash.
    {
        const auto all = composeLayers (typed ("Intro", "KIT HITS IN PAIRS", 0, 0), spans, c);
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::snare, 0.0, 4.0) == std::set<int> { 0, 1, 6, 7, 12, 13 });
        CHECK (starts (d, GmDrum::snare, 4.0, 8.0).empty());
        CHECK (starts (d, GmDrum::crash, 4.0, 8.0) == std::set<int> { 0 });
    }
    // TWO CELLS IN THREES: the keys' cell of three notes never follows the chords (the same three classes over F as over Am).
    {
        const auto all = composeLayers (typed ("Intro", "TWO CELLS IN THREES", 0, 0), spans, c);
        std::set<int> overAm, overF;
        for (const auto& n : all[(size_t) ComposerLayer::chords].notes)
            (n.beat < 4.0 ? overAm : n.beat < 8.0 ? overF : overAm).insert (n.pitch % 12);
        CHECK (overAm.size() == 3);
        CHECK (overAm == overF);
    }
    // EVERY OTHER BAR EMPTY: bar 2 silent; the last chord held a whole bar.
    {
        const auto all = composeLayers (typed ("Outro", "EVERY OTHER BAR EMPTY", 0, 0), spans, c);
        auto inBar2 = 0;
        double lastLength = 0.0;
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
        {
            if (n.beat >= 4.0 - 1.0e-6 && n.beat < 8.0 - 1.0e-6)
                ++inBar2;
            if (n.beat >= 28.0 - 1.0e-6)
                lastLength = juce::jmax (lastLength, n.length);
        }
        CHECK (inBar2 == 0);
        CHECK (lastLength > 3.5);
    }
    // SINGLE-NOTE BAR, CHORD BAR: the first bar single notes with the 1 empty; the second power chords from the 1.
    {
        const auto all = composeLayers (typed ("Outro", "SINGLE-NOTE BAR, CHORD BAR", 0, 0), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        CHECK (starts (g, 33, 0.0, 4.0) == std::set<int> { 2, 4, 6, 8, 10, 12, 14 });
        CHECK (starts (g, 48, 4.0, 8.0).count (0) == 1);   // (F's fifth over the low F: C3)
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (starts (d, GmDrum::snare, 0.0, 4.0) == std::set<int> { 8 });
        CHECK (starts (d, GmDrum::crash, 0.0, 4.0) == std::set<int> { 0, 4, 8, 12 });
    }
}

TEST_CASE ("0.139.0: the twelve verse types from the 61 songs, every lane's options a different part", "[sections][types][verse139]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> names { "BASS RIFF, HIGH NOTES RUNG", "RIFF BAR, HOOK BAR", "DRUMS, BASS, GUITAR IN TURN", "TOMS AND OCTAVE BASS",
                                            "TOM FIGURE VERSE", "SIX-SIXTEENTH CELLS", "HOLED SIXTEENTH PEDAL", "GUITAR CALLS, BASS ANSWERS",
                                            "THREE-BAR CYCLE", "THREE-BAR RIFF, KICK AND BASS ON THE BEATS", "KICK AND BASS IN UNISON", "BASS GROOVE, GUITAR SQUEALS" };
    const auto verse = sectionTypesFor ("Verse");
    REQUIRE (verse.size() == 44u);   // (0.146.0: five more after them)
    for (size_t i = 0; i < names.size(); ++i)
    {
        CHECK (verse[14 + i].name == names[i]);
        CHECK (sectionTypeCalled (names[i])->band == SectionType::Band::verse);
        CHECK (isSectionTypeOf ("Pre-Chorus", names[i]));
    }
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (const auto& name : names)
    {
        ComposerState s;
        applySectionType (s, "Verse", name);
        REQUIRE (sectionBand (s) == SectionType::Band::verse);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        const auto all = composeLayers (s, spans, c);
        for (const auto& idea : all)
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.139.0: the verse types' shapes", "[sections][types][verse139]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();
    const auto typed = [] (const juce::String& name, int guitar = 0, int bass = 0, int keys = 0, int drums = 0)
    {
        ComposerState s;
        applySectionType (s, "Verse", name);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::bass).approach = bass;
        s.lane (ComposerLayer::chords).on = true;
        s.lane (ComposerLayer::chords).approach = keys;
        s.lane (ComposerLayer::drums).approach = drums;
        return s;
    };
    const auto has = [] (const LineIdea& idea, double beat, int pitch)
    {
        return std::any_of (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return std::abs (n.beat - beat) < 1.0e-6 && n.pitch == pitch; });
    };
    const auto starts = [] (const LineIdea& idea, double from, double to)
    {
        return std::count_if (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return n.beat >= from - 1.0e-6 && n.beat < to - 1.0e-6; });
    };
    // BASS RIFF, HIGH NOTES RUNG: the bass in sixteenths, the guitar only up high; the kick on the bass's hits.
    {
        const auto all = composeLayers (typed ("BASS RIFF, HIGH NOTES RUNG"), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        REQUIRE_FALSE (g.notes.empty());
        for (const auto& n : g.notes)
            CHECK (n.pitch >= 55);
        const auto& b = all[(size_t) ComposerLayer::bass];
        CHECK (starts (b, 0.0, 4.0) == 12);   // (x-xxx-xxx-xx-xxx)
        const auto& d = all[(size_t) ComposerLayer::drums];
        for (const auto& n : b.notes)
            CHECK (has (d, n.beat, GmDrum::kick));
    }
    // RIFF BAR, HOOK BAR: the bass and the kick in the even bars only, the keys' hook only in the odd ones.
    {
        const auto all = composeLayers (typed ("RIFF BAR, HOOK BAR"), spans, c);
        const auto& b = all[(size_t) ComposerLayer::bass];
        const auto& k = all[(size_t) ComposerLayer::chords];
        for (int bar = 0; bar < 8; ++bar)
        {
            INFO ("bar " << bar);
            CHECK ((starts (b, bar * 4.0, bar * 4.0 + 4.0) > 0) == (bar % 2 == 0));
            CHECK ((starts (k, bar * 4.0, bar * 4.0 + 4.0) > 0) == (bar % 2 == 1));
        }
        CHECK (all[(size_t) ComposerLayer::guitar].notes.empty());   // (OUT)
    }
    // DRUMS, BASS, GUITAR IN TURN: the drums alone for two bars, the bass from the third, the guitar from the fifth.
    {
        const auto all = composeLayers (typed ("DRUMS, BASS, GUITAR IN TURN"), spans, c);
        CHECK (starts (all[(size_t) ComposerLayer::drums], 0.0, 8.0) > 0);
        CHECK (starts (all[(size_t) ComposerLayer::bass], 0.0, 8.0) == 0);
        CHECK (starts (all[(size_t) ComposerLayer::bass], 8.0, 12.0) > 0);
        CHECK (starts (all[(size_t) ComposerLayer::guitar], 0.0, 16.0) == 0);
        CHECK (starts (all[(size_t) ComposerLayer::guitar], 16.0, 20.0) > 0);
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK_FALSE (has (d, 1.0, GmDrum::snare));
        CHECK (has (d, 25.0, GmDrum::snare));   // (the last quarter's backbeat)
    }
    // TOMS AND OCTAVE BASS: no snare before the last bar; the bass jumps the octave every other eighth.
    {
        const auto all = composeLayers (typed ("TOMS AND OCTAVE BASS"), spans, c);
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.pitch == GmDrum::snare)
                CHECK (n.beat >= 28.0 - 1.0e-6);
        const auto& b = all[(size_t) ComposerLayer::bass];
        REQUIRE (b.notes.size() > 2);
        CHECK (b.notes[1].pitch == b.notes[0].pitch + 12);
        CHECK (b.notes[2].pitch == b.notes[0].pitch);
    }
    // THREE-BAR CYCLE: the guitar's cell of nine and three comes round every twelve sixteenths across the bar lines; the snare
    // every six sixteenths, never on beat 2.
    {
        const auto all = composeLayers (typed ("THREE-BAR CYCLE"), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        for (int i = 0; i < 48; ++i)
        {
            const auto hit = std::any_of (g.notes.begin(), g.notes.end(), [&] (const LineNote& n) { return std::abs (n.beat - i * 0.25) < 1.0e-6; });
            CHECK (hit == (i % 12 < 9));
        }
        const auto& d = all[(size_t) ComposerLayer::drums];
        CHECK (has (d, 0.75, GmDrum::snare));
        CHECK (has (d, 2.25, GmDrum::snare));
        CHECK_FALSE (has (d, 1.0, GmDrum::snare));
    }
    // KICK AND BASS IN UNISON: the kick hit for hit with the bass, no snare for three bars; the guitar out for the first half.
    {
        const auto all = composeLayers (typed ("KICK AND BASS IN UNISON"), spans, c);
        const auto& b = all[(size_t) ComposerLayer::bass];
        const auto& d = all[(size_t) ComposerLayer::drums];
        REQUIRE_FALSE (b.notes.empty());
        for (const auto& n : b.notes)
            CHECK (has (d, n.beat, GmDrum::kick));
        for (const auto& n : d.notes)
            if (n.pitch == GmDrum::kick)
                CHECK (std::any_of (b.notes.begin(), b.notes.end(), [&] (const LineNote& x) { return std::abs (x.beat - n.beat) < 1.0e-6; }));
        CHECK (starts (d, 0.0, 12.0) == starts (b, 0.0, 12.0));   // (the kick alone)
        CHECK (starts (all[(size_t) ComposerLayer::guitar], 0.0, 16.0) == 0);
        CHECK (starts (all[(size_t) ComposerLayer::guitar], 16.0, 20.0) > 0);
    }
    // GUITAR CALLS, BASS ANSWERS: the guitar only in each bar's first three beats, the bass only in the fourth.
    {
        const auto all = composeLayers (typed ("GUITAR CALLS, BASS ANSWERS"), spans, c);
        for (const auto& n : all[(size_t) ComposerLayer::guitar].notes)
            CHECK (std::fmod (n.beat, 4.0) < 3.0 - 1.0e-6);
        for (const auto& n : all[(size_t) ComposerLayer::bass].notes)
            CHECK (std::fmod (n.beat, 4.0) >= 3.0 - 1.0e-6);
    }
    // HOLED SIXTEENTH PEDAL: the stream's two holes (the sixth and twelfth sixteenths) in the guitar and the kick.
    {
        const auto all = composeLayers (typed ("HOLED SIXTEENTH PEDAL"), spans, c);
        const auto& g = all[(size_t) ComposerLayer::guitar];
        const auto& d = all[(size_t) ComposerLayer::drums];
        for (int k = 0; k < 16; ++k)
        {
            const auto hole = k == 5 || k == 11;
            CHECK (has (g, k * 0.25, 33) != hole);
            CHECK (has (d, k * 0.25, GmDrum::kick) != hole);
        }
    }
    // SIX-SIXTEENTH CELLS: root-and-tritone dyads on the cells' third and fifth sixteenths.
    {
        const auto g = composeLayers (typed ("SIX-SIXTEENTH CELLS"), spans, c)[(size_t) ComposerLayer::guitar];
        CHECK (has (g, 0.5, 33 + 6));
        CHECK (has (g, 2.0, 33 + 6));   // (the second cell's third sixteenth: 6 + 2)
        CHECK (has (g, 1.5, 33));
    }
    // BASS GROOVE, GUITAR SQUEALS: a high held note with vibrato in the even bars, a burst at the odd bars' end.
    {
        const auto g = composeLayers (typed ("BASS GROOVE, GUITAR SQUEALS"), spans, c)[(size_t) ComposerLayer::guitar];
        CHECK (std::any_of (g.notes.begin(), g.notes.end(), [] (const LineNote& n) { return n.beat < 1.0e-6 && n.pitch >= 79 && n.effect == LineNote::Effect::vibrato; }));
        CHECK (starts (g, 6.5, 8.0) == 6);
    }
}

TEST_CASE ("0.140.0: the 23 chorus types from the 61 songs, every lane's options a different part", "[sections][types][chorus140]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> names { "DOTTED STREAM", "NEIGHBOUR TRILL WALL", "TWO RUNG CHORDS A BAR", "THREE DOTTED PUSHES", "FOUR ON THE FLOOR, DYAD RIFF",
                                            "SPLIT ON THE AND OF THREE", "TREMOLO DYADS OVER RUNG CHORDS", "DOTTED CHORDS, HIGH TREMOLO NOTE", "THE RIFF IS THE CHORUS",
                                            "HIGH TREMOLO DYAD OVER THE RIFF", "HELD CHORDS IN 3+3+2", "DOTTED PEDAL CELL", "FIXED SHAPE, CREEPING ROOT",
                                            "CHROMATIC CHORDS UNDER A FIXED TREMOLO", "HALF-NOTE CHORDS, OFF-BEAT OPEN HATS", "HIT, HOLD, BURST INTO THE NEXT",
                                            "PUSH BAR, CELL BAR", "SYNCOPATED CHORDS WITH A PUSH BAR", "THREE-SIXTEENTH CELLS AFTER A HELD ONE", "SHRINKING GROUPS",
                                            "HELD ROOTS CLIMBING, PUSHED LATE", "FALLING CELLS IN SIXES", "FOUR EIGHTHS, THEN A PUSH" };
    const auto chorus = sectionTypesFor ("Chorus");
    REQUIRE (chorus.size() == 11 + names.size() + 11);   // (0.148.0: eleven more)
    for (size_t i = 0; i < names.size(); ++i)
    {
        CHECK (chorus[11 + i].name == names[i]);
        CHECK (sectionTypeCalled (names[i])->band == SectionType::Band::chorus);
        CHECK (isSectionTypeOf ("Post-Chorus", names[i]));   // (a post-chorus lists the chorus's types after its own)
    }
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (const auto& name : names)
    {
        ComposerState s;
        applySectionType (s, "Chorus", name);
        REQUIRE (sectionBand (s) == SectionType::Band::chorus);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        const auto all = composeLayers (s, spans, c);
        for (const auto& idea : all)
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.140.0: the chorus types' shapes", "[sections][types][chorus140]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();
    const auto typed = [] (const juce::String& name, int guitar = 0, int bass = 0, int drums = 0, int lead = 0)
    {
        ComposerState s;
        applySectionType (s, "Chorus", name);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::bass).approach = bass;
        s.lane (ComposerLayer::drums).approach = drums;
        s.lane (ComposerLayer::lead).on = true;
        s.lane (ComposerLayer::lead).approach = lead;
        return s;
    };
    const auto starts = [] (const LineIdea& idea)
    {
        std::set<int> at;
        for (const auto& n : idea.notes)
            at.insert (juce::roundToInt (n.beat * 4.0));   // (sixteenths from the start)
        return at;
    };
    const auto has = [] (const LineIdea& idea, double beat, int pitch)
    {
        return std::any_of (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return std::abs (n.beat - beat) < 1.0e-6 && n.pitch == pitch; });
    };
    // DOTTED STREAM: the guitar on 3+3+3+3+2+2; the bass on the same hits; the kick on the dotted ones.
    {
        const auto all = composeLayers (typed ("DOTTED STREAM"), spans, c);
        const auto g = starts (all[(size_t) ComposerLayer::guitar]);
        for (int k = 0; k < 16; ++k)
            CHECK ((g.count (k) > 0) == (std::string ("x--x--x--x--x-xx")[(size_t) k] == 'x'));
        CHECK (starts (all[(size_t) ComposerLayer::bass]) == g);
        const auto& d = all[(size_t) ComposerLayer::drums];
        for (const auto k : { 0, 3, 6, 10 })
            CHECK (has (d, k * 0.25, GmDrum::kick));
    }
    // NEIGHBOUR TRILL WALL: a chord on every sixteenth, the bass alternating two notes.
    {
        const auto all = composeLayers (typed ("NEIGHBOUR TRILL WALL"), spans, c);
        CHECK (starts (all[(size_t) ComposerLayer::guitar]).size() == 128u);
        const auto& b = all[(size_t) ComposerLayer::bass];
        REQUIRE (b.notes.size() > 3);
        CHECK (b.notes[0].pitch != b.notes[1].pitch);
        CHECK (b.notes[0].pitch == b.notes[2].pitch);
    }
    // SPLIT ON THE AND OF THREE: the next bar's chord pushed in on the and of 3 (Am bar 0, F next: F's root at 2.5).
    {
        const auto g = composeLayers (typed ("SPLIT ON THE AND OF THREE"), spans, c)[(size_t) ComposerLayer::guitar];
        CHECK (has (g, 0.0, 33));      // (A)
        CHECK (has (g, 2.5, 41));      // (F, pushed)
    }
    // FOUR ON THE FLOOR, DYAD RIFF: the kick on every beat, an open hat on every off-beat eighth.
    {
        const auto d = composeLayers (typed ("FOUR ON THE FLOOR, DYAD RIFF"), spans, c)[(size_t) ComposerLayer::drums];
        for (int b = 0; b < 32; ++b)
        {
            CHECK (has (d, b, GmDrum::kick));
            CHECK (has (d, b + 0.5, GmDrum::openHat));
            CHECK_FALSE (has (d, b, GmDrum::openHat));
        }
    }
    // SHRINKING GROUPS: a crash on each group's start: eighths 0, 9, 17 and 24 of every four bars.
    {
        const auto d = composeLayers (typed ("SHRINKING GROUPS"), spans, c)[(size_t) ComposerLayer::drums];
        for (const auto eighth : { 0, 9, 17, 24, 32, 41 })
            CHECK (has (d, eighth * 0.5, GmDrum::crash));
        CHECK_FALSE (has (d, 4.0, GmDrum::crash));   // (bar 2's one is inside the first group)
    }
    // HIGH TREMOLO DYAD OVER THE RIFF: the lead the same two notes the whole chorus, over every chord.
    {
        const auto l = composeLayers (typed ("HIGH TREMOLO DYAD OVER THE RIFF"), spans, c)[(size_t) ComposerLayer::lead];
        std::set<int> pitches;
        for (const auto& n : l.notes)
            pitches.insert (n.pitch);
        CHECK (pitches.size() == 2u);
        CHECK (l.notes.size() == 256u);   // (two notes on every sixteenth of eight bars)
    }
    // HIT, HOLD, BURST INTO THE NEXT: the next chord's root on the bar's last three sixteenths but one.
    {
        const auto g = composeLayers (typed ("HIT, HOLD, BURST INTO THE NEXT"), spans, c)[(size_t) ComposerLayer::guitar];
        for (const auto k : { 12, 13, 15 })
            CHECK (has (g, k * 0.25, 41));   // (F after Am)
        CHECK_FALSE (has (g, 14 * 0.25, 41));
    }
    // FIXED SHAPE, CREEPING ROOT: the arpeggio's top notes the same in every bar; only its root moves.
    {
        const auto g = composeLayers (typed ("FIXED SHAPE, CREEPING ROOT"), spans, c)[(size_t) ComposerLayer::guitar];
        std::set<int> tops;
        std::set<int> roots;
        for (const auto& n : g.notes)
        {
            const auto inBar = std::fmod (n.beat, 4.0);
            (inBar < 1.0e-6 ? roots : tops).insert (n.pitch);
        }
        CHECK (tops.size() == 3u);
        CHECK (roots.size() == 4u);   // (Am F C G)
    }
}

TEST_CASE ("0.141.0: the pre-chorus and post-chorus types from the 61 songs, every lane's options a different part", "[sections][types][round141]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> pres { "KICK OUT, LOW TREMOLO", "UNISON ROCKING RIFF", "RING & STAB UNDER CYMBAL EIGHTHS", "UNISON EIGHTHS BUILD",
                                           "TREMOLO PEDAL, THEN OCTAVE PUMP", "PAIRS IN 3+3+2, RING BAR", "OCTAVES ROCKING TO THE TRITONE" };
    const std::vector<juce::String> posts { "DOWN TO THE LOOP", "HATS AND ROOTS", "HITS ON FOUR", "UNISON CHROMATIC RUNS", "DOTTED NOTE OVER THE RIFF",
                                            "SNARE ONLY ON FOUR AND FOUR-AND" };
    const auto pre = sectionTypesFor ("Pre-Chorus");
    const auto post = sectionTypesFor ("Post-Chorus");
    for (size_t i = 0; i < pres.size(); ++i)
    {
        CHECK (pre[12 + i].name == pres[i]);
        CHECK (sectionTypeCalled (pres[i])->band == SectionType::Band::preChorus);
        CHECK_FALSE (isSectionTypeOf ("Verse", pres[i]));
    }
    for (size_t i = 0; i < posts.size(); ++i)
    {
        CHECK (post[11 + i].name == posts[i]);
        CHECK (sectionTypeCalled (posts[i])->band == SectionType::Band::interlude);
        CHECK (isSectionTypeOf ("Interlude", posts[i]));
    }
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    auto all = pres;
    all.insert (all.end(), posts.begin(), posts.end());
    for (const auto& name : all)
    {
        ComposerState s;
        applySectionType (s, std::find (pres.begin(), pres.end(), name) != pres.end() ? "Pre-Chorus" : "Post-Chorus", name);
        REQUIRE (s.sectionType == name);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        for (const auto& idea : composeLayers (s, spans, c))
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.141.0: the pre-chorus and post-chorus types' shapes", "[sections][types][round141]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();
    const auto typed = [] (const juce::String& section, const juce::String& name)
    {
        ComposerState s;
        applySectionType (s, section, name);
        for (const auto l : composerLayers())
            s.lane (l).approach = 0;
        return s;
    };
    const auto has = [] (const LineIdea& idea, double beat, int pitch)
    {
        return std::any_of (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return std::abs (n.beat - beat) < 1.0e-6 && n.pitch == pitch; });
    };
    const auto count = [] (const LineIdea& idea, int pitch)
    {
        return std::count_if (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return n.pitch == pitch; });
    };
    // KICK OUT, LOW TREMOLO: no kick at all; the guitar on every sixteenth.
    {
        const auto all = composeLayers (typed ("Pre-Chorus", "KICK OUT, LOW TREMOLO"), spans, c);
        CHECK (count (all[(size_t) ComposerLayer::drums], GmDrum::kick) == 0);
        CHECK (has (all[(size_t) ComposerLayer::guitar], 0.25, 33));
        CHECK (has (all[(size_t) ComposerLayer::guitar], 1.25, 34));   // (beat 2: a half step up)
    }
    // UNISON EIGHTHS BUILD: the kick, the snare and the floor tom on every eighth but in the last bar.
    {
        const auto d = composeLayers (typed ("Pre-Chorus", "UNISON EIGHTHS BUILD"), spans, c)[(size_t) ComposerLayer::drums];
        for (int k = 0; k < 8; ++k)
            for (const auto note : { GmDrum::kick, GmDrum::snare, GmDrum::tomFloor })
                CHECK (has (d, k * 0.5, note));
        CHECK_FALSE (has (d, 29.0, GmDrum::kick));   // (the last bar only on 1, 1-and, 4 and 4-and)
        CHECK (has (d, 30.0, GmDrum::kick) == false);
        CHECK (has (d, 31.0, GmDrum::kick));
    }
    // HITS ON FOUR: the guitar only on beat 4, the next chord (F after Am).
    {
        const auto g = composeLayers (typed ("Post-Chorus", "HITS ON FOUR"), spans, c)[(size_t) ComposerLayer::guitar];
        for (const auto& n : g.notes)
            CHECK (std::fmod (n.beat, 4.0) == Catch::Approx (3.0));
        CHECK (has (g, 3.0, 41));
    }
    // SNARE ONLY ON FOUR AND FOUR-AND: no snare but on beat 4 and its and.
    {
        const auto d = composeLayers (typed ("Post-Chorus", "SNARE ONLY ON FOUR AND FOUR-AND"), spans, c)[(size_t) ComposerLayer::drums];
        for (const auto& n : d.notes)
            if (n.pitch == GmDrum::snare)
            {
                const auto inBar = std::fmod (n.beat, 4.0);
                CHECK ((std::abs (inBar - 3.0) < 1.0e-6 || std::abs (inBar - 3.5) < 1.0e-6));
            }
        CHECK (count (d, GmDrum::snare) == 16);
    }
    // DOTTED NOTE OVER THE RIFF: the guitar every three sixteenths.
    {
        const auto g = composeLayers (typed ("Post-Chorus", "DOTTED NOTE OVER THE RIFF"), spans, c)[(size_t) ComposerLayer::guitar];
        REQUIRE (g.notes.size() > 4);
        for (size_t i = 0; i < g.notes.size(); ++i)
            CHECK (g.notes[i].beat == Catch::Approx (0.75 * (double) i));
    }
    // DOWN TO THE LOOP: the keys' one note on twelve sixteenths, then three rising.
    {
        const auto k = composeLayers (typed ("Post-Chorus", "DOWN TO THE LOOP"), spans, c)[(size_t) ComposerLayer::chords];
        REQUIRE (k.notes.size() >= 15);
        for (int i = 0; i < 12; ++i)
            CHECK (k.notes[(size_t) i].pitch == k.notes[0].pitch);
        CHECK (k.notes[12].pitch > k.notes[0].pitch);
    }
}

TEST_CASE ("0.142.0: the bridge and breakdown types from the 61 songs, every lane's options a different part", "[sections][types][round142]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> bridges { "CLEAN FIGURE, THEN DISTORTED", "TOM OSTINATO, BAND THIN", "TOMS DOUBLE THE PEDAL", "DOMINANT PEDAL BUILD",
                                              "TREMOLO CLUSTER OVER OPEN CHORDS", "PIANO AND CHOIR ALONE", "TRESILLO BOTH HALVES", "BASS ALONE, DOTTED",
                                              "RIFF ON THE FOUR, NO BACKBEAT, THEN HOME", "BASS HOLDS THE GRID, THE BAND CARVES IT", "DRUMS DROP OUT, THE BAND CARRIES" };
    const std::vector<juce::String> breakdowns { "FOUR ON THE FLOOR, NO SNARE", "OCTAVE PUMP WALL", "BASS RIFF ALONE, GHOST GROOVE JOINS",
                                                 "SEVEN-BEAT RIFF ACROSS THE BAR", "KEYS ALONE, THEN RINGING HITS" };
    const auto bridge = sectionTypesFor ("Bridge");
    const auto breakdown = sectionTypesFor ("Breakdown");
    for (size_t i = 0; i < bridges.size(); ++i)
    {
        CHECK (bridge[16 + i].name == bridges[i]);
        CHECK (sectionTypeCalled (bridges[i])->band == SectionType::Band::bridge);
    }
    for (size_t i = 0; i < breakdowns.size(); ++i)
    {
        CHECK (breakdown[14 + i].name == breakdowns[i]);
        CHECK (sectionTypeCalled (breakdowns[i])->band == SectionType::Band::breakdown);
    }
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    auto all = bridges;
    all.insert (all.end(), breakdowns.begin(), breakdowns.end());
    for (const auto& name : all)
    {
        ComposerState s;
        applySectionType (s, std::find (bridges.begin(), bridges.end(), name) != bridges.end() ? "Bridge" : "Breakdown", name);
        REQUIRE (s.sectionType == name);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        for (const auto& idea : composeLayers (s, spans, c))
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.142.0: the bridge and breakdown types' shapes", "[sections][types][round142]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();
    const auto typed = [] (const juce::String& section, const juce::String& name)
    {
        ComposerState s;
        applySectionType (s, section, name);
        for (const auto l : composerLayers())
            s.lane (l).approach = 0;
        return s;
    };
    const auto count = [] (const LineIdea& idea, int pitch, double from = 0.0, double to = 1.0e9)
    {
        return std::count_if (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return n.pitch == pitch && n.beat >= from - 1.0e-6 && n.beat < to - 1.0e-6; });
    };
    // TOM OSTINATO, BAND THIN: no cymbals at all; the bass's three stages.
    {
        const auto all = composeLayers (typed ("Bridge", "TOM OSTINATO, BAND THIN"), spans, c);
        const auto& d = all[(size_t) ComposerLayer::drums];
        for (const auto cymbal : { GmDrum::crash, GmDrum::ride, GmDrum::closedHat, GmDrum::openHat })
            CHECK (count (d, cymbal) == 0);
        const auto& b = all[(size_t) ComposerLayer::bass];
        CHECK (std::count_if (b.notes.begin(), b.notes.end(), [] (const LineNote& n) { return n.beat < 8.0; }) == 1);   // (one root in the first two bars)
        CHECK (std::count_if (b.notes.begin(), b.notes.end(), [] (const LineNote& n) { return n.beat >= 24.0; }) == 16);   // (eighths at the end)
    }
    // CLEAN FIGURE, THEN DISTORTED: only the kick in the first half, hats from halfway.
    {
        const auto d = composeLayers (typed ("Bridge", "CLEAN FIGURE, THEN DISTORTED"), spans, c)[(size_t) ComposerLayer::drums];
        CHECK (count (d, GmDrum::kick, 0.0, 16.0) == 16);
        CHECK (count (d, GmDrum::closedHat, 0.0, 16.0) == 0);
        CHECK (count (d, GmDrum::closedHat, 16.0, 32.0) == 32);
    }
    // DOMINANT PEDAL BUILD: every guitar note on the key's fifth (E) or its neighbours in the first third.
    {
        const auto g = composeLayers (typed ("Bridge", "DOMINANT PEDAL BUILD"), spans, c)[(size_t) ComposerLayer::guitar];
        for (const auto& n : g.notes)
            if (n.beat >= 12.0)
                CHECK ((n.pitch % 12 == 4 || n.pitch % 12 == 11));   // (E, or the fifth over it in the power chords)
    }
    // OCTAVE PUMP WALL: the kick on every eighth, a crash on every beat.
    {
        const auto d = composeLayers (typed ("Breakdown", "OCTAVE PUMP WALL"), spans, c)[(size_t) ComposerLayer::drums];
        CHECK (count (d, GmDrum::kick) == 64);
        CHECK (count (d, GmDrum::crash) == 32);
    }
    // FOUR ON THE FLOOR, NO SNARE: the kick on every beat, no snare before the last bar.
    {
        const auto d = composeLayers (typed ("Breakdown", "FOUR ON THE FLOOR, NO SNARE"), spans, c)[(size_t) ComposerLayer::drums];
        CHECK (count (d, GmDrum::kick) == 32);
        CHECK (count (d, GmDrum::snare, 0.0, 28.0) == 0);
        CHECK (count (d, GmDrum::snare, 28.0) > 0);
    }
    // SEVEN-BEAT RIFF ACROSS THE BAR: the riff comes round every seven beats.
    {
        const auto g = composeLayers (typed ("Breakdown", "SEVEN-BEAT RIFF ACROSS THE BAR"), spans, c)[(size_t) ComposerLayer::guitar];
        std::set<int> at;
        for (const auto& n : g.notes)
            at.insert (juce::roundToInt (n.beat * 4.0));
        for (int i = 0; i < 28; ++i)
            CHECK (at.count (i) == at.count (i + 28));
    }
    // PIANO AND CHOIR ALONE: nothing but the keys (and a roll to end).
    {
        const auto all = composeLayers (typed ("Bridge", "PIANO AND CHOIR ALONE"), spans, c);
        CHECK (all[(size_t) ComposerLayer::guitar].notes.empty());
        CHECK (all[(size_t) ComposerLayer::bass].notes.empty());
        CHECK_FALSE (all[(size_t) ComposerLayer::chords].notes.empty());
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            CHECK (n.beat >= 31.0 - 1.0e-6);
    }
}

TEST_CASE ("0.143.0: the intro and outro types from the 61 songs, every lane's options a different part", "[sections][types][round143]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const std::vector<juce::String> intros { "HOOK KEPT, BAND BUILT UNDER IT", "RIFF GROWS FROM THE RIDE", "TOM GROOVE ALONE, THEN THE RIFF", "TOMS ON THE CELL",
                                             "PADS, THEN THE MACHINE LOOP", "HEARTBEAT KICK", "DRONE, DRUMS, THEN THE RIFF", "KEYS ALONE, THEN THE RIFF" };
    const std::vector<juce::String> outros { "UNISON LINE TO THE END", "DOTTED-EIGHTH HIT CHAIN", "UNISON CLIMB TO THE END", "THREE DOTTED PUSHES, THEN RING",
                                             "HITS AT BOTH ENDS", "CHANT OVER A PEDAL BURST", "BAND DROPS TO PADS AND PIANO", "TOM WALL UNDER THE LAST CHORD" };
    const auto intro = sectionTypesFor ("Intro");
    const auto outro = sectionTypesFor ("Outro");
    for (size_t i = 0; i < intros.size(); ++i)
    {
        CHECK (intro[15 + i].name == intros[i]);
        CHECK (sectionTypeCalled (intros[i])->band == SectionType::Band::bookend);
    }
    for (size_t i = 0; i < outros.size(); ++i)
    {
        CHECK (outro[12 + i].name == outros[i]);
        CHECK (sectionTypeCalled (outros[i])->band == SectionType::Band::bookend);
    }
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    auto all = intros;
    all.insert (all.end(), outros.begin(), outros.end());
    for (const auto& name : all)
    {
        ComposerState s;
        applySectionType (s, std::find (intros.begin(), intros.end(), name) != intros.end() ? "Intro" : "Outro", name);
        REQUIRE (s.sectionType == name);
        for (const auto l : composerLayers())
        {
            const auto options = sectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) options.size());
            std::set<juce::String> seen;
            for (const auto& idea : choices)
            {
                INFO (name << " " << composerLayerName (l) << " " << idea.style);
                CHECK (idea.notes.size() + (idea.style.containsIgnoreCase ("out") ? 1 : 0) > 0);
                seen.insert (key (idea));
            }
            INFO (name << " " << composerLayerName (l));
            CHECK (seen.size() == choices.size());
        }
        for (const auto& idea : composeLayers (s, spans, c))
            for (const auto& n : idea.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
    }
}

TEST_CASE ("0.143.0: the intro and outro types' shapes", "[sections][types][round143]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();
    const auto typed = [] (const juce::String& section, const juce::String& name)
    {
        ComposerState s;
        applySectionType (s, section, name);
        for (const auto l : composerLayers())
            s.lane (l).approach = 0;
        return s;
    };
    const auto count = [] (const LineIdea& idea, int pitch, double from = 0.0, double to = 1.0e9)
    {
        return std::count_if (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return n.pitch == pitch && n.beat >= from - 1.0e-6 && n.beat < to - 1.0e-6; });
    };
    const auto notesIn = [] (const LineIdea& idea, double from, double to)
    {
        return std::count_if (idea.notes.begin(), idea.notes.end(), [&] (const LineNote& n) { return n.beat >= from - 1.0e-6 && n.beat < to - 1.0e-6; });
    };
    // HOOK KEPT, BAND BUILT UNDER IT: the keys from the start, the drums from a quarter in, the guitar only at the landing (bar 7).
    {
        const auto all = composeLayers (typed ("Intro", "HOOK KEPT, BAND BUILT UNDER IT"), spans, c);
        CHECK (notesIn (all[(size_t) ComposerLayer::chords], 0.0, 4.0) > 0);
        CHECK (notesIn (all[(size_t) ComposerLayer::drums], 0.0, 8.0) == 0);
        CHECK (notesIn (all[(size_t) ComposerLayer::drums], 8.0, 12.0) > 0);
        CHECK (notesIn (all[(size_t) ComposerLayer::guitar], 0.0, 24.0) == 0);
        CHECK (notesIn (all[(size_t) ComposerLayer::guitar], 24.0, 28.0) > 0);
    }
    // HEARTBEAT KICK: the kick on 1, the a of 1 and 3 (and its a), nothing else in the first half.
    {
        const auto d = composeLayers (typed ("Intro", "HEARTBEAT KICK"), spans, c)[(size_t) ComposerLayer::drums];
        CHECK (count (d, GmDrum::kick, 0.0, 4.0) == 4);
        CHECK (notesIn (d, 0.0, 16.0) == 16);
    }
    // PADS, THEN THE MACHINE LOOP: no drums before the last third, the pedal hat on every sixteenth after.
    {
        const auto d = composeLayers (typed ("Intro", "PADS, THEN THE MACHINE LOOP"), spans, c)[(size_t) ComposerLayer::drums];
        CHECK (notesIn (d, 0.0, 20.0) == 0);
        CHECK (count (d, GmDrum::pedalHat, 20.0, 24.0) == 16);
        CHECK (count (d, GmDrum::snare) == 0);
    }
    // DOTTED-EIGHTH HIT CHAIN: hits on the one, then every three sixteenths (4, 7, 10, 13, 16...) across the bar lines.
    {
        const auto g = composeLayers (typed ("Outro", "DOTTED-EIGHTH HIT CHAIN"), spans, c)[(size_t) ComposerLayer::guitar];
        std::set<int> at;
        for (const auto& n : g.notes)
            at.insert (juce::roundToInt (n.beat * 4.0));
        for (const auto k : { 0, 4, 7, 10, 13, 16, 19, 22 })
            CHECK (at.count (k) == 1);
        CHECK (at.count (1) == 0);
        CHECK (at.count (28) == 1);   // (the last bar's held chord)
    }
    // HITS AT BOTH ENDS: the guitar only on 1, 1-and, 4 and 4-and.
    {
        const auto g = composeLayers (typed ("Outro", "HITS AT BOTH ENDS"), spans, c)[(size_t) ComposerLayer::guitar];
        for (const auto& n : g.notes)
        {
            const auto k = juce::roundToInt (std::fmod (n.beat, 4.0) * 4.0);
            CHECK ((k == 0 || k == 2 || k == 12 || k == 14));
        }
    }
    // TOM WALL UNDER THE LAST CHORD: the kick and the floor tom on every sixteenth of the second half.
    {
        const auto d = composeLayers (typed ("Outro", "TOM WALL UNDER THE LAST CHORD"), spans, c)[(size_t) ComposerLayer::drums];
        CHECK (count (d, GmDrum::kick, 16.0, 32.0) == 64);
        CHECK (count (d, GmDrum::tomFloor, 16.0, 32.0) == 64);
    }
    // BAND DROPS TO PADS AND PIANO: the guitar and bass only in the first two bars.
    {
        const auto all = composeLayers (typed ("Outro", "BAND DROPS TO PADS AND PIANO"), spans, c);
        CHECK (notesIn (all[(size_t) ComposerLayer::guitar], 8.0, 32.0) == 0);
        CHECK (notesIn (all[(size_t) ComposerLayer::bass], 8.0, 32.0) == 0);
        CHECK (notesIn (all[(size_t) ComposerLayer::chords], 28.0, 32.0) > 0);
    }
}

TEST_CASE ("0.144.0: the 21 transitions from the 61 songs", "[sections][transitions][trans144]")
{
    const std::vector<juce::String> names { "3+3+2 PICKUP, CUT ON THREE", "DOTTED-EIGHTH FILL", "RIFF ALONE BAR", "BURST PICKUP", "SCALE RUN UP",
                                            "BAND EIGHTH RAMP", "HALF-STEP PICKUP STABS", "HELD BAR", "TREMOLO HOLD", "RIFF CUT SHORT", "TWO SILENT BARS",
                                            "DROP TO BASS AND KICK", "TOM CASCADE", "STRINGS RUN, THEN HOLD", "HIT, TWO PUSHES, SILENCE",
                                            "SNARE BURST, HIT AND HOLD", "LONG STOP, BELL COUNT-IN", "UNISON EIGHTHS BAR", "CYMBAL ROLL BAR",
                                            "GROWING CHORD", "KICK & SNARE ROLL" };
    const auto& list = sectionTransitions();
    for (size_t i = 0; i < names.size(); ++i)
        CHECK (list[14 + i].name == names[i]);
    const auto plain = withTransition (noTransition);
    for (const auto& name : names)
    {
        INFO (name);
        const auto all = withTransition (name);
        // The melody untouched, the bars before the last two as they were, nothing past the end.
        CHECK (sameNotes (all[(size_t) ComposerLayer::melody], plain[(size_t) ComposerLayer::melody]));
        bool changed = false;
        for (const auto l : bandLanes)
        {
            const auto& a = all[(size_t) l];
            const auto& b = plain[(size_t) l];
            changed = changed || ! sameNotes (a, b);
            for (const auto& n : a.notes)
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
            std::vector<LineNote> early, earlyPlain;
            for (const auto& n : a.notes)
                if (n.beat + n.length <= 20.0 + 1.0e-6)
                    early.push_back (n);
            for (const auto& n : b.notes)
                if (n.beat + n.length <= 20.0 + 1.0e-6)
                    earlyPlain.push_back (n);
            CHECK (early.size() == earlyPlain.size());
        }
        CHECK (changed);
    }
    // TWO SILENT BARS: nothing starts in the last two bars.
    for (const auto l : bandLanes)
        checkQuietFrom (withTransition ("TWO SILENT BARS")[(size_t) l], 24.0, -1.0, 24.0);
    // RIFF CUT SHORT: nothing after the last bar's twelfth sixteenth.
    for (const auto l : bandLanes)
        checkQuietFrom (withTransition ("RIFF CUT SHORT")[(size_t) l], 31.0, -1.0, 31.0);
    // DROP TO BASS AND KICK: only kicks in the drums' last bar, the guitar and keys silent there.
    {
        const auto all = withTransition ("DROP TO BASS AND KICK");
        for (const auto& n : all[(size_t) ComposerLayer::drums].notes)
            if (n.beat >= 28.0 - 1.0e-6)
                CHECK (n.pitch == 36);
        checkQuietFrom (all[(size_t) ComposerLayer::guitar], 28.0, -1.0, 28.0);
    }
    // DOTTED-EIGHTH FILL: the snare and toms on sixteenths 4, 7, 10 and 13 of the last bar.
    {
        const auto d = withTransition ("DOTTED-EIGHTH FILL")[(size_t) ComposerLayer::drums];
        std::set<int> at;
        for (const auto& n : d.notes)
            if (n.beat >= 28.0 - 1.0e-6 && (n.pitch == 38 || n.pitch == 45))
                at.insert (juce::roundToInt ((n.beat - 28.0) * 4.0));
        CHECK (at == std::set<int> { 4, 7, 10, 13 });
    }
    // TOM CASCADE: sixteen tom hits falling high to floor.
    {
        const auto d = withTransition ("TOM CASCADE")[(size_t) ComposerLayer::drums];
        std::vector<int> toms;
        for (const auto& n : d.notes)
            if (n.beat >= 28.0 - 1.0e-6)
                toms.push_back (n.pitch);
        REQUIRE (toms.size() == 16u);
        CHECK (toms.front() == 48);
        CHECK (toms.back() == 41);
    }
    // SCALE RUN UP: the bass's last note a step under the next chord's root (A, the first chord, after G).
    {
        const auto b = withTransition ("SCALE RUN UP")[(size_t) ComposerLayer::bass];
        REQUIRE_FALSE (b.notes.empty());
        CHECK ((b.notes.back().pitch % 12 == 7));   // (G, a step under A)
        CHECK (b.notes.back().beat == Catch::Approx (31.75));
    }
}

TEST_CASE ("0.154.0: round four's 27 transitions", "[sections][transitions][trans154]")
{
    const std::vector<juce::String> names { "RISER INTO THE NEXT", "UNISON SYNCOPATED BAR", "CLAP BREAK", "DOTTED HITS, THEN A SILENT BAR",
                                            "THREE STABS AND A HOLD, TWICE", "TIMPANI ROLL", "HIT ON ONE, BASS FIGURE", "THREE BEATS OF SILENCE",
                                            "TRITONE HIT, RUNG BAR", "CHORD ON ONE, SNARE PICKUP", "GALLOP BAR, SNARE ON THE ANDS", "TWO HITS AND TOMS",
                                            "PUSHED STOP", "HELD BAR, DRUMS PICK UP", "STRINGS CLIMB, TOM ROLL", "HIT, THEN MUTED EIGHTHS IN",
                                            "DROP TO CLEAN, SNARE BUILD", "STOP, THREE-SNARE PICKUP", "HITS ON TWO AND THREE, TOMS UNDER",
                                            "TOM RUMBLE BARS", "HALF-STEP BURST BAR", "RIFF ALONE, TWO BARS", "UNISON SIXTEENTHS BAR",
                                            "SIXTEENTH SWELL, CUT SHORT", "SIX-SIXTEENTH TURNAROUND", "DROP TO KEYS AND LEAD", "CUT AT THE HALF BAR" };
    const auto& list = sectionTransitions();
    REQUIRE (list.size() == 35 + names.size());
    for (size_t i = 0; i < names.size(); ++i)
        CHECK (list[35 + i].name == names[i]);
    const auto plain = withTransition (noTransition);
    for (const auto& name : names)
    {
        INFO (name);
        const auto all = withTransition (name);
        CHECK (sameNotes (all[(size_t) ComposerLayer::melody], plain[(size_t) ComposerLayer::melody]));
        bool changed = false;
        for (const auto l : bandLanes)
        {
            const auto& a = all[(size_t) l];
            const auto& b = plain[(size_t) l];
            changed = changed || ! sameNotes (a, b);
            for (const auto& n : a.notes)
            {
                CHECK (n.beat + n.length <= 32.0 + 1.0e-6);
                CHECK (n.beat >= 0.0);
                CHECK (n.length > 0.0);
            }
            std::vector<LineNote> early, earlyPlain;
            for (const auto& n : a.notes)
                if (n.beat + n.length <= 20.0 + 1.0e-6)
                    early.push_back (n);
            for (const auto& n : b.notes)
                if (n.beat + n.length <= 20.0 + 1.0e-6)
                    earlyPlain.push_back (n);
            CHECK (early.size() == earlyPlain.size());
        }
        CHECK (changed);
    }
    const auto lastBarOf = [] (const LineIdea& idea, int pitch)
    {
        std::set<int> at;
        for (const auto& n : idea.notes)
            if (! n.keyswitch && n.beat >= 28.0 - 1.0e-6 && (pitch < 0 || n.pitch == pitch))
                at.insert (juce::roundToInt ((n.beat - 28.0) * 4.0));
        return at;
    };
    // THREE BEATS OF SILENCE / CUT AT THE HALF BAR: nothing starts in the cut.
    for (const auto l : bandLanes)
    {
        checkQuietFrom (withTransition ("THREE BEATS OF SILENCE")[(size_t) l], 29.0, -1.0, 29.0);
        checkQuietFrom (withTransition ("CUT AT THE HALF BAR")[(size_t) l], 30.0, -1.0, 30.0);
    }
    // CLAP BREAK: only claps on the beats in the last bar, the band out.
    {
        const auto all = withTransition ("CLAP BREAK");
        CHECK (lastBarOf (all[(size_t) ComposerLayer::drums], -1) == std::set<int> { 0, 4, 8, 12 });
        CHECK (lastBarOf (all[(size_t) ComposerLayer::drums], 39) == std::set<int> { 0, 4, 8, 12 });
        checkQuietFrom (all[(size_t) ComposerLayer::guitar], 28.0, -1.0, 28.0);
    }
    // UNISON SYNCOPATED BAR: the guitar on 0, 3, 6, 10 and 13.
    CHECK (lastBarOf (withTransition ("UNISON SYNCOPATED BAR")[(size_t) ComposerLayer::guitar], -1) == std::set<int> { 0, 3, 6, 10, 13 });
    // GALLOP BAR: the snare on every and.
    CHECK (lastBarOf (withTransition ("GALLOP BAR, SNARE ON THE ANDS")[(size_t) ComposerLayer::drums], 38) == std::set<int> { 2, 6, 10, 14 });
    // STOP, THREE-SNARE PICKUP: the snares on the last three sixteenths.
    CHECK (lastBarOf (withTransition ("STOP, THREE-SNARE PICKUP")[(size_t) ComposerLayer::drums], 38) == std::set<int> { 13, 14, 15 });
    // TRITONE HIT: the guitar's chord holds the tritone over its root.
    {
        const auto g = withTransition ("TRITONE HIT, RUNG BAR")[(size_t) ComposerLayer::guitar];
        std::set<int> intervals;
        int low = 127;
        for (const auto& n : g.notes)
            if (! n.keyswitch && n.beat >= 28.0 - 1.0e-6)
                low = juce::jmin (low, n.pitch);
        for (const auto& n : g.notes)
            if (! n.keyswitch && n.beat >= 28.0 - 1.0e-6)
                intervals.insert (n.pitch - low);
        CHECK (intervals == std::set<int> { 0, 6, 12 });
    }
    // PUSHED STOP: everyone's hit on the and of 4 of the bar before, nothing starting after it.
    {
        const auto g = withTransition ("PUSHED STOP")[(size_t) ComposerLayer::guitar];
        REQUIRE_FALSE (g.notes.empty());
        CHECK (g.notes.back().beat == Catch::Approx (27.5));
    }
    // HIT ON ONE, BASS FIGURE: the bass's last note a step under the next chord's root (A after G).
    {
        const auto b = withTransition ("HIT ON ONE, BASS FIGURE")[(size_t) ComposerLayer::bass];
        REQUIRE_FALSE (b.notes.empty());
        CHECK (b.notes.back().pitch % 12 == 7);
    }
    // DROP TO KEYS AND LEAD: the keys as they were, the guitar, bass and drums out in the last bar.
    {
        const auto all = withTransition ("DROP TO KEYS AND LEAD");
        const auto sorted = [] (LineIdea idea)   // (every transition sorts its lanes by time: the order can differ)
        {
            std::stable_sort (idea.notes.begin(), idea.notes.end(), [] (const LineNote& a, const LineNote& b)
                              { return std::tie (a.beat, a.pitch) < std::tie (b.beat, b.pitch); });
            return idea;
        };
        CHECK (sameNotes (sorted (all[(size_t) ComposerLayer::chords]), sorted (plain[(size_t) ComposerLayer::chords])));
        for (const auto l : { ComposerLayer::guitar, ComposerLayer::bass, ComposerLayer::drums })
            checkQuietFrom (all[(size_t) l], 28.0, -1.0, 28.0);
    }
    // RIFF ALONE, TWO BARS: the guitar as it was, the rest quiet for two bars.
    {
        const auto all = withTransition ("RIFF ALONE, TWO BARS");
        auto a = all[(size_t) ComposerLayer::guitar], b = plain[(size_t) ComposerLayer::guitar];
        for (auto* idea : { &a, &b })
            std::stable_sort (idea->notes.begin(), idea->notes.end(), [] (const LineNote& x, const LineNote& y)
                              { return std::tie (x.beat, x.pitch) < std::tie (y.beat, y.pitch); });
        CHECK (sameNotes (a, b));
        checkQuietFrom (all[(size_t) ComposerLayer::bass], 24.0, -1.0, 24.0);
    }
}

TEST_CASE ("0.145.0: the drums and keys options from the 61 songs under every named section type", "[sections][types][extras145]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    const auto drums = extraApproaches (ComposerLayer::drums);
    const auto keys = extraApproaches (ComposerLayer::chords);
    CHECK (drums.size() == 9 + 11);   // (0.155.0: eleven more drums, fifteen more keys)
    CHECK (keys.size() == 5 + 15);
    CHECK (extraApproaches (ComposerLayer::guitar).isEmpty());
    CHECK (extraApproaches (ComposerLayer::bass).isEmpty());

    // Under STANDARD (no named type) nothing is added.
    {
        ComposerState s;
        CHECK (sectionApproaches (s, ComposerLayer::drums) == baseSectionApproaches (s, ComposerLayer::drums));
    }

    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << ";";
        return k;
    };
    for (const auto& [section, type] : std::vector<std::pair<juce::String, juce::String>> { { "Chorus", "DRIVING" }, { "Verse", "CLEAN VERSE" },
                                                                                            { "Bridge", "HALF-TIME MELODIC" } })
    {
        ComposerState s;
        applySectionType (s, section, type);
        REQUIRE (s.sectionType == type);
        for (const auto l : { ComposerLayer::drums, ComposerLayer::chords })
        {
            const auto base = baseSectionApproaches (s, l);
            const auto all = sectionApproaches (s, l);
            REQUIRE (! base.isEmpty());
            const auto& extras = l == ComposerLayer::drums ? drums : keys;
            int added = 0;
            for (const auto& x : extras)
                added += base.contains (x) ? 0 : 1;   // (one the type already has stays its own)
            CHECK (all.size() == base.size() + added);
            for (int i = 0; i < base.size(); ++i)
                CHECK (all[i] == base[i]);   // (the type's own first: songs keep their choices)
            std::set<juce::String> seen;
            for (int i = base.size(); i < all.size(); ++i)
            {
                auto t = s;
                t.lane (l).approach = i;
                const auto idea = composeLayers (t, spans, c)[(size_t) l];
                INFO (type << " " << all[i]);
                CHECK (extras.contains (all[i]));
                CHECK (idea.style == all[i].toLowerCase());
                CHECK (! idea.notes.empty());
                CHECK (seen.insert (key (idea)).second);
                for (const auto& n : idea.notes)
                    CHECK (n.beat < 32.0 - 1.0e-6);
            }
        }
    }

    // The punk beat: the snare on every off-beat eighth; the bass LOCKED WITH THE KICK follows the new drums.
    ComposerState s;
    applySectionType (s, "Chorus", "DRIVING");
    const auto base = baseSectionApproaches (s, ComposerLayer::drums);
    s.lane (ComposerLayer::drums).approach = base.size();   // DOUBLE-TIME PUNK BEAT
    s.lane (ComposerLayer::bass).approach = sectionApproaches (s, ComposerLayer::bass).size() - 1;
    const auto layers = composeLayers (s, spans, c);
    const auto& beat = layers[(size_t) ComposerLayer::drums];
    std::set<int> snares, kicks;
    for (const auto& n : beat.notes)
    {
        const auto step = juce::roundToInt (std::fmod (n.beat, 4.0) * 4.0);
        if (n.pitch == GmDrum::snare)
            snares.insert (step);
        if (n.pitch == GmDrum::kick)
            kicks.insert (juce::roundToInt (n.beat * 4.0));
    }
    CHECK (snares == std::set<int> { 2, 6, 10, 14 });
    const auto& bass = layers[(size_t) ComposerLayer::bass];
    REQUIRE (! bass.notes.empty());
    for (const auto& n : bass.notes)
        CHECK (kicks.count (juce::roundToInt (n.beat * 4.0)) == 1);
}

TEST_CASE ("0.155.0: round four's drums and keys options", "[sections][types][extras155]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars)
    ComposerState s;
    applySectionType (s, "Chorus", "DRIVING");
    const auto play = [&] (ComposerLayer l, const juce::String& name)
    {
        auto t = s;
        const auto all = sectionApproaches (t, l);
        REQUIRE (all.contains (name));
        t.lane (l).approach = all.indexOf (name);
        return composeLayers (t, spans, c)[(size_t) l];
    };
    const auto stepsOf = [] (const LineIdea& idea, int pitch, int bar)
    {
        std::set<int> at;
        for (const auto& n : idea.notes)
            if (n.pitch == pitch && n.beat >= bar * 4.0 - 1.0e-6 && n.beat < bar * 4.0 + 4.0 - 1.0e-6)
                at.insert (juce::roundToInt ((n.beat - bar * 4.0) * 4.0));
        return at;
    };
    // FOUR ON THE FLOOR, CRASH ON TWO THREE FOUR.
    {
        const auto d = play (ComposerLayer::drums, "FOUR ON THE FLOOR, CRASH ON TWO THREE FOUR");
        CHECK (stepsOf (d, GmDrum::kick, 1) == std::set<int> { 0, 4, 8, 12 });
        CHECK (stepsOf (d, GmDrum::crash, 1) == std::set<int> { 4, 8, 12 });
    }
    // OFF-SIXTEENTH SNARE: the snare on 3, 11 and 14.
    CHECK (stepsOf (play (ComposerLayer::drums, "OFF-SIXTEENTH SNARE"), GmDrum::snare, 2) == std::set<int> { 3, 11, 14 });
    // CRASH QUARTERS BUILD: every fourth bar the snare on every eighth.
    {
        const auto d = play (ComposerLayer::drums, "CRASH QUARTERS BUILD");
        CHECK (stepsOf (d, GmDrum::snare, 3) == std::set<int> { 0, 2, 4, 6, 8, 10, 12, 14 });
        CHECK (stepsOf (d, GmDrum::snare, 2) == std::set<int> { 4, 12 });
    }
    // SOFT SNARE QUARTERS: no cymbals.
    for (const auto& n : play (ComposerLayer::drums, "SOFT SNARE QUARTERS").notes)
        CHECK ((n.pitch == GmDrum::kick || n.pitch == GmDrum::snare));
    // FIXED HIGH DYAD / TRIAD ARPEGGIO: the same notes over every chord.
    for (const auto* name : { "FIXED HIGH DYAD IN SIXTEENTHS", "TRIAD ARPEGGIO IN SIXTEENTHS", "ONE-NOTE SIXTEENTH LOOP" })
    {
        INFO (name);
        std::set<int> pitches;
        for (const auto& n : play (ComposerLayer::chords, name).notes)
            pitches.insert (n.pitch);
        CHECK (pitches.size() <= 3u);
        for (const auto p : pitches)
            CHECK ((p % 12 == 9 || p % 12 == 0 || p % 12 == 4));   // (A minor's triad)
    }
    // HIGH STAB ON TWO AND FOUR: only on 2 and 4.
    for (const auto& n : play (ComposerLayer::chords, "HIGH STAB ON TWO AND FOUR").notes)
    {
        const auto step = juce::roundToInt (std::fmod (n.beat, 4.0) * 4.0);
        CHECK ((step == 4 || step == 12));
    }
    // CHOIR STABS ON THE DOTTED GRID: every three sixteenths.
    for (const auto& n : play (ComposerLayer::chords, "CHOIR STABS ON THE DOTTED GRID").notes)
        CHECK (juce::roundToInt (std::fmod (n.beat, 4.0) * 4.0) % 3 == 0);
}

TEST_CASE ("0.146.0: the guitar ideas as verse types, every lane's options a different part", "[sections][types][guitar146]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars; the guitar's low A 33)
    const std::vector<juce::String> names { "OCTAVE-JUMP PEDAL", "PEDAL 7+3+3+3", "DRIVEN ARPEGGIO", "CHUG PAIRS, HIGH PINGS", "MUTED PEDAL, ACCENTS IN THREES" };
    const auto verse = sectionTypesFor ("Verse");
    REQUIRE (verse.size() == 44u);
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (size_t i = 0; i < names.size(); ++i)
    {
        INFO (names[i]);
        CHECK (verse[26 + i].name == names[i]);
        CHECK (sectionTypeCalled (names[i])->band == SectionType::Band::verse);
        ComposerState s;
        applySectionType (s, "Verse", names[i]);
        REQUIRE (s.sectionType == names[i]);
        for (const auto l : composerLayers())
        {
            const auto options = baseSectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && l != ComposerLayer::lead && l != ComposerLayer::clean));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) sectionApproaches (s, l).size());
            std::set<juce::String> seen;
            for (size_t o = 0; o < choices.size(); ++o)
            {
                INFO (composerLayerName (l) << " " << o);
                CHECK (seen.insert (key (choices[o])).second);
                if (! (l == ComposerLayer::chords && o == 0))
                    CHECK (! choices[o].notes.empty());
            }
        }
    }

    const auto layersOf = [&] (const juce::String& type, int guitar, int bass, int drums)
    {
        ComposerState s;
        applySectionType (s, "Verse", type);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::bass).approach = bass;
        s.lane (ComposerLayer::drums).approach = drums;
        return composeLayers (s, spans, c);
    };
    const auto at = [] (const LineIdea& idea, double beat)
    {
        std::vector<int> pitches;
        for (const auto& n : idea.notes)
            if (std::abs (n.beat - beat) < 1.0e-6 && ! n.keyswitch)
                pitches.push_back (n.pitch);
        std::sort (pitches.begin(), pitches.end());
        return pitches;
    };
    // OCTAVE-JUMP PEDAL: the root, its octave on the 4th and 11th sixteenths, the half step over it on the 6th; nothing on the 7th.
    {
        const auto layers = layersOf ("OCTAVE-JUMP PEDAL", 0, 0, 0);
        const auto& g = layers[(size_t) ComposerLayer::guitar];
        CHECK (at (g, 0.0) == std::vector<int> { 33 });
        CHECK (at (g, 0.75) == std::vector<int> { 45 });
        CHECK (at (g, 1.25) == std::vector<int> { 46 });
        CHECK (at (g, 1.5).empty());
        CHECK (at (g, 2.5) == std::vector<int> { 45 });
    }
    // PEDAL 7+3+3+3: a power chord opening each group, moving a scale step each time; the muted root between.
    {
        const auto layers = layersOf ("PEDAL 7+3+3+3", 0, 0, 0);
        const auto& g = layers[(size_t) ComposerLayer::guitar];
        CHECK (at (g, 0.0) == std::vector<int> { 33, 40 });
        CHECK (at (g, 0.25) == std::vector<int> { 33 });
        CHECK (at (g, 2.0) == std::vector<int> { 35, 42 });   // (B: A minor's next scale step)
        CHECK (at (g, 2.75) == std::vector<int> { 36, 43 });
        CHECK (at (g, 3.5) == std::vector<int> { 38, 45 });
        CHECK (at (g, 1.75).empty());
        const auto one = layersOf ("PEDAL 7+3+3+3", 1, 0, 0);
        CHECK (at (one[(size_t) ComposerLayer::guitar], 2.0) == std::vector<int> { 33, 40 });
    }
    // DRIVEN ARPEGGIO: one note an eighth, root fifth octave tenth twelfth tenth octave fifth (minor: b3); the bass on 3+3+2.
    {
        const auto layers = layersOf ("DRIVEN ARPEGGIO", 0, 0, 0);
        const auto& g = layers[(size_t) ComposerLayer::guitar];
        const int order[] { 0, 7, 12, 15, 19, 15, 12, 7 };
        for (int k = 0; k < 8; ++k)
            CHECK (at (g, k * 0.5) == std::vector<int> { 33 + order[k] });
        const auto& b = layers[(size_t) ComposerLayer::bass];
        std::set<int> starts;
        for (const auto& n : b.notes)
            if (n.beat < 4.0)
                starts.insert (juce::roundToInt (n.beat * 4.0));
        CHECK (starts == std::set<int> { 0, 6, 12 });
    }
    // CHUG PAIRS, HIGH PINGS: the pairs low, the pings three octaves up; CHUG PAIRS ONLY without them.
    {
        const auto layers = layersOf ("CHUG PAIRS, HIGH PINGS", 0, 0, 0);
        const auto& g = layers[(size_t) ComposerLayer::guitar];
        CHECK (at (g, 0.0) == std::vector<int> { 33 });
        CHECK (at (g, 0.25) == std::vector<int> { 33 });
        CHECK (at (g, 0.5).empty());
        CHECK (at (g, 2.5) == std::vector<int> { 69 });
        CHECK (at (g, 3.5) == std::vector<int> { 76 });
        const auto only = layersOf ("CHUG PAIRS, HIGH PINGS", 1, 0, 0);
        for (const auto& n : only[(size_t) ComposerLayer::guitar].notes)
            CHECK (n.pitch < 60);
    }
    // MUTED PEDAL, ACCENTS IN THREES: six dyads a bar on 3+3+2+3+3+2, the muted pedal between.
    {
        const auto layers = layersOf ("MUTED PEDAL, ACCENTS IN THREES", 0, 0, 0);
        const auto& g = layers[(size_t) ComposerLayer::guitar];
        for (const auto k : { 0, 3, 6, 8, 11, 14 })
            CHECK (at (g, k * 0.25) == std::vector<int> { 33, 40 });
        for (const auto k : { 1, 2, 4, 5, 7, 9 })
            CHECK (at (g, k * 0.25) == std::vector<int> { 33 });
        // over the key's root: in the F bar the pedal stays on A, the accents on F
        const auto onKey = layersOf ("MUTED PEDAL, ACCENTS IN THREES", 1, 0, 0);
        CHECK (at (onKey[(size_t) ComposerLayer::guitar], 4.25) == std::vector<int> { 33 });
        CHECK (at (onKey[(size_t) ComposerLayer::guitar], 4.0) == std::vector<int> { 41, 48 });
    }
    // The bass LOCKED WITH THE KICK follows KICK WITH THE CHUG PAIRS.
    {
        ComposerState s;
        applySectionType (s, "Verse", "CHUG PAIRS, HIGH PINGS");
        s.lane (ComposerLayer::drums).approach = 0;
        s.lane (ComposerLayer::bass).approach = sectionApproaches (s, ComposerLayer::bass).size() - 1;
        const auto layers = composeLayers (s, spans, c);
        std::set<int> kicks;
        for (const auto& n : layers[(size_t) ComposerLayer::drums].notes)
            if (n.pitch == GmDrum::kick)
                kicks.insert (juce::roundToInt (n.beat * 4.0));
        CHECK (kicks.count (1) == 1);   // (the pair's second chug)
        const auto& bass = layers[(size_t) ComposerLayer::bass];
        REQUIRE (! bass.notes.empty());
        for (const auto& n : bass.notes)
            CHECK (kicks.count (juce::roundToInt (n.beat * 4.0)) == 1);
    }
}

TEST_CASE ("0.147.0: round four's verse types, every lane's options a different part", "[sections][types][verse147]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars; the guitar's low A 33)
    const std::vector<juce::String> names { "HOOK OVER THE BEAT, GUITARS OUT", "SEQUENCER VERSE, NO KIT", "HALF-STEP DROP PAIRS",
                                            "TOM BURST WITH THE CELL, THEN SILENCE", "TWO-NOTE SEESAW IN EIGHTHS", "OFF-BEAT STABS WITH THE SNARE",
                                            "TRILL HOOK OVER HATS", "HELD CHORD A BAR, TWO HITS EVERY FOURTH", "STRINGS ALONE VERSE",
                                            "TREMOLO LINE IN GROUPS", "RIFF BARS AND THIN BARS IN TURN" };
    const auto verse = sectionTypesFor ("Verse");
    REQUIRE (verse.size() == 44u);
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (size_t i = 0; i < names.size(); ++i)
    {
        INFO (names[i]);
        CHECK (verse[31 + i].name == names[i]);
        CHECK (sectionTypeCalled (names[i])->band == SectionType::Band::verse);
        CHECK (isSectionTypeOf ("Pre-Chorus", names[i]));
        ComposerState s;
        applySectionType (s, "Verse", names[i]);
        REQUIRE (s.sectionType == names[i]);
        for (const auto l : composerLayers())
        {
            const auto options = baseSectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && l != ComposerLayer::lead && l != ComposerLayer::clean));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) sectionApproaches (s, l).size());
            std::set<juce::String> seen;
            for (size_t o = 0; o < choices.size(); ++o)
            {
                INFO (composerLayerName (l) << " " << o << " " << sectionApproaches (s, l)[(int) o]);
                CHECK (seen.insert (key (choices[o])).second);
                const auto name = sectionApproaches (s, l)[(int) o];
                if (name != "OUT" && name != "SILENT")
                    CHECK (! choices[o].notes.empty());
                for (const auto& n : choices[o].notes)
                    CHECK (n.beat < 32.0 - 1.0e-6);
            }
        }
    }

    const auto layersOf = [&] (const juce::String& type, int guitar, int bass, int drums, int keys = 0)
    {
        ComposerState s;
        applySectionType (s, "Verse", type);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::bass).approach = bass;
        s.lane (ComposerLayer::drums).approach = drums;
        s.lane (ComposerLayer::chords).approach = keys;
        return composeLayers (s, spans, c);
    };
    const auto at = [] (const LineIdea& idea, double beat)
    {
        std::vector<int> pitches;
        for (const auto& n : idea.notes)
            if (std::abs (n.beat - beat) < 1.0e-6 && ! n.keyswitch)
                pitches.push_back (n.pitch);
        std::sort (pitches.begin(), pitches.end());
        return pitches;
    };
    // HALF-STEP DROP PAIRS: the chord a half step above, then the root, on 1, the a of 2 and 4.
    {
        const auto layers = layersOf ("HALF-STEP DROP PAIRS", 0, 0, 0);
        const auto& g = layers[(size_t) ComposerLayer::guitar];
        CHECK (at (g, 0.0) == std::vector<int> { 34, 41 });
        CHECK (at (g, 0.25) == std::vector<int> { 33, 40 });
        CHECK (at (g, 1.5) == std::vector<int> { 34, 41 });
        CHECK (at (g, 3.0) == std::vector<int> { 34, 41 });
        CHECK (at (g, 0.5).empty());
    }
    // HOOK OVER THE BEAT: the guitar out, the keys' root and fifth in eighths over every chord.
    {
        const auto layers = layersOf ("HOOK OVER THE BEAT, GUITARS OUT", 0, 0, 0, 0);
        CHECK (layers[(size_t) ComposerLayer::guitar].notes.empty());
        const auto& k = layers[(size_t) ComposerLayer::chords];
        CHECK (at (k, 0.0) == std::vector<int> { 69 });
        CHECK (at (k, 0.5) == std::vector<int> { 76 });
        CHECK (at (k, 4.0) == std::vector<int> { 69 });   // (over F too)
    }
    // TREMOLO LINE IN GROUPS: 4, 3, 3, 6 sixteenths on the root, the third, the fifth, the third.
    {
        const auto layers = layersOf ("TREMOLO LINE IN GROUPS", 0, 0, 0);
        const auto& g = layers[(size_t) ComposerLayer::guitar];
        CHECK (at (g, 0.75) == std::vector<int> { 45 });
        CHECK (at (g, 1.0) == std::vector<int> { 48 });
        CHECK (at (g, 1.75) == std::vector<int> { 52 });
        CHECK (at (g, 2.5) == std::vector<int> { 48 });
    }
    // TOM BURST WITH THE CELL: the toms under the cell's bar, nothing in the next; no kick, snare or cymbal.
    {
        const auto layers = layersOf ("TOM BURST WITH THE CELL, THEN SILENCE", 0, 0, 0);
        for (const auto& n : layers[(size_t) ComposerLayer::drums].notes)
        {
            CHECK (n.pitch != GmDrum::kick);
            CHECK (n.pitch != GmDrum::snare);
            CHECK (std::fmod (n.beat, 8.0) < 4.0);
        }
        for (const auto& n : layers[(size_t) ComposerLayer::guitar].notes)
            CHECK (std::fmod (n.beat, 8.0) < 4.0);
    }
}

TEST_CASE ("0.148.0: round four's chorus types, every lane's options a different part", "[sections][types][chorus148]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars; the guitar's low A 33)
    const std::vector<juce::String> names { "ACOUSTIC STRUM CHORUS", "BURST & HOLD, THEN A CARPET IN 3+2+3", "BREAKBEAT UNDER HELD CHORDS", "STOMP IN 3+3+2",
                                            "FALLING PUSHED CHORDS", "HITS, THEN A WALL THE BASS LEAVES", "RING THE ONE, RUN THE REST",
                                            "BASS SIXTEENTHS, GUITARS OUT", "THREE CHORDS, THEN A HELD BAR", "HELD CHORD WITH A RINGING TOP, BUSY KICK",
                                            "CHANGES ON THREE" };
    const auto chorus = sectionTypesFor ("Chorus");
    REQUIRE (chorus.size() == 45u);
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    for (size_t i = 0; i < names.size(); ++i)
    {
        INFO (names[i]);
        CHECK (chorus[34 + i].name == names[i]);
        CHECK (sectionTypeCalled (names[i])->band == SectionType::Band::chorus);
        ComposerState s;
        applySectionType (s, "Chorus", names[i]);
        REQUIRE (s.sectionType == names[i]);
        for (const auto l : composerLayers())
        {
            const auto options = baseSectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && l != ComposerLayer::lead && l != ComposerLayer::clean));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) sectionApproaches (s, l).size());
            std::set<juce::String> seen;
            for (size_t o = 0; o < choices.size(); ++o)
            {
                const auto name = sectionApproaches (s, l)[(int) o];
                INFO (composerLayerName (l) << " " << name);
                CHECK (seen.insert (key (choices[o])).second);
                if (name != "OUT" && name != "SILENT" && name != "NO DRUMS")
                    CHECK (! choices[o].notes.empty());
                for (const auto& n : choices[o].notes)
                    CHECK (n.beat < 32.0 - 1.0e-6);
            }
        }
    }
    const auto layersOf = [&] (const juce::String& type, int guitar, int drums)
    {
        ComposerState s;
        applySectionType (s, "Chorus", type);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::drums).approach = drums;
        return composeLayers (s, spans, c);
    };
    const auto at = [] (const LineIdea& idea, double beat)
    {
        std::vector<int> pitches;
        for (const auto& n : idea.notes)
            if (std::abs (n.beat - beat) < 1.0e-6 && ! n.keyswitch)
                pitches.push_back (n.pitch);
        std::sort (pitches.begin(), pitches.end());
        return pitches;
    };
    // FALLING PUSHED CHORDS: two root chugs, then G, F, E on the ands of 2, 3 and 4 (A minor's steps down).
    {
        const auto layers = layersOf ("FALLING PUSHED CHORDS", 0, 0);
        const auto& g = layers[(size_t) ComposerLayer::guitar];
        CHECK (at (g, 0.0) == std::vector<int> { 33 });
        CHECK (at (g, 0.5) == std::vector<int> { 33 });
        CHECK (at (g, 1.5) == std::vector<int> { 43, 50 });
        CHECK (at (g, 2.5) == std::vector<int> { 41, 48 });
        CHECK (at (g, 3.5) == std::vector<int> { 40, 47 });
    }
    // STOMP IN 3+3+2: the kick and floor tom together on 1, the a of 1, 3 and the a of 3.
    {
        const auto layers = layersOf ("STOMP IN 3+3+2", 0, 0);
        std::set<int> kicks, toms;
        for (const auto& n : layers[(size_t) ComposerLayer::drums].notes)
            if (n.beat < 4.0)
            {
                if (n.pitch == GmDrum::kick) kicks.insert (juce::roundToInt (n.beat * 4.0));
                if (n.pitch == GmDrum::tomFloor) toms.insert (juce::roundToInt (n.beat * 4.0));
            }
        CHECK (kicks == std::set<int> { 0, 3, 8, 11 });
        CHECK (toms == kicks);
    }
    // CHANGES ON THREE: the second bar's first half still the first bar's chord, its second half F.
    {
        const auto layers = layersOf ("CHANGES ON THREE", 0, 0);
        const auto& g = layers[(size_t) ComposerLayer::guitar];
        CHECK (at (g, 4.0) == std::vector<int> { 33, 40 });
        CHECK (at (g, 6.0) == std::vector<int> { 41, 48 });
    }
    // ACOUSTIC STRUM CHORUS with NO DRUMS: no drum notes at all.
    CHECK (layersOf ("ACOUSTIC STRUM CHORUS", 0, 0)[(size_t) ComposerLayer::drums].notes.empty());
}

TEST_CASE ("0.149.0: round four's pre-chorus and post-chorus types, every lane's options a different part", "[sections][types][pre149]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars; the guitar's low A 33)
    const std::vector<juce::String> pres { "STOP BARS IN TURN WITH THE RIFF", "MARCHING SNARE OVER HELD CHORDS", "DRUMS OUT, KEYS PULSE AND PUSHES",
                                           "SIXTEENTH ROOTS STEPPING DOWN", "SIDESTICK DROP", "HIGH PEDAL IN 3+3+2 OVER A MOVING BASS" };
    const std::vector<juce::String> posts { "OFF-BEAT LOW DYADS UNDER A HIGH DRONE", "CLAPS AND ONE GUITAR", "BASS RIFF ALONE UNDER A HELD HIGH NOTE" };
    const auto pre = sectionTypesFor ("Pre-Chorus");
    const auto post = sectionTypesFor ("Post-Chorus");
    for (size_t i = 0; i < pres.size(); ++i)
    {
        CHECK (pre[19 + i].name == pres[i]);
        CHECK (sectionTypeCalled (pres[i])->band == SectionType::Band::preChorus);
    }
    for (size_t i = 0; i < posts.size(); ++i)
    {
        CHECK (post[17 + i].name == posts[i]);
        CHECK (sectionTypeCalled (posts[i])->band == SectionType::Band::interlude);
        CHECK (isSectionTypeOf ("Interlude", posts[i]));
    }
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    auto all = pres;
    all.insert (all.end(), posts.begin(), posts.end());
    for (const auto& name : all)
    {
        INFO (name);
        ComposerState s;
        applySectionType (s, std::find (pres.begin(), pres.end(), name) != pres.end() ? "Pre-Chorus" : "Post-Chorus", name);
        REQUIRE (s.sectionType == name);
        for (const auto l : composerLayers())
        {
            const auto options = baseSectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) sectionApproaches (s, l).size());
            std::set<juce::String> seen;
            for (size_t o = 0; o < choices.size(); ++o)
            {
                const auto option = sectionApproaches (s, l)[(int) o];
                INFO (composerLayerName (l) << " " << option);
                CHECK (seen.insert (key (choices[o])).second);
                if (option != "OUT" && option != "SILENT")
                    CHECK (! choices[o].notes.empty());
                for (const auto& n : choices[o].notes)
                    CHECK (n.beat < 32.0 - 1.0e-6);
            }
        }
    }
    const auto layersOf = [&] (const juce::String& section, const juce::String& type, int guitar, int drums)
    {
        ComposerState s;
        applySectionType (s, section, type);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::drums).approach = drums;
        return composeLayers (s, spans, c);
    };
    // MARCHING SNARE: no kick or cymbals, and nothing after beat 2 of the last bar.
    const auto marching = layersOf ("Pre-Chorus", "MARCHING SNARE OVER HELD CHORDS", 0, 0);   // (kept: never loop over a temporary's [] member)
    for (const auto& n : marching[(size_t) ComposerLayer::drums].notes)
    {
        CHECK (n.pitch == GmDrum::snare);
        CHECK (n.beat < 7 * 4.0 + 2.0);
    }
    // SIXTEENTH ROOTS STEPPING DOWN: one root a bar, falling (A, then F, then E...), never under the low string.
    {
        const auto layers = layersOf ("Pre-Chorus", "SIXTEENTH ROOTS STEPPING DOWN", 1, 0);
        std::vector<int> firsts;
        for (int bar = 0; bar < 4; ++bar)
            for (const auto& n : layers[(size_t) ComposerLayer::guitar].notes)
                if (std::abs (n.beat - bar * 4.0) < 1.0e-6)
                {
                    firsts.push_back (n.pitch);
                    break;
                }
        CHECK (firsts == std::vector<int> { 45, 41, 40, 39 });
    }
    // CLAPS AND ONE GUITAR: no kick in the first drum option; claps on every beat.
    {
        int claps = 0;
        const auto clapping = layersOf ("Post-Chorus", "CLAPS AND ONE GUITAR", 0, 0);
        for (const auto& n : clapping[(size_t) ComposerLayer::drums].notes)
        {
            CHECK (n.pitch != GmDrum::kick);
            claps += n.pitch == 39 ? 1 : 0;
        }
        CHECK (claps == 32);
    }
}

TEST_CASE ("0.150.0: round four's bridge and breakdown types, every lane's options a different part", "[sections][types][bridge150]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars; the guitar's low A 33)
    const std::vector<juce::String> bridges { "PARALLEL CLASH OVER THE BASS", "ORCHESTRA ALONE, THEN THE WALL", "SNARE CARPET OVER THE FLOOR", "LONG PULSE BUILD",
                                              "PAIRS EVERY THREE UNDER A HELD CHORD", "THE HOOK'S RHYTHM ON LOW CHORDS", "DOUBLED HITS ON ONE AND TWO, THEN HOLD",
                                              "BURSTS OF FIVE, LAYERED", "BELL GROOVE UNDER A TWO-CHORD DRONE", "HIGH CHROMATIC FALL OVER THE CHUG CELL",
                                              "TRIBAL TOMS, DOTTED OCTAVES", "UNISON STABS OVER HATS ONLY", "EIGHTHS WITH AIR AT BOTH ENDS",
                                              "ARPEGGIO UP AN OCTAVE, BASS STEPPING DOWN" };
    const std::vector<juce::String> breakdowns { "HALF-BAR CHUGS, HALF STEP EVERY FOURTH", "PAIR STABS, THEN THE THREES",
                                                 "QUIET FALLING CELL OVER A LAMENT BASS", "HIGH NOTE TREMOLO OVER A HALF-TIME CHUG" };
    const auto bridge = sectionTypesFor ("Bridge");
    const auto breakdown = sectionTypesFor ("Breakdown");
    for (size_t i = 0; i < bridges.size(); ++i)
    {
        CHECK (bridge[27 + i].name == bridges[i]);   // (after 0.142.0's)
        CHECK (sectionTypeCalled (bridges[i])->band == SectionType::Band::bridge);
    }
    for (size_t i = 0; i < breakdowns.size(); ++i)
    {
        CHECK (breakdown[19 + i].name == breakdowns[i]);
        CHECK (sectionTypeCalled (breakdowns[i])->band == SectionType::Band::breakdown);
    }
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    auto all = bridges;
    all.insert (all.end(), breakdowns.begin(), breakdowns.end());
    for (const auto& name : all)
    {
        INFO (name);
        ComposerState s;
        applySectionType (s, std::find (bridges.begin(), bridges.end(), name) != bridges.end() ? "Bridge" : "Breakdown", name);
        REQUIRE (s.sectionType == name);
        for (const auto l : composerLayers())
        {
            const auto options = baseSectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) sectionApproaches (s, l).size());
            std::set<juce::String> seen;
            for (size_t o = 0; o < choices.size(); ++o)
            {
                const auto option = sectionApproaches (s, l)[(int) o];
                INFO (composerLayerName (l) << " " << option);
                CHECK (seen.insert (key (choices[o])).second);
                if (option != "OUT" && option != "SILENT")
                    CHECK (! choices[o].notes.empty());
                for (const auto& n : choices[o].notes)
                {
                    CHECK (n.beat < 32.0 - 1.0e-6);
                    if (l == ComposerLayer::guitar && ! n.keyswitch)
                        CHECK (n.pitch >= 33);   // (never under the 7-string's low A)
                }
            }
        }
    }
    const auto layersOf = [&] (const juce::String& section, const juce::String& type, int guitar, int drums)
    {
        ComposerState s;
        applySectionType (s, section, type);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::drums).approach = drums;
        return composeLayers (s, spans, c);
    };
    // TRIBAL TOMS ONLY: no kick, snare or cymbals.
    {
        const auto tribal = layersOf ("Bridge", "TRIBAL TOMS, DOTTED OCTAVES", 0, 1);   // (kept: never loop over a temporary's [] member)
        REQUIRE (! tribal[(size_t) ComposerLayer::drums].notes.empty());
        for (const auto& n : tribal[(size_t) ComposerLayer::drums].notes)
            CHECK ((n.pitch == GmDrum::tomFloor || n.pitch == GmDrum::tomLow));
    }
    // HALF-BAR CHUGS: every fourth bar a half step over the others.
    {
        const auto chugs = layersOf ("Breakdown", "HALF-BAR CHUGS, HALF STEP EVERY FOURTH", 0, 0);
        int bar0 = -1, bar3 = -1;
        for (const auto& n : chugs[(size_t) ComposerLayer::guitar].notes)
        {
            if (n.keyswitch)
                continue;
            if (std::abs (n.beat) < 1.0e-6 && bar0 < 0) bar0 = n.pitch;
            if (std::abs (n.beat - 12.0) < 1.0e-6 && bar3 < 0) bar3 = n.pitch;
        }
        CHECK (bar0 == 33);   // (bar 1 is Am: the low A)
        CHECK (bar3 == 43 + 1);   // (bar 4 is G: the low G 43, a half step up)
    }
    // QUIET FALLING CELL: the drums out in the first option.
    CHECK (layersOf ("Breakdown", "QUIET FALLING CELL OVER A LAMENT BASS", 0, 0)[(size_t) ComposerLayer::drums].notes.empty());
}

TEST_CASE ("0.151.0: round four's intro and outro types, every lane's options a different part", "[sections][types][bookend151]")
{
    const ComposerContext c;
    const auto spans = cleanSpans();   // (A minor: Am F C G, a bar each, 8 bars; the guitar's low A 33)
    const std::vector<juce::String> intros { "SNARE-ROLL GROOVE, THE BAND JOINS", "HYMN, THEN DOUBLE KICK UNDER IT", "QUARTER STRUMS OVER AN OFF-SIXTEENTH SNARE",
                                             "SIDE STICK AND HATS ON THE RIFF", "HAND PERCUSSION ALONE, THEN THE RIFF", "ONE CHORD STRUMMED 3+3+3+3+4, RIFF JOINS",
                                             "RIFF ALONE, SIXTEENTH ROOT PEDAL JOINS", "SWELLS, THEN A FIXED HIGH FIGURE, THE KIT LAST" };
    const std::vector<juce::String> outros { "BURSTS OFF THE BEAT TO THE END", "BASS RIFF ALONE TO THE END", "CALL BAR, ANSWER BAR, CUT", "OSTINATO AND BEAT TO THE END" };
    const auto intro = sectionTypesFor ("Intro");
    const auto outro = sectionTypesFor ("Outro");
    for (size_t i = 0; i < intros.size(); ++i)
    {
        CHECK (intro[23 + i].name == intros[i]);   // (after 0.143.0's)
        CHECK (sectionTypeCalled (intros[i])->band == SectionType::Band::bookend);
    }
    for (size_t i = 0; i < outros.size(); ++i)
    {
        CHECK (outro[20 + i].name == outros[i]);
        CHECK (sectionTypeCalled (outros[i])->band == SectionType::Band::bookend);
    }
    const auto key = [] (const LineIdea& idea)
    {
        juce::String k;
        for (const auto& n : idea.notes)
            k << n.pitch << "@" << juce::roundToInt (n.beat * 96.0) << "/" << juce::roundToInt (n.length * 96.0) << ";";
        return k;
    };
    auto all = intros;
    all.insert (all.end(), outros.begin(), outros.end());
    for (const auto& name : all)
    {
        INFO (name);
        ComposerState s;
        applySectionType (s, std::find (intros.begin(), intros.end(), name) != intros.end() ? "Intro" : "Outro", name);
        REQUIRE (s.sectionType == name);
        for (const auto l : composerLayers())
        {
            const auto options = baseSectionApproaches (s, l);
            CHECK (sectionTypeWritesLane (l, s, c) == (l != ComposerLayer::melody && ! options.isEmpty()));
            if (options.isEmpty())
                continue;
            if (l == ComposerLayer::bass)
                CHECK (options[options.size() - 1] == "LOCKED WITH THE KICK");
            const auto choices = composerChoices (l, s, spans, c);
            REQUIRE (choices.size() == (size_t) sectionApproaches (s, l).size());
            std::set<juce::String> seen;
            for (size_t o = 0; o < choices.size(); ++o)
            {
                const auto option = sectionApproaches (s, l)[(int) o];
                INFO (composerLayerName (l) << " " << option);
                CHECK (seen.insert (key (choices[o])).second);
                if (option != "OUT" && option != "SILENT")
                    CHECK (! choices[o].notes.empty());
                for (const auto& n : choices[o].notes)
                {
                    CHECK (n.beat < 32.0 - 1.0e-6);
                    if (l == ComposerLayer::guitar && ! n.keyswitch)
                        CHECK (n.pitch >= 33);
                }
            }
        }
    }
    const auto layersOf = [&] (const juce::String& section, const juce::String& type, int guitar, int drums)
    {
        ComposerState s;
        applySectionType (s, section, type);
        s.lane (ComposerLayer::guitar).approach = guitar;
        s.lane (ComposerLayer::drums).approach = drums;
        return composeLayers (s, spans, c);
    };
    // SIDE STICK AND HATS: no kick or snare.
    {
        const auto side = layersOf ("Intro", "SIDE STICK AND HATS ON THE RIFF", 0, 0);   // (kept: never loop over a temporary's [] member)
        REQUIRE (! side[(size_t) ComposerLayer::drums].notes.empty());
        for (const auto& n : side[(size_t) ComposerLayer::drums].notes)
            CHECK ((n.pitch != GmDrum::kick && n.pitch != GmDrum::snare));
    }
    // BASS RIFF ALONE TO THE END: the guitar and drums out in their first options, the bass playing.
    {
        const auto alone = layersOf ("Outro", "BASS RIFF ALONE TO THE END", 0, 0);
        CHECK (alone[(size_t) ComposerLayer::guitar].notes.empty());
        CHECK (alone[(size_t) ComposerLayer::drums].notes.empty());
        CHECK (! alone[(size_t) ComposerLayer::bass].notes.empty());
    }
    // CALL BAR, ANSWER BAR, CUT: nothing after beat 2 of the last bar.
    {
        const auto cut = layersOf ("Outro", "CALL BAR, ANSWER BAR, CUT", 0, 0);
        for (const auto& n : cut[(size_t) ComposerLayer::guitar].notes)
            CHECK (n.beat < 7 * 4.0 + 2.0);
    }
}
