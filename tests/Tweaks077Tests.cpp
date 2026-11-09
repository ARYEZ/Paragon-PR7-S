// 0.77.1 (the owner: "why can't I grab the left side of anything? I can only grab the right side"): a MIDI
// part's left edge in the arrangement and a note's left end in the MIDI editor can be dragged (before, only
// audio clips had a left edge; MIDI parts and notes moved instead).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Engine.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/Hub.h"
#include "ui/MainView.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {})
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, 1, at != downAt);
    }

    template <typename Target>
    void dragOn (Target& target, juce::Point<float> from, juce::Point<float> to)
    {
        target.mouseDown (mouseAt (target, from, from));
        for (int step = 1; step <= 4; ++step)
            target.mouseDrag (mouseAt (target, from + (to - from) * ((float) step / 4.0f), from));
        target.mouseUp (mouseAt (target, to, from));
    }
}

TEST_CASE ("0.77.1: a MIDI part's left edge drags its start; the notes stay where they are in the song", "[ui][edges][077]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 4.0, 4.0, "Riff", { { 0.0, 1.0, 60, 0.8f }, { 2.0, 1.0, 64, 0.8f } } });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto bounds = arrange.getPartBounds (track, 0);
    const auto pixelsPerBeat = bounds.getWidth() / 4.0f;
    const auto part = [&] { return studio.getProject().findTrack (track)->parts.at (0); };

    // Two beats left: the part starts at beat 2, its notes still at song beats 4 and 6.
    dragOn (arrange, { bounds.getX() + 2.0f, bounds.getCentreY() }, { bounds.getX() + 2.0f - pixelsPerBeat * 2.0f, bounds.getCentreY() });
    CHECK_THAT (part().start, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (part().length, WithinAbs (6.0, 1.0e-9));
    REQUIRE (part().notes.size() == 2);
    CHECK_THAT (part().start + part().notes[0].start, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (part().start + part().notes[1].start, WithinAbs (6.0, 1.0e-9));

    // Three beats right (to beat 5): the first note, now before the part's start, is left out.
    bounds = arrange.getPartBounds (track, 0);
    dragOn (arrange, { bounds.getX() + 2.0f, bounds.getCentreY() }, { bounds.getX() + 2.0f + pixelsPerBeat * 3.0f, bounds.getCentreY() });
    CHECK_THAT (part().start, WithinAbs (5.0, 1.0e-9));
    CHECK_THAT (part().length, WithinAbs (3.0, 1.0e-9));
    REQUIRE (part().notes.size() == 1);
    CHECK_THAT (part().start + part().notes[0].start, WithinAbs (6.0, 1.0e-9));

    // One undo step each.
    studio.undo();
    CHECK_THAT (part().start, WithinAbs (2.0, 1.0e-9));
    CHECK (part().notes.size() == 2);

    // The middle still moves it.
    bounds = arrange.getPartBounds (track, 0);
    dragOn (arrange, bounds.getCentre(), bounds.getCentre().translated (pixelsPerBeat, 0.0f));
    CHECK_THAT (part().start, WithinAbs (3.0, 1.0e-9));
    CHECK_THAT (part().length, WithinAbs (6.0, 1.0e-9));
}

TEST_CASE ("0.77.1: a note's left end drags its start; its end stays", "[ui][edges][077]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 1300);
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 4.0, 4.0, "Riff", { { 1.0, 2.0, 60, 0.8f } } });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    const auto area = arrange.getPartBounds (track, 0);
    arrange.mouseDoubleClick (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), area.getCentre(),
                                                juce::ModifierKeys::leftButtonModifier, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &arrange, &arrange,
                                                juce::Time::getCurrentTime(), area.getCentre(), juce::Time::getCurrentTime(), 2, false));
    studio.dispatchPendingMessages();
    REQUIRE (view.isKeyEditorOpen());
    view.setKeyEditorSize (100);
    auto& editor = view.getKeyEditor();
    editor.setTool (KeyEditor::Tool::select);
    const auto notes = [&] { return editor.getPart()->notes; };
    const auto beatWidth = (float) (editor.xForBeat (5.0) - editor.xForBeat (4.0));

    // Half a beat left: starts at 0.5, still ends at 3.
    auto note = editor.getNoteBounds (0);
    dragOn (editor, { note.getX() + 2.0f, note.getCentreY() }, { note.getX() + 2.0f - beatWidth * 0.5f, note.getCentreY() });
    CHECK_THAT (notes()[0].start, WithinAbs (0.5, 1.0e-9));
    CHECK_THAT (notes()[0].length, WithinAbs (2.5, 1.0e-9));
    CHECK (notes()[0].pitch == 60);

    // Far left: no further than the part's start.
    note = editor.getNoteBounds (0);
    dragOn (editor, { note.getX() + 2.0f, note.getCentreY() }, { note.getX() - beatWidth * 6.0f, note.getCentreY() });
    CHECK_THAT (notes()[0].start, WithinAbs (0.0, 1.0e-9));
    CHECK_THAT (notes()[0].length, WithinAbs (3.0, 1.0e-9));

    // Far right: it keeps at least a grid step.
    note = editor.getNoteBounds (0);
    dragOn (editor, { note.getX() + 2.0f, note.getCentreY() }, { note.getRight() + beatWidth * 4.0f, note.getCentreY() });
    CHECK_THAT (notes()[0].start + notes()[0].length, WithinAbs (3.0, 1.0e-9));
    CHECK (notes()[0].length > 0.0);
    CHECK (notes()[0].length < 1.0);
}

