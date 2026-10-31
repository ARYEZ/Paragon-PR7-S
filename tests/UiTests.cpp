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
    view.setKeyEditorSize (100);   // (these tests place the mouse at 100%; 200% has its own test)
    auto& editor = view.getKeyEditor();
    auto& frame = view.getKeyEditorFrame();
    CHECK (frame.isVisible());
    CHECK (frame.getBottom() <= view.getHeight());
    CHECK (arrange.getBottom() <= frame.getY());
    REQUIRE (editor.getPart() != nullptr);
    CHECK (editor.getPart()->name == "Riff");
    const auto notes = [&] { return editor.getPart()->notes; };
    // The middle of a grid cell: a beat into the song, and a pitch's row.
    const auto at = [&] (double songBeat, int pitch) { return juce::Point<float> (editor.xForBeat (songBeat) + 2.0f, editor.yForPitch (pitch) + editor.getRowHeight() * 0.5f); };
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
    CHECK_FALSE (frame.isVisible());
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
    auto& frame = view.getKeyEditorFrame();
    view.setKeyEditorSize (100);
    arrange.select (trackId, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    REQUIRE (editor.getPart() != nullptr);
    const auto dockedArrangeHeight = arrange.getHeight();
    const auto dockedRows = editor.getNotesArea().getHeight();
    CHECK (frame.getParentComponent() == &view);

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
    CHECK (frame.getParentComponent() == &view);
    CHECK (editor.isVisible());
    CHECK_FALSE (editor.isInWindow());
    CHECK (arrange.getHeight() == dockedArrangeHeight);

    // Closed while in its window, it opens in its window again next time.
    view.setKeyEditorInWindow (true);
    view.closeKeyEditor();
    CHECK (view.getKeyEditorWindow() == nullptr);
    CHECK (frame.getParentComponent() == &view);
    CHECK_FALSE (frame.isVisible());
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

    // The playhead before the part (at 0): the part grows back to the bar
    // there to hold them; its notes stay where they are in the song.
    studio.setPosition (0.0);
    CHECK (view.keyPressed (ctrl ('v')));
    REQUIRE (notes().size() == 5);
    CHECK (editor.getPart()->start == 0.0);
    CHECK (editor.getPart()->length == 12.0);
    CHECK (notes()[0].start == 4.0);   // the C3 still at song beat 4
    CHECK (notes()[3] == Note { 0.0, 0.5, 60, 0.9f });
    CHECK (notes()[4] == Note { 1.0, 0.75, 64, 0.4f });
    studio.undo();   // the part and its notes back in one step
    studio.dispatchPendingMessages();
    CHECK (editor.getPart()->start == 4.0);
    CHECK (notes().size() == 3);

    // Ctrl+D (after a click in the editor): the selected notes again, on the
    // beat after them (they last 1.75 beats, so 2 beats on), and again.
    editor.selectNotes ({ 0, 1 });
    CHECK (view.keyPressed (ctrl ('d')));
    REQUIRE (notes().size() == 5);
    CHECK_THAT (notes()[3].start, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (notes()[4].start, WithinAbs (3.0, 1.0e-9));
    CHECK (editor.getSelectedNotes() == std::vector<int> { 3, 4 });
    CHECK (view.keyPressed (ctrl ('d')));
    REQUIRE (notes().size() == 7);
    CHECK_THAT (notes()[5].start, WithinAbs (4.0, 1.0e-9));
    CHECK (studio.getProject().tracks[0].parts.size() == 1);   // the notes, not the part

    // Cut: copied, and gone.
    editor.selectNotes ({ 2 });   // the G3
    CHECK (view.keyPressed (ctrl ('x')));
    CHECK (notes().size() == 6);
    for (const auto& n : notes())
        CHECK (n.pitch != 67);

    // Into another part on another track, just selected in the arrangement
    // (the last click there): at the playhead, in it.
    const auto other = studio.addTrack ({});
    studio.addPart (other, { 16.0, 4.0, "Bass", {} });
    view.mouseDown (mouseAt (arrange, { 10.0f, 10.0f }, { 10.0f, 10.0f }));
    arrange.select (other, 0);
    studio.dispatchPendingMessages();
    REQUIRE (editor.getPart() != nullptr);
    REQUIRE (editor.getPart()->name == "Bass");
    studio.setPosition (17.0);
    CHECK (view.keyPressed (ctrl ('v')));
    REQUIRE (notes().size() == 1);
    CHECK (notes()[0] == Note { 1.0, 0.5, 67, 0.7f });
    CHECK (studio.getProject().tracks[0].parts[0].notes.size() == 6);   // the first part as it was
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
    // Ctrl-click resets the fader and the pan (as the manual has it).
    two.getVolume().setValue (-6.0, juce::sendNotificationSync);
    const auto ctrlClick = [] (juce::Slider& slider)
    {
        const auto at = slider.getLocalBounds().getCentre().toFloat() + juce::Point<float> (0.0f, 10.0f);
        slider.mouseDown (mouseAt (slider, at, at, juce::ModifierKeys (juce::ModifierKeys::commandModifier)));
        slider.mouseUp (mouseAt (slider, at, at, juce::ModifierKeys (juce::ModifierKeys::commandModifier)));
    };
    ctrlClick (two.getVolume());
    ctrlClick (two.getPan());
    CHECK_THAT (studio.getProject().findTrack (second)->volumeDb, WithinAbs (0.0, 1.0e-6));   // (within rounding: macOS)
    CHECK_THAT (studio.getProject().findTrack (second)->pan, WithinAbs (0.0, 1.0e-6));
    two.getPan().setValue (-0.5, juce::sendNotificationSync);
    two.getMute().onClick();
    CHECK (studio.getProject().findTrack (second)->mute);
    master.getVolume().setValue (-3.0, juce::sendNotificationSync);
    CHECK (studio.getProject().masterVolumeDb == -3.0f);

    // Inserts show their effects; every empty slot offers to add. Eight
    // shown, more as they fill (up to 16); those after the fader below a line.
    Insert insert;
    insert.plugin.name = "PR7-C";
    REQUIRE (studio.addInsert (first, insert, timesEffect (1.0f, "PR7-C")));
    studio.dispatchPendingMessages();
    CHECK (one.getInsertSlot (0).getButtonText() == "PR7-C");
    CHECK (one.getInsertSlot (1).getButtonText() == "+");
    CHECK (one.getInsertSlot (2).getButtonText() == "+");
    CHECK (two.getInsertSlot (0).getButtonText() == "+");
    CHECK (one.getShownSlots() == 8);
    REQUIRE (studio.addInsert (first, insert, timesEffect (1.0f, "PR7-C"), 9));
    studio.dispatchPendingMessages();
    CHECK (one.getShownSlots() == 11);
    CHECK (one.getInsertSlot (10).isVisible());
    CHECK (one.getInsertSlot (11).isVisible() == false);
    CHECK (one.getVolume().getHeight() >= 100);   // the fader keeps its room
    studio.removeInsert (first, 9);
    studio.dispatchPendingMessages();
    CHECK (one.getShownSlots() == 8);

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

TEST_CASE ("EXPORT: the button in the header; the page offers the song or the cycle, the rate, the bits and the click", "[ui][export]")
{
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    std::vector<juce::TextButton*> buttons;
    findAll (view, buttons);
    CHECK (std::any_of (buttons.begin(), buttons.end(), [] (auto* b) { return b->getButtonText() == "EXPORT" && b->isVisible() && ! b->getBounds().isEmpty(); }));

    {
        // An empty song: only the cycle can be exported.
        Windows::ExportPage page (studio);
        CHECK (page.range.getSelectedId() == 2);
        CHECK (page.canExport());
        page.setRange (Windows::ExportPage::Range::song);
        CHECK_FALSE (page.canExport());
        CHECK_FALSE (page.exportButton.isEnabled());
    }

    const auto keys = studio.getProject().tracks[0].id;
    studio.addPart (keys, { 4.0, 8.0, "Keys", { { 0.0, 1.0, 60, 0.8f } } });
    studio.setCycle (8.0, 16.0);
    Windows::ExportPage page (studio);
    CHECK (page.range.getSelectedId() == 1);   // cycle off: the whole song
    auto options = page.getOptions();
    CHECK (options.start == 0.0);
    CHECK (options.end == 12.0);
    CHECK (options.sampleRate == 0.0);   // as the interface
    CHECK (options.bitsPerSample == 24);
    CHECK_FALSE (options.click);
    CHECK (options.tailSeconds == 30.0);   // the whole song rings out

    page.setRange (Windows::ExportPage::Range::cycle);
    page.rate.setSelectedId (2);    // 44.1 kHz
    page.bits.setSelectedId (16);
    page.click.setToggleState (true, juce::dontSendNotification);
    options = page.getOptions();
    CHECK (options.start == 8.0);
    CHECK (options.end == 16.0);
    CHECK (options.sampleRate == 44100.0);
    CHECK (options.bitsPerSample == 16);
    CHECK (options.click);
    CHECK (options.tailSeconds == 0.0);   // the cycle stops at its end
    page.bits.setSelectedId (32);
    CHECK (page.getOptions().bitsPerSample == 32);
    page.bits.setSelectedId (16);

    // EXPORT... hands the choices on.
    std::optional<Studio::ExportOptions> asked;
    page.onExport = [&asked] (const Studio::ExportOptions& o) { asked = o; };
    page.exportButton.onClick();
    REQUIRE (asked.has_value());
    CHECK (asked->end == 16.0);
    CHECK (asked->bitsPerSample == 16);
}

TEST_CASE ("Dragging as the manual has it: Ctrl keeps a part to one direction, Alt copies; the cycle's range clicked and moved; a double-click between the locators; a double-click deletes a note", "[ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& arrange = view.getArrangeView();
    const auto first = studio.getProject().tracks[0].id;
    const auto second = studio.addTrack ({});
    studio.addPart (first, { 4.0, 4.0, "Riff", { { 0.0, 1.0, 60, 0.8f } } });
    studio.setCycle (16.0, 24.0);
    studio.dispatchPendingMessages();
    const auto ctrl = juce::ModifierKeys (juce::ModifierKeys::commandModifier);
    const auto dragPart = [&] (juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods)
    {
        arrange.mouseDown (mouseAt (arrange, from, from, mods));
        arrange.mouseDrag (mouseAt (arrange, to, from, mods));
        arrange.mouseUp (mouseAt (arrange, to, from, mods));
    };
    const auto partCentre = [&] (int track, int index) { return arrange.getPartBounds (track, index).getCentre(); };
    const auto beatPixels = (float) Timeline().pixelsPerBeat;

    // Ctrl, mostly sideways: along the song only (it stays on its track).
    auto from = partCentre (first, 0);
    dragPart (from, from + juce::Point<float> (2.0f * beatPixels, 20.0f), ctrl);
    REQUIRE (studio.getProject().findTrack (first)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (first)->parts[0].start == 6.0);
    // Ctrl, mostly down: to the next track only (it keeps its beat). No copy.
    from = partCentre (first, 0);
    dragPart (from, from + juce::Point<float> (20.0f, (float) Timeline::trackHeight), ctrl);
    CHECK (studio.getProject().findTrack (first)->parts.empty());
    REQUIRE (studio.getProject().findTrack (second)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (second)->parts[0].start == 6.0);
    // Alt: a copy.
    from = partCentre (second, 0);
    dragPart (from, from + juce::Point<float> (4.0f * beatPixels, 0.0f), juce::ModifierKeys (juce::ModifierKeys::altModifier));
    CHECK (studio.getProject().findTrack (second)->parts.size() == 2);

    // The ruler's top strip: a click inside the cycle's range turns CYCLE on, again off; a drag inside moves it.
    const juce::Point<float> inRange (20.0f * beatPixels, 5.0f);
    CHECK_FALSE (studio.getProject().loopOn);
    dragPart (inRange, inRange, {});
    CHECK (studio.getProject().loopOn);
    dragPart (inRange, inRange, {});
    CHECK_FALSE (studio.getProject().loopOn);
    dragPart (inRange, inRange + juce::Point<float> (4.0f * beatPixels, 0.0f), {});
    CHECK (studio.getProject().loopStart == 20.0);
    CHECK (studio.getProject().loopEnd == 28.0);

    // A double-click on an empty lane between the locators: a part from the left one to the right.
    const juce::Point<float> between (22.0f * beatPixels, (float) Timeline::rulerHeight + 0.5f * (float) Timeline::trackHeight);
    arrange.mouseDoubleClick (mouseAt (arrange, between, between));
    REQUIRE (studio.getProject().findTrack (first)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (first)->parts[0].start == 20.0);
    CHECK (studio.getProject().findTrack (first)->parts[0].length == 8.0);

    // In the key editor, a double-click on a note deletes it.
    arrange.select (second, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    auto& editor = view.getKeyEditor();
    REQUIRE (editor.getPart() != nullptr);
    REQUIRE (editor.getPart()->notes.size() == 1);
    const auto note = editor.getNoteBounds (0).getCentre();
    editor.mouseDoubleClick (mouseAt (editor, note, note));
    CHECK (editor.getPart()->notes.empty());
}

TEST_CASE ("Automation on screen: lanes under their track, lined up; click adds a point (R on), on the line adds none; drag, select, delete, draw, Esc; R/W keys; the mixer follows", "[ui][automation]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& arrange = view.getArrangeView();
    const auto first = studio.getProject().tracks[0].id;
    const auto second = studio.addTrack ({});
    studio.dispatchPendingMessages();
    const auto secondTop = arrange.trackTop (second);

    // The keys: REC arms; R and W are automation's (and the header's R and W).
    std::vector<juce::TextButton*> buttons;
    findAll (view.getTrackList(), buttons);
    const auto count = [&] (const juce::String& text) { return std::count_if (buttons.begin(), buttons.end(), [&] (auto* b) { return b->getButtonText() == text; }); };
    CHECK (count ("REC") == 2);
    CHECK (count ("R") == 3);   // two tracks and the master
    CHECK (count ("W") == 3);

    // Show Automation: the volume lane under the first track; the second moves down by a lane.
    studio.setAutomationOpen (first, true);
    studio.dispatchPendingMessages();
    const auto lane = arrange.getLaneArea (first, 0);
    REQUIRE_FALSE (lane.isEmpty());
    CHECK (arrange.trackTop (second) == secondTop + (float) Timeline::laneHeight);
    CHECK (lane.getY() >= arrange.trackTop (first) + (float) Timeline::trackHeight);
    // The track list's rows line up with the arrangement's.
    std::vector<juce::Label*> labels;
    findAll (view.getTrackList(), labels);
    for (auto* label : labels)
        if (label->getText() == "Synth 2")
            CHECK ((float) label->getParentComponent()->getY() == arrange.trackTop (second));

    // A click on the lane: a point there (snapped to the beat), and R comes on.
    const auto click = [&] (juce::Point<float> at, juce::ModifierKeys mods = {})
    {
        arrange.mouseDown (mouseAt (arrange, at, at, mods));
        arrange.mouseUp (mouseAt (arrange, at, at, mods));
    };
    const auto beatPixels = (float) Timeline().pixelsPerBeat;
    const auto yFor = [&] (float value) { return lane.getBottom() - value * lane.getHeight(); };
    CHECK_FALSE (studio.getAutomationRead (first));
    click ({ 4.1f * beatPixels, yFor (0.75f) });
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 1);
    CHECK (studio.getAutomation (first)->at (0).points[0].beat == 4.0);
    CHECK_THAT (studio.getAutomation (first)->at (0).points[0].value, WithinAbs (0.75, 0.01));
    CHECK (studio.getAutomationRead (first));
    click ({ 8.0f * beatPixels, yFor (0.25f) });
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 2);
    // On the line between them: nothing added (as the manual has it).
    click ({ 6.0f * beatPixels, yFor (0.5f) });
    CHECK (studio.getAutomation (first)->at (0).points.size() == 2);

    // Drag the second point up (Ctrl: up and down only, so it stays on its beat).
    const auto point = arrange.getPointPosition (first, 0, 1);
    const auto ctrl = juce::ModifierKeys (juce::ModifierKeys::commandModifier);
    arrange.mouseDown (mouseAt (arrange, point, point, ctrl));
    arrange.mouseDrag (mouseAt (arrange, point + juce::Point<float> (6.0f, -lane.getHeight() * 0.5f), point, ctrl));
    arrange.mouseUp (mouseAt (arrange, point + juce::Point<float> (6.0f, -lane.getHeight() * 0.5f), point, ctrl));
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 2);
    CHECK (studio.getAutomation (first)->at (0).points[1].beat == 8.0);
    CHECK_THAT (studio.getAutomation (first)->at (0).points[1].value, WithinAbs (0.75, 0.02));
    // Undo: back.
    studio.undo();
    studio.dispatchPendingMessages();
    CHECK_THAT (studio.getAutomation (first)->at (0).points[1].value, WithinAbs (0.25, 0.01));

    // Esc while dragging: back where it was, nothing changed.
    const auto p0 = arrange.getPointPosition (first, 0, 0);
    arrange.mouseDown (mouseAt (arrange, p0, p0));
    arrange.mouseDrag (mouseAt (arrange, p0 + juce::Point<float> (60.0f, 0.0f), p0));
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    arrange.mouseUp (mouseAt (arrange, p0 + juce::Point<float> (60.0f, 0.0f), p0));
    CHECK (studio.getAutomation (first)->at (0).points[0].beat == 4.0);

    // A rectangle round both: selected; Delete removes them (not the part).
    const juce::Point<float> from (2.0f * beatPixels, lane.getY() + 1.0f), to (10.0f * beatPixels, lane.getBottom() - 1.0f);
    arrange.mouseDown (mouseAt (arrange, from, from));
    arrange.mouseDrag (mouseAt (arrange, to, from));
    arrange.mouseUp (mouseAt (arrange, to, from));
    CHECK (arrange.getPointSelection().points.size() == 2);
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    CHECK (studio.getAutomation (first)->at (0).points.empty());

    // Alt-drag draws several.
    const auto alt = juce::ModifierKeys (juce::ModifierKeys::altModifier);
    const juce::Point<float> start (2.0f * beatPixels, yFor (0.2f));
    arrange.mouseDown (mouseAt (arrange, start, start, alt));
    for (int i = 1; i <= 6; ++i)
        arrange.mouseDrag (mouseAt (arrange, start + juce::Point<float> ((float) i * beatPixels, (float) (i % 2) * -20.0f), start, alt));
    arrange.mouseUp (mouseAt (arrange, start + juce::Point<float> (6.0f * beatPixels, 0.0f), start, alt));
    CHECK (studio.getAutomation (first)->at (0).points.size() >= 4);

    // Alt+R / Alt+W: read / write for all tracks; the header's keys light.
    CHECK (view.keyPressed (juce::KeyPress ('w', juce::ModifierKeys::altModifier, 'w')));
    CHECK (studio.getAutomationWrite (first));
    CHECK (studio.getAutomationWrite (second));
    CHECK (studio.getAutomationWrite (Engine::masterBus));
    CHECK (view.keyPressed (juce::KeyPress ('w', juce::ModifierKeys::altModifier, 'w')));
    CHECK_FALSE (studio.anyAutomationWrite());
    CHECK (view.keyPressed (juce::KeyPress ('r', juce::ModifierKeys::altModifier, 'r')));
    CHECK_FALSE (studio.anyAutomationRead());
    // A function key is never taken for a letter: Windows gives F3 the code 0x10072 ('r' with a flag), which upper-cased reads as R.
    view.keyPressed (juce::KeyPress (0x10000 | 'r', juce::ModifierKeys::altModifier, 0));
    CHECK_FALSE (studio.anyAutomationRead());
    CHECK_FALSE (studio.getEngine().isRecording());   // (not R, which records)
    CHECK (view.keyPressed (juce::KeyPress ('r', juce::ModifierKeys::altModifier, 'r')));

    // The mixer's fader follows the automation while it's read.
    studio.setAutomationPoints (first, 0, { { 0.0, volumeToTravel (-20.0f) } });
    view.openMixer();
    studio.dispatchPendingMessages();
    renderSamples (studio.getEngine(), 512);
    auto& strip = view.getMixer().getStrip (0);
    strip.followMix();
    CHECK_THAT (strip.getVolume().getValue(), WithinAbs (-20.0, 0.05));
    CHECK (strip.getRead().getToggleState());

    // The master's lanes: its row, R and W, and the volume lane under it.
    studio.setAutomationOpen (Engine::masterBus, true);
    studio.dispatchPendingMessages();
    CHECK_FALSE (arrange.getLaneArea (Engine::masterBus, 0).isEmpty());
    CHECK (studio.getAutomation (Engine::masterBus)->at (0).target == AutomationLane::Target::volume);

    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
    }
}

