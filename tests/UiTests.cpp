#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    template <typename Type>
    void findAll (juce::Component& parent, std::vector<Type*>& found)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* match = dynamic_cast<Type*> (child))
                found.push_back (match);
            findAll (*child, found);
        }
    }

    // A mouse event on `target`, as JUCE would send it (the button held down since `downAt`).
    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt,
                              juce::ModifierKeys mods = {}, int clicks = 1)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, clicks, at != downAt);
    }

    // A click or a drag in the key editor, in steps, as a hand would; the
    // window hears the click too (so the keys go to the editor afterwards).
    void drag (MainView& view, juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods = {})
    {
        auto& editor = view.getKeyEditor();
        view.mouseDown (mouseAt (editor, from, from, mods));
        editor.mouseDown (mouseAt (editor, from, from, mods));
        if (to != from)
            for (int step = 1; step <= 4; ++step)
                editor.mouseDrag (mouseAt (editor, from + (to - from) * ((float) step / 4.0f), from, mods));
        editor.mouseUp (mouseAt (editor, to, from, mods));
    }

    void click (MainView& view, juce::Point<float> at, juce::ModifierKeys mods = {}) { drag (view, at, at, mods); }

    juce::TextButton* buttonNamed (juce::Component& parent, const juce::String& text)
    {
        std::vector<juce::TextButton*> buttons;
        findAll (parent, buttons);
        for (auto* button : buttons)
            if (button->getButtonText() == text)
                return button;
        return nullptr;
    }
}

TEST_CASE ("The studio's window: transport, tracks lined up with their lanes, parts where they belong", "[ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);

    // The transport's screens.
    auto& transport = view.getTransport();
    CHECK (transport.getPositionText() == "1. 1. 1.   0");
    CHECK (transport.getTempoText() == "120.00");
    studio.setPosition (4.5);
    CHECK (transport.getPositionText() == "2. 1. 3.   0");

    // Its keys run the transport.
    auto* play = buttonNamed (transport, "PLAY");
    auto* stop = buttonNamed (transport, "STOP");
    auto* cycle = buttonNamed (transport, "CYCLE");
    REQUIRE (play != nullptr);
    REQUIRE (stop != nullptr);
    REQUIRE (cycle != nullptr);
    play->onClick();
    CHECK (studio.getEngine().isPlaying());
    stop->onClick();
    CHECK_FALSE (studio.getEngine().isPlaying());
    cycle->onClick();
    CHECK (studio.getProject().loopOn);

    // A part on the second track is drawn in that track's lane, from its beat.
    const auto second = studio.addTrack ({});
    studio.addPart (second, { 8.0, 4.0, "Verse", { { 0.0, 1.0, 60, 0.8f } } });
    studio.dispatchPendingMessages();   // the screen hears about changes by message, as it would in the app
    auto& arrange = view.getArrangeView();
    const auto bounds = arrange.getPartBounds (second, 0);
    CHECK (bounds.getY() > (float) (Timeline::rulerHeight + Timeline::trackHeight));
    CHECK (arrange.trackAtY (bounds.getCentreY()) == second);
    CHECK (bounds.getX() > 0.0f);

    // Its track row in the list sits level with its lane.
    std::vector<juce::Label*> labels;
    findAll (view.getTrackList(), labels);
    juce::Label* name = nullptr;
    for (auto* label : labels)
        if (label->getText() == "Synth 2")
            name = label;
    REQUIRE (name != nullptr);
    const auto rowTop = name->getParentComponent()->getY();
    CHECK (rowTop == Timeline::rulerHeight + Timeline::trackHeight);

    // Selecting and deleting the part, through the arrangement.
    arrange.select (second, 0);
    CHECK (arrange.getSelection().isValid());
    arrange.deleteSelected();
    CHECK (studio.getProject().findTrack (second)->parts.empty());

    // Keys: space plays and stops, / toggles the cycle.
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    CHECK (studio.getEngine().isPlaying());
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    CHECK_FALSE (studio.getEngine().isPlaying());
    view.keyPressed (juce::KeyPress ('/', {}, '/'));
    CHECK_FALSE (studio.getProject().loopOn);

    // It draws in every finish.
    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
    }
}