TEST_CASE ("0.77.1: muted notes are kept and saved but not played", "[engine][mute][077]")
{
    Engine engine;
    engine.prepare (48000.0, 512);
    Project project;
    Track track;
    track.id = 1;
    project.tracks.push_back (track);
    auto instrument = std::make_unique<Probe>();
    auto* probe = instrument.get();
    engine.setInstrument (1, std::move (instrument));
    Part part { 0.0, 4.0, "Riff", { { 0.0, 0.5, 60, 0.8f }, { 1.0, 0.5, 64, 0.8f, true }, { 2.0, 0.5, 67, 0.8f } } };
    project.tracks[0].parts = { part };
    engine.update (project);
    engine.play();
    renderSamples (engine, 48000 * 2);
    const auto on = probe->notesOn();
    REQUIRE (on.size() == 2);
    CHECK (on[0].message.getNoteNumber() == 60);
    CHECK (on[1].message.getNoteNumber() == 67);

    // Saved and opened again: still muted (an unmuted note writes nothing new).
    const auto xml = project.toXml();
    const auto copy = Project::fromXml (*xml);
    REQUIRE (copy.has_value());
    const auto& notes = copy->tracks[0].parts[0].notes;
    REQUIRE (notes.size() == 3);
    CHECK_FALSE (notes[0].muted);
    CHECK (notes[1].muted);
}

TEST_CASE ("0.77.1: the MIDI editor's MUTE tool: a click mutes and unmutes, a box mutes what it touches", "[ui][mute][077]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 1300);
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 4.0, 4.0, "Riff", { { 0.0, 1.0, 60, 0.8f }, { 1.0, 1.0, 62, 0.8f }, { 2.0, 1.0, 64, 0.8f } } });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    const auto area = arrange.getPartBounds (track, 0);
    arrange.mouseDoubleClick (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), area.getCentre(),
                                                juce::ModifierKeys::leftButtonModifier, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &arrange, &arrange,
                                                juce::Time::getCurrentTime(), area.getCentre(), juce::Time::getCurrentTime(), 2, false));
    studio.dispatchPendingMessages();
    REQUIRE (view.isKeyEditorOpen());
    view.setKeyEditorSize (100);
    auto& editor = view.getKeyEditor();
    // Key 7 picks it.
    CHECK (editor.keyPressed (juce::KeyPress ('7', {}, '7')));
    REQUIRE (editor.getTool() == KeyEditor::Tool::mute);
    const auto notes = [&] { return editor.getPart()->notes; };

    // A click: muted; again: unmuted; each one undo step.
    const auto middle = editor.getNoteBounds (1).getCentre();
    dragOn (editor, middle, middle);
    CHECK (notes()[1].muted);
    CHECK_FALSE (notes()[0].muted);
    dragOn (editor, middle, middle);
    CHECK_FALSE (notes()[1].muted);
    studio.undo();
    CHECK (notes()[1].muted);

    // A box over all three (one muted already): all muted; the same box again: all unmuted.
    const auto first = editor.getNoteBounds (0), last = editor.getNoteBounds (2);
    const juce::Point<float> from (first.getX() - 4.0f, juce::jmin (first.getY(), last.getY()) - 4.0f);
    const juce::Point<float> to (last.getRight() + 4.0f, juce::jmax (first.getBottom(), last.getBottom()) + 4.0f);
    // (Drawn from the bottom right: the first note starts at the editor's left edge, beside the keys.)
    dragOn (editor, to, from);
    for (const auto& n : notes())
        CHECK (n.muted);
    dragOn (editor, to, from);
    for (const auto& n : notes())
        CHECK_FALSE (n.muted);
    CHECK (notes().size() == 3);   // nothing erased
}

