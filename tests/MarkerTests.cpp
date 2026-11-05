// Markers (0.49.0, the manual's Markers chapter): the marker track, position and cycle markers,
// IDs, locating, the keys, the Markers window, MIDI file markers, saving and undo.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/MidiImport.h"
#include "engine/Studio.h"
#include "ui/ArrangeView.h"
#include "ui/Finish.h"
#include "ui/MainView.h"
#include "ui/MarkersPage.h"

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {}, int clicks = 1)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, clicks, at != downAt);
    }
}

TEST_CASE ("0.49.0: markers in the song: IDs, sorting, locating, Cycle Follows, saving and undo", "[markers]")
{
    Studio studio (false);
    const auto& track = studio.getMarkerTrack();
    CHECK_FALSE (track.shown);

    // Adding one shows the track; IDs from 1 by kind; the list in time order.
    CHECK (studio.addPositionMarker (8.0, "Chorus") == 0);
    CHECK (track.shown);
    studio.addPositionMarker (0.0, "Intro");
    const auto cycle = studio.addCycleMarker (16.0, 32.0, "Bridge");
    REQUIRE (track.markers.size() == 3);
    CHECK (track.markers[0].name == "Intro");
    CHECK (track.markers[0].id == 2);   // (IDs in the order added: the manual's Reassign puts them in order)
    CHECK (track.markers[1].id == 1);
    CHECK (track.markers[(size_t) cycle].cycle);
    CHECK (track.markers[(size_t) cycle].id == 1);
    CHECK (markerIdText (track.markers[(size_t) cycle]) == "(1)");

    // Reassign Position Marker IDs: in the order they're in.
    studio.reassignMarkerIds (false);
    CHECK (track.markers[0].id == 1);
    CHECK (track.markers[1].id == 2);

    // Locate: the playhead; Next / Previous; To Marker n.
    studio.setPosition (3.0);
    CHECK (studio.locateNextMarker (true));
    CHECK_THAT (studio.getEngine().getPosition(), WithinAbs (8.0, 1.0e-9));
    CHECK (studio.locateNextMarker (true));
    CHECK_THAT (studio.getEngine().getPosition(), WithinAbs (16.0, 1.0e-9));
    CHECK_FALSE (studio.locateNextMarker (true));   // (none after)
    CHECK (studio.locateNextMarker (false));
    CHECK_THAT (studio.getEngine().getPosition(), WithinAbs (8.0, 1.0e-9));
    CHECK (studio.locateMarkerId (1));
    CHECK_THAT (studio.getEngine().getPosition(), WithinAbs (0.0, 1.0e-9));
    CHECK_FALSE (studio.locateMarkerId (7));

    // A cycle marker double-clicked: the locators to it. Cycle Follows: locating sets them too.
    CHECK (studio.cycleToMarker (cycle));
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (16.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (32.0, 1.0e-9));
    studio.setMarkerSettings (true, true, true);
    studio.locateMarkerId (1);   // Intro (0): to the next marker (Chorus, 8)
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (0.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (8.0, 1.0e-9));

    // Set Marker n: moves that marker to the playhead, or adds it with that ID.
    studio.setPosition (40.0);
    CHECK (studio.setMarkerIdAtCursor (2));
    CHECK_THAT (track.markers[(size_t) track.indexOf (2, false)].start, WithinAbs (40.0, 1.0e-9));
    CHECK (studio.setMarkerIdAtCursor (7));
    CHECK (track.find (7, false) != nullptr);

    // Undo takes each edit back.
    const auto before = track.markers.size();
    studio.undo();
    CHECK (track.markers.size() == before - 1);
    studio.redo();
    CHECK (track.markers.size() == before);

    // Locked: no edits, but locating works.
    studio.setMarkerTrackLocked (true);
    CHECK (studio.addPositionMarker (2.0) == -1);
    CHECK_FALSE (studio.removeMarkers ({ 0 }));
    CHECK (studio.locateMarkerId (1));
    studio.setMarkerTrackLocked (false);

    // Saved and opened again; settings and names kept.
    const auto opened = Project::fromXml (*studio.getProject().toXml());
    REQUIRE (opened);
    CHECK (opened->markerTrack.markers == track.markers);
    CHECK (opened->markerTrack.cycleFollows);
    CHECK (opened->markerTrack.shown);

    // Removed: hidden, the markers kept; Erase's Alt: all from a beat on.
    studio.removeMarkerTrack();
    CHECK_FALSE (track.shown);
    CHECK (track.markers.size() == before);
    studio.addMarkerTrack();
    CHECK (studio.removeMarkersFrom (16.0));
    for (const auto& m : track.markers)
        CHECK (m.start < 16.0);

    // Move Markers to Cursor: a cycle marker keeps its length.
    const auto c2 = studio.addCycleMarker (4.0, 6.0);
    studio.setPosition (20.0);
    CHECK (studio.moveMarkersToCursor ({ c2 }));
    const auto* moved = track.find (track.markers[(size_t) track.markers.size() - 1].id, true);
    REQUIRE (moved != nullptr);
    CHECK_THAT (moved->start, WithinAbs (20.0, 1.0e-9));
    CHECK_THAT (moved->end, WithinAbs (22.0, 1.0e-9));
}