TEST_CASE ("Sends and FX channels on screen: the SENDS rack, a send's dot, level and Ctrl-click; OUT; FX and group rows; no parts on them", "[ui][sends]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto track = studio.getProject().tracks[0].id;
    Insert reverb;
    reverb.plugin.name = "Reverb";
    const auto fx = studio.addFxChannel (reverb, timesEffect (1.0f, "Reverb"));
    const auto group = studio.addGroupChannel();
    studio.setSend (track, 0, { fx, -10.0f, true, false });
    studio.setOutput (track, group);
    studio.selectTrack (track);
    view.openMixer();
    studio.dispatchPendingMessages();

    // The FX and group rows: no REC (they record nothing), and say what they are.
    std::vector<juce::TextButton*> buttons;
    findAll (view.getTrackList(), buttons);
    const auto visibleCount = [&] (const juce::String& text)
    {
        return std::count_if (buttons.begin(), buttons.end(), [&] (auto* b) { return b->getButtonText() == text && b->isVisible(); });
    };
    CHECK (visibleCount ("REC") == 1);
    // (0.9.2: the rows have no instrument key, as the owner asked: the Inspector says what a channel is.)
    CHECK (visibleCount ("FX: Reverb") == 0);
    CHECK (visibleCount ("GROUP CHANNEL") == 0);
    studio.selectTrack (fx);
    studio.dispatchPendingMessages();
    std::vector<juce::TextButton*> inInspector;
    findAll (view.getInspector(), inInspector);
    CHECK (std::any_of (inInspector.begin(), inInspector.end(), [] (auto* b) { return b->getButtonText() == "FX: Reverb" && b->isVisible(); }));
    studio.selectTrack (track);
    studio.dispatchPendingMessages();

    // The mixer: OUT shows where the track goes; SENDS swaps the racks.
    auto& mixer = view.getMixer();
    auto& strip = mixer.getStrip (0);
    CHECK (strip.getOutput().getButtonText() == "OUT: Group 1");
    CHECK (strip.getInsertSlot (0).isVisible());
    CHECK_FALSE (strip.getSendSlot (0).isVisible());
    mixer.setShowSends (true);
    CHECK_FALSE (strip.getInsertSlot (0).isVisible());
    CHECK (strip.getSendSlot (0).isVisible());
    CHECK_FALSE (mixer.getStrip (1).getSendSlot (0).isVisible());   // the FX channel: no sends

    // The dot turns the send off and on.
    auto& send = strip.getSendSlot (0);
    const juce::Point<float> dot (6.0f, (float) send.getHeight() * 0.5f);
    send.mouseDown (mouseAt (send, dot, dot));
    CHECK_FALSE (studio.getProject().findTrack (track)->sends[0].on);
    send.mouseDown (mouseAt (send, dot, dot));
    CHECK (studio.getProject().findTrack (track)->sends[0].on);

    // Dragging right turns it up; one undo step; Ctrl-click: 0 dB.
    const juce::Point<float> from ((float) send.getWidth() * 0.5f, (float) send.getHeight() * 0.5f);
    send.mouseDown (mouseAt (send, from, from));
    send.mouseDrag (mouseAt (send, from + juce::Point<float> (20.0f, 0.0f), from));
    send.mouseUp (mouseAt (send, from + juce::Point<float> (20.0f, 0.0f), from));
    const auto raised = studio.getProject().findTrack (track)->sends[0].levelDb;
    CHECK (raised > -10.0f);
    studio.undo();
    CHECK (studio.getProject().findTrack (track)->sends[0].levelDb == -10.0f);
    const auto ctrl = juce::ModifierKeys (juce::ModifierKeys::commandModifier);
    send.mouseDown (mouseAt (send, from, from, ctrl));
    CHECK (studio.getProject().findTrack (track)->sends[0].levelDb == 0.0f);

    // A part can't be dragged onto an FX or group channel's row; a double-click there makes none.
    studio.addPart (track, { 0.0, 4.0, "Riff", {} });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    const auto part = arrange.getPartBounds (track, 0).getCentre();
    const juce::Point<float> onFx (part.x, arrange.trackTop (fx) + 20.0f);
    arrange.mouseDown (mouseAt (arrange, part, part));
    arrange.mouseDrag (mouseAt (arrange, onFx, part));
    arrange.mouseUp (mouseAt (arrange, onFx, part));
    CHECK (studio.getProject().findTrack (fx)->parts.empty());
    CHECK (studio.getProject().findTrack (track)->parts.size() == 1);
    arrange.mouseDoubleClick (mouseAt (arrange, onFx + juce::Point<float> (200.0f, 0.0f), onFx));
    CHECK (studio.getProject().findTrack (fx)->parts.empty());

    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
    }
}

