// The drum editor and drum maps (0.26.0): the GM Map, maps saved and loaded (ours and .drm),
// played through (O-notes, mutes, channels) and played in (I-notes), the studio's actions, the
// MIDI editor's drum view and the Drum Map Setup.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/DrumMapPage.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {}, int clicks = 1)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, mods.withFlags (juce::ModifierKeys::leftButtonModifier),
                                 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &target, &target, now, downAt, now, clicks, at != downAt);
    }
    void drag (MainView& view, juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods = {})
    {
        auto& editor = view.getKeyEditor();
        view.mouseDown (mouseAt (editor, from, from, mods));
        editor.mouseDown (mouseAt (editor, from, from, mods));
        if (to != from)
            for (int step = 1; step <= 8; ++step)
                editor.mouseDrag (mouseAt (editor, from + (to - from) * ((float) step / 8.0f), from, mods));
        editor.mouseUp (mouseAt (editor, to, from, mods));
    }

    // A drum map as the other DAW writes it (its layout as we know it: a "Map" list of items by pitch, an "Order" list).
    juce::String drmText()
    {
        juce::String items;
        for (int pitch = 0; pitch < 128; ++pitch)
        {
            juce::String name = pitch == 24 ? "Kick Hard" : pitch == 26 ? "Snare Rim" : pitch == 30 ? "Hat Tight" : "";
            const auto out = pitch == 24 ? 36 : pitch;
            items << "<item><int name=\"INote\" value=\"" << (pitch == 24 ? 36 : pitch) << "\"/><int name=\"ONote\" value=\"" << out
                  << "\"/><int name=\"Channel\" value=\"" << (pitch == 30 ? 9 : -1) << "\"/><float name=\"Length\" value=\"200\"/>"
                  << "<int name=\"Mute\" value=\"" << (pitch == 26 ? 1 : 0) << "\"/><string name=\"Name\" value=\"" << name << "\" wide=\"true\"/></item>\n";
        }
        return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<DrumMap>\n<string name=\"Name\" value=\"My Kit\" wide=\"true\"/>\n"
               "<list name=\"Map\" type=\"list\">\n" + items + "</list>\n"
               "<list name=\"Order\" type=\"int\"><item value=\"30\"/><item value=\"24\"/><item value=\"26\"/></list>\n"
               "<int name=\"Flags\" value=\"0\"/>\n</DrumMap>\n";
    }
}

TEST_CASE ("The GM Map: the General MIDI drums by name, every note to itself", "[drums]")
{
    const auto gm = DrumMap::gm();
    CHECK (gm.name == "GM Map");
    CHECK (gm.sounds[35].name == "Acoustic Bass Drum");
    CHECK (gm.sounds[36].name == "Bass Drum 1");
    CHECK (gm.sounds[38].name == "Acoustic Snare");
    CHECK (gm.sounds[42].name == "Closed Hi-Hat");
    CHECK (gm.sounds[44].name == "Pedal Hi-Hat");
    CHECK (gm.sounds[46].name == "Open Hi-Hat");
    CHECK (gm.sounds[49].name == "Crash Cymbal 1");
    CHECK (gm.sounds[51].name == "Ride Cymbal 1");
    CHECK (gm.sounds[52].name == "Chinese Cymbal");
    CHECK (gm.sounds[81].name == "Open Triangle");
    CHECK (gm.sounds[87].name == "Open Surdo");
    CHECK (gm.sounds[20].name.isEmpty());
    CHECK (gm.isIdentity());
    CHECK (gm.order.size() == 128);
    CHECK (gm.pitchForInput (40) == 40);
    // Our own XML and the .drm layout both come back the same.
    auto edited = gm;
    edited.name = "Edited";
    edited.sounds[36].outNote = 24;
    edited.sounds[38].mute = true;
    edited.sounds[42].channel = 9;
    edited.sounds[50].inNote = 40;
    std::swap (edited.order[0], edited.order[36]);
    CHECK_FALSE (edited.isIdentity());
    CHECK (DrumMap::fromXml (*edited.toXml()) == edited);
    juce::String error;
    const auto drm = DrumMap::readDrm (edited.toDrm(), error);
    REQUIRE (drm.has_value());
    CHECK (*drm == edited);
}