TEST_CASE ("The key editor: draw, move, copy, lengthen, velocity, erase, keys, quantize, undo", "[ui][editor]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, { 4.0, 4.0, "Riff", { { 0.0, 1.0, 60, 0.8f }, { 1.0, 0.5, 64, 0.8f } } });

    // Double-clicking the part opens the editor on it.
    auto& arrange = view.getArrangeView();
    studio.dispatchPendingMessages();
    const auto partArea = arrange.getPartBounds (trackId, 0);
    arrange.mouseDoubleClick (mouseAt (arrange, partArea.getCentre(), partArea.getCentre(), {}, 2));
    studio.dispatchPendingMessages();
    REQUIRE (view.isKeyEditorOpen());
    auto& editor = view.getKeyEditor();
    CHECK (editor.isVisible());
    CHECK (editor.getBottom() <= view.getHeight());
    CHECK (arrange.getBottom() <= editor.getY());
    REQUIRE (editor.getPart() != nullptr);
    CHECK (editor.getPart()->name == "Riff");
    const auto notes = [&] { return editor.getPart()->notes; };
    // The middle of a grid cell: a beat into the song, and a pitch's row.
    const auto at = [&] (double songBeat, int pitch) { return juce::Point<float> (editor.xForBeat (songBeat) + 2.0f, editor.yForPitch (pitch) + editor.rowHeight * 0.5f); };
    REQUIRE (editor.getNotesArea().contains (at (4.0, 60)));
    REQUIRE (editor.getNotesArea().contains (at (7.9, 67)));

    // DRAW: a click makes a sixteenth (the grid) from the step clicked in, at the row's pitch, velocity 100.
    editor.setTool (KeyEditor::Tool::draw);
    click (view, at (6.1, 67));
    REQUIRE (notes().size() == 3);
    CHECK_THAT (notes()[2].start, WithinAbs (2.0, 1.0e-9));   // song beat 6 = the part's beat 2
    CHECK_THAT (notes()[2].length, WithinAbs (0.25, 1.0e-9));
    CHECK (notes()[2].pitch == 67);
    CHECK (juce::roundToInt (notes()[2].velocity * 127.0f) == 100);
    CHECK (editor.getSelectedNotes() == std::vector<int> { 2 });
    // Dragging right while drawing makes it longer, to the grid step reached.
    drag (view, at (7.0, 62), at (7.8, 62));
    REQUIRE (notes().size() == 4);
    CHECK_THAT (notes()[3].start, WithinAbs (3.0, 1.0e-9));
    CHECK_THAT (notes()[3].length, WithinAbs (1.0, 1.0e-9));
    // The pencil on a note removes it.
    click (view, at (7.2, 62));
    CHECK (notes().size() == 3);

    // SELECT: drag the first note a beat later and two semitones up.
    editor.setTool (KeyEditor::Tool::select);
    drag (view, at (4.3, 60), at (5.3, 62));
    CHECK_THAT (notes()[0].start, WithinAbs (1.0, 1.0e-9));
    CHECK (notes()[0].pitch == 62);
    CHECK (notes()[0].length == 1.0);   // its length stays
    CHECK (editor.getSelectedNotes() == std::vector<int> { 0 });

    // Its right end, dragged half a beat on: longer.
    const auto end = editor.getNoteBounds (0).getRight() - 2.0f;
    const auto row = editor.getNoteBounds (0).getCentreY();
    drag (view, { end, row }, { end + (float) (0.5 * (editor.xForBeat (5.0) - editor.xForBeat (4.0))), row });
    CHECK_THAT (notes()[0].length, WithinAbs (1.5, 1.0e-9));

    // Alt-drag: a copy moves, the original stays.
    drag (view, at (5.2, 62), at (5.2, 69), juce::ModifierKeys::altModifier);
    REQUIRE (notes().size() == 4);
    CHECK (notes()[0].pitch == 62);
    CHECK (notes()[3].pitch == 69);
    CHECK (notes()[3].start == notes()[0].start);
    CHECK (editor.getSelectedNotes() == std::vector<int> { 3 });

    // Velocity: a click at the top of the lane, on the copy's bar, plays it as hard as it goes.
    const auto lane = editor.getVelocityArea();
    click (view, { editor.xForBeat (4.0 + notes()[3].start) + 1.0f, lane.getY() + 2.0f });
    CHECK (notes()[3].velocity == 1.0f);
    CHECK (notes()[0].velocity == 0.8f);   // the original, under it but not selected, stays

    // A box dragged round two notes selects them; Up moves them a semitone, Delete removes them (not the part).
    REQUIRE (editor.getNotesArea().contains (at (4.05, 57)));
    click (view, at (4.05, 57));   // empty space: nothing selected
    CHECK (editor.getSelectedNotes().empty());
    // From inside G3's row (above it, A3 isn't in the box) down into E3's (below it, D3 isn't).
    const auto boxFrom = juce::Point<float> (editor.xForBeat (4.9), editor.yForPitch (68) + 3.0f);
    const auto boxTo = juce::Point<float> (editor.xForBeat (6.6), editor.yForPitch (63) - 3.0f);
    drag (view, boxFrom, boxTo);
    CHECK (editor.getSelectedNotes() == std::vector<int> { 1, 2 });   // E3 at beat 1, G3 at beat 2
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    CHECK (notes()[1].pitch == 65);
    CHECK (notes()[2].pitch == 68);
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK (notes().size() == 2);
    REQUIRE (studio.getProject().tracks[0].parts.size() == 1);
    // With nothing selected in the editor, Delete still doesn't remove the part.
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK (studio.getProject().tracks[0].parts.size() == 1);

    // ERASE: drag across a note to remove it.
    editor.setTool (KeyEditor::Tool::erase);
    drag (view, at (5.1, 69), at (5.6, 69));
    REQUIRE (notes().size() == 1);
    CHECK (notes()[0].pitch == 62);

    // Q quantizes (all the notes, with none selected) to the 1/4 grid.
    editor.setTool (KeyEditor::Tool::select);
    editor.setGrid (1.0);
    studio.setNotes (trackId, 0, { { 1.4, 0.5, 62, 0.8f }, { 2.6, 0.5, 65, 0.8f } });
    studio.dispatchPendingMessages();
    CHECK (view.keyPressed (juce::KeyPress ('q', {}, 'q')));
    CHECK_THAT (notes()[0].start, WithinAbs (1.0, 1.0e-9));
    CHECK_THAT (notes()[1].start, WithinAbs (3.0, 1.0e-9));

    // Each of those was one undo step.
    studio.undo();
    CHECK_THAT (notes()[0].start, WithinAbs (1.4, 1.0e-9));
    studio.undo();   // the notes set directly
    studio.undo();   // the erase
    CHECK (notes().size() == 2);

    // After a click in the arrangement, Delete is the arrangement's again (the part goes).
    view.mouseDown (mouseAt (arrange, partArea.getCentre(), partArea.getCentre()));
    arrange.select (trackId, 0);
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK (studio.getProject().tracks[0].parts.empty());
    studio.dispatchPendingMessages();
    CHECK (editor.getPart() == nullptr);   // its part is gone: the editor says so
    studio.undo();
    studio.dispatchPendingMessages();
    CHECK (editor.getPart() != nullptr);   // and it's back

    // Moved to another track, the editor stays on it.
    const auto other = studio.addTrack ({});
    studio.movePart (trackId, 0, other, 16.0);
    studio.dispatchPendingMessages();
    REQUIRE (editor.getPart() != nullptr);
    CHECK (editor.getTrackId() == other);

    // Escape closes it; the arrangement takes the room back. It draws in every finish.
    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
    }
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK_FALSE (view.isKeyEditorOpen());
    CHECK_FALSE (editor.isVisible());
}