TEST_CASE ("Zoom, Auto-Scroll and scrolling (H in, G out, Shift+F, F; the scroll bars); the lower zone's line dragged; the mixer's rack hidden", "[ui][view]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, { 4.0, 4.0, "Riff", { { 0.0, 1.0, 60, 0.8f } } });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& across = view.getArrangeScrollBarAcross();
    const auto partX = [&] { return arrange.getPartBounds (trackId, 0).getX(); };
    const auto partWidth = [&] { return arrange.getPartBounds (trackId, 0).getWidth(); };
    const auto key = [&] (juce::juce_wchar c, juce::ModifierKeys mods = {}) { return view.keyPressed (juce::KeyPress ((int) c, mods, c)); };

    SECTION ("H zooms in a step and G out, centred on the playhead; Shift+F shows the whole song")
    {
        studio.setPosition (32.0);
        const auto width = partWidth();
        CHECK (key ('h'));
        CHECK_THAT (partWidth(), WithinAbs (width * Timeline::zoomStep, 0.01));
        // The playhead in the middle: half the view's beats before it.
        CHECK_THAT (across.getCurrentRangeStart() + across.getCurrentRangeSize() * 0.5, WithinAbs (32.0, 0.01));
        CHECK (key ('g'));
        CHECK (key ('G'));
        CHECK_THAT (partWidth(), WithinAbs (width / Timeline::zoomStep, 0.01));
        CHECK (key ('F', juce::ModifierKeys::shiftModifier));
        CHECK (across.getCurrentRangeStart() == 0.0);
        CHECK_THAT (across.getCurrentRangeSize(), WithinAbs (Timeline::scrollLength (studio), 0.01));
    }

    SECTION ("F turns Auto-Scroll off and on (AUTO-SCROLL lit); while on, a jump out of sight turns the page")
    {
        auto& button = view.getAutoScrollButton();
        CHECK (button.getToggleState());   // on at first
        CHECK (key ('f'));
        CHECK_FALSE (button.getToggleState());
        studio.setPosition (200.0);
        arrange.followPlayhead();
        CHECK (across.getCurrentRangeStart() == 0.0);   // off: the view stays
        CHECK (key ('f'));
        CHECK (button.getToggleState());
        studio.setPosition (300.0);
        arrange.followPlayhead();
        CHECK_THAT (across.getCurrentRangeStart(), WithinAbs (299.0, 0.01));   // the page turned: the playhead near the left
        button.onClick();   // the key does the same
        studio.dispatchPendingMessages();
        CHECK_FALSE (button.getToggleState());
        // The bar goes far enough to hold the view.
        CHECK (across.getMaximumRangeLimit() >= across.getCurrentRange().getEnd());
    }

    SECTION ("The scroll bars move the arrangement (and the track list with it)")
    {
        const auto x = partX();
        across.setCurrentRangeStart (4.0, juce::sendNotificationSync);
        CHECK_THAT (partX(), WithinAbs (x - 4.0f * (partWidth() / 4.0f), 0.01));
        CHECK (across.getMaximumRangeLimit() >= Timeline::scrollLength (studio));
        CHECK (Timeline::scrollLength (studio) == 32.0 * 4.0);   // 8 bars past the part, at least 32 bars

        for (int i = 0; i < 12; ++i)
            studio.addTrack ({});
        studio.dispatchPendingMessages();
        auto& down = view.getArrangeScrollBarDown();
        const auto top = arrange.trackTop (trackId);
        down.setCurrentRangeStart (100.0, juce::sendNotificationSync);
        CHECK_THAT (arrange.trackTop (trackId), WithinAbs (top - 100.0f, 0.01));
        down.setCurrentRangeStart (1.0e6, juce::sendNotificationSync);   // no further than the last row
        CHECK (down.getCurrentRange().getEnd() <= down.getMaximumRangeLimit());
    }

    SECTION ("In the key editor (after a click in it), its own zoom and Auto-Scroll")
    {
        const auto partArea = arrange.getPartBounds (trackId, 0);
        arrange.mouseDoubleClick (mouseAt (arrange, partArea.getCentre(), partArea.getCentre(), {}, 2));
        studio.dispatchPendingMessages();
        REQUIRE (view.isKeyEditorOpen());
        auto& editor = view.getKeyEditor();
        REQUIRE (editor.getPart() != nullptr);
        view.mouseDown (mouseAt (editor, editor.getNotesArea().getCentre(), editor.getNotesArea().getCentre()));
        const auto editorZoom = editor.getPixelsPerBeat();
        const auto arrangeBeats = across.getCurrentRangeSize();
        CHECK (key ('h'));
        CHECK_THAT (editor.getPixelsPerBeat(), WithinAbs (editorZoom * Timeline::zoomStep, 0.01));
        CHECK_THAT (across.getCurrentRangeSize(), WithinAbs (arrangeBeats, 0.01));   // the arrangement's own is as it was
        CHECK (editor.getAutoScroll());
        CHECK (key ('f'));
        CHECK_FALSE (editor.getAutoScroll());
        CHECK (view.getAutoScrollButton().getToggleState());   // the arrangement's still on
        // Its scroll bars: across moves the notes, down the pitches.
        const auto noteX = editor.getNoteBounds (0).getX();
        editor.getScrollBarAcross().setCurrentRangeStart (editor.getFirstBeat() + 1.0, juce::sendNotificationSync);
        CHECK_THAT (editor.getNoteBounds (0).getX(), WithinAbs (noteX - (float) editor.getPixelsPerBeat(), 0.01));
        const auto noteY = editor.getNoteBounds (0).getY();
        editor.getScrollBarDown().setCurrentRangeStart (editor.getScrollBarDown().getCurrentRangeStart() - 24.0, juce::sendNotificationSync);
        CHECK_THAT (editor.getNoteBounds (0).getY(), WithinAbs (noteY + 24.0f, 0.01));
    }

    SECTION ("The line above the docked mixer: dragged up and down, kept between the mixer's least and the arrangement's room; its rack hidden")
    {
        view.openMixer();
        auto& mixer = view.getMixer();
        auto& divider = view.getZoneDivider();
        REQUIRE (divider.isVisible());
        CHECK (divider.getBottom() <= mixer.getY());
        CHECK (arrange.getBottom() <= divider.getY());
        const auto dragDivider = [&] (float dy)
        {
            const juce::Point<float> from (100.0f, 3.0f);
            divider.mouseDown (mouseAt (divider, from, from));
            divider.mouseDrag (mouseAt (divider, from.translated (0.0f, dy), from));
            divider.mouseUp (mouseAt (divider, from.translated (0.0f, dy), from));
        };
        const auto height = mixer.getHeight();
        dragDivider (-60.0f);
        CHECK (mixer.getHeight() == height + 60);
        dragDivider (-2000.0f);   // the arrangement keeps its room
        CHECK (arrange.getHeight() + view.getArrangeScrollBarAcross().getHeight() >= MainView::minimumArrangeHeight - 1);
        dragDivider (2000.0f);    // no shorter than the mixer can be
        const auto withRack = mixer.getHeight();
        CHECK (withRack == mixer.getMinimumHeight());

        // INSERTS lit; clicking it hides the rack: the mixer can go shorter.
        auto* inserts = buttonNamed (mixer, "INSERTS");
        REQUIRE (inserts != nullptr);
        CHECK (inserts->getToggleState());
        CHECK (mixer.getStrip (0).getInsertSlot (0).isVisible());
        inserts->onClick();
        studio.dispatchPendingMessages();
        CHECK_FALSE (mixer.isRackShown());
        CHECK_FALSE (inserts->getToggleState());
        CHECK_FALSE (mixer.getStrip (0).getInsertSlot (0).isVisible());
        CHECK (mixer.getMinimumHeight() < withRack - 100);
        dragDivider (2000.0f);
        CHECK (mixer.getHeight() == mixer.getMinimumHeight());
        CHECK (mixer.getStrip (0).getVolume().getHeight() > 40);   // the fader keeps its room
        // SENDS shows the sends' rack (the mixer grows to hold it); INSERTS again, the inserts.
        buttonNamed (mixer, "SENDS")->onClick();
        studio.dispatchPendingMessages();
        CHECK (mixer.isRackShown());
        CHECK (mixer.isShowingSends());
        CHECK (mixer.getHeight() >= mixer.getMinimumHeight());
        inserts->onClick();
        studio.dispatchPendingMessages();
        CHECK (mixer.getStrip (0).getInsertSlot (0).isVisible());
        CHECK (mixer.getHeight() == withRack);

        // The key editor docked has its own height (the default: a little over half).
        view.openKeyEditor();
        CHECK (view.getKeyEditorFrame().getHeight() != withRack);
        CHECK (divider.getBottom() <= view.getKeyEditorFrame().getY());
    }
}