TEST_CASE ("Reading a .drm: names, I-notes, O-notes, channels, mutes and the order", "[drums]")
{
    juce::String error;
    const auto map = DrumMap::readDrm (drmText(), error);
    REQUIRE (map.has_value());
    CHECK (map->name == "My Kit");
    CHECK (map->sounds[24].name == "Kick Hard");
    CHECK (map->sounds[24].inNote == 36);
    CHECK (map->sounds[24].outNote == 36);
    CHECK (map->sounds[26].mute);
    CHECK (map->sounds[30].channel == 9);
    CHECK (map->order.size() == 128);
    CHECK (map->order[0] == 30);
    CHECK (map->order[1] == 24);
    CHECK (map->order[2] == 26);
    CHECK (map->order[3] == 0);
    // Playing key 36 plays the "Kick Hard" sound (its I-note), not the sound at 36.
    CHECK (map->pitchForInput (36) == 24);
    // Not a drum map.
    CHECK_FALSE (DrumMap::readDrm ("<Song/>", error).has_value());
    CHECK (error.isNotEmpty());
    CHECK_FALSE (DrumMap::readDrm ("not xml at all", error).has_value());
}

TEST_CASE ("Drum maps are saved with the song, and a track keeps its map", "[drums][project]")
{
    Project project;
    Track track;
    track.id = 1;
    track.drumMap = "GM Map";
    track.drumEditor = true;
    project.tracks.push_back (track);
    auto map = DrumMap::gm();
    map.sounds[38].mute = true;
    project.drumMaps.push_back (map);
    const auto opened = Project::fromXml (*project.toXml());
    REQUIRE (opened.has_value());
    REQUIRE (opened->drumMaps.size() == 1);
    CHECK (opened->drumMaps[0] == map);
    CHECK (opened->tracks[0].drumMap == "GM Map");
    CHECK (opened->tracks[0].drumEditor);
    REQUIRE (opened->drumMapOf (opened->tracks[0]) != nullptr);
    CHECK (opened->drumMapOf (opened->tracks[0])->sounds[38].mute);
    // A map the song doesn't have: the track has none.
    project.drumMaps.clear();
    const auto again = Project::fromXml (*project.toXml());
    REQUIRE (again.has_value());
    CHECK (again->tracks[0].drumMap.isEmpty());
    CHECK_FALSE (again->tracks[0].drumEditor);
}

TEST_CASE ("A drum map plays the notes as its O-notes, on its channels, muted sounds silent; keys played in are its I-notes", "[drums][engine]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    auto instrument = std::make_unique<Probe>();
    auto* probe = instrument.get();
    studio.getEngine().setInstrument (trackId, std::move (instrument));
    auto map = DrumMap::gm();
    map.name = "Kit";
    map.sounds[36].outNote = 24;   // the kick: the instrument's C0
    map.sounds[38].mute = true;    // the snare: muted
    map.sounds[42].channel = 9;    // the hat: on channel 10
    map.sounds[40].inNote = 37;    // the electric snare: played from key 37
    REQUIRE (studio.addDrumMap (map) == "Kit");
    studio.setTrackDrumMap (trackId, "Kit");
    CHECK (studio.getProject().findTrack (trackId)->drumEditor);   // (the drum view, at first)
    studio.addPart (trackId, { 0.0, 4.0, "Beat", { { 0.0, 0.25, 36, 0.8f }, { 1.0, 0.25, 38, 0.8f }, { 2.0, 0.25, 42, 0.8f } } });
    studio.play();
    renderSamples (studio.getEngine(), 24000 * 4);
    studio.stop();
    const auto on = probe->notesOn();
    REQUIRE (on.size() == 2);
    CHECK (on[0].message.getNoteNumber() == 24);
    CHECK (on[0].message.getChannel() == 1);
    CHECK (on[1].message.getNoteNumber() == 42);
    CHECK (on[1].message.getChannel() == 10);

    // Played in: key 37 is the electric snare (40), sent as itself (its O-note); recorded as 40.
    probe->received.clear();
    studio.setPosition (0.0);
    studio.record();
    renderSamples (studio.getEngine(), 12000);
    studio.getEngine().getKeyboardState().noteOn (1, 37, 0.9f);
    renderSamples (studio.getEngine(), 12000);
    studio.getEngine().getKeyboardState().noteOff (1, 37, 0.0f);
    renderSamples (studio.getEngine(), 12000);
    studio.stop();
    auto heard = false;
    for (const auto& r : probe->notesOn())
        heard |= r.message.getNoteNumber() == 40;
    CHECK (heard);
    const auto& parts = studio.getProject().findTrack (trackId)->parts;
    REQUIRE (parts.size() == 2);
    REQUIRE (parts[1].notes.size() == 1);
    CHECK (parts[1].notes[0].pitch == 40);
}