TEST_CASE ("0.49.0: a MIDI file's markers come onto the marker track", "[markers][midi]")
{
    juce::MidiMessageSequence sequence;
    sequence.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0.0);
    sequence.addEvent (juce::MidiMessage::noteOff (1, 60), 480.0);
    auto verse = juce::MidiMessage::textMetaEvent (6, "Verse");
    verse.setTimeStamp (960.0 * 4.0);
    sequence.addEvent (verse);
    auto chorus = juce::MidiMessage::textMetaEvent (6, "Chorus");
    chorus.setTimeStamp (960.0 * 8.0);
    sequence.addEvent (chorus);
    sequence.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote (960);
    file.addTrack (sequence);
    juce::MemoryOutputStream out;
    REQUIRE (file.writeTo (out));
    juce::MemoryInputStream in (out.getData(), out.getDataSize(), false);
    juce::String error;
    const auto read = readMidiFile (in, error);
    REQUIRE (read);
    REQUIRE (read->markers.size() == 2);
    CHECK_THAT (read->markers[0].beat, WithinAbs (4.0, 1.0e-9));
    CHECK (read->markers[1].name == "Chorus");

    Studio studio (false);
    studio.importMidi (*read, false, 8.0, false, 0);
    const auto& markers = studio.getMarkerTrack().markers;
    REQUIRE (markers.size() == 2);
    CHECK_THAT (markers[0].start, WithinAbs (12.0, 1.0e-9));   // (from where it went in)
    CHECK (markers[0].name == "Verse");
    CHECK (studio.getMarkerTrack().shown);
}