TEST_CASE ("Taller and shorter rows (Shift+H / Shift+G); Auto-Scroll's settings: Stationary Cursor, Suspend Auto-Scroll when Editing", "[ui][view]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto first = studio.getProject().tracks[0].id;
    const auto second = studio.addTrack ({});
    studio.addPart (first, { 4.0, 4.0, "Riff", { { 0.0, 1.0, 60, 0.8f } } });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = view.getTimeline();
    const auto key = [&] (juce::juce_wchar c, juce::ModifierKeys mods = {}) { return view.keyPressed (juce::KeyPress ((int) c, mods, c)); };
    const auto shift = juce::ModifierKeys::shiftModifier;

    SECTION ("Shift+H taller rows, Shift+G shorter, the track list with them; short ones show one line")
    {
        const auto gap = [&] { return arrange.trackTop (second) - arrange.trackTop (first); };
        CHECK (gap() == (float) Timeline::trackHeight);
        CHECK (key ('H', shift));
        CHECK (gap() == (float) juce::roundToInt (Timeline::trackHeight * Timeline::rowZoomStep));
        CHECK_THAT (arrange.getPartBounds (first, 0).getHeight(), WithinAbs (gap() - 6.0f, 0.01));
        // The track list's rows line up with them.
        std::vector<juce::TextButton*> buttons;
        findAll (view.getTrackList(), buttons);
        const auto visibleNamed = [&] (const juce::String& text)
        {
            auto count = 0;
            for (auto* b : buttons)
                count += b->getButtonText() == text && b->isVisible() ? 1 : 0;
            return count;
        };
        CHECK (visibleNamed ("R") == 3);   // two tracks and the master
        for (int i = 0; i < 12; ++i)
            key ('G', shift);
        CHECK (timeline.rowZoom == Timeline::minRowZoom);
        CHECK (gap() == (float) Timeline::trackHeight * (float) Timeline::minRowZoom);
        CHECK (visibleNamed ("R") == 1);   // the tracks' second line put away: only the master's
        CHECK (visibleNamed ("M") >= 2);   // the first line (name, M, S, REC) still there
        for (int i = 0; i < 12; ++i)
            key ('H', shift);
        CHECK (timeline.rowZoom == Timeline::maxRowZoom);
        CHECK (visibleNamed ("R") == 3);
    }

    SECTION ("Stationary Cursor: while playing, the playhead in the middle; Page Scroll: only when it reaches the edge")
    {
        AutoScroll scroll;
        // Page Scroll: in sight, nothing; past the edge, the page turns.
        CHECK_FALSE (scroll.follow (10.0, 0.0, 20.0, 1000.0f, true, true).has_value());
        CHECK (scroll.follow (60.0, 0.0, 20.0, 1000.0f, true, true) == 59.0);
        scroll.stationary = true;
        CHECK (scroll.follow (40.0, 0.0, 20.0, 1000.0f, true, true) == 15.0);   // 25 beats across: 12.5 before it
        CHECK_FALSE (scroll.follow (5.0, 0.0, 20.0, 1000.0f, true, true).has_value());   // near the start: it can't be in the middle
        CHECK_FALSE (scroll.follow (10.0, 0.0, 20.0, 1000.0f, false, true).has_value());   // stopped, in sight: stays
        scroll.on = false;
        CHECK_FALSE (scroll.follow (40.0, 0.0, 20.0, 1000.0f, true, true).has_value());

        // In the arrangement: playing with Stationary Cursor, the view keeps the playhead in the middle.
        timeline.autoScroll.stationary = true;
        studio.setPosition (100.0);
        studio.play();
        arrange.followPlayhead();
        CHECK_THAT (timeline.xForBeat (studio.getEngine().getPosition()), WithinAbs ((float) arrange.getWidth() * 0.5f, 1.0));
        studio.stop();
    }

    SECTION ("Suspend Auto-Scroll when Editing: a click in the tracks while playing stops it following until playback stops; F carries on")
    {
        auto& autoScrollKey = view.getAutoScrollButton();
        timeline.autoScroll.suspendWhenEditing = true;
        studio.play();
        const juce::Point<float> inLanes (40.0f, (float) Timeline::rulerHeight + 200.0f);
        arrange.mouseDown (mouseAt (arrange, inLanes, inLanes));
        arrange.mouseUp (mouseAt (arrange, inLanes, inLanes));
        CHECK (timeline.autoScroll.suspended);
        CHECK (autoScrollKey.getButtonText() == "SUSPENDED");
        CHECK (autoScrollKey.getToggleState());
        studio.setPosition (400.0);
        arrange.followPlayhead();
        CHECK (timeline.firstBeat == 0.0);   // not following
        CHECK (key ('f'));                   // carries on (still on)
        CHECK_FALSE (timeline.autoScroll.suspended);
        CHECK (timeline.autoScroll.on);
        CHECK (autoScrollKey.getButtonText() == "AUTO-SCROLL");
        // Suspended again, then playback stops: back to following.
        arrange.mouseDown (mouseAt (arrange, inLanes, inLanes));
        arrange.mouseUp (mouseAt (arrange, inLanes, inLanes));
        CHECK (timeline.autoScroll.suspended);
        studio.stop();
        arrange.followPlayhead();
        CHECK_FALSE (timeline.autoScroll.suspended);
        // Off, the click does nothing to it.
        timeline.autoScroll.suspendWhenEditing = false;
        studio.play();
        arrange.mouseDown (mouseAt (arrange, inLanes, inLanes));
        arrange.mouseUp (mouseAt (arrange, inLanes, inLanes));
        CHECK_FALSE (timeline.autoScroll.suspended);
        studio.stop();
    }

    SECTION ("In the key editor: its own taller and shorter rows (the pitch in the middle stays), and its own suspend")
    {
        const auto partArea = arrange.getPartBounds (first, 0);
        arrange.mouseDoubleClick (mouseAt (arrange, partArea.getCentre(), partArea.getCentre(), {}, 2));
        studio.dispatchPendingMessages();
        auto& editor = view.getKeyEditor();
        REQUIRE (editor.getPart() != nullptr);
        const auto middle = editor.getNotesArea().getCentreY();
        view.mouseDown (mouseAt (editor, editor.getNotesArea().getCentre(), editor.getNotesArea().getCentre()));
        const auto pitch = editor.pitchForY (middle);
        const auto rows = editor.getRowHeight();
        CHECK (key ('H', shift));
        CHECK_THAT (editor.getRowHeight(), WithinAbs (rows * (float) Timeline::rowZoomStep, 0.01));
        CHECK (std::abs (editor.pitchForY (middle) - pitch) <= 1);
        CHECK (timeline.rowZoom == 1.0);   // the arrangement's rows as they were
        for (int i = 0; i < 20; ++i)
            key ('G', shift);
        CHECK (editor.getRowHeight() == KeyEditor::minRowHeight);

        editor.getAutoScrollSettings().suspendWhenEditing = true;
        studio.play();
        const auto inNotes = editor.getNotesArea().getCentre();
        editor.mouseDown (mouseAt (editor, inNotes, inNotes));
        editor.mouseUp (mouseAt (editor, inNotes, inNotes));
        CHECK (editor.getAutoScrollSettings().suspended);
        CHECK_FALSE (timeline.autoScroll.suspended);   // the arrangement's own carries on
        studio.stop();
        editor.followPlayhead();
        CHECK_FALSE (editor.getAutoScrollSettings().suspended);
    }
}

TEST_CASE ("The editing tools: keys and the toolbar; SPLIT, GLUE, ERASE, MUTE, DRAW, ZOOM, PLAY on the tracks; SELECT's Alt-click and Alt-drag; Alt+X", "[ui][tools]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 4.0, 8.0, "Riff", { { 0.0, 1.0, 60, 0.8f }, { 6.0, 1.0, 64, 0.8f } } });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& tools = view.getTools();
    const auto key = [&] (juce::juce_wchar c, juce::ModifierKeys mods = {}) { return view.keyPressed (juce::KeyPress ((int) c, mods, c)); };
    const auto parts = [&] { return studio.getProject().findTrack (track)->parts; };
    const auto click = [&] (juce::Point<float> at, juce::ModifierKeys mods = {})
    {
        arrange.mouseDown (mouseAt (arrange, at, at, mods));
        arrange.mouseUp (mouseAt (arrange, at, at, mods));
    };
    const auto dragAcross = [&] (juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods = {})
    {
        arrange.mouseDown (mouseAt (arrange, from, from, mods));
        for (int step = 1; step <= 4; ++step)
            arrange.mouseDrag (mouseAt (arrange, from + (to - from) * ((float) step / 4.0f), from, mods));
        arrange.mouseUp (mouseAt (arrange, to, from, mods));
    };
    const auto xFor = [&] (double beat) { return arrange.getPartBounds (track, 0).getX() + (float) ((beat - parts()[0].start) * arrange.getPartBounds (track, 0).getWidth() / parts()[0].length); };
    const auto y = arrange.getPartBounds (track, 0).getCentreY();

    SECTION ("The tool keys (1, 3 to 9) and F9 / F10 choose the tool; its key in the toolbar lights")
    {
        CHECK (arrange.getTool() == ArrangeView::Tool::select);
        CHECK (tools.getButton (ArrangeView::Tool::select).getToggleState());
        CHECK (key ('3'));
        CHECK (arrange.getTool() == ArrangeView::Tool::split);
        CHECK (tools.getButton (ArrangeView::Tool::split).getToggleState());
        CHECK_FALSE (tools.getButton (ArrangeView::Tool::select).getToggleState());
        CHECK (key ('9'));
        CHECK (arrange.getTool() == ArrangeView::Tool::play);
        CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F10Key)));
        CHECK (arrange.getTool() == ArrangeView::Tool::select);   // round again
        CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F9Key)));
        CHECK (arrange.getTool() == ArrangeView::Tool::play);
        tools.getButton (ArrangeView::Tool::glue).onClick();
        CHECK (arrange.getTool() == ArrangeView::Tool::glue);
    }

    SECTION ("SPLIT: a click cuts the part there, on the grid (Ctrl: anywhere); Alt: equal pieces; then GLUE joins them back")
    {
        key ('3');
        click ({ xFor (6.1), y });   // snaps to beat 6 (the grid at this zoom: a beat)
        REQUIRE (parts().size() == 2);
        CHECK (parts()[0].length == 2.0);
        CHECK (parts()[1].start == 6.0);
        studio.undo();
        click ({ xFor (6.1), y }, juce::ModifierKeys::commandModifier);   // (Ctrl; Cmd on a Mac, as the app's every Ctrl)
        REQUIRE (parts().size() == 2);
        CHECK (std::abs (parts()[1].start - 6.1) < 0.05);
        studio.undo();
        click ({ xFor (6.0), y }, juce::ModifierKeys::altModifier);
        CHECK (parts().size() == 4);   // 8 beats in pieces of 2

        key ('4');
        click ({ xFor (5.0), y }, juce::ModifierKeys::altModifier);   // the first, glued to all after it
        REQUIRE (parts().size() == 1);
        CHECK (parts()[0].length == 8.0);
        CHECK (parts()[0].notes.size() == 2);
    }

    SECTION ("ERASE removes the part clicked; MUTE mutes it (a click again unmutes), or every part in a rectangle")
    {
        studio.addPart (track, { 16.0, 4.0, "Later", {} });
        studio.dispatchPendingMessages();
        key ('7');
        click ({ xFor (5.0), y });
        CHECK (parts()[0].muted);
        click ({ xFor (5.0), y });
        CHECK_FALSE (parts()[0].muted);
        dragAcross ({ xFor (3.0), y - 10.0f }, { xFor (18.0), y + 10.0f });
        CHECK (parts()[0].muted);
        CHECK (parts()[1].muted);

        key ('5');
        // A right-click is the part's menu, whatever the tool: nothing erased.
        click ({ xFor (5.0), y }, juce::ModifierKeys::rightButtonModifier);
        juce::PopupMenu::dismissAllActiveMenus();
        CHECK (parts().size() == 2);
        click ({ xFor (5.0), y });
        REQUIRE (parts().size() == 1);
        CHECK (parts()[0].name == "Later");
    }

    SECTION ("DRAW: a drag on an empty stretch of an instrument track makes a new empty part over it (whole grid steps)")
    {
        key ('8');
        dragAcross ({ xFor (14.2), y }, { xFor (17.6), y });
        REQUIRE (parts().size() == 2);
        CHECK (parts()[1].start == 14.0);
        CHECK (parts()[1].length == 4.0);   // 14 to 18
        CHECK (parts()[1].notes.empty());
    }

    SECTION ("SELECT: Alt-click a part splits it there; Alt-drag on an empty stretch draws a part; Alt-drag a part still copies it")
    {
        click ({ xFor (8.0), y }, juce::ModifierKeys::altModifier);
        REQUIRE (parts().size() == 2);
        CHECK (parts()[1].start == 8.0);
        studio.undo();
        dragAcross ({ xFor (20.0), y }, { xFor (23.5), y }, juce::ModifierKeys::altModifier);
        REQUIRE (parts().size() == 2);
        CHECK (parts()[1].start == 20.0);
        studio.undo();
        dragAcross ({ xFor (5.0), y }, { xFor (17.0), y }, juce::ModifierKeys::altModifier);
        CHECK (parts().size() == 2);   // the copy
        CHECK (parts()[0].start == 4.0);
    }

    SECTION ("Alt+X splits the parts the playhead is in (only the selected one, if any); the part's menu can too")
    {
        const auto second = studio.addTrack ({});
        studio.addPart (second, { 0.0, 16.0, "Pad", {} });
        studio.setPosition (6.0);
        CHECK (key ('x', juce::ModifierKeys::altModifier));
        CHECK (parts().size() == 2);
        CHECK (studio.getProject().findTrack (second)->parts.size() == 2);
        studio.undo();
        arrange.select (track, 0);
        CHECK (key ('x', juce::ModifierKeys::altModifier));
        CHECK (parts().size() == 2);
        CHECK (studio.getProject().findTrack (second)->parts.size() == 1);
    }

    SECTION ("ZOOM: a click zooms in there, Alt-click out, a drag fills the view with the stretch; PLAY: held, it plays from there")
    {
        key ('6');
        const auto width = arrange.getPartBounds (track, 0).getWidth();
        const auto clickedAt = xFor (8.0);
        click ({ clickedAt, y });
        CHECK_THAT (arrange.getPartBounds (track, 0).getWidth(), WithinAbs (width * (float) Timeline::zoomStep, 0.1));
        CHECK_THAT (xFor (8.0), WithinAbs (clickedAt, 0.1));   // the beat clicked stays under the mouse
        click ({ 300.0f, y }, juce::ModifierKeys::altModifier);
        CHECK_THAT (arrange.getPartBounds (track, 0).getWidth(), WithinAbs (width, 0.1));
        dragAcross ({ xFor (4.0), y }, { xFor (12.0), y });
        CHECK_THAT (arrange.getPartBounds (track, 0).getX(), WithinAbs (0.0, 1.0));
        CHECK_THAT (arrange.getPartBounds (track, 0).getWidth(), WithinAbs ((float) arrange.getWidth(), 1.0));

        key ('9');
        studio.setPosition (1.0);
        const juce::Point<float> at (xFor (6.0), y);
        arrange.mouseDown (mouseAt (arrange, at, at));
        CHECK (studio.getEngine().isPlaying());
        CHECK_THAT (studio.getEngine().getPosition(), WithinAbs (6.0, 0.05));
        arrange.mouseUp (mouseAt (arrange, at, at));
        CHECK_FALSE (studio.getEngine().isPlaying());
        CHECK (studio.getEngine().getPosition() == 1.0);   // back where it was
    }
}