TEST_CASE ("The studio's drum maps: the GM Map on choosing it, the folder's maps, renames and removals", "[drums][studio]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    CHECK (studio.getDrumMapOf (trackId) == nullptr);
    studio.setTrackDrumMap (trackId, "GM Map");
    REQUIRE (studio.getDrumMapOf (trackId) != nullptr);
    CHECK (studio.getProject().drumMaps.size() == 1);
    // Names stay unique.
    CHECK (studio.addDrumMap (DrumMap::gm()) == "GM Map 2");
    // A map in the drum map folder is offered, and loaded into the song when chosen.
    const auto folder = juce::File::createTempFile ("drums");
    folder.createDirectory();
    REQUIRE (folder.getChildFile ("Studio Kit.drm").replaceWithText (drmText()));
    studio.setDrumMapFolderForTests (folder);
    REQUIRE (studio.getLibraryDrumMaps().size() == 1);
    studio.setTrackDrumMap (trackId, "Studio Kit");
    REQUIRE (studio.getDrumMapOf (trackId) != nullptr);
    CHECK (studio.getDrumMapOf (trackId)->sounds[24].name == "Kick Hard");
    // Renamed: the track follows. Removed: the track has none.
    auto renamed = *studio.getDrumMapOf (trackId);
    renamed.name = "Rock Kit";
    CHECK (studio.setDrumMap ("Studio Kit", renamed));
    CHECK (studio.getProject().findTrack (trackId)->drumMap == "Rock Kit");
    renamed.name = "GM Map";
    CHECK_FALSE (studio.setDrumMap ("Rock Kit", renamed));   // (another map's name)
    studio.removeDrumMap ("Rock Kit");
    CHECK (studio.getProject().findTrack (trackId)->drumMap.isEmpty());
    // Loaded from a file.
    juce::String error;
    CHECK (studio.loadDrumMap (folder.getChildFile ("Studio Kit.drm"), error) == "My Kit");
    CHECK (studio.loadDrumMap (folder.getChildFile ("missing.drm"), error).isEmpty());
    // Audio tracks take no map.
    const auto audio = studio.addAudioTrack (false);
    studio.setTrackDrumMap (audio, "GM Map");
    CHECK (studio.getDrumMapOf (audio) == nullptr);
    folder.deleteRecursively();
}