// 0.77.2 (the owner: "the MIDI editor in PR7-S isn't showing the same notes as the drum map in PR7-D"): PR7-D
// starts on its GROOVE LIBRARY layout, not General MIDI; the PR7-D Map names its notes as PR7-D does.
TEST_CASE ("0.77.2: the PR7-D Map names notes as PR7-D's Groove Library layout does", "[drums][077]")
{
    const auto map = DrumMap::pr7d();
    CHECK (map.name == "PR7-D Map");
    CHECK (map.isIdentity());   // names only: every note plays as it is
    CHECK (map.sounds[36].name == "Kick: Hit (Stack)");
    CHECK (map.sounds[38].name == "Snare: Center (Stack)");
    CHECK (map.sounds[27].name == "China 1: Crash");          // GM: High Q
    CHECK (map.sounds[49].name == "Cymbal 2: Crash");         // GM: Crash Cymbal 1
    CHECK (map.sounds[50].name == "Cymbal 2: Mute Hit");      // GM: High Tom
    CHECK (map.sounds[41].name == "Floortom 2: Center");
    CHECK (map.sounds[127].name == "Snare: Sidestick");
    CHECK (map.sounds[5].name.isEmpty());
    int named = 0;
    for (const auto& sound : map.sounds)
        named += sound.name.isNotEmpty() ? 1 : 0;
    CHECK (named == 122);   // notes 6-127, as PR7-D's table
    CHECK (DrumMap::builtIn ("PR7-D Map").has_value());
    CHECK (DrumMap::builtIn ("GM Map").has_value());
    CHECK_FALSE (DrumMap::builtIn ("My Kit").has_value());
}

TEST_CASE ("0.77.2: a PR7-D track gets the PR7-D Map (chosen, or from the GM Map when a song opens)", "[drums][077]")
{
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    CHECK (studio.defaultDrumMapFor (track) == "GM Map");   // PR7-K: GM

    // A song whose drum track plays PR7-D, on the GM Map (as songs before 0.77.2 had it).
    auto song = studio.getProject();
    song.tracks[0].instrument.kind = Instrument::Kind::plugin;
    song.tracks[0].instrument.plugin.name = "Paragon PR7-D";
    song.tracks[0].drumMap = "GM Map";
    song.tracks[0].drumEditor = true;
    song.drumMaps.push_back (DrumMap::gm());
    const auto file = juce::File::createTempFile (Project::fileExtension);
    REQUIRE (song.toXml()->writeTo (file));
    juce::String problems;
    studio.open (file, problems);   // (PR7-D isn't installed here: reported missing)
    const auto& opened = studio.getProject().tracks[0];
    CHECK (opened.drumMap == "PR7-D Map");
    CHECK (studio.getDrumMapOf (opened.id)->sounds[50].name == "Cymbal 2: Mute Hit");
    CHECK (studio.defaultDrumMapFor (opened.id) == "PR7-D Map");

    // A map of the owner's own is left alone.
    studio.addDrumMap ([] { auto m = DrumMap::gm(); m.name = "My Kit"; return m; }());
    studio.setTrackDrumMap (opened.id, "My Kit");
    studio.followDefaultDrumMap (opened.id);
    CHECK (studio.getProject().tracks[0].drumMap == "My Kit");
    studio.getLibrary().removeRecent (file);
    file.deleteFile();
}