TEST_CASE ("RANGE (2): drag a range across tracks; its edges; double-click; drag its contents (Alt: copy); the keys", "[ui][tools][range]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    studio.addPart (a, { 0.0, 8.0, "A", { { 3.0, 2.0, 60, 0.8f } } });
    studio.addPart (b, { 0.0, 4.0, "B1", {} });
    studio.addPart (b, { 6.0, 4.0, "B2", {} });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    const auto key = [&] (int code, juce::ModifierKeys mods = {}, juce::juce_wchar c = 0) { return view.keyPressed (juce::KeyPress (code, mods, c)); };
    const auto press = [&] (juce::juce_wchar c, juce::ModifierKeys mods = {}) { return key ((int) c, mods, c); };
    const auto cmd = juce::ModifierKeys (juce::ModifierKeys::commandModifier);
    const auto shift = juce::ModifierKeys (juce::ModifierKeys::shiftModifier);
    const auto ppb = arrange.getPartBounds (a, 0).getWidth() / 8.0f;   // pixels per beat
    const auto x = [&] (double beat) { return arrange.getPartBounds (a, 0).getX() + (float) beat * ppb; };
    const auto yA = arrange.getPartBounds (a, 0).getCentreY(), yB = arrange.getPartBounds (b, 0).getCentreY();
    const auto dragAcross = [&] (juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods = {})
    {
        arrange.mouseDown (mouseAt (arrange, from, from, mods));
        for (int step = 1; step <= 4; ++step)
            arrange.mouseDrag (mouseAt (arrange, from + (to - from) * ((float) step / 4.0f), from, mods));
        arrange.mouseUp (mouseAt (arrange, to, from, mods));
    };
    const auto spans = [&] (int id)
    {
        std::vector<std::pair<double, double>> out;
        for (const auto& part : studio.getProject().findTrack (id)->parts)
            out.push_back ({ part.start, part.length });
        std::sort (out.begin(), out.end());
        return out;
    };
    using Spans = std::vector<std::pair<double, double>>;

    CHECK (press ('2'));
    REQUIRE (arrange.getTool() == ArrangeView::Tool::range);
    dragAcross ({ x (2.1), yA }, { x (6.9), yB });   // snapped: 2 to 7, both tracks
    REQUIRE (arrange.hasRange());
    CHECK (arrange.getRange()->start == 2.0);
    CHECK (arrange.getRange()->end == 7.0);
    CHECK (arrange.getRange()->trackIds == std::vector<int> { a, b });

    SECTION ("Shift+X splits at its edges; Delete removes what's in it; Shift+Backspace closes the gap; Esc clears it")
    {
        CHECK (press ('X', shift));
        CHECK (spans (a) == Spans { { 0.0, 2.0 }, { 2.0, 5.0 }, { 7.0, 1.0 } });
        studio.undo();
        CHECK (key (juce::KeyPress::deleteKey));
        CHECK (spans (a) == Spans { { 0.0, 2.0 }, { 7.0, 1.0 } });
        CHECK (arrange.hasRange());   // it stays
        studio.undo();
        CHECK (key (juce::KeyPress::backspaceKey, shift));
        CHECK (spans (b) == Spans { { 0.0, 2.0 }, { 2.0, 3.0 } });
        CHECK (key (juce::KeyPress::escapeKey));
        CHECK_FALSE (arrange.hasRange());
    }

    SECTION ("Ctrl+X cuts it to the clipboard; Ctrl+V pastes at the playhead on the selected track down; Ctrl+D duplicates")
    {
        CHECK (press ('X', cmd));
        CHECK (spans (a) == Spans { { 0.0, 2.0 }, { 7.0, 1.0 } });
        studio.selectTrack (a);
        studio.setPosition (16.0);
        CHECK (press ('V', cmd));
        CHECK (spans (a) == Spans { { 0.0, 2.0 }, { 7.0, 1.0 }, { 16.0, 5.0 } });
        CHECK (spans (b) == Spans { { 0.0, 2.0 }, { 7.0, 3.0 }, { 16.0, 2.0 }, { 20.0, 1.0 } });
        studio.undo();
        studio.undo();
        CHECK (press ('D', cmd));   // a copy straight after it: 7 to 12
        CHECK (spans (a) == Spans { { 0.0, 8.0 }, { 7.0, 5.0 } });
    }

    SECTION ("Its edges dragged; Shift-drag outside it extends it")
    {
        dragAcross ({ x (7.0), yA }, { x (9.0), yA });
        CHECK (arrange.getRange()->end == 9.0);
        dragAcross ({ x (2.0), yA }, { x (1.0), yA });
        CHECK (arrange.getRange()->start == 1.0);
        arrange.setRange (Studio::Range { 2.0, 4.0, { a } });
        dragAcross ({ x (6.0), yB }, { x (6.0), yB }, shift);   // to beat 6 and down to track b
        CHECK (arrange.getRange()->end == 6.0);
        CHECK (arrange.getRange()->trackIds == std::vector<int> { a, b });
    }

    SECTION ("Dragging inside it moves what's in it (the range goes too); Alt copies")
    {
        arrange.setRange (Studio::Range { 2.0, 4.0, { a } });
        dragAcross ({ x (3.0), yA }, { x (13.0), yA });
        CHECK (spans (a) == Spans { { 0.0, 2.0 }, { 4.0, 4.0 }, { 12.0, 2.0 } });
        CHECK (arrange.getRange()->start == 12.0);
        studio.undo();
        arrange.setRange (Studio::Range { 2.0, 4.0, { a } });
        dragAcross ({ x (3.0), yA }, { x (13.0), yB }, juce::ModifierKeys::altModifier);   // copied, onto b
        CHECK (spans (a) == Spans { { 0.0, 8.0 } });
        CHECK (spans (b) == Spans { { 0.0, 4.0 }, { 6.0, 4.0 }, { 12.0, 2.0 } });
    }

    SECTION ("Double-click a part: a range over it; between parts: over the gap; a click alone clears it; another tool keeps it")
    {
        arrange.mouseDoubleClick (mouseAt (arrange, { x (7.0), yB }, { x (7.0), yB }, {}, 2));
        CHECK (arrange.getRange()->start == 6.0);
        CHECK (arrange.getRange()->end == 10.0);
        arrange.mouseDoubleClick (mouseAt (arrange, { x (5.0), yB }, { x (5.0), yB }, {}, 2));
        CHECK (arrange.getRange()->start == 4.0);
        CHECK (arrange.getRange()->end == 6.0);
        CHECK (arrange.getRange()->trackIds == std::vector<int> { b });
        press ('1');
        CHECK (arrange.hasRange());
        press ('2');
        arrange.mouseDown (mouseAt (arrange, { x (12.0), yA }, { x (12.0), yA }));
        arrange.mouseUp (mouseAt (arrange, { x (12.0), yA }, { x (12.0), yA }));
        CHECK_FALSE (arrange.hasRange());
    }
}

TEST_CASE ("SPAN ALL SCREENS: across the screens side by side, as tall as they all are; stacked screens, the box round them", "[ui][window]")
{
    // Three 1920 x 1080 screens, the middle one's taskbar 40 high at the bottom, the right one a 1200-high screen.
    using R = juce::Rectangle<int>;
    const juce::Array<R> three { R (-1920, 0, 1920, 1080), R (0, 0, 1920, 1040), R (1920, -60, 1920, 1200) };
    CHECK (Windows::spanOf (three) == juce::Rectangle<int> (-1920, 0, 5760, 1040));
    const juce::Array<R> one { R (0, 0, 2560, 1400) };
    CHECK (Windows::spanOf (one) == juce::Rectangle<int> (0, 0, 2560, 1400));
    const juce::Array<R> stacked { R (0, 0, 1920, 1080), R (0, 1080, 1920, 1080) };
    CHECK (Windows::spanOf (stacked) == juce::Rectangle<int> (0, 0, 1920, 2160));
    CHECK (Windows::spanOf ({}).isEmpty());
}