TEST_CASE ("The key editor in a window of its own, and back", "[ui][editor]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, { 0.0, 16.0, "Riff", { { 0.0, 1.0, 60, 0.8f }, { 15.0, 1.0, 64, 0.8f } } });
    auto& arrange = view.getArrangeView();
    auto& editor = view.getKeyEditor();
    arrange.select (trackId, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    REQUIRE (editor.getPart() != nullptr);
    const auto dockedArrangeHeight = arrange.getHeight();
    const auto dockedRows = editor.getNotesArea().getHeight();
    CHECK (editor.getParentComponent() == &view);

    // WINDOW: the editor moves into its own window; the arrangement takes the room back.
    view.setKeyEditorInWindow (true);
    auto* window = view.getKeyEditorWindow();
    REQUIRE (window != nullptr);
    CHECK (editor.getTopLevelComponent() == window);
    CHECK (editor.isVisible());
    CHECK (editor.isInWindow());
    CHECK (arrange.getHeight() > dockedArrangeHeight + 200);
    CHECK (view.isKeyEditorOpen());
    studio.renameTrack (trackId, "Guitar");   // any change: the title follows the part
    studio.dispatchPendingMessages();
    CHECK (window->getName() == "Key Editor: Riff");

    // Stretched wide (as across three screens): the editor fills it, and fits the whole part.
    // (How tall it can be is up to the system: macOS keeps a window within
    // the screen's height, and the test machines' screens are small.)
    window->setSize (5760, 1000);
    CHECK (editor.getWidth() >= 5700);
    CHECK (editor.getHeight() == window->getHeight());
    editor.fitToPart();
    CHECK (editor.getNotesArea().contains (editor.getNoteBounds (1).getCentre()));   // the last note, 15 beats in
    CHECK (editor.getNotesArea().getHeight() > dockedRows);                         // and more rows than docked

    // Keys in the window: the editor's first, then the studio's.
    editor.selectNotes ({ 0 });
    CHECK (window->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK (editor.getPart()->notes.size() == 1);
    CHECK (studio.getProject().tracks[0].parts.size() == 1);   // the part stays
    CHECK (window->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    CHECK (studio.getEngine().isPlaying());
    CHECK (window->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    CHECK_FALSE (studio.getEngine().isPlaying());

    // DOCK: back under the arrangement.
    view.setKeyEditorInWindow (false);
    CHECK (view.getKeyEditorWindow() == nullptr);
    CHECK (editor.getParentComponent() == &view);
    CHECK (editor.isVisible());
    CHECK_FALSE (editor.isInWindow());
    CHECK (arrange.getHeight() == dockedArrangeHeight);

    // Closed while in its window, it opens in its window again next time.
    view.setKeyEditorInWindow (true);
    view.closeKeyEditor();
    CHECK (view.getKeyEditorWindow() == nullptr);
    CHECK (editor.getParentComponent() == &view);
    CHECK_FALSE (editor.isVisible());
    view.openKeyEditor();
    REQUIRE (view.getKeyEditorWindow() != nullptr);
    CHECK (editor.getTopLevelComponent() == view.getKeyEditorWindow());
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));   // Escape closes it from the main window too
    CHECK (view.getKeyEditorWindow() == nullptr);
}

TEST_CASE ("The key editor: copy, cut and paste notes; Shift+A selects them all", "[ui][editor]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    // An 8-beat part from beat 4: a two-note phrase (C3 then E3, a beat apart), and a G3 later.
    studio.addPart (trackId, { 4.0, 8.0, "Riff", { { 0.0, 0.5, 60, 0.9f }, { 1.0, 0.75, 64, 0.4f }, { 6.0, 0.5, 67, 0.7f } } });
    auto& arrange = view.getArrangeView();
    auto& editor = view.getKeyEditor();
    arrange.select (trackId, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    REQUIRE (editor.getPart() != nullptr);
    const auto notes = [&] { return editor.getPart()->notes; };
    const auto ctrl = [] (char c) { return juce::KeyPress (c, juce::ModifierKeys::commandModifier, 0); };

    // Shift+A selects them all.
    CHECK (view.keyPressed (juce::KeyPress ('A', juce::ModifierKeys::shiftModifier, 'A')));
    CHECK (editor.getSelectedNotes() == std::vector<int> { 0, 1, 2 });

    // Copy the phrase; paste at the playhead (song beat 7 = the part's beat 3), in the same timing.
    editor.selectNotes ({ 0, 1 });
    CHECK (view.keyPressed (ctrl ('c')));
    CHECK (notes().size() == 3);   // copying changes nothing
    studio.setPosition (7.0);
    CHECK (view.keyPressed (ctrl ('v')));
    REQUIRE (notes().size() == 5);
    CHECK (notes()[3] == Note { 3.0, 0.5, 60, 0.9f });
    CHECK (notes()[4] == Note { 4.0, 0.75, 64, 0.4f });
    CHECK (editor.getSelectedNotes() == std::vector<int> { 3, 4 });   // what was pasted

    // One undo takes the paste away.
    studio.undo();
    studio.dispatchPendingMessages();
    CHECK (notes().size() == 3);

    // The playhead outside the part: on the beat after the notes copied (they
    // last 1.75 beats, so 2 beats on), and again after those.
    studio.setPosition (0.0);
    CHECK (view.keyPressed (ctrl ('v')));
    REQUIRE (notes().size() == 5);
    CHECK_THAT (notes()[3].start, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (notes()[4].start, WithinAbs (3.0, 1.0e-9));
    CHECK (view.keyPressed (ctrl ('v')));
    REQUIRE (notes().size() == 7);
    CHECK_THAT (notes()[5].start, WithinAbs (4.0, 1.0e-9));
    // Notes that would land past the part's end aren't pasted (they wouldn't be heard).
    CHECK (view.keyPressed (ctrl ('v')));
    CHECK (view.keyPressed (ctrl ('v')));   // the phrase from beat 8 would start past the end (8)
    CHECK (notes().size() == 9);

    // Cut: copied, and gone.
    editor.selectNotes ({ 2 });   // the G3
    CHECK (view.keyPressed (ctrl ('x')));
    CHECK (notes().size() == 8);
    for (const auto& n : notes())
        CHECK (n.pitch != 67);

    // Into another part on another track, just selected in the arrangement
    // (the last click there): it goes to that part's start.
    const auto other = studio.addTrack ({});
    studio.addPart (other, { 16.0, 4.0, "Bass", {} });
    view.mouseDown (mouseAt (arrange, { 10.0f, 10.0f }, { 10.0f, 10.0f }));
    arrange.select (other, 0);
    studio.dispatchPendingMessages();
    REQUIRE (editor.getPart() != nullptr);
    REQUIRE (editor.getPart()->name == "Bass");
    CHECK (view.keyPressed (ctrl ('v')));
    REQUIRE (notes().size() == 1);
    CHECK (notes()[0] == Note { 0.0, 0.5, 67, 0.7f });
    CHECK (studio.getProject().tracks[0].parts[0].notes.size() == 8);   // the first part as it was
}

TEST_CASE ("The mixer: a strip per track and the master; faders, pan, mute, inserts; it shares the lower zone; no on-screen keyboard", "[ui][mixer]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto first = studio.getProject().tracks[0].id;
    const auto second = studio.addTrack ({});
    studio.dispatchPendingMessages();

    // The on-screen keyboard has gone (the lower zone has its room).
    std::vector<juce::MidiKeyboardComponent*> keyboards;
    findAll (view, keyboards);
    CHECK (keyboards.empty());

    // Alt+F3 opens the mixer under the arrangement, down to the window's bottom.
    auto& mixer = view.getMixer();
    auto& arrange = view.getArrangeView();
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F3Key, juce::ModifierKeys::altModifier, 0)));
    REQUIRE (view.isMixerOpen());
    CHECK_FALSE (view.isMixerInWindow());
    CHECK (mixer.isVisible());
    CHECK (mixer.getY() > arrange.getBottom());
    CHECK (mixer.getBottom() >= view.getHeight() - 10);
    CHECK (mixer.getHeight() > 400);

    // A strip for each track, in order, and the master's last.
    REQUIRE (mixer.getNumStrips() == 3);
    auto& one = mixer.getStrip (0);
    auto& two = mixer.getStrip (1);
    auto& master = mixer.getStrip (2);
    CHECK (one.getBusId() == first);
    CHECK (two.getBusId() == second);
    CHECK (master.getBusId() == Engine::masterBus);
    CHECK (two.getX() > one.getX());
    CHECK (master.getVolume().getHeight() > 100);   // room for a real fader

    // Its fader, pan and mute change the track; the fader is undoable.
    two.getVolume().setValue (-6.0, juce::sendNotificationSync);
    CHECK (studio.getProject().findTrack (second)->volumeDb == -6.0f);
    studio.undo();
    CHECK (studio.getProject().findTrack (second)->volumeDb == 0.0f);
    studio.dispatchPendingMessages();
    CHECK_THAT (two.getVolume().getValue(), WithinAbs (0.0, 1.0e-6));   // the fader follows (to within rounding: macOS gives 3e-15)
    two.getPan().setValue (-0.5, juce::sendNotificationSync);
    CHECK (studio.getProject().findTrack (second)->pan == -0.5f);
    two.getMute().onClick();
    CHECK (studio.getProject().findTrack (second)->mute);
    master.getVolume().setValue (-3.0, juce::sendNotificationSync);
    CHECK (studio.getProject().masterVolumeDb == -3.0f);

    // Inserts show their effects; the next empty one offers to add.
    Insert insert;
    insert.plugin.name = "PR7-C";
    REQUIRE (studio.addInsert (first, insert, timesEffect (1.0f, "PR7-C")));
    studio.dispatchPendingMessages();
    CHECK (one.getInsertSlot (0).getButtonText() == "PR7-C");
    CHECK (one.getInsertSlot (1).getButtonText() == "+");
    CHECK (one.getInsertSlot (2).getButtonText().isEmpty());
    CHECK (two.getInsertSlot (0).getButtonText() == "+");

    // A new track gets a strip.
    studio.addTrack ({});
    studio.dispatchPendingMessages();
    CHECK (mixer.getNumStrips() == 4);

    // One docked at a time: the key editor docked puts the mixer away, and back.
    view.openKeyEditor();
    CHECK_FALSE (view.isMixerOpen());
    CHECK_FALSE (mixer.isVisible());
    CHECK (view.getKeyEditor().isVisible());
    view.openMixer();
    CHECK_FALSE (view.isKeyEditorOpen());
    CHECK (mixer.isVisible());

    // The mixer in a window of its own: the key editor can be docked meanwhile.
    view.setMixerInWindow (true);
    auto* window = view.getMixerWindow();
    REQUIRE (window != nullptr);
    CHECK (mixer.getTopLevelComponent() == window);
    CHECK (window->getName() == "Mixer");
    view.openKeyEditor();
    CHECK (view.isMixerOpen());
    CHECK (view.isKeyEditorOpen());
    // Its keys work there too: F3 closes it.
    CHECK (window->keyPressed (juce::KeyPress (juce::KeyPress::F3Key)));
    CHECK_FALSE (view.isMixerOpen());
    CHECK (view.getMixerWindow() == nullptr);
    CHECK (mixer.getParentComponent() == &view);
    // F3 again: the mixer in its own window (as Cubase's F3).
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F3Key)));
    REQUIRE (view.getMixerWindow() != nullptr);
    CHECK (view.isMixerOpen());
    // Alt+F3 from there brings it back under the arrangement (the key editor makes way).
    CHECK (view.getMixerWindow()->keyPressed (juce::KeyPress (juce::KeyPress::F3Key, juce::ModifierKeys::altModifier, 0)));
    CHECK (view.isMixerOpen());
    CHECK_FALSE (view.isMixerInWindow());
    CHECK (view.getMixerWindow() == nullptr);
    CHECK (mixer.isVisible());
    CHECK_FALSE (view.isKeyEditorOpen());
    // Docked and open, F3 moves it into its window; Alt+F3 again closes it from docked.
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F3Key)));
    CHECK (view.getMixerWindow() != nullptr);
    view.toggleMixerDocked();
    CHECK (view.isMixerOpen());
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F3Key, juce::ModifierKeys::altModifier, 0)));
    CHECK_FALSE (view.isMixerOpen());
    view.openMixer();

    // It draws in every finish.
    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
    }
}