TEST_CASE ("0.49.0: the marker track on screen: Alt-click, Ctrl-drag, drag, edges, double-click, Delete, keys", "[markers][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    studio.setSnap (true);
    studio.setQuantizeBeats (1.0);
    studio.addMarkerTrack();
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = view.getTimeline();
    const auto area = arrange.getMarkerArea();
    REQUIRE (! area.isEmpty());
    const auto lower = area.getY() + area.getHeight() * 0.75f, upper = area.getY() + area.getHeight() * 0.25f;
    const auto& markers = studio.getMarkerTrack().markers;

    // Alt-click: a position marker, snapped to the beat.
    const juce::Point<float> at (timeline.xForBeat (4.2), lower);
    arrange.mouseDown (mouseAt (arrange, at, at, juce::ModifierKeys::altModifier));
    arrange.mouseUp (mouseAt (arrange, at, at, juce::ModifierKeys::altModifier));
    REQUIRE (markers.size() == 1);
    CHECK_THAT (markers[0].start, WithinAbs (4.0, 1.0e-9));
    CHECK (arrange.getMarkerSelection() == std::vector<int> { 0 });

    // Ctrl-drag: a cycle marker over what's dragged.
    const juce::Point<float> from (timeline.xForBeat (8.0), upper), to (timeline.xForBeat (16.0), upper);
    arrange.mouseDown (mouseAt (arrange, from, from, juce::ModifierKeys::commandModifier));
    arrange.mouseDrag (mouseAt (arrange, to, from, juce::ModifierKeys::commandModifier));
    arrange.mouseUp (mouseAt (arrange, to, from, juce::ModifierKeys::commandModifier));
    REQUIRE (markers.size() == 2);
    CHECK (markers[1].cycle);
    CHECK_THAT (markers[1].start, WithinAbs (8.0, 1.0e-9));
    CHECK_THAT (markers[1].end, WithinAbs (16.0, 1.0e-9));

    // Its end edge dragged to 20; the position marker dragged to 6.
    const juce::Point<float> edge (timeline.xForBeat (16.0) - 1.0f, upper), later (timeline.xForBeat (20.0), upper);
    arrange.mouseDown (mouseAt (arrange, edge, edge));
    arrange.mouseDrag (mouseAt (arrange, later, edge));
    arrange.mouseUp (mouseAt (arrange, later, edge));
    CHECK_THAT (markers[1].end, WithinAbs (20.0, 1.0e-9));
    const auto flag = arrange.getMarkerBounds (0).getCentre();
    const juce::Point<float> moved (flag.x + (float) (2.0 * timeline.pixelsPerBeat), flag.y);
    arrange.mouseDown (mouseAt (arrange, flag, flag));
    arrange.mouseDrag (mouseAt (arrange, moved, flag));
    arrange.mouseUp (mouseAt (arrange, moved, flag));
    CHECK_THAT (markers[0].start, WithinAbs (6.0, 1.0e-9));
    studio.undo();
    CHECK_THAT (markers[0].start, WithinAbs (4.0, 1.0e-9));
    studio.redo();

    // A double-click on the cycle marker: the locators to it.
    const auto bar = arrange.getMarkerBounds (1).getCentre();
    arrange.mouseDown (mouseAt (arrange, bar, bar));
    arrange.mouseUp (mouseAt (arrange, bar, bar));
    arrange.mouseDoubleClick (mouseAt (arrange, bar, bar, {}, 2));
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (8.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (20.0, 1.0e-9));

    // The Range tool's double-click between markers: that range on every track.
    arrange.setTool (ArrangeView::Tool::range);
    const juce::Point<float> between (timeline.xForBeat (7.0), lower);
    arrange.mouseDoubleClick (mouseAt (arrange, between, between, {}, 2));
    REQUIRE (arrange.getRange());
    CHECK_THAT (arrange.getRange()->start, WithinAbs (6.0, 1.0e-9));
    CHECK_THAT (arrange.getRange()->end, WithinAbs (8.0, 1.0e-9));
    arrange.setRange (std::nullopt);
    arrange.setTool (ArrangeView::Tool::select);

    // Keys: Insert adds at the playhead; Shift+N / Shift+B; Shift+1; Ctrl+3; Delete removes the selected.
    studio.setPosition (30.0);
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::insertKey)));
    REQUIRE (markers.size() == 3);
    CHECK_THAT (markers[2].start, WithinAbs (30.0, 1.0e-9));
    studio.setPosition (0.0);
    CHECK (view.keyPressed (juce::KeyPress ('N', juce::ModifierKeys::shiftModifier, 'N')));
    CHECK_THAT (studio.getEngine().getPosition(), WithinAbs (6.0, 1.0e-9));
    CHECK (view.keyPressed (juce::KeyPress ('N', juce::ModifierKeys::shiftModifier, 'N')));
    CHECK_THAT (studio.getEngine().getPosition(), WithinAbs (8.0, 1.0e-9));
    CHECK (view.keyPressed (juce::KeyPress ('B', juce::ModifierKeys::shiftModifier, 'B')));
    CHECK_THAT (studio.getEngine().getPosition(), WithinAbs (6.0, 1.0e-9));
    studio.setPosition (12.0);
    CHECK (view.keyPressed (juce::KeyPress ('3', juce::ModifierKeys::commandModifier, 0)));   // Set Marker 3
    REQUIRE (studio.getMarkerTrack().find (3, false) != nullptr);
    studio.setPosition (0.0);
    CHECK (view.keyPressed (juce::KeyPress ('3', juce::ModifierKeys::shiftModifier, '#')));   // To Marker 3
    CHECK_THAT (studio.getEngine().getPosition(), WithinAbs (12.0, 1.0e-9));
    arrange.selectMarkers ({ (int) studio.getMarkerTrack().indexOf (3, false) });
    const auto count = markers.size();
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK (markers.size() == count - 1);
    CHECK (studio.getProject().loopEnd > 0.0);   // (Shift+8 is still a marker key, not *: nothing recorded)

    // Ctrl+M: the Markers window, and closed again.
    CHECK (view.keyPressed (juce::KeyPress ('M', juce::ModifierKeys::commandModifier, 0)));
    CHECK (view.getMarkersWindow() != nullptr);
    CHECK (view.keyPressed (juce::KeyPress ('M', juce::ModifierKeys::commandModifier, 0)));
    CHECK (view.getMarkersWindow() == nullptr);

    // Drawn in every finish.
    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
    }
}