TEST_CASE ("The MIDI editor's drum view: named rows, diamonds, the drumstick, moving by rows, mutes", "[drums][ui][editor]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 1000);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, { 0.0, 4.0, "Beat", { { 0.0, 0.25, 36, 0.8f }, { 1.0, 0.25, 38, 0.8f } } });
    studio.dispatchPendingMessages();
    view.getArrangeView().select (trackId, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    view.setKeyEditorSize (100);
    auto& editor = view.getKeyEditor();
    editor.setSize (1300, 700);
    REQUIRE (editor.getPart() != nullptr);
    const auto notes = [&] { return editor.getPart()->notes; };
    CHECK_FALSE (editor.isDrumView());

    // DRUMS: the GM Map chosen, the drum list instead of the piano.
    editor.setDrumView (true);
    REQUIRE (editor.isDrumView());
    CHECK (studio.getProject().findTrack (trackId)->drumMap == "GM Map");
    CHECK (editor.listWidth() == KeyEditor::drumListWidth);
    // The named sounds (27 to 87), in the map's order.
    CHECK (editor.getDrumRows().size() == 61);
    CHECK (editor.getDrumRows().front() == 27);
    CHECK (editor.drumNameOf (36) == "Bass Drum 1");
    // A row a drum: the kick's and the snare's rows a row apart... wherever they are in the list.
    const auto kickY = editor.yForPitch (36), snareY = editor.yForPitch (38);
    CHECK_THAT (snareY - kickY, WithinAbs (2.0 * editor.getRowHeight(), 0.01));   // (37, Side Stick, between them)
    CHECK (editor.pitchForY (kickY + 1.0f) == 36);
    // Hits are diamonds, centred on their start.
    const auto bounds = editor.getNoteBounds (0);
    CHECK_THAT (bounds.getCentreX(), WithinAbs (editor.xForBeat (0.0), 0.5));
    // Only the sounds with notes, then the named ones again.
    editor.setDrumRowsShown (KeyEditor::DrumRowsShown::withEvents);
    CHECK (editor.getDrumRows() == std::vector<int> { 36, 38 });
    editor.setDrumRowsShown (KeyEditor::DrumRowsShown::named);

    // The drumstick dragged right: a hit on every grid step (sixteenths) along the closed hi-hat's row.
    editor.setTool (KeyEditor::Tool::draw);
    const auto hatY = editor.yForPitch (42) + editor.getRowHeight() * 0.5f;
    drag (view, { editor.xForBeat (2.0) + 2.0f, hatY }, { editor.xForBeat (2.9), hatY });
    auto hats = 0;
    for (const auto& n : notes())
        if (n.pitch == 42)
            ++hats;
    CHECK (hats == 4);   // 2, 2.25, 2.5, 2.75
    // One undo step for the run.
    studio.undo();
    CHECK (notes().size() == 2);
    studio.redo();
    CHECK (notes().size() == 6);

    // SELECT: the kick dragged down a row is now the next drum in the list (Side Stick, 37).
    editor.setTool (KeyEditor::Tool::select);
    const auto kick = editor.getNoteBounds (0).getCentre();
    REQUIRE (editor.getNotesArea().contains (kick));   // (the whole diamond in view, though the hit is on the part's first beat)
    drag (view, kick, kick + juce::Point<float> (0.0f, editor.getRowHeight()));
    CHECK (notes()[0].pitch == 37);
    // Up (the key) moves the selected hit one row up the list: back to the kick.
    editor.selectNotes ({ 0 });
    editor.keyPressed (juce::KeyPress (juce::KeyPress::upKey));
    CHECK (notes()[0].pitch == 36);

    // M: muting the snare in the map (hollow hits, silent: the engine's test checks the sound).
    const auto mute = editor.getDrumMuteBox (38);
    drag (view, mute.getCentre(), mute.getCentre());
    CHECK (studio.getDrumMapOf (trackId)->sounds[38].mute);
    editor.toggleDrumMute (38);
    CHECK_FALSE (studio.getDrumMapOf (trackId)->sounds[38].mute);
    // Renaming a drum.
    editor.renameDrumSound (36, "Kick");
    CHECK (editor.drumNameOf (36) == "Kick");
    // Selecting all the notes of a sound.
    editor.selectNotesOfSound (42);
    CHECK (editor.getSelectedNotes().size() == 4);

    // DRUMS off: the piano again, its rows as they were.
    editor.setDrumView (false);
    CHECK_FALSE (editor.isDrumView());
    CHECK (editor.listWidth() == KeyEditor::keysWidth);
    CHECK (studio.getProject().findTrack (trackId)->drumMap == "GM Map");   // (the map stays)
}

TEST_CASE ("The Drum Map Setup: new maps, copies, names, notes, channels, mutes, the order", "[drums][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addDrumMap (DrumMap::gm());
    DrumMapPage page (studio, [trackId] { return trackId; });
    CHECK (page.getSelectedMapName() == "GM Map");
    const auto name = page.newMap();
    CHECK (name == "Empty Map");
    CHECK (studio.getProject().findDrumMap (name)->sounds[0].name == "Sound 1");
    CHECK (page.renameSelected ("Metal Kit"));
    CHECK (studio.getProject().findDrumMap ("Metal Kit") != nullptr);
    page.setSoundName (36, "Kick Left");
    page.setInNote (36, 48);
    page.setOutNote (36, 24);
    page.setChannel (36, 9);
    page.toggleMute (37);
    page.moveSound (36, true);
    const auto* map = studio.getProject().findDrumMap ("Metal Kit");
    REQUIRE (map != nullptr);
    CHECK (map->sounds[36] == DrumSound { "Kick Left", 48, 24, 9, false });
    CHECK (map->sounds[37].mute);
    CHECK (map->order[35] == 36);
    CHECK (map->order[36] == 35);
    // For the track; a copy; removing the map takes it from the track.
    page.assignToTrack();
    CHECK (studio.getProject().findTrack (trackId)->drumMap == "Metal Kit");
    CHECK (page.newCopy() == "Metal Kit 2");
    page.selectMap ("Metal Kit");
    page.removeSelected();
    CHECK (studio.getProject().findDrumMap ("Metal Kit") == nullptr);
    CHECK (studio.getProject().findTrack (trackId)->drumMap.isEmpty());
    CHECK (page.getTable().getNumRows() == 128);
}