TEST_CASE ("Audio tracks on screen: MON and the input in the track list; clips trimmed and faded in the arrangement", "[ui][audio]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto synth = studio.getProject().tracks[0].id;
    const auto guitar = studio.addAudioTrack (false);
    // A two-second recording, as a clip from beat 4 to 8 (four beats at 120 BPM).
    REQUIRE (writeWav (studio.getAudioFolder().getChildFile ("Gtr 01.wav"), pluckedTake (2.0, 48000.0), 48000.0));
    Part clip { 4.0, 4.0, "Gtr", {} };
    clip.audioFile = "Gtr 01.wav";
    studio.addPart (guitar, clip);
    studio.dispatchPendingMessages();

    // The track list: MON and the input where an instrument track has its instrument.
    std::vector<juce::TextButton*> buttons;
    findAll (view.getTrackList(), buttons);
    juce::TextButton* monitor = nullptr;
    juce::TextButton* input = nullptr;
    auto monitors = 0;
    for (auto* b : buttons)
    {
        if (b->getButtonText() == "MON" && b->isVisible())
        {
            monitor = b;
            ++monitors;
        }
        if (b->getButtonText() == "IN 1")
            input = b;
    }
    REQUIRE (monitor != nullptr);
    CHECK (monitors == 1);   // only the audio track has one
    CHECK (input != nullptr);
    monitor->onClick();
    CHECK (studio.getProject().findTrack (guitar)->monitor);
    studio.setTrackInput (guitar, 0, true);
    studio.dispatchPendingMessages();
    CHECK (input->getButtonText() == "IN 1+2");

    // The arrangement: drag the clip's left edge a beat later. The start
    // moves and the audio stays put (half a second further into the recording).
    auto& arrange = view.getArrangeView();
    const auto pixelsPerBeat = (float) (arrange.getPartBounds (guitar, 0).getWidth() / 4.0f);
    const auto drag = [&] (juce::Point<float> from, juce::Point<float> to)
    {
        arrange.mouseDown (mouseAt (arrange, from, from));
        for (int step = 1; step <= 4; ++step)
            arrange.mouseDrag (mouseAt (arrange, from + (to - from) * ((float) step / 4.0f), from));
        arrange.mouseUp (mouseAt (arrange, to, from));
    };
    auto bounds = arrange.getPartBounds (guitar, 0);
    drag ({ bounds.getX() + 2.0f, bounds.getCentreY() }, { bounds.getX() + 2.0f + pixelsPerBeat, bounds.getCentreY() });
    auto now = studio.getProject().findTrack (guitar)->parts[0];
    CHECK_THAT (now.start, WithinAbs (5.0, 1.0e-9));
    CHECK_THAT (now.length, WithinAbs (3.0, 1.0e-9));
    CHECK_THAT (now.audioOffset, WithinAbs (0.5, 1.0e-9));

    // Its right edge, dragged far on: no further than the recording goes (2 s = 4 beats from its start).
    bounds = arrange.getPartBounds (guitar, 0);
    drag ({ bounds.getRight() - 2.0f, bounds.getCentreY() }, { bounds.getRight() + pixelsPerBeat * 6.0f, bounds.getCentreY() });
    now = studio.getProject().findTrack (guitar)->parts[0];
    CHECK_THAT (now.length, WithinAbs ((2.0 - 0.5) / 0.5, 1.0e-6));   // 3 beats: from 0.5 s to the recording's end

    // The fade-in handle, dragged a beat in: a fade of about half a second.
    bounds = arrange.getPartBounds (guitar, 0);
    const juce::Point<float> handle (bounds.getX() + 4.0f, bounds.getY() + 20.0f);
    drag (handle, handle.translated (pixelsPerBeat, 0.0f));
    now = studio.getProject().findTrack (guitar)->parts[0];
    CHECK_THAT (now.fadeIn, WithinAbs ((4.0 + pixelsPerBeat) / pixelsPerBeat * 0.5, 0.02));
    CHECK (now.fadeOut == 0.0);
    studio.undo();   // each of those one step
    CHECK (studio.getProject().findTrack (guitar)->parts[0].fadeIn == 0.0);
    studio.redo();

    // A clip can't be dragged onto an instrument track.
    bounds = arrange.getPartBounds (guitar, 0);
    const auto synthLane = arrange.getPartBounds (guitar, 0).getCentreY() - (float) Timeline::trackHeight;
    REQUIRE (arrange.trackAtY (synthLane) == synth);
    drag (bounds.getCentre(), { bounds.getCentreX(), synthLane });
    CHECK (studio.getProject().findTrack (synth)->parts.empty());
    CHECK (studio.getProject().findTrack (guitar)->parts.size() == 1);

    // Double-clicking a clip doesn't open the key editor (it has no notes).
    bounds = arrange.getPartBounds (guitar, 0);
    arrange.mouseDoubleClick (mouseAt (arrange, bounds.getCentre(), bounds.getCentre(), {}, 2));
    CHECK_FALSE (view.isKeyEditorOpen());

    // It draws, with the waveform, in every finish.
    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
    }
    studio.getAudioFolder().deleteRecursively();
}