// 0.77.3 (the owner: "a setting in the Hub to set a default drum map so that every project can load with it"; a
// horizontal scroll bar always visible in the MIDI editor; the editors' tool icons as the main window's).
TEST_CASE ("0.77.3: HUB SETTINGS' DEFAULT DRUM MAP is the one drum tracks get", "[drums][077]")
{
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    CHECK (studio.getDefaultDrumMap().isEmpty());   // automatic
    CHECK (studio.defaultDrumMapFor (track) == "GM Map");

    // A built-in one, for every track (PR7-K's too).
    studio.setDefaultDrumMap ("PR7-D Map");
    CHECK (studio.defaultDrumMapFor (track) == "PR7-D Map");
    studio.setTrackDrumEditor (track, true);
    CHECK (studio.getProject().findTrack (track)->drumMap == "PR7-D Map");

    // One from the Drum Maps folder.
    const auto folder = juce::File::createTempFile ("drums077");
    folder.createDirectory();
    auto kit = DrumMap::gm();
    kit.name = "Studio Kit";
    kit.sounds[36].name = "Big Kick";
    REQUIRE (kit.writeDrm (folder.getChildFile ("Studio Kit.drm")));
    studio.setDrumMapFolderForTests (folder);
    studio.setDefaultDrumMap ("Studio Kit");
    CHECK (studio.defaultDrumMapFor (track) == "Studio Kit");

    // A song saved on a built-in map opens on the default; a map gone missing falls back to automatic.
    auto song = studio.getProject();
    song.tracks[0].drumMap = "GM Map";
    song.drumMaps = { DrumMap::gm() };
    const auto file = juce::File::createTempFile (Project::fileExtension);
    REQUIRE (song.toXml()->writeTo (file));
    juce::String problems;
    studio.open (file, problems);
    CHECK (studio.getProject().tracks[0].drumMap == "Studio Kit");
    CHECK (studio.getDrumMapOf (studio.getProject().tracks[0].id)->sounds[36].name == "Big Kick");
    studio.setDefaultDrumMap ("Gone Kit");
    CHECK_FALSE (studio.isDrumMapAvailable ("Gone Kit"));
    CHECK (studio.defaultDrumMapFor (studio.getProject().tracks[0].id) == "GM Map");

    // The Hub's box: automatic, the two built in, the folder's.
    studio.setDefaultDrumMap ({});
    Hub hub (studio);
    hub.setSize (1500, 900);
    auto& box = hub.getDrumMapBox();
    CHECK (box.getNumItems() == 4);
    CHECK (box.getSelectedId() == 1);
    box.setSelectedId (3, juce::sendNotificationSync);
    CHECK (studio.getDefaultDrumMap() == "PR7-D Map");
    box.setSelectedId (4, juce::sendNotificationSync);
    CHECK (studio.getDefaultDrumMap() == "Studio Kit");
    box.setSelectedId (1, juce::sendNotificationSync);
    CHECK (studio.getDefaultDrumMap().isEmpty());

    studio.getLibrary().removeRecent (file);
    file.deleteFile();
    folder.deleteRecursively();
}

TEST_CASE ("0.77.3: the MIDI editor's horizontal scroll bar always has a handle, and its tools the arrangement's icons", "[ui][077]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 1300);
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 0.0, 4.0, "Riff", { { 0.0, 1.0, 60, 0.8f } } });
    studio.dispatchPendingMessages();
    view.getArrangeView().select (track, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    REQUIRE (view.isKeyEditorOpen());
    auto& editor = view.getKeyEditor();
    // Zoomed far out (the whole song in view): there's still more to the right, so the handle shows.
    for (int i = 0; i < 12; ++i)
        editor.zoom (false);
    auto& across = editor.getScrollBarAcross();
    CHECK (across.isVisible());
    CHECK (across.getCurrentRangeSize() < across.getMaximumRangeLimit() - across.getMinimumRangeLimit());

    // MUTE is a tool key with the arrangement's MUTE icon (and its pointer).
    CHECK (editor.keyPressed (juce::KeyPress ('7', {}, '7')));
    REQUIRE (editor.getTool() == KeyEditor::Tool::mute);
    CHECK (editor.getMouseCursor() != juce::MouseCursor (juce::MouseCursor::NormalCursor));
}

// 0.77.4 (the owner: the track clicked in the track list lit in VISIBILITY too; the MIDI editor's scroll bar
// across moved up under the notes: at the bottom, under the velocities, it was off the screen).
TEST_CASE ("0.77.4: VISIBILITY lights the selected track's row", "[ui][077]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto first = studio.getProject().tracks[0].id;
    const auto second = studio.addTrack ({});
    studio.selectTrack (second);
    studio.dispatchPendingMessages();
    auto& inspector = view.getInspector();
    inspector.setTab (Inspector::Tab::visibility);
    CHECK (inspector.isVisibilityRowLit (second));
    CHECK_FALSE (inspector.isVisibilityRowLit (first));
    studio.selectTrack (first);
    studio.dispatchPendingMessages();
    CHECK (inspector.isVisibilityRowLit (first));
    CHECK_FALSE (inspector.isVisibilityRowLit (second));
}

TEST_CASE ("0.77.4: the MIDI editor's scroll bar across sits under the notes, over the lanes", "[ui][077]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 1300);
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 0.0, 4.0, "Riff", { { 0.0, 1.0, 60, 0.8f } } });
    studio.dispatchPendingMessages();
    view.getArrangeView().select (track, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    REQUIRE (view.isKeyEditorOpen());
    auto& editor = view.getKeyEditor();
    const auto across = editor.getScrollBarAcross().getBounds().toFloat();
    const auto notes = editor.getNotesArea();
    const auto velocities = editor.getVelocityArea();
    REQUIRE (! velocities.isEmpty());
    CHECK (std::abs (across.getY() - notes.getBottom()) <= 1.0f);
    CHECK (std::abs (across.getBottom() - velocities.getY()) <= 1.0f);
    CHECK (velocities.getBottom() <= (float) editor.getHeight() + 0.5f);
    CHECK (across.getBottom() < (float) editor.getHeight() - 20.0f);   // well off the bottom edge
}