TEST_CASE ("The locators: Ctrl-click the left, Alt-click the right, Ctrl+Alt-click both; drag one's handle; P, Alt+P; the number pad's 1 and 2", "[ui][locators]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 8.0, 4.0, "Riff", {} });
    studio.setCycle (4.0, 16.0);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    const auto ppb = arrange.getPartBounds (track, 0).getWidth() / 4.0f;
    const auto x = [&] (double beat) { return arrange.getPartBounds (track, 0).getX() + (float) (beat - 8.0) * ppb; };
    const auto y = 6.0f;   // the ruler's top strip
    const auto& project = studio.getProject();
    const auto click = [&] (float atX, juce::ModifierKeys mods = {})
    {
        arrange.mouseDown (mouseAt (arrange, { atX, y }, { atX, y }, mods));
        arrange.mouseUp (mouseAt (arrange, { atX, y }, { atX, y }, mods));
    };
    const auto cmd = juce::ModifierKeys (juce::ModifierKeys::commandModifier);
    const auto alt = juce::ModifierKeys (juce::ModifierKeys::altModifier);

    click (x (2.1), cmd);   // the left locator (snapped to the beat)
    CHECK (project.loopStart == 2.0);
    CHECK (project.loopEnd == 16.0);
    click (x (20.0), alt);  // the right one
    CHECK (project.loopEnd == 20.0);
    click (x (24.0), cmd);  // the left past the right: they swap
    CHECK (project.loopStart == 20.0);
    CHECK (project.loopEnd == 24.0);
    click (x (9.5), juce::ModifierKeys (juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier));   // both: round the grid step clicked in (a beat at this zoom)
    CHECK (project.loopStart == 9.0);
    CHECK (project.loopEnd == 10.0);

    // In the lower part of the ruler too (where a plain click moves the playhead): Ctrl / Alt set the locators, the playhead stays.
    studio.setPosition (1.0);
    const auto lower = (float) Timeline::rulerHeight - 6.0f;
    const auto clickLower = [&] (float atX, juce::ModifierKeys mods)
    {
        arrange.mouseDown (mouseAt (arrange, { atX, lower }, { atX, lower }, mods));
        arrange.mouseUp (mouseAt (arrange, { atX, lower }, { atX, lower }, mods));
    };
    clickLower (x (3.0), cmd);
    CHECK (project.loopStart == 3.0);
    clickLower (x (14.0), alt);
    CHECK (project.loopEnd == 14.0);
    CHECK (studio.getEngine().getPosition() == 1.0);

    // A locator's handle dragged: just that one.
    studio.setCycle (4.0, 16.0);
    const auto dragFrom = [&] (float fromX, float toX)
    {
        arrange.mouseDown (mouseAt (arrange, { fromX, y }, { fromX, y }));
        for (int step = 1; step <= 4; ++step)
            arrange.mouseDrag (mouseAt (arrange, { fromX + (toX - fromX) * (float) step / 4.0f, y }, { fromX, y }));
        arrange.mouseUp (mouseAt (arrange, { toX, y }, { fromX, y }));
    };
    dragFrom (x (16.0), x (12.0));
    CHECK (project.loopStart == 4.0);
    CHECK (project.loopEnd == 12.0);
    dragFrom (x (4.0), x (6.0));
    CHECK (project.loopStart == 6.0);
    CHECK (project.loopEnd == 12.0);

    // P: the locators round the selected part; Alt+P: and round it it plays.
    arrange.select (track, 0);
    CHECK (view.keyPressed (juce::KeyPress ('p', {}, 'p')));
    CHECK (project.loopStart == 8.0);
    CHECK (project.loopEnd == 12.0);
    studio.setCycleOn (false);
    CHECK (view.keyPressed (juce::KeyPress ('p', alt, 'p')));
    CHECK (project.loopOn);
    CHECK (studio.getEngine().isPlaying());
    studio.stop();

    // The number pad: 1 / 2 to the locators; Ctrl: a locator to the playhead. (Not the tools, though the pad types 1 and 2 too.)
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::numberPad2, {}, '2')));
    CHECK (studio.getEngine().getPosition() == 12.0);
    CHECK (arrange.getTool() == ArrangeView::Tool::select);
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::numberPad1, {}, '1')));
    CHECK (studio.getEngine().getPosition() == 8.0);
    studio.setPosition (5.0);
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::numberPad1, cmd, 0)));
    CHECK (project.loopStart == 5.0);
    studio.setPosition (14.0);
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::numberPad2, cmd, 0)));
    CHECK (project.loopEnd == 14.0);
}

TEST_CASE ("Tracks in the list: Ctrl / Shift-click to select several; drag rows to reorder (the mixer follows); Up / Down; double-click to add; Shift+Delete", "[ui][tracks]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    const auto c = studio.addTrack ({});
    studio.addPart (b, { 0.0, 4.0, "B", {} });
    studio.dispatchPendingMessages();
    auto& list = view.getTrackList();
    auto& arrange = view.getArrangeView();
    const auto order = [&]
    {
        std::vector<int> ids;
        for (const auto& track : studio.getProject().tracks)
            ids.push_back (track.id);
        return ids;
    };
    using Ids = std::vector<int>;
    // A row: where it is in the list (its middle), from the arrangement's matching row.
    const auto rowY = [&] (int id) { return arrange.trackTop (id) + (float) Timeline::trackHeight * 0.5f; };
    const auto rowAt = [&] (int id) -> juce::Component*
    {
        for (auto* child : list.getChildren())
            if (child->getBounds().toFloat().contains (20.0f, rowY (id)) && child->getHeight() == Timeline::trackHeight)
                return child;
        return nullptr;
    };
    const auto clickRow = [&] (int id, juce::ModifierKeys mods = {})
    {
        auto* row = rowAt (id);
        REQUIRE (row != nullptr);
        const juce::Point<float> at (20.0f, 8.0f);
        row->mouseDown (mouseAt (*row, at, at, mods));
        row->mouseUp (mouseAt (*row, at, at, mods));
    };

    clickRow (a);
    clickRow (c, juce::ModifierKeys::commandModifier);
    CHECK (studio.getSelectedTracks() == Ids { a, c });
    clickRow (b, juce::ModifierKeys::shiftModifier);   // the run from c (clicked last) to b
    CHECK (studio.getSelectedTracks() == Ids { a, b, c });
    clickRow (b);   // a plain click on one of several: it alone
    CHECK (studio.getSelectedTracks() == Ids { b });

    // Dragged below c: the drop line, then the order (the mixer's strips too).
    {
        auto* row = rowAt (b);
        REQUIRE (row != nullptr);
        const juce::Point<float> from (20.0f, 8.0f);
        const auto to = juce::Point<float> (20.0f, rowY (c) - (float) row->getY() + 30.0f);   // past c's middle
        row->mouseDown (mouseAt (*row, from, from));
        row->mouseDrag (mouseAt (*row, from + (to - from) * 0.5f, from));
        row->mouseDrag (mouseAt (*row, to, from));
        CHECK (list.getDropIndex() == 3);
        row->mouseUp (mouseAt (*row, to, from));
    }
    CHECK (order() == Ids { a, c, b });
    studio.dispatchPendingMessages();
    CHECK (view.getMixer().getStrip (1).getBusId() == c);
    CHECK (view.getMixer().getStrip (2).getBusId() == b);
    studio.undo();
    CHECK (order() == Ids { a, b, c });

    // Up / Down: the track above / below (and its first part, if a part was selected).
    studio.selectTrack (a);
    arrange.select (a, -1);
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    CHECK (studio.getSelectedTrack() == b);
    studio.addPart (c, { 12.0, 4.0, "C later", {} });
    studio.addPart (c, { 8.0, 4.0, "C first", {} });
    arrange.select (b, 0);
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    CHECK (studio.getSelectedTrack() == c);
    CHECK (arrange.getSelection().trackId == c);   // its first part (by where it starts)
    CHECK (arrange.getSelection().index == 1);
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    CHECK (studio.getSelectedTrack() == b);
    CHECK (arrange.getSelection().index == 0);

    // A double-click under the tracks: a new one like the selected (an instrument track here).
    const auto before = studio.getProject().tracks.size();
    list.mouseDoubleClick (mouseAt (list, { 100.0f, (float) list.getHeight() - 100.0f }, { 100.0f, (float) list.getHeight() - 100.0f }, {}, 2));
    CHECK (studio.getProject().tracks.size() == before + 1);
    const auto added = studio.getSelectedTrack();

    // Shift+Delete: the selected tracks (empty ones straight away).
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey, juce::ModifierKeys::shiftModifier, 0)));
    CHECK (studio.getProject().findTrack (added) == nullptr);

    // Moving by the menu's actions: to the top, down one.
    studio.selectTrack (c);
    list.moveSelectedTracks (TrackList::MoveTo::first);
    CHECK (order() == Ids { c, a, b });
    list.moveSelectedTracks (TrackList::MoveTo::next);
    CHECK (order() == Ids { a, c, b });
    list.moveSelectedTracks (TrackList::MoveTo::last);
    CHECK (order() == Ids { a, b, c });
}

TEST_CASE ("Audio files dropped on the arrangement: on the audio track under them, at the bar; elsewhere a new audio track", "[ui][import]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto audio = studio.addAudioTrack (false);
    studio.dispatchPendingMessages();
    const auto file = juce::File::createTempFile ("dropped.wav");
    {
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (48000.0).withNumChannels (1).withBitsPerSample (24));
        REQUIRE (writer != nullptr);
        juce::AudioBuffer<float> buffer (1, 48000);
        buffer.clear();
        writer->writeFromAudioSampleBuffer (buffer, 0, 48000);
    }
    CHECK (view.isInterestedInFileDrag ({ file.getFullPathName() }));
    auto& arrange = view.getArrangeView();
    // Over the audio track's row, in bar 3 (beats 8 to 12 at 4/4).
    const auto inArrange = juce::Point<float> ((float) (9.5 * Timeline {}.pixelsPerBeat), arrange.trackTop (audio) + 20.0f);
    const auto inView = view.getLocalPoint (&arrange, inArrange.toInt());
    view.filesDropped ({ file.getFullPathName() }, inView.x, inView.y);
    const auto& parts = studio.getProject().findTrack (audio)->parts;
    REQUIRE (parts.size() == 1);
    CHECK (parts[0].start == 8.0);
    // Dropped under the tracks: a new audio track.
    const auto tracks = studio.getProject().tracks.size();
    view.filesDropped ({ file.getFullPathName() }, inView.x, view.getHeight() - 60);
    CHECK (studio.getProject().tracks.size() == tracks + 1);
    CHECK (studio.getProject().tracks.back().isAudio());
    file.deleteFile();
    studio.getAudioFolder().deleteRecursively();
}