TEST_CASE ("0.49.0: the Markers window lists, edits, sorts and locates", "[markers][ui]")
{
    Studio studio (false);
    studio.addPositionMarker (8.0, "Chorus");
    studio.addPositionMarker (0.0, "Intro");
    studio.addCycleMarker (16.0, 24.0, "Bridge");
    MarkersPage page (studio);
    page.setSize (MarkersPage::preferredWidth, MarkersPage::preferredHeight);
    REQUIRE (page.getNumRows() == 3);
    CHECK (page.getCellText (0, MarkersPage::description) == "Intro");
    CHECK (page.getCellText (2, MarkersPage::type) == "CYCLE");
    CHECK (page.getCellText (2, MarkersPage::id) == "(1)");
    CHECK (page.getCellText (0, MarkersPage::end).isEmpty());
    CHECK (page.getCellText (2, MarkersPage::length) == "2.0.0.0");

    // Typed: a position moves it, a cycle marker's length changes its end, a description names it.
    CHECK (page.setCell (1, MarkersPage::position, "3.1.1.0"));   // Chorus to bar 3 (beat 8: already) -> fine
    CHECK (page.setCell (2, MarkersPage::length, "1.0.0.0"));
    CHECK_THAT (studio.getMarkerTrack().markers[2].end, WithinAbs (20.0, 1.0e-9));
    CHECK (page.setCell (0, MarkersPage::description, "Count-in"));
    CHECK (studio.getMarkerTrack().markers[0].name == "Count-in");
    CHECK_FALSE (page.setCell (0, MarkersPage::end, "5.1.1.0"));   // (a position marker has no end)
    // An ID typed that another has: the two swap.
    CHECK (page.setCell (0, MarkersPage::id, "1"));
    CHECK (studio.getMarkerTrack().markers[0].id == 1);
    CHECK (studio.getMarkerTrack().markers[1].id == 2);

    // The locate arrow: at or before the playhead; clicking it (Return) goes there.
    studio.setPosition (10.0);
    CHECK (page.getLocateRow() == 1);
    CHECK (page.locateRow (2));
    CHECK_THAT (studio.getEngine().getPosition(), WithinAbs (16.0, 1.0e-9));

    // Sorted by description (the position second).
    page.sortBy (MarkersPage::description, true);
    CHECK (page.getCellText (0, MarkersPage::description) == "Bridge");
    page.sortBy (MarkersPage::position, true);

    // Remove the selected; the settings' keys.
    page.selectRows ({ 0 });
    CHECK (page.removeSelected());
    CHECK (studio.getMarkerTrack().markers.size() == 2);
    page.keyNamed ("SHOW IDS")->onClick();
    CHECK_FALSE (studio.getMarkerTrack().showIds);
    page.keyNamed ("CYCLE FOLLOWS")->onClick();
    CHECK (studio.getMarkerTrack().cycleFollows);
    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (page.createComponentSnapshot (page.getLocalBounds()).isValid());
    }
}