TEST_CASE ("CPU: the engine measures each block's load; the meter shows the average and the peak; an overload lights its light until clicked", "[ui][cpu]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& engine = studio.getEngine();
    const auto rate = engine.getSampleRate();
    const auto blockSeconds = 512.0 / rate;

    // A block that took a fifth of its time, many times: the average settles at 0.2.
    for (int i = 0; i < 100; ++i)
        engine.notePerformance (blockSeconds * 0.2, 512);
    auto now = engine.takePerformance();
    CHECK_THAT (now.average, WithinAbs (0.2, 0.01));
    CHECK_THAT (now.peak, WithinAbs (0.2, 0.01));
    CHECK (now.overloads == 0);
    CHECK (engine.takePerformance().peak == 0.0f);   // the peak is since the last look

    // One that took longer than it lasts: an overload (a dropout).
    engine.notePerformance (blockSeconds * 1.3, 512);
    auto& transport = view.getTransport();
    transport.repaint();
    juce::Timer::callPendingTimersSynchronously();
    for (int i = 0; i < 3 && ! transport.isOverloadLit(); ++i)
    {
        juce::Thread::sleep (40);
        juce::Timer::callPendingTimersSynchronously();
    }
    CHECK (transport.isOverloadLit());
    CHECK (transport.getShownPeak() > 1.0f);
    // A click on the meter resets it (as the manual has it).
    const auto at = transport.getCpuArea().getCentre();
    transport.mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys::leftButtonModifier,
                                           0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &transport, &transport, juce::Time::getCurrentTime(), at,
                                           juce::Time::getCurrentTime(), 1, false));
    CHECK_FALSE (transport.isOverloadLit());
    CHECK (engine.takePerformance().overloads == 0);
}

TEST_CASE ("A wide window (spanning screens): the position screen centred at its own width, the keys on its left, the tools on its right; the header's keys centred", "[ui][wide]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    auto& transport = view.getTransport();
    auto& tools = view.getTools();

    // One screen: as before (the tools in their row under the transport, the screen as wide as there's room).
    view.setSize (1500, 900);
    CHECK_FALSE (view.isWide());
    CHECK (tools.getY() >= transport.getBottom());
    CHECK (transport.getPositionScreen().getWidth() > 300.0f);

    // Three screens.
    CHECK_FALSE (view.isMixerOpen());
    view.setSize (5760, 1000);
    REQUIRE (view.isWide());
    // Turning wide: the mixer docked, its racks hidden, as short as it goes (the owner's choice).
    CHECK (view.isMixerOpen());
    CHECK_FALSE (view.isMixerInWindow());
    CHECK_FALSE (view.getMixer().isRackShown());
    CHECK (view.getMixer().getHeight() == view.getMixer().getMinimumHeight());
    // What's changed after it turned wide stays (it isn't done again on every resize).
    view.getMixer().setRackShown (true);
    view.setSize (5700, 1000);
    CHECK (view.getMixer().isRackShown());
    const auto screen = transport.getPositionScreen();
    CHECK (screen.getWidth() == (float) TransportBar::widePositionWidth);
    CHECK_THAT (transport.getX() + screen.getCentreX(), WithinAbs (view.getWidth() * 0.5f, 300.0f));   // near the middle
    // The keys on its left, the tools on its right (on the transport, no row of their own).
    auto* clickKey = buttonNamed (transport, "CLICK");
    REQUIRE (clickKey != nullptr);
    // CLICK, then the CPU screen, then the position screen (the owner's order).
    const auto cpu = transport.getCpuArea();
    CHECK (clickKey->getRight() <= (int) cpu.getX());
    CHECK (clickKey->getRight() > (int) cpu.getX() - 30);
    CHECK (cpu.getRight() <= screen.getX());
    CHECK (cpu.getRight() > screen.getX() - 30);
    // AUTO-SCROLL just before HOME.
    CHECK (view.getAutoScrollButton().getRight() < buttonNamed (transport, "|<")->getX() + transport.getX());
    CHECK (view.getAutoScrollButton().getRight() > buttonNamed (transport, "|<")->getX() + transport.getX() - 50);
    CHECK (tools.getX() > transport.getX() + (int) screen.getRight());
    CHECK (tools.getBottom() <= transport.getBottom());
    // All of it on the middle screen (1920 to 3840): no screen edge through it.
    CHECK (buttonNamed (transport, "|<")->getX() + transport.getX() >= 1920);
    CHECK (tools.getRight() <= 3840);
    CHECK (tools.getY() >= transport.getY());
    CHECK (view.getArrangeView().getY() < transport.getBottom() + MainView::toolbarHeight);   // the tracks have the row's room
    // The header's keys: NEW to W in the middle.
    auto* newKey = buttonNamed (view, "NEW");
    auto* mixerKey = buttonNamed (view, "MIXER");
    REQUIRE (newKey != nullptr);
    REQUIRE (mixerKey != nullptr);
    CHECK (std::abs ((newKey->getX() + mixerKey->getRight()) / 2 - view.getWidth() / 2) < 60);   // (R and W go a little past MIXER)
    CHECK (newKey->getX() > 2000);
}

TEST_CASE ("RENDER IN PLACE: the page's choices; rendering as last time from the main window", "[ui][render]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto synth = studio.getProject().tracks[0].id;
    studio.addPart (synth, { 4.0, 4.0, "Riff", { { 0.0, 1.0, 60, 1.0f } } });

    // The page: what's chosen comes back as the options (Remove only for whole tracks).
    Windows::RenderPage parts (studio, false);
    CHECK_FALSE (parts.source.isItemEnabled (3));
    Windows::RenderPage tracks (studio, true);
    CHECK (tracks.source.isItemEnabled (3));
    parts.mode.setSelectedId (2, juce::dontSendNotification);
    parts.processing.setSelectedId (1, juce::dontSendNotification);
    parts.tail.setSelectedId (6, juce::dontSendNotification);   // 2 seconds
    parts.bits.setSelectedId (32, juce::dontSendNotification);
    parts.source.setSelectedId (2, juce::dontSendNotification);
    const auto chosen = parts.getOptions();
    CHECK (chosen.mode == Studio::RenderOptions::Mode::blocks);
    CHECK (chosen.processing == Studio::RenderOptions::Processing::dry);
    CHECK (chosen.tail == Studio::RenderOptions::Tail::seconds);
    CHECK (chosen.tailSize == 2.0);
    CHECK (chosen.bitsPerSample == 32);
    CHECK (chosen.source == Studio::RenderOptions::Source::mute);

    // As last time (no settings file in tests: the defaults): a new audio track under the synth.
    view.renderInPlace ({ { synth, 4.0, 8.0, false } }, false);
    REQUIRE (studio.getProject().tracks.size() == 2);
    CHECK (studio.getProject().tracks[1].isAudio());
    CHECK (studio.getProject().tracks[1].parts.size() == 1);
    studio.getAudioFolder().deleteRecursively();
}

TEST_CASE ("The Inspector: the selected track's settings, by kind, in sections that fold", "[ui][inspector]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& inspector = view.getInspector();
    REQUIRE (view.isInspectorShown());
    CHECK (inspector.isVisible());
    CHECK (inspector.getWidth() == Inspector::width);
    CHECK (inspector.getRight() < view.getArrangeView().getX());

    const auto keys = studio.getProject().tracks[0].id;
    studio.selectTrack (keys);
    studio.dispatchPendingMessages();   // the screen hears about changes by message
    CHECK (inspector.getTrack() == keys);
    using S = Inspector::Section;

    SECTION ("An instrument track: MIDI IN and CHANNEL, its inserts and sends, its fader")
    {
        CHECK (inspector.getMidiInput().isVisible());
        CHECK (inspector.getChannel().isVisible());
        CHECK_FALSE (inspector.getAudioInput().isVisible());
        CHECK (inspector.getKey ("REC").isVisible());
        CHECK_FALSE (inspector.getKey ("MON").isVisible());
        CHECK (inspector.getInsertSlot (0).isVisible());
        CHECK (inspector.getHeader (S::sends).isVisible());
        CHECK (inspector.getFader().getSingleTrack() == keys);
        CHECK (inspector.getFader().getNumStrips() == 2);   // its strip (and the master's, not shown)

        // Its controls change the track.
        inspector.getChannel().setSelectedId (7, juce::sendNotificationSync);
        CHECK (studio.getProject().findTrack (keys)->midiChannel == 7);
        inspector.getVolume().setValue (-6.0, juce::sendNotificationSync);
        CHECK (studio.getProject().findTrack (keys)->volumeDb == -6.0f);
        inspector.getKey ("M").onClick();
        CHECK (studio.getProject().findTrack (keys)->mute);
        // And follow it.
        studio.setPan (keys, -0.5f);
        studio.dispatchPendingMessages();
        CHECK (inspector.getPan().getValue() == -0.5);
        studio.setMidiChannel (keys, 3);
        studio.dispatchPendingMessages();
        CHECK (inspector.getChannel().getSelectedId() == 3);
    }
    SECTION ("An audio track: its input and MON, no MIDI; an FX channel: no sends, no REC")
    {
        const auto guitar = studio.addAudioTrack (true);
        studio.selectTrack (guitar);
        studio.dispatchPendingMessages();
        CHECK (inspector.getTrack() == guitar);
        CHECK (inspector.getAudioInput().isVisible());
        CHECK (inspector.getAudioInput().getText().contains ("IN 1+2"));
        CHECK_FALSE (inspector.getMidiInput().isVisible());
        CHECK (inspector.getKey ("MON").isVisible());

        const auto fx = studio.addFxChannel (nullptr);
        studio.selectTrack (fx);
        studio.dispatchPendingMessages();
        CHECK_FALSE (inspector.getHeader (S::sends).isVisible());
        CHECK_FALSE (inspector.getKey ("REC").isVisible());
        CHECK_FALSE (inspector.getOutput().isEnabled());   // FX channels go to the master
    }
    SECTION ("A section's title folds it; the others move up")
    {
        const auto routingY = inspector.getHeader (S::routing).getY();
        const auto height = inspector.getContentHeight();
        inspector.setFolded (S::basics, true);
        CHECK_FALSE (inspector.getVolume().isVisible());
        CHECK (inspector.getHeader (S::routing).getY() < routingY);
        CHECK (inspector.getContentHeight() < height);
        inspector.setFolded (S::basics, false);
        CHECK (inspector.getVolume().isVisible());
        inspector.setFolded (S::fader, true);
        CHECK_FALSE (inspector.getFader().isVisible());
    }
    SECTION ("Ctrl+Alt+L (or INSPECTOR) hides it, and the track list takes its place")
    {
        const auto arrangeX = view.getArrangeView().getX();
        CHECK (view.keyPressed (juce::KeyPress ('l', juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier, 0)));
        CHECK_FALSE (view.isInspectorShown());
        CHECK_FALSE (inspector.isVisible());
        CHECK (view.getArrangeView().getX() < arrangeX);
        view.toggleInspector();
        CHECK (inspector.isVisible());
        CHECK (view.getArrangeView().getX() == arrangeX);
    }
}

TEST_CASE ("VISIBILITY: every track with a box to show or hide it; hidden and disabled tracks leave the track list and the mixer", "[ui][visibility]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& inspector = view.getInspector();
    const auto keys = studio.getProject().tracks[0].id;
    const auto bass = studio.addTrack ({});
    const auto lead = studio.addTrack ({});
    studio.dispatchPendingMessages();
    const auto& timeline = view.getTimeline();
    const auto trackRows = [&]
    {
        int n = 0;
        for (const auto& row : timeline.layout (studio))
            n += row.kind == Timeline::Row::Kind::track ? 1 : 0;
        return n;
    };
    REQUIRE (trackRows() == 3);
    CHECK (view.getMixer().getNumStrips() == 4);   // three tracks and the master

    // The tabs.
    inspector.getTabKey (Inspector::Tab::visibility).onClick();
    CHECK (inspector.getTab() == Inspector::Tab::visibility);
    CHECK_FALSE (inspector.getVolume().isShowing());
    REQUIRE (inspector.getVisibilityBox (bass) != nullptr);
    CHECK (inspector.getVisibilityBox (bass)->getToggleState());

    // Untick: hidden (the arrangement, the track list and the mixer leave it out).
    auto* box = inspector.getVisibilityBox (bass);
    box->setToggleState (false, juce::dontSendNotification);
    box->onClick();
    studio.dispatchPendingMessages();
    CHECK (studio.getProject().findTrack (bass)->hidden);
    CHECK (trackRows() == 2);
    CHECK (view.getMixer().getNumStrips() == 3);
    CHECK (inspector.getShowAll().isEnabled());

    // Disabled: greyed in the track list, out of the mixer, listed as DISABLED; ENABLE TRACK in its Inspector.
    studio.setTracksDisabled ({ lead }, true);
    studio.dispatchPendingMessages();
    CHECK (trackRows() == 2);   // still shown
    CHECK (view.getMixer().getNumStrips() == 2);
    inspector.setTab (Inspector::Tab::inspector);
    studio.selectTrack (lead);
    studio.dispatchPendingMessages();
    CHECK (inspector.getEnableKey().isVisible());
    inspector.getEnableKey().onClick();
    studio.dispatchPendingMessages();
    CHECK_FALSE (studio.getProject().findTrack (lead)->disabled);
    CHECK_FALSE (inspector.getEnableKey().isVisible());

    // SHOW ALL.
    inspector.setTab (Inspector::Tab::visibility);
    inspector.getShowAll().onClick();
    studio.dispatchPendingMessages();
    CHECK (trackRows() == 3);
    CHECK (view.getMixer().getNumStrips() == 4);
    CHECK_FALSE (inspector.getShowAll().isEnabled());
    juce::ignoreUnused (keys);
}

TEST_CASE ("SELECT: a rectangle, Shift-click and Ctrl+A select several parts; they move, copy, cut, paste, duplicate and delete together", "[ui][multiselect]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& arrange = view.getArrangeView();
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    const auto c = studio.addTrack ({});
    studio.addPart (a, { 0.0, 4.0, "A1", { { 0.0, 1.0, 60, 1.0f } } });
    studio.addPart (a, { 8.0, 4.0, "A2", { { 0.0, 1.0, 62, 1.0f } } });
    studio.addPart (b, { 0.0, 4.0, "B1", { { 0.0, 1.0, 64, 1.0f } } });
    studio.addPart (c, { 16.0, 4.0, "C1", { { 0.0, 1.0, 67, 1.0f } } });
    studio.dispatchPendingMessages();
    const auto idOf = [&] (int track, int index) { return studio.getProject().findTrack (track)->parts[(size_t) index].id; };
    const auto mouse = [&] (juce::Point<float> at, juce::ModifierKeys mods = {})
    {
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &arrange, &arrange, juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 1, false);
    };
    const auto drag = [&] (juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods = {})
    {
        arrange.mouseDown (mouse (from, mods));
        auto moved = juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), to, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                       &arrange, &arrange, juce::Time::getCurrentTime(), from, juce::Time::getCurrentTime(), 1, false);
        arrange.mouseDrag (moved);
        arrange.mouseUp (moved);
    };

    SECTION ("A rectangle over empty space and parts selects all it touches")
    {
        const auto a1 = arrange.getPartBounds (a, 0), b1 = arrange.getPartBounds (b, 0);
        // From empty space right of A1 (beat 5) down and left over B1.
        drag ({ arrange.getPartBounds (a, 0).getRight() + 20.0f, a1.getY() + 2.0f }, { 2.0f, b1.getBottom() - 2.0f });
        const auto selected = arrange.getSelectedParts();
        CHECK (selected.size() == 2);
        CHECK (arrange.isPartSelected (idOf (a, 0)));
        CHECK (arrange.isPartSelected (idOf (b, 0)));
        CHECK_FALSE (arrange.isPartSelected (idOf (a, 1)));

        // Shift-click adds one; again, takes it out.
        const auto a2 = arrange.getPartBounds (a, 1).getCentre();
        arrange.mouseDown (mouse (a2, juce::ModifierKeys::shiftModifier));
        arrange.mouseUp (mouse (a2, juce::ModifierKeys::shiftModifier));
        CHECK (arrange.getSelectedParts().size() == 3);
        arrange.mouseDown (mouse (a2, juce::ModifierKeys::shiftModifier));
        arrange.mouseUp (mouse (a2, juce::ModifierKeys::shiftModifier));
        CHECK (arrange.getSelectedParts().size() == 2);

        // Dragging one moves both: 4 beats later, the same tracks.
        const auto beat = (float) view.getTimeline().pixelsPerBeat;
        const auto from = arrange.getPartBounds (a, 0).getCentre();
        drag (from, from.translated (4.0f * beat, 0.0f));
        studio.dispatchPendingMessages();
        CHECK (studio.getProject().findTrack (a)->parts.size() == 2);
        double aStart = -1.0, bStart = -1.0;
        for (const auto& p : studio.getProject().findTrack (a)->parts)
            if (p.name == "A1") aStart = p.start;
        bStart = studio.getProject().findTrack (b)->parts[0].start;
        CHECK (aStart == 4.0);
        CHECK (bStart == 4.0);
        CHECK (arrange.getSelectedParts().size() == 2);   // still selected
        studio.undo();   // one undo step
        CHECK (studio.getProject().findTrack (b)->parts[0].start == 0.0);
    }
    SECTION ("Ctrl+A, then Delete: all gone in one undo step")
    {
        CHECK (view.keyPressed (juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0)));
        CHECK (arrange.getSelectedParts().size() == 4);
        CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
        for (const auto id : { a, b, c })
            CHECK (studio.getProject().findTrack (id)->parts.empty());
        studio.undo();
        CHECK (studio.getProject().findTrack (a)->parts.size() == 2);
        CHECK (studio.getProject().findTrack (c)->parts.size() == 1);
    }
    SECTION ("Ctrl+C, Ctrl+V at the playhead on the selected track; Alt+V at the origin; Ctrl+X; Ctrl+D")
    {
        arrange.selectParts ({ idOf (a, 0), idOf (b, 0) });
        CHECK (view.keyPressed (juce::KeyPress ('c', juce::ModifierKeys::commandModifier, 0)));
        studio.selectTrack (b);
        studio.setPosition (32.0);
        CHECK (view.keyPressed (juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0)));
        // A's part onto B, B's onto C (the selected track down), at bar 9.
        CHECK (studio.getProject().findTrack (b)->parts.size() == 2);
        CHECK (studio.getProject().findTrack (b)->parts.back().start == 32.0);
        CHECK (studio.getProject().findTrack (b)->parts.back().notes[0].pitch == 60);
        CHECK (studio.getProject().findTrack (c)->parts.back().start == 32.0);
        CHECK (studio.getProject().findTrack (c)->parts.back().notes[0].pitch == 64);
        // Paste at Origin: where they came from.
        CHECK (view.keyPressed (juce::KeyPress ('v', juce::ModifierKeys::altModifier, 0)));
        CHECK (studio.getProject().findTrack (a)->parts.size() == 3);

        // Ctrl+X: they go; Ctrl+D on two: copied after them as one unit.
        arrange.selectParts ({ idOf (c, 0) });
        CHECK (view.keyPressed (juce::KeyPress ('x', juce::ModifierKeys::commandModifier, 0)));
        CHECK (studio.getProject().findTrack (c)->parts.size() == 1);
        arrange.selectParts ({ idOf (a, 0), idOf (a, 1) });
        CHECK (view.keyPressed (juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0)));
        auto latest = 0.0;
        for (const auto& p : studio.getProject().findTrack (a)->parts)
            latest = juce::jmax (latest, p.start);
        CHECK (latest == 20.0);   // the pair spans 0 to 12: the copy starts at 12 (A2's copy at 20)
    }
}

TEST_CASE ("The Inspector: five insert slots in view (the rest scroll), its width dragged; track rows: the name large, no instrument key", "[ui][inspector]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& inspector = view.getInspector();
    studio.selectTrack (studio.getProject().tracks[0].id);
    studio.dispatchPendingMessages();
    auto& inserts = inspector.getInsertsView();
    CHECK (inserts.isVisible());
    const auto slot = inspector.getInsertSlot (0).getHeight();
    CHECK (inserts.getHeight() < 6 * slot + 12);
    CHECK (inserts.getHeight() >= 5 * slot);
    CHECK (inspector.getInsertSlot (15).getBottom() > inserts.getHeight());   // the 16th: scrolled to
    CHECK (inserts.canScrollVertically());

    const auto arrangeX = view.getArrangeView().getX();
    view.setInspectorWidth (320);
    CHECK (inspector.getWidth() == 320);
    CHECK (view.getArrangeView().getX() == arrangeX + 320 - Inspector::width);
    view.setInspectorWidth (20);
    CHECK (inspector.getWidth() == MainView::minimumInspectorWidth);

    std::vector<juce::TextButton*> buttons;
    findAll (view.getTrackList(), buttons);
    CHECK (std::none_of (buttons.begin(), buttons.end(), [] (auto* b) { return b->getButtonText() == "PARAGON SYNTH" && b->isVisible(); }));
}

TEST_CASE ("The key editor at 200% (the default), SPLIT, the pencil, whole and half note grids", "[ui][editor]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, { 0.0, 8.0, "Riff", { { 0.0, 4.0, 60, 0.8f }, { 0.0, 4.0, 64, 0.8f }, { 4.0, 1.0, 67, 0.8f } } });
    view.getArrangeView().select (trackId, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    auto& editor = view.getKeyEditor();
    auto& frame = view.getKeyEditorFrame();

    // Twice the size: laid out at half the frame and drawn doubled.
    CHECK (view.getKeyEditorSize() == 200);
    CHECK (std::abs (editor.getWidth() * 2 - frame.getWidth()) <= 1);
    CHECK (editor.getTransform().getScaleFactor() == 2.0f);
    CHECK (std::abs (editor.getScreenBounds().getWidth() - frame.getScreenBounds().getWidth()) <= 2);   // it fills its frame on screen
    view.setKeyEditorSize (150);
    CHECK (view.getKeyEditorSize() == 150);
    view.setKeyEditorSize (200);

    // The grids reach whole and half notes; QUANTIZE to a half note.
    editor.setGrid (4.0);
    CHECK (editor.getGrid() == 4.0);
    editor.setGrid (2.0);

    // SPLIT (3): the pencil and scissors as pointers; a click cuts a note on the grid.
    CHECK (editor.keyPressed (juce::KeyPress ('3', {}, '3')));
    CHECK (editor.getTool() == KeyEditor::Tool::split);
    CHECK (editor.getMouseCursor() != juce::MouseCursor (juce::MouseCursor::NormalCursor));
    REQUIRE (editor.splitAt (0, 2.0));
    auto notes = editor.getPart()->notes;
    CHECK (notes.size() == 4);
    auto halves = 0;
    for (const auto& n : notes)
        if (n.pitch == 60)
            halves += (n.length == 2.0) ? 1 : 0;
    CHECK (halves == 2);
    // Several selected: all of those the cut crosses (the manual's).
    editor.selectNotes ({ 0, 1 });   // 60's first half and 64
    studio.undo();
    editor.selectNotes ({ 0, 1 });
    REQUIRE (editor.splitAt (1, 1.0));
    CHECK (editor.getPart()->notes.size() == 5);
    CHECK_FALSE (editor.splitAt (2, 4.5 + 0.5));   // 67 ends at 5: a cut at its end does nothing
    editor.setTool (KeyEditor::Tool::draw);
    CHECK (editor.getMouseCursor() != juce::MouseCursor (juce::MouseCursor::CrosshairCursor));
}
