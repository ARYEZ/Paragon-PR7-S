#include <set>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/MainView.h"
#include "ui/TrackActions.h"
#include "ui/Windows.h"

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
    CHECK (rowTop == Timeline::rulerHeight + Timeline::masterHeight + Timeline::trackHeight);   // (0.9.12: the master's row first)

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
    view.setSize (1500, 1300);   // (0.71.0: the docked editor leaves five tracks above it; these notes need its room)
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

    // Q quantizes (all the notes, with none selected) to the song's quantize value, 1/4 (0.31.0: the header's, not the editor's grid).
    editor.setTool (KeyEditor::Tool::select);
    studio.setQuantizeBeats (1.0);
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
    CHECK (window->getName() == "MIDI Editor: Riff");

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
    view.setSize (1500, 1300);   // (0.71.0: the docked mixer leaves five tracks above it)
    const auto first = studio.getProject().tracks[0].id;
    const auto second = studio.addTrack ({});
    studio.dispatchPendingMessages();

    // The on-screen keyboard has gone (the lower zone has its room).
    std::vector<juce::MidiKeyboardComponent*> keyboards;
    findAll (view, keyboards);
    CHECK (keyboards.empty());

    // F3 opens the mixer where it was: under the arrangement at first, down to the window's bottom (0.9.10).
    auto& mixer = view.getMixer();
    auto& arrange = view.getArrangeView();
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F3Key)));
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
    // In its window both racks show, the inserts above the sends, whatever was chosen docked (0.9.14).
    {
        REQUIRE (mixer.getNumStrips() >= 2);
        mixer.setRackShown (false);
        auto& strip = mixer.getStrip (0);
        CHECK (mixer.showsInserts());
        CHECK (mixer.showsSends());
        CHECK (strip.getInsertSlot (0).isVisible());
        CHECK (strip.getSendSlot (0).isVisible());
        CHECK (strip.getSendSlot (0).getY() > strip.getInsertSlot (0).getBottom());
        auto inserts = false;
        for (auto* child : mixer.getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (child); b != nullptr && b->getButtonText() == "INSERTS")
                inserts = b->isVisible();
        CHECK_FALSE (inserts);   // (INSERTS / SENDS are the docked choice)
    }
    view.openKeyEditor();
    CHECK (view.isMixerOpen());
    CHECK (view.isKeyEditorOpen());
    // Its keys work there too: F3 closes it.
    CHECK (window->keyPressed (juce::KeyPress (juce::KeyPress::F3Key)));
    CHECK_FALSE (view.isMixerOpen());
    CHECK (view.getMixerWindow() == nullptr);
    CHECK (mixer.getParentComponent() == &view);
    // F3 again: open where it was, in its own window.
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
    // Docked again: the docked choice (none) holds.
    CHECK_FALSE (mixer.showsInserts());
    CHECK_FALSE (mixer.showsSends());
    CHECK_FALSE (mixer.getStrip (0).getInsertSlot (0).isVisible());
    mixer.setRackShown (true);
    // Docked: F3 closes it, F3 opens it docked again; Alt+F3 takes it full screen.
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F3Key)));
    CHECK_FALSE (view.isMixerOpen());
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F3Key)));
    CHECK (view.isMixerOpen());
    CHECK_FALSE (view.isMixerInWindow());
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F3Key, juce::ModifierKeys::altModifier, 0)));
    CHECK (view.isMixerInWindow());
    REQUIRE (view.getMixerWindow() != nullptr);
    view.setMixerInWindow (false);
    view.openMixer();

    // F2 the MIDI editor (Alt+F2: its own window), F1 the Inspector.
    view.closeMixer();
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F2Key)));
    CHECK (view.isKeyEditorOpen());
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F2Key, juce::ModifierKeys::altModifier, 0)));
    CHECK (view.isKeyEditorInWindow());
    CHECK (view.isKeyEditorOpen());
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F2Key, juce::ModifierKeys::altModifier, 0)));
    CHECK_FALSE (view.isKeyEditorInWindow());
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F2Key)));
    CHECK_FALSE (view.isKeyEditorOpen());
    const auto inspectorShown = view.isInspectorShown();
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::F1Key)));
    CHECK (view.isInspectorShown() != inspectorShown);
    view.keyPressed (juce::KeyPress (juce::KeyPress::F1Key));
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
    CHECK (monitor == nullptr);   // (0.35.0, the owner asked: MON and LANES are in the Inspector, not on the rows)
    CHECK (monitors == 0);
    CHECK (input != nullptr);
    studio.selectTrack (guitar);
    studio.dispatchPendingMessages();
    auto& inspector = view.getInspector();
    REQUIRE (inspector.getKey ("MON").isVisible());
    REQUIRE (inspector.getKey ("LANES").isVisible());
    // Under RENDER and PRESET, LANES then MON.
    CHECK (inspector.getKey ("LANES").getY() > inspector.getKey ("RENDER").getY());
    CHECK (inspector.getKey ("MON").getY() == inspector.getKey ("LANES").getY());
    CHECK (inspector.getKey ("MON").getX() > inspector.getKey ("LANES").getX());
    inspector.getKey ("MON").onClick();
    CHECK (studio.getProject().findTrack (guitar)->monitor);
    inspector.getKey ("LANES").onClick();
    CHECK (studio.getProject().findTrack (guitar)->showLanes);
    inspector.getKey ("LANES").onClick();
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
    const juce::Point<float> between (22.0f * beatPixels, (float) (Timeline::rulerHeight + Timeline::masterHeight) + 0.5f * (float) Timeline::trackHeight);
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

    // The keys (0.9.9: the tracks' REC, R and W are in the Inspector and the mixer, not on the rows; the master's row has R and W).
    std::vector<juce::TextButton*> buttons;
    findAll (view.getTrackList(), buttons);
    const auto count = [&] (const juce::String& text) { return std::count_if (buttons.begin(), buttons.end(), [&] (auto* b) { return b->getButtonText() == text && b->isVisible(); }); };
    CHECK (count ("REC") == 0);
    CHECK (count ("R") == 1);
    CHECK (count ("W") == 1);

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
    // (The owner's report, 0.45.1: a click puts the point on the line as it is, not at the mouse's
    // height: the first takes the fader's value, the next ones the line's, wherever the click is.)
    CHECK_FALSE (studio.getAutomationRead (first));
    const auto fader = pr7s::volumeToTravel (0.0f);
    click ({ 4.1f * beatPixels, yFor (0.75f) });
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 1);
    CHECK (studio.getAutomation (first)->at (0).points[0].beat == 4.0);
    CHECK_THAT (studio.getAutomation (first)->at (0).points[0].value, WithinAbs (fader, 0.001));
    CHECK (studio.getAutomationRead (first));
    click ({ 8.0f * beatPixels, yFor (0.25f) });
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 2);
    CHECK_THAT (studio.getAutomation (first)->at (0).points[1].value, WithinAbs (fader, 0.001));
    click ({ 10.0f * beatPixels, yFor (0.95f) });
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 3);
    CHECK_THAT (studio.getAutomation (first)->at (0).points[2].value, WithinAbs (fader, 0.001));
    studio.undo();
    studio.dispatchPendingMessages();
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 2);
    // (0.46.1, the owner asked) Between them too: a point on the line there, selected.
    click ({ 6.0f * beatPixels, yFor (0.5f) });
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 3);
    CHECK (studio.getAutomation (first)->at (0).points[1].beat == 6.0);
    CHECK_THAT (studio.getAutomation (first)->at (0).points[1].value, WithinAbs (fader, 0.001));
    CHECK (arrange.getPointSelection().points == std::vector<int> { 1 });
    // Delete: it goes and the next point (beat 8) is selected; Delete again takes that, then the
    // one before it at the end of the lane (beat 4).
    arrange.deleteSelectedPoints();
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 2);
    REQUIRE (arrange.getPointSelection().points == std::vector<int> { 1 });
    CHECK (studio.getAutomation (first)->at (0).points[1].beat == 8.0);
    arrange.deleteSelectedPoints();
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 1);
    CHECK (arrange.getPointSelection().points == std::vector<int> { 0 });
    studio.undo();
    studio.undo();
    studio.dispatchPendingMessages();
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 3);
    studio.undo();   // (the point at beat 6 out again)
    studio.dispatchPendingMessages();
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 2);

    // Drag the second point down (Ctrl: up and down only, so it stays on its beat).
    const auto point = arrange.getPointPosition (first, 0, 1);
    const auto ctrl = juce::ModifierKeys (juce::ModifierKeys::commandModifier);
    arrange.mouseDown (mouseAt (arrange, point, point, ctrl));
    arrange.mouseDrag (mouseAt (arrange, point + juce::Point<float> (6.0f, lane.getHeight() * 0.5f), point, ctrl));
    arrange.mouseUp (mouseAt (arrange, point + juce::Point<float> (6.0f, lane.getHeight() * 0.5f), point, ctrl));
    REQUIRE (studio.getAutomation (first)->at (0).points.size() == 2);
    CHECK (studio.getAutomation (first)->at (0).points[1].beat == 8.0);
    CHECK_THAT (studio.getAutomation (first)->at (0).points[1].value, WithinAbs (fader - 0.5f, 0.02));
    // Undo: back.
    studio.undo();
    studio.dispatchPendingMessages();
    CHECK_THAT (studio.getAutomation (first)->at (0).points[1].value, WithinAbs (fader, 0.01));

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

    // Alt+W: write for all tracks; the header's keys light. (Read All: no default key since 0.31.0, Alt+R renders in place.)
    CHECK (view.keyPressed (juce::KeyPress ('w', juce::ModifierKeys::altModifier, 'w')));
    CHECK (studio.getAutomationWrite (first));
    CHECK (studio.getAutomationWrite (second));
    CHECK (studio.getAutomationWrite (Engine::masterBus));
    CHECK (view.keyPressed (juce::KeyPress ('w', juce::ModifierKeys::altModifier, 'w')));
    CHECK_FALSE (studio.anyAutomationWrite());
    studio.setAutomationReadAll (false);
    CHECK_FALSE (studio.anyAutomationRead());
    // A function key is never taken for a letter: Windows gives F3 the code 0x10072 ('r' with a flag), which upper-cased reads as R.
    const auto tracksBefore = studio.getProject().tracks.size();
    view.keyPressed (juce::KeyPress (0x10000 | 'r', juce::ModifierKeys::altModifier, 0));
    CHECK (studio.getProject().tracks.size() == tracksBefore);   // (not Alt+R: nothing rendered)
    CHECK_FALSE (studio.getEngine().isRecording());   // (not R, which records)
    studio.setAutomationReadAll (true);

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
    CHECK (visibleCount ("REC") == 0);   // (0.9.9: no REC on any row; the Inspector has it)
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
    CHECK (mixer.getStrip (1).getSendSlot (0).isVisible());   // the FX channel (0.60.0: FX channels send too)

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
        down.setCurrentRangeStart (0.0, juce::sendNotificationSync);   // (0.70.0: the new track was brought into view: back to the top)
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
        dragDivider (-20.0f);   // (0.37.0: 20, not 60: the info line took 42 px of the room over it)
        CHECK (mixer.getHeight() == height + 20);
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
        CHECK (visibleNamed ("R") == 1);   // (0.9.9: the tracks' rows have no M S REC R W; the master's R)
        for (int i = 0; i < 12; ++i)
            key ('G', shift);
        CHECK (timeline.rowZoom == Timeline::minRowZoom);
        CHECK (gap() == (float) Timeline::trackHeight * (float) Timeline::minRowZoom);
        CHECK (visibleNamed ("R") == 1);
        for (int i = 0; i < 12; ++i)
            key ('H', shift);
        CHECK (timeline.rowZoom == Timeline::maxRowZoom);
        CHECK (visibleNamed ("R") == 1);
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
    studio.setQuantizeBeats (1.0);   // (0.9.10: snap to the quantize value; a beat, as this test was written for)
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
    studio.setQuantizeBeats (1.0);   // (0.9.10: the grid step is the quantize value: a beat here)
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
    click (x (9.5), juce::ModifierKeys (juce::ModifierKeys::commandModifier | juce::ModifierKeys::altModifier));   // both: round the grid step clicked in (the quantize value, 1/4)
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
    studio.setQuantizeBeats (4.0);   // (0.18.1: drops snap as the arrangement does; 1/1 = the bar)
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
    CHECK (transport.getPositionScreen().getWidth() > 140.0f);   // (0.9.10: POSITION and TIME, two screens alike)
    CHECK (transport.getTimeScreen().getWidth() == transport.getPositionScreen().getWidth());
    CHECK (transport.getTimeScreen().getX() > transport.getPositionScreen().getRight());

    // Three screens.
    CHECK_FALSE (view.isMixerOpen());
    view.setSize (5760, 1000);
    REQUIRE (view.isWide());
    // Turning wide: the mixer docked, its racks hidden (the owner's choice); since 0.71.0 at the
    // default height (the master and five tracks above it, or its least if that's more).
    CHECK (view.isMixerOpen());
    CHECK_FALSE (view.isMixerInWindow());
    CHECK_FALSE (view.getMixer().isRackShown());
    CHECK (view.getMixer().getHeight() >= view.getMixer().getMinimumHeight());
    CHECK (view.getArrangeView().getHeight() <= Timeline::rulerHeight + view.getTimeline().masterRowHeight() + 5 * view.getTimeline().trackRowHeight() + 4);
    // What's changed after it turned wide stays (it isn't done again on every resize).
    view.getMixer().setRackShown (true);
    view.setSize (5700, 1000);
    CHECK (view.getMixer().isRackShown());
    const auto screen = transport.getPositionScreen();
    CHECK (screen.getWidth() + transport.getTimeScreen().getWidth() + 12.0f == (float) TransportBar::widePositionWidth);
    CHECK_THAT (transport.getX() + screen.getRight(), WithinAbs (view.getWidth() * 0.5f, 300.0f));   // near the middle
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
    CHECK (tools.getX() > transport.getX() + (int) transport.getTimeScreen().getRight());
    CHECK (tools.getBottom() <= transport.getBottom());
    // All of it on the middle screen (1920 to 3840): no screen edge through it.
    CHECK (buttonNamed (transport, "|<")->getX() + transport.getX() >= 1920);
    CHECK (tools.getRight() <= 3840);
    CHECK (tools.getY() >= transport.getY());
    // (No tools row: the info line (0.37.0) and the track list's header are all that's over it.)
    CHECK (view.getArrangeView().getY() < transport.getBottom() + MainView::toolbarHeight + (view.isInfoLineShown() ? InfoLine::height + 2 : 0));   // the tracks have the row's room
    // The header's keys: NEW to the quantize value in the middle.
    auto* newKey = buttonNamed (view, "NEW");
    auto& lastKey = view.getQuantizeButton();
    REQUIRE (newKey != nullptr);
    CHECK (std::abs ((newKey->getX() + lastKey.getRight()) / 2 - view.getWidth() / 2) < 4);
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
    SECTION ("An audio track: its input and MON, no MIDI; an FX channel: sends and an output, no REC")
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
        CHECK (inspector.getHeader (S::sends).isVisible());     // (0.60.0: FX channels send too)
        CHECK_FALSE (inspector.getKey ("REC").isVisible());
        CHECK (inspector.getOutput().isEnabled());               // (0.60.0: and go to any group or FX channel)
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
    CHECK (inspector.getEnableKey().getButtonText() == "ENABLE TRACK");
    inspector.getEnableKey().onClick();
    studio.dispatchPendingMessages();
    CHECK_FALSE (studio.getProject().findTrack (lead)->disabled);
    // Enabled: the key stays (0.17.1, the owner asked), now DISABLE TRACK.
    CHECK (inspector.getEnableKey().isVisible());
    CHECK (inspector.getEnableKey().getButtonText() == "DISABLE TRACK");
    inspector.getEnableKey().onClick();
    studio.dispatchPendingMessages();
    CHECK (studio.getProject().findTrack (lead)->disabled);
    inspector.getEnableKey().onClick();
    studio.dispatchPendingMessages();
    CHECK_FALSE (studio.getProject().findTrack (lead)->disabled);

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

TEST_CASE ("Humanize in the MIDI editor: the selected notes loosened, the others untouched, one undo step (0.20.0)", "[ui][editor][humanize]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    Part riff { 0.0, 8.0, "Riff", {} };
    for (int i = 0; i < 32; ++i)
        riff.notes.push_back ({ i * 0.25, 0.2, 40 + i % 12, (float) (i % 2 == 0 ? 44 : 96) / 127.0f });
    studio.addPart (trackId, riff);
    view.getArrangeView().select (trackId, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    auto& editor = view.getKeyEditor();
    REQUIRE (editor.getPart() != nullptr);
    CHECK (editor.getHumanizeKey().getButtonText() == "HUMANIZE 50%");

    CHECK_FALSE (editor.humanizeSelected());   // nothing selected
    std::vector<int> first16;
    for (int i = 0; i < 16; ++i)
        first16.push_back (i);
    editor.selectNotes (first16);
    const auto before = editor.getPart()->notes;
    CHECK (editor.humanizeSelected (1.0f));
    const auto& after = editor.getPart()->notes;
    REQUIRE (after.size() == before.size());
    auto changed = 0;
    for (size_t i = 0; i < after.size(); ++i)
    {
        if (i >= 16)
            CHECK (after[i] == before[i]);   // not selected: untouched
        else
            changed += after[i] != before[i] ? 1 : 0;
    }
    CHECK (changed > 8);
    // Layers kept: a palm mute (44) stays under 60, a sustain (96) stays 60-119; pitches the same set.
    std::multiset<int> pitchesBefore, pitchesAfter;
    for (size_t i = 0; i < 16; ++i)
    {
        pitchesBefore.insert (before[i].pitch);
        pitchesAfter.insert (after[i].pitch);
        const auto v = juce::roundToInt (after[i].velocity * 127.0f);
        CHECK (((v <= 59) || (v >= 60 && v <= 119)));
        CHECK (std::abs (after[i].start - before[i].start) <= 12.0 * 120.0 / 60000.0 + 1.0e-9);
    }
    CHECK (pitchesBefore == pitchesAfter);
    CHECK (editor.getSelectedNotes().size() == 16);   // still selected: press again to loosen further
    // One undo step.
    studio.undo();
    CHECK (editor.getPart()->notes == before);
}

TEST_CASE ("Random velocities: Alt+B / N / M give the selected notes velocities within LOW / MID / HIGH", "[ui][editor][velocity]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    Part riff { 0.0, 8.0, "Riff", {} };
    for (int i = 0; i < 32; ++i)
        riff.notes.push_back ({ i * 0.25, 0.25, 60 + i % 12, 0.5f });
    studio.addPart (trackId, riff);
    view.getArrangeView().select (trackId, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    auto& editor = view.getKeyEditor();
    REQUIRE (editor.getPart() != nullptr);

    // The ranges at first, on the keys.
    CHECK (editor.getVelocityRange (0) == std::pair<int, int> { 40, 70 });
    CHECK (editor.getVelocityRange (1) == std::pair<int, int> { 70, 100 });
    CHECK (editor.getVelocityRange (2) == std::pair<int, int> { 100, 127 });
    CHECK (editor.getVelocityKey (0).getButtonText() == "LOW 40-70");

    // Nothing selected: nothing changes.
    CHECK_FALSE (editor.randomizeVelocities (0));
    for (const auto& n : editor.getPart()->notes)
        CHECK (n.velocity == 0.5f);

    // The first 16 selected, HIGH: within 100-127, and not all the same; the others untouched.
    std::vector<int> first16;
    for (int i = 0; i < 16; ++i)
        first16.push_back (i);
    editor.selectNotes (first16);
    CHECK (editor.keyPressed (juce::KeyPress ('m', juce::ModifierKeys::altModifier, 0)));
    std::set<int> seen;
    for (int i = 0; i < 32; ++i)
    {
        const auto v = juce::roundToInt (editor.getPart()->notes[(size_t) i].velocity * 127.0f);
        if (i < 16)
        {
            CHECK (v >= 100);
            CHECK (v <= 127);
            seen.insert (v);
        }
        else
            CHECK (editor.getPart()->notes[(size_t) i].velocity == 0.5f);
    }
    CHECK (seen.size() > 1);
    CHECK (editor.getSelectedNotes().size() == 16);   // still selected: again for another roll
    studio.undo();   // one undo step
    CHECK (editor.getPart()->notes[0].velocity == 0.5f);

    // A range set (the wrong way round, and past the ends): put right, kept on the key; Alt+B from the main window.
    editor.setVelocityRange (0, 200, 5);
    CHECK (editor.getVelocityRange (0) == std::pair<int, int> { 5, 127 });
    editor.setVelocityRange (0, 20, 30);
    CHECK (editor.getVelocityKey (0).getButtonText() == "LOW 20-30");
    editor.selectNotes ({ 3, 4, 5 });
    CHECK (view.keyPressed (juce::KeyPress ('b', juce::ModifierKeys::altModifier, 0)));
    for (const auto i : { 3, 4, 5 })
    {
        const auto v = juce::roundToInt (editor.getPart()->notes[(size_t) i].velocity * 127.0f);
        CHECK (v >= 20);
        CHECK (v <= 30);
    }
}

TEST_CASE ("The Inspector's QUICK CONTROLS: 8 slots; an assigned one has a bar that moves its control", "[ui][inspector][quick]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto track = studio.getProject().tracks[0].id;
    studio.selectTrack (track);
    studio.dispatchPendingMessages();
    auto& inspector = view.getInspector();
    CHECK (inspector.getHeader (Inspector::Section::quick).isVisible());
    CHECK (inspector.getQuickControlKey (0).isVisible());
    CHECK (inspector.getQuickControlKey (0).getButtonText() == "QC 1");
    CHECK_FALSE (inspector.getQuickControlBar (0).isVisible());

    const auto controls = studio.getAutomationControls (track);
    studio.setQuickControl (track, 0, controls.front());   // volume
    studio.dispatchPendingMessages();
    CHECK (inspector.getQuickControlBar (0).isVisible());
    CHECK (inspector.getQuickControlKey (0).getButtonText() != "QC 1");
    inspector.getQuickControlBar (0).setValue (pr7s::volumeToTravel (-3.0f), juce::sendNotificationSync);
    CHECK (std::abs (studio.getProject().findTrack (track)->volumeDb + 3.0f) < 0.05f);
}

TEST_CASE ("Track presets on screen: RENDER (0.17.1, in FREEZE's place) and PRESET; a preset file round trip", "[ui][freeze][preset]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& inspector = view.getInspector();
    const auto synth = studio.getProject().tracks[0].id;
    studio.selectTrack (synth);
    studio.dispatchPendingMessages();
    auto& render = inspector.getKey ("RENDER");
    auto& preset = inspector.getKey ("PRESET");
    CHECK (render.isVisible());
    CHECK (preset.isVisible());
    CHECK_FALSE (render.isEnabled());   // (no parts: nothing to render)
    CHECK (inspector.getKey ("REC").isVisible());
    studio.addPart (synth, { 0.0, 4.0, "Riff", { { 0.0, 1.0, 60, 1.0f } } });
    studio.dispatchPendingMessages();
    CHECK (render.isEnabled());
    // RENDER asks for Render in Place of this track (the screen runs it as last time).
    auto onRender = inspector.onRender;
    int asked = 0;
    inspector.onRender = [&asked] (int id) { asked = id; };
    render.onClick();
    CHECK (asked == synth);
    inspector.onRender = onRender;

    // Frozen (from the track's menu): RENDER off (it plays its file), and so is DISABLE.
    juce::String error;
    REQUIRE (TrackActions::freezeSelected (studio, 0.0, error));
    studio.dispatchPendingMessages();
    CHECK (studio.isFrozen (synth));
    CHECK_FALSE (render.isEnabled());
    CHECK_FALSE (inspector.getEnableKey().isEnabled());
    TrackActions::unfreezeSelected (studio, true);
    studio.dispatchPendingMessages();
    CHECK (render.isEnabled());

    // A group channel has neither; an FX channel only PRESET; neither can be disabled.
    studio.selectTrack (studio.addGroupChannel());
    studio.dispatchPendingMessages();
    CHECK_FALSE (render.isVisible());
    CHECK_FALSE (preset.isVisible());
    CHECK_FALSE (inspector.getEnableKey().isVisible());
    studio.selectTrack (studio.addFxChannel (nullptr));
    studio.dispatchPendingMessages();
    CHECK_FALSE (render.isVisible());
    CHECK (preset.isVisible());

    // The preset file: saved, then a new track from it.
    studio.setVolume (synth, -3.0f);
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-test.pr7track");
    REQUIRE (TrackActions::savePresetTo (studio, synth, file));
    const auto made = TrackActions::addTrackFrom (studio, file, error);
    REQUIRE (made != 0);
    CHECK (studio.getProject().findTrack (made)->volumeDb == -3.0f);
    CHECK (TrackActions::addTrackFrom (studio, juce::File ("/nowhere/none.pr7track"), error) == 0);
    file.deleteFile();
    studio.getAudioFolder().deleteRecursively();
}

TEST_CASE ("Typing a value (0.9.9): dB and pan read from text; the mixer's fader and pan, the Inspector's numbers, the track's knob", "[ui][entry]")
{
    using ValueEntry::parseDb, ValueEntry::parsePan;
    CHECK (parseDb ("-6").value() == -6.0);
    CHECK (parseDb ("+3 dB").value() == 3.0);
    CHECK (parseDb (" -12,5db ").value() == -12.5);
    CHECK (parseDb ("-inf").value() == -60.0);
    CHECK (parseDb ("20").value() == 6.0);      // to the fader's top
    CHECK_FALSE (parseDb ("loud").has_value());
    CHECK_FALSE (parseDb ("").has_value());
    CHECK (parsePan ("C").value() == 0.0);
    CHECK (parsePan ("L30").value() == -0.3);
    CHECK (parsePan ("30 L").value() == -0.3);
    CHECK (parsePan ("R 15").value() == 0.15);
    CHECK (parsePan ("-50").value() == -0.5);
    CHECK (parsePan ("100").value() == 1.0);
    CHECK_FALSE (parsePan ("left").has_value());

    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto track = studio.getProject().tracks[0].id;
    view.openMixer();
    studio.selectTrack (track);
    studio.dispatchPendingMessages();

    // The mixer: typed, set (one undo step); nonsense refused, nothing changed.
    auto& strip = view.getMixer().getStrip (0);
    CHECK (strip.getVolume().enter ("-7.5"));
    CHECK (studio.getProject().findTrack (track)->volumeDb == -7.5f);
    CHECK_FALSE (strip.getVolume().enter ("loud"));
    CHECK (studio.getProject().findTrack (track)->volumeDb == -7.5f);
    studio.undo();
    CHECK (studio.getProject().findTrack (track)->volumeDb == 0.0f);
    CHECK (strip.getPan().enter ("L25"));
    CHECK_THAT (studio.getProject().findTrack (track)->pan, WithinAbs (-0.25, 0.001));
    // The master's fader too.
    auto& master = view.getMixer().getStrip (view.getMixer().getNumStrips() - 1);
    CHECK (master.getVolume().enter ("-3"));
    CHECK (studio.getProject().masterVolumeDb == -3.0f);

    // The Inspector's numbers: double-click and type (the label's editor).
    auto& inspector = view.getInspector();
    auto& readout = inspector.getVolumeReadout();
    CHECK (readout.isEditableOnDoubleClick());
    readout.setText ("+2", juce::sendNotificationSync);
    CHECK (studio.getProject().findTrack (track)->volumeDb == 2.0f);
    CHECK (readout.getText().startsWith ("+2.0"));
    readout.setText ("nonsense", juce::sendNotificationSync);
    CHECK (studio.getProject().findTrack (track)->volumeDb == 2.0f);
    CHECK (readout.getText().startsWith ("+2.0"));   // put back as it is
    inspector.getPanReadout().setText ("R40", juce::sendNotificationSync);
    CHECK_THAT (studio.getProject().findTrack (track)->pan, WithinAbs (0.4, 0.001));
}

TEST_CASE ("Track rows without M S REC R W (0.9.9): the state written under the name; the header's Deactivate All Mute / Solo keys", "[ui][solo]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    studio.dispatchPendingMessages();
    auto& list = view.getTrackList();
    auto& muteAll = list.getMuteAllKey();
    auto& soloAll = list.getSoloAllKey();
    CHECK (muteAll.isVisible());
    CHECK (soloAll.isVisible());
    CHECK_FALSE (soloAll.getToggleState());
    studio.setSolo (a, true);
    studio.setMute (b, true);
    studio.dispatchPendingMessages();
    CHECK (soloAll.getToggleState());
    CHECK (muteAll.getToggleState());
    soloAll.onClick();
    muteAll.onClick();
    studio.dispatchPendingMessages();
    CHECK_FALSE (studio.anySolo());
    CHECK_FALSE (studio.anyMute());
    CHECK_FALSE (soloAll.getToggleState());
}

namespace
{
    // An instrument that counts how often it's asked whether it has a window (a VST3 builds its view to answer).
    struct AskedForEditor final : TestPlugin
    {
        AskedForEditor() : TestPlugin (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) { instrument = true; }
        bool hasEditor() const override { ++asked; return true; }
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { buffer.clear(); }
        mutable int asked = 0;
    };
}

TEST_CASE ("A plugin is asked once whether it has a window (0.9.9: asked after every change, Kontakt made each click stick)", "[ui][perf-fix]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto track = studio.getProject().tracks[0].id;
    auto plugin = std::make_unique<AskedForEditor>();
    auto* asked = plugin.get();
    studio.getEngine().setInstrument (track, std::move (plugin));
    studio.selectTrack (track);
    studio.dispatchPendingMessages();
    for (int i = 0; i < 10; ++i)
    {
        studio.setPosition (i * 2.0);
        studio.setCycleOn (i % 2 == 0);
        studio.dispatchPendingMessages();
    }
    CHECK (asked->asked == 1);
    CHECK (view.getInspector().getKey ("M").isVisible());
}

TEST_CASE ("Typing a value: the box opens on the fader with its value, Enter sets it, Esc leaves it", "[ui][entry]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    view.setVisible (true);
    view.addToDesktop (0);
    const auto track = studio.getProject().tracks[0].id;
    view.openMixer();
    studio.dispatchPendingMessages();
    auto& fader = view.getMixer().getStrip (0).getVolume();
    const auto findBox = []
    {
        juce::TextEditor* editor = nullptr;
        for (int i = 0; i < juce::Desktop::getInstance().getNumComponents(); ++i)
            if (auto* box = dynamic_cast<juce::CallOutBox*> (juce::Desktop::getInstance().getComponent (i)); box != nullptr && box->isVisible())
            {
                std::vector<juce::Component*> todo { box };
                while (! todo.empty() && editor == nullptr)
                {
                    auto* c = todo.back();
                    todo.pop_back();
                    editor = dynamic_cast<juce::TextEditor*> (c);
                    for (auto* child : c->getChildren())
                        todo.push_back (child);
                }
            }
        return editor;
    };
    fader.mouseDoubleClick (mouseAt (fader, { 4.0f, 4.0f }, { 4.0f, 4.0f }));
    studio.dispatchPendingMessages();
    auto* editor = findBox();
    REQUIRE (editor != nullptr);
    CHECK (editor->getText() == "0.0 dB");
    CHECK (studio.getProject().findTrack (track)->volumeDb == 0.0f);   // a double-click no longer jumps the fader
    editor->setText ("-9");
    editor->onReturnKey();
    studio.dispatchPendingMessages();
    CHECK (studio.getProject().findTrack (track)->volumeDb == -9.0f);
    CHECK (findBox() == nullptr);   // closed
    view.removeFromDesktop();
}

TEST_CASE ("Snap and the quantize value (0.9.10): the header's SNAP (J) and 1/1 to 1/64; parts and the position snap to it, or not; saved with the song", "[ui][snap]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& snap = view.getSnapButton();
    auto& quantize = view.getQuantizeButton();
    studio.dispatchPendingMessages();
    CHECK (snap.isVisible());
    CHECK (quantize.isVisible());
    CHECK (snap.getToggleState());              // on by default
    CHECK (quantize.getButtonText() == "1/16");
    // The INSPECTOR, KEY EDITOR and MIXER keys are gone (F1, F2, F3).
    for (const auto* name : { "MIXER", "INSPECTOR", "KEY EDITOR" })
    {
        const auto* gone = buttonNamed (view, name);
        CHECK ((gone == nullptr || ! gone->isVisible()));
    }

    auto& arrange = view.getArrangeView();
    auto& timeline = view.getTimeline();
    const auto& project = studio.getProject();
    CHECK (timeline.snapGrid (project) == 0.25);
    studio.setQuantizeBeats (1.0);              // 1/4
    studio.dispatchPendingMessages();
    CHECK (quantize.getButtonText() == "1/4");
    CHECK (timeline.snapGrid (project) == 1.0);
    // J: off; nothing snaps; the step (Ctrl+Alt in the ruler) is still the quantize value.
    CHECK (view.keyPressed (juce::KeyPress ('j')));
    studio.dispatchPendingMessages();
    CHECK_FALSE (project.snapOn);
    CHECK_FALSE (snap.getToggleState());
    CHECK (timeline.snapGrid (project) == 0.0);
    CHECK (timeline.grid (project) == 1.0);
    snap.onClick();
    CHECK (project.snapOn);
    studio.setQuantizeBeats (4.0 / 64.0);
    studio.dispatchPendingMessages();
    CHECK (quantize.getButtonText() == "1/64");
    juce::ignoreUnused (arrange);

    // Saved and opened again.
    studio.setSnap (false);
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-snap.pr7s");
    REQUIRE (studio.save (file));
    Studio other (false);
    juce::String problems;
    REQUIRE (other.open (file, problems));
    CHECK_FALSE (other.getProject().snapOn);
    CHECK (other.getProject().quantizeBeats == 4.0 / 64.0);
    file.deleteFile();
}

TEST_CASE ("Velocity handles (0.9.10, the manual's Controller Event Editor): a box round the selected notes' bars; move, scale, tilt, squash; one undo step", "[ui][editor][velocity]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    view.setKeyEditorSize (100);
    const auto trackId = studio.getProject().tracks[0].id;
    Part riff { 0.0, 8.0, "Riff", {} };
    for (int i = 0; i < 4; ++i)
        riff.notes.push_back ({ i * 1.0, 0.5, 60, 0.3f + 0.1f * (float) i });   // 0.3, 0.4, 0.5, 0.6
    studio.addPart (trackId, riff);
    view.getArrangeView().select (trackId, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    auto& editor = view.getKeyEditor();
    REQUIRE (editor.getPart() != nullptr);
    const auto velocities = [&]
    {
        std::vector<float> v;
        for (const auto& n : editor.getPart()->notes)
            v.push_back (n.velocity);
        return v;
    };
    const auto lane = editor.getVelocityArea().reduced (0.0f, 6.0f);
    const auto unit = lane.getHeight();   // pixels for the whole of 0 to 1

    // One note selected: no box.
    editor.selectNotes ({ 0 });
    CHECK (editor.getVelocityBox().isEmpty());
    // All four: a box from the first bar to the last, up to the loudest.
    editor.selectNotes ({ 0, 1, 2, 3 });
    const auto box = editor.getVelocityBox();
    REQUIRE_FALSE (box.isEmpty());
    CHECK_THAT (box.getY(), WithinAbs (lane.getBottom() - unit * 0.6f, 0.5));
    CHECK (box.getBottom() == lane.getBottom());
    using H = KeyEditor::VelocityHandle;
    CHECK (editor.velocityHandleAt ({ box.getX() + box.getWidth() * 0.3f, box.getY() }) == H::move);
    CHECK (editor.velocityHandleAt ({ box.getCentreX(), box.getY() }) == H::scale);
    CHECK (editor.velocityHandleAt (box.getTopLeft()) == H::tiltLeft);
    CHECK (editor.velocityHandleAt (box.getTopRight()) == H::tiltRight);
    CHECK (editor.velocityHandleAt ({ box.getRight(), box.getCentreY() }) == H::scaleCentre);
    CHECK (editor.velocityHandleAt (box.getCentre().translated (0.0f, 10.0f)) == H::none);

    const auto dragBy = [&] (juce::Point<float> from, float dy)
    {
        const auto to = from.translated (0.0f, dy);
        editor.mouseDown (mouseAt (editor, from, from));
        for (int step = 1; step <= 4; ++step)
            editor.mouseDrag (mouseAt (editor, from + (to - from) * ((float) step / 4.0f), from));
        editor.mouseUp (mouseAt (editor, to, from));
        studio.dispatchPendingMessages();
    };
    const auto close = [] (std::vector<float> got, std::vector<float> want)
    {
        REQUIRE (got.size() == want.size());
        for (size_t i = 0; i < got.size(); ++i)
            CHECK_THAT (got[i], WithinAbs (want[i], 0.012));
    };

    // Move: all up by a tenth.
    dragBy ({ box.getX() + box.getWidth() * 0.3f, box.getY() }, -unit * 0.1f);
    close (velocities(), { 0.4f, 0.5f, 0.6f, 0.7f });
    // One undo step puts them back.
    studio.undo();
    studio.dispatchPendingMessages();
    close (velocities(), { 0.3f, 0.4f, 0.5f, 0.6f });

    // Scale (from 0): the loudest 0.6 to 0.3 halves them all.
    editor.selectNotes ({ 0, 1, 2, 3 });
    dragBy ({ editor.getVelocityBox().getCentreX(), editor.getVelocityBox().getY() }, unit * 0.3f);
    close (velocities(), { 0.15f, 0.2f, 0.25f, 0.3f });
    studio.undo();
    studio.dispatchPendingMessages();

    // Tilt right: the right end up by 0.2, the left end stays.
    editor.selectNotes ({ 0, 1, 2, 3 });
    const auto b = editor.getVelocityBox();
    dragBy (b.getTopRight(), -unit * 0.2f);
    const auto tilted = velocities();
    CHECK_THAT (tilted[0], WithinAbs (0.3f + 0.2f * (2.0f + 6.0f) / b.getWidth() * 0.0f + 0.2f * ((editor.xForBeat (0.0) + 2.0f - b.getX()) / b.getWidth()), 0.012));
    CHECK (tilted[3] > 0.6f + 0.15f);
    CHECK (tilted[1] - 0.4f < tilted[3] - 0.6f);   // more on the right
    studio.undo();
    studio.dispatchPendingMessages();

    // Squash towards the middle (0.45): dragging the right edge's handle down by a quarter halves the spread.
    editor.selectNotes ({ 0, 1, 2, 3 });
    const auto c = editor.getVelocityBox();
    dragBy ({ c.getRight(), c.getCentreY() }, unit * 0.25f);
    close (velocities(), { 0.375f, 0.425f, 0.475f, 0.525f });

    // Never past 1 or below 1/127.
    editor.selectNotes ({ 0, 1, 2, 3 });
    const auto d = editor.getVelocityBox();
    dragBy ({ d.getX() + d.getWidth() * 0.3f, d.getY() }, -unit * 2.0f);
    for (const auto v : velocities())
        CHECK (v == 1.0f);
}

TEST_CASE ("0.9.11: SPLIT's cut line leaves nothing behind; Add Track in the track menu; saving in the background; the zone window across the screens", "[ui][v0911]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 0.0, 16.0, "Riff", { { 0.0, 1.0, 60, 0.8f } } });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();

    // The cut line is drawn on the grid; its strip is where it's drawn, not where the mouse is.
    arrange.setTool (ArrangeView::Tool::split);
    const auto bounds = arrange.getPartBounds (track, 0);
    const auto at = juce::Point<float> (bounds.getX() + bounds.getWidth() * 0.37f, bounds.getCentreY());
    arrange.mouseMove (mouseAt (arrange, at, at));
    const auto line = arrange.splitLineX();
    REQUIRE (line >= 0.0f);
    const auto beat = view.getTimeline().beatForX (line);
    CHECK_THAT (beat / 0.25, WithinAbs (std::round (beat / 0.25), 1.0e-6));   // on the 1/16 grid
    const auto away = juce::Point<float> (bounds.getRight() + 50.0f, bounds.getBottom() + 200.0f);
    arrange.mouseMove (mouseAt (arrange, away, away));
    CHECK (arrange.splitLineX() < 0.0f);

    // Add Track, from the track menu's ids (offset) or the empty space's menu.
    auto& list = view.getTrackList();
    const auto before = studio.getProject().tracks.size();
    CHECK (list.addTrackChosen (1));                 // the studio's synth
    CHECK (list.addTrackChosen (3));                 // a mono audio track
    CHECK (studio.getProject().tracks.size() == before + 2);
    CHECK (studio.getProject().tracks.back().isAudio());
    CHECK_FALSE (list.getAddTrackButton().isVisible());
    // The menu's items carry the offset (they sit in the track's own menu).
    auto menu = list.makeAddTrackMenu (TrackList::addTrackOffset);
    auto ids = 0, lowest = std::numeric_limits<int>::max();
    for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
        if (it.getItem().itemID != 0)
        {
            ++ids;
            lowest = juce::jmin (lowest, it.getItem().itemID);
        }
    CHECK (ids >= 6);
    CHECK (lowest > TrackList::addTrackOffset);
    // The studio's synth: not looked for in tests (the old synth plays them, the same everywhere).
    CHECK_FALSE (studio.getPluginHost().isStudioSynthFound());
    CHECK (pr7s::Instrument {}.getName() == "PR7-K");

    // Saving in the background: marked saved at once; the file there once the thread has written it.
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-background.pr7s");
    file.deleteFile();
    std::atomic<bool> done { false };
    bool written = false;
    studio.saveInBackground (file, [&] (bool ok) { written = ok; done = true; });
    CHECK_FALSE (studio.isModified());
    CHECK (studio.getFile() == file);
    CHECK (studio.waitForSaves (5000));   // (the thread's done; its call back comes by message, which tests can't run)
    juce::ignoreUnused (written);
    CHECK (file.existsAsFile());
    Studio other (false);
    juce::String problems;
    CHECK (other.open (file, problems));
    CHECK (other.getProject().tracks.size() == studio.getProject().tracks.size());
    file.deleteFile();
}

TEST_CASE ("0.9.12: the master's row at the top and its Inspector; the track menu A to Z; notes coloured by velocity; plugin windows pass the studio's keys", "[ui][v0912]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto first = studio.getProject().tracks[0].id;
    studio.dispatchPendingMessages();

    // The master's row is the very top one.
    const auto rows = view.getTimeline().layout (studio);
    REQUIRE_FALSE (rows.empty());
    CHECK (rows.front().kind == Timeline::Row::Kind::master);

    // Choosing it: the Inspector shows the master (R W, volume, inserts, fader); a track again: the track.
    auto& inspector = view.getInspector();
    studio.selectMaster();
    studio.dispatchPendingMessages();
    CHECK (studio.isMasterSelected());
    CHECK (studio.getSelectedTracks().empty());
    CHECK (inspector.isShowingMaster());
    CHECK (inspector.getKey ("R").isVisible());
    CHECK_FALSE (inspector.getKey ("M").isVisible());
    CHECK_FALSE (inspector.getKey ("REC").isVisible());
    CHECK (inspector.getVolume().isVisible());
    CHECK_FALSE (inspector.getPan().isVisible());
    CHECK (inspector.getInsertsView().isVisible());
    CHECK (inspector.getVolume().enter ("-4"));
    CHECK (studio.getProject().masterVolumeDb == -4.0f);
    CHECK (studio.getProject().tracks[0].volumeDb == 0.0f);   // (not a track's)
    studio.selectTrack (first);
    studio.dispatchPendingMessages();
    CHECK_FALSE (studio.isMasterSelected());
    CHECK_FALSE (inspector.isShowingMaster());
    CHECK (inspector.getKey ("M").isVisible());

    // The track menu's order: A to Z, no separators.
    juce::PopupMenu menu;
    menu.addItem (1, "Show Automation");
    menu.addSeparator();
    menu.addItem (2, "Delete Track");
    menu.addSectionHeader ("SOMETHING");
    menu.addItem (3, "add track");
    menu.addItem (4, "Hide Track");
    juce::StringArray order;
    const auto sorted = TrackList::alphabetical (menu);
    for (juce::PopupMenu::MenuItemIterator it (sorted); it.next();)
        order.add (it.getItem().text);
    CHECK (order.joinIntoString ("|") == "add track|Delete Track|Hide Track|Show Automation");

    // The MIDI editor's notes: LOW (and under) orange, MID pink, HIGH red (the random velocity ranges' edges: 70 and 100).
    auto& editor = view.getKeyEditor();
    CHECK (editor.velocityColour (20.0f / 127.0f) == pr7s::FamilyColours::orange);
    CHECK (editor.velocityColour (69.0f / 127.0f) == pr7s::FamilyColours::orange);
    CHECK (editor.velocityColour (70.0f / 127.0f) == pr7s::FamilyColours::pink);
    CHECK (editor.velocityColour (99.0f / 127.0f) == pr7s::FamilyColours::pink);
    CHECK (editor.velocityColour (100.0f / 127.0f) == pr7s::FamilyColours::red);
    CHECK (editor.velocityColour (1.0f) == pr7s::FamilyColours::red);

    // A plugin's window: the keys it doesn't use go to the studio (Space plays).
    SteadyTone plugin (0.1f, 0.1f);
    Windows::Instrument window ("Keys", plugin);
    auto heard = 0;
    window.onKey = [&] (const juce::KeyPress&) { ++heard; return true; };
    CHECK (window.keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    CHECK (heard == 1);
}

TEST_CASE ("The track menu tells its instruments from its outputs (0.17.3: Change instrument did nothing)", "[ui]")
{
    // An output: 30000 + a group's id; an instrument: from instrumentItems on.
    STATIC_CHECK (TrackList::isOutputItem (30000 + 5));
    STATIC_CHECK_FALSE (TrackList::isOutputItem (TrackList::instrumentItems));
    STATIC_CHECK_FALSE (TrackList::isOutputItem (TrackList::instrumentItems + 12));
    STATIC_CHECK_FALSE (TrackList::isOutputItem (30));
    STATIC_CHECK_FALSE (TrackList::isOutputItem (20000 + 3));   // (link groups)
}

#include "ui/TrackColours.h"

TEST_CASE ("The colour menu: only the family's colours, every tint; the selected tracks coloured; default back; saved (0.17.3)", "[ui][colour]")
{
    using namespace pr7s;
    // The palette: red leads every swatch (nothing green, blue, violet or yellow), deep to pale.
    for (int h = 0; h < FamilyColours::paletteHues; ++h)
        for (int s = 0; s < FamilyColours::paletteShades; ++s)
        {
            const auto c = FamilyColours::paletteColour (h, s);
            INFO ("hue " << h << ", shade " << s << ": " << c.toString());
            CHECK (c.getRed() >= c.getGreen());
            CHECK (c.getRed() >= c.getBlue());
            const auto hue = c.getHue() * 360.0f;
            CHECK ((hue >= 340.0f || hue <= 30.0f));   // (from the pink to the orange)
            CHECK (FamilyColours::isFamily (c));
            if (s > 0)
                CHECK (c.getPerceivedBrightness() > FamilyColours::paletteColour (h, s - 1).getPerceivedBrightness());
        }
    CHECK (FamilyColours::isFamily (FamilyColours::orange));
    CHECK_FALSE (FamilyColours::isFamily (juce::Colours::blue));

    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id, b = studio.addTrack ({}), c = studio.addTrack ({});
    studio.selectTrack (a);
    studio.toggleTrackSelected (c);
    TrackColourMenu menu (studio, studio.getSelectedTracks());
    menu.resized();
    const auto pale = FamilyColours::paletteColour (7, 6);
    const auto at = menu.swatchFor (pale).getCentre();
    REQUIRE (menu.colourAt (at) == pale);
    menu.mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                      &menu, &menu, juce::Time::getCurrentTime(), at, juce::Time::getCurrentTime(), 1, false));
    CHECK (studio.getProject().findTrack (a)->colour == pale);
    CHECK (studio.getProject().findTrack (c)->colour == pale);
    CHECK (studio.getProject().findTrack (b)->colour == trackColour (1));   // (not selected)
    // Kept when saved and opened (a tint of the family's, not mapped to the three).
    const auto opened = Project::fromXml (*studio.getProject().toXml());
    REQUIRE (opened.has_value());
    CHECK (opened->findTrack (a)->colour == pale);
    // Undo, then SET TRACK COLOUR TO DEFAULT: the automatic colour by its place.
    studio.undo();
    CHECK (studio.getProject().findTrack (a)->colour == trackColour (0));
    studio.setTracksColour ({ c }, FamilyColours::paletteColour (0, 0));
    menu.defaultKey.onClick();
    CHECK (studio.getProject().findTrack (c)->colour == trackColour (2));
    // Not the family's: the nearest of the three instead.
    studio.setTracksColour ({ b }, juce::Colour (0xffff7aa8));
    CHECK (studio.getProject().findTrack (b)->colour == FamilyColours::pink);
}

TEST_CASE ("The playhead glides into a new place while stopped, exact while playing; the ruler shows where a click lands (0.17.4)", "[ui][playhead]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    pr7s::Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& arrange = view.getArrangeView();
    for (int i = 0; i < 20; ++i)
        arrange.advancePlayhead();
    CHECK (arrange.getShownPlayhead() == 0.0);

    // Stopped: a click a bar on (as the ruler does it, snapped) glides: part of the way each frame, then there.
    studio.setPosition (4.0);
    arrange.advancePlayhead();
    const auto first = arrange.getShownPlayhead();
    CHECK (first > 0.0);
    CHECK (first < 4.0);
    CHECK_THAT (first, Catch::Matchers::WithinAbs (4.0 * ArrangeView::glideStep, 1.0e-9));
    arrange.advancePlayhead();
    CHECK (arrange.getShownPlayhead() > first);
    for (int i = 0; i < 20; ++i)
        arrange.advancePlayhead();
    CHECK (arrange.getShownPlayhead() == 4.0);   // (landed exactly)

    // A long jump (more than half the view): straight there.
    studio.setPosition (400.0);
    arrange.advancePlayhead();
    CHECK (arrange.getShownPlayhead() == 400.0);

    // Playing: exactly with the song, no glide.
    studio.setPosition (8.0);
    studio.play();
    arrange.advancePlayhead();
    CHECK (arrange.getShownPlayhead() == studio.getEngine().getPosition());
    studio.stop();

    // Hovering the ruler's lower part: the snapped beat a click would set (the grid: the quantize value, 1/16 at first).
    const auto y = (float) (Timeline::cycleStripHeight + 6);
    const auto x = view.getTimeline().xForBeat (2.13);
    arrange.mouseMove (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { x, y }, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         &arrange, &arrange, juce::Time::getCurrentTime(), { x, y }, juce::Time::getCurrentTime(), 0, false));
    REQUIRE (arrange.getRulerHover().has_value());
    CHECK_THAT (*arrange.getRulerHover(), Catch::Matchers::WithinAbs (pr7s::snap (2.13, view.getTimeline().snapGrid (studio.getProject())), 1.0e-9));
    arrange.mouseExit (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { x, y }, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         &arrange, &arrange, juce::Time::getCurrentTime(), { x, y }, juce::Time::getCurrentTime(), 0, false));
    CHECK_FALSE (arrange.getRulerHover().has_value());
}

TEST_CASE ("The MIDI editor's playhead glides as the arrangement's (0.17.5)", "[ui][playhead]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    pr7s::Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 0.0, 16.0, "Riff", { { 0.0, 1.0, 60, 0.8f } } });
    view.setKeyEditorSize (100);
    view.getArrangeView().select (track, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    auto& editor = view.getKeyEditor();
    editor.advancePlayhead();
    CHECK (editor.getShownPlayhead() == 0.0);
    studio.setPosition (2.0);
    editor.advancePlayhead();
    CHECK (editor.getShownPlayhead() > 0.0);
    CHECK (editor.getShownPlayhead() < 2.0);
    for (int i = 0; i < 30; ++i)
        editor.advancePlayhead();
    CHECK (editor.getShownPlayhead() == 2.0);
    studio.play();
    editor.advancePlayhead();
    CHECK (editor.getShownPlayhead() == studio.getEngine().getPosition());
    studio.stop();
}

TEST_CASE ("A MIDI file dropped on an instrument track lands on it, snapped, shown while dragged (0.18.1)", "[ui][drop]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    pr7s::Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto synth = studio.getProject().tracks[0].id;
    const auto audio = studio.addAudioTrack (false);
    studio.dispatchPendingMessages();

    // A two-bar MIDI file (one track, 120 BPM, as the song).
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0.0);
    seq.addEvent (juce::MidiMessage::noteOff (1, 60), 480.0);
    seq.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 1920.0);
    seq.addEvent (juce::MidiMessage::noteOff (1, 64), 2400.0);
    seq.updateMatchedPairs();
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (480);
    mf.addTrack (seq);
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-drop.mid");
    file.deleteFile();
    {
        juce::FileOutputStream out (file);
        REQUIRE (mf.writeTo (out));
    }
    auto& arrange = view.getArrangeView();
    const auto pointOver = [&] (int trackId, double beat)
    {
        const auto at = juce::Point<float> (view.getTimeline().xForBeat (beat), arrange.trackTop (trackId) + 10.0f);
        return view.getLocalPoint (&arrange, at.toInt());
    };

    // Snap on, 1/1: whole bars. Dragged over the synth track at beat 5.3: shown at bar 2 (beat 4).
    studio.setSnap (true);
    studio.setQuantizeBeats (4.0);
    const juce::StringArray files { file.getFullPathName() };
    auto p = pointOver (synth, 5.3);
    view.fileDragEnter (files, p.x, p.y);
    REQUIRE (arrange.getDropPreview().has_value());
    CHECK (arrange.getDropPreview()->trackId == synth);
    CHECK (arrange.getDropPreview()->start == 4.0);
    CHECK (arrange.getDropPreview()->length == 8.0);
    CHECK (arrange.getDropPreview()->notes.size() == 2);   // (0.18.2: its notes drawn inside)
    // The strength (0.18.2): the Hub's setting, 60% at first, kept between 10% and 100%.
    CHECK (studio.getDropHighlight() == pr7s::Studio::defaultDropHighlight);
    studio.setDropHighlight (2.0f);
    CHECK (studio.getDropHighlight() == 1.0f);
    studio.setDropHighlight (0.3f);
    CHECK (std::abs (studio.getDropHighlight() - 0.3f) < 1.0e-6f);
    // Moved on to 6.4: the next whole note (bar 3, beat 8).
    p = pointOver (synth, 6.4);
    view.fileDragMove (files, p.x, p.y);
    CHECK (arrange.getDropPreview()->start == 8.0);
    // Over the audio track: nothing shown (it'd go on a new track).
    p = pointOver (audio, 6.4);
    view.fileDragMove (files, p.x, p.y);
    CHECK_FALSE (arrange.getDropPreview().has_value());

    // Dropped on the synth track: there, not a new track (0.18.0 made a new one).
    const auto tracksBefore = studio.getProject().tracks.size();
    p = pointOver (synth, 6.4);
    view.fileDragMove (files, p.x, p.y);
    view.filesDropped (files, p.x, p.y);
    CHECK_FALSE (arrange.getDropPreview().has_value());
    CHECK (studio.getProject().tracks.size() == tracksBefore);
    const auto* track = studio.getProject().findTrack (synth);
    REQUIRE (track->parts.size() == 1);
    CHECK (track->parts[0].start == 8.0);
    CHECK (track->parts[0].length == 8.0);
    REQUIRE (track->parts[0].notes.size() == 2);
    CHECK (track->parts[0].notes[1].start == 4.0);   // (its second bar)

    // Snap off: where it is.
    studio.setSnap (false);
    p = pointOver (synth, 13.0);
    CHECK (std::abs (view.dropPlace (p.x, p.y).second - 13.0) < 0.1);

    // Dropped on the audio track: a new instrument track, as before.
    studio.setSnap (true);
    p = pointOver (audio, 2.0);
    view.filesDropped (files, p.x, p.y);
    CHECK (studio.getProject().tracks.size() == tracksBefore + 1);
    file.deleteFile();
}

TEST_CASE ("0.38.0: the MIDI editor's info line: the first selected note's values, changed together (the owner's D#-1 to D-2)", "[ui][keyeditor][infoline]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    // D#-1 (15) first, then G-1 and C0; a note left out at E3.
    studio.addPart (trackId, { 4.0, 8.0, "Bass", { { 2.0, 1.0, 19, 0.5f }, { 0.0, 1.0, 15, 0.8f }, { 4.0, 0.5, 24, 0.6f }, { 1.0, 1.0, 64, 0.7f } } });
    studio.dispatchPendingMessages();
    view.getArrangeView().select (trackId, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    view.setKeyEditorSize (100);
    auto& editor = view.getKeyEditor();
    editor.setSize (1300, 700);
    REQUIRE (editor.getPart() != nullptr);
    const auto pitches = [&] { std::vector<int> p; for (const auto& n : editor.getPart()->notes) p.push_back (n.pitch); return p; };

    // Names both ways (middle C = C3).
    CHECK (pr7s::noteName (15) == "D#-1");
    CHECK (KeyEditor::parseNoteName ("D#-1") == 15);
    CHECK (KeyEditor::parseNoteName ("d-2") == 2);
    CHECK (KeyEditor::parseNoteName ("Eb3") == 63);
    CHECK (KeyEditor::parseNoteName ("C3") == 60);
    CHECK (KeyEditor::parseNoteName ("64") == 64);
    CHECK_FALSE (KeyEditor::parseNoteName ("H2").has_value());
    CHECK_FALSE (KeyEditor::parseNoteName ("C9").has_value());   // (above G8: off the keyboard)

    // Nothing selected: empty.
    CHECK (editor.getInfoField ("PITCH").isEmpty());
    editor.selectNotes ({ 0, 1, 2 });
    // The first is the earliest (not the first in the list): D#-1 at bar 2.
    CHECK (editor.getInfoField ("PITCH") == "D#-1");
    CHECK (editor.getInfoField ("START") == "2.1.1.0");
    CHECK (editor.getInfoField ("LENGTH") == "0.1.0.0");
    CHECK (editor.getInfoField ("VELOCITY") == "102");
    // D#-1 typed as D-2: all 13 semitones down; the unselected E3 stays.
    REQUIRE (editor.enterInfo ("PITCH", "D-2", false));
    CHECK (pitches() == std::vector<int> { 6, 2, 11, 64 });
    // As far as they can go: the lowest is D-2 (2), so C-2 and below stops at C-2 for it, the shape kept.
    REQUIRE (editor.enterInfo ("PITCH", "C-3", false) == false);   // (not a note: below the keyboard)
    REQUIRE (editor.enterInfo ("PITCH", "C-2", false));
    CHECK (pitches() == std::vector<int> { 4, 0, 9, 64 });
    // Ctrl: all to the note.
    REQUIRE (editor.enterInfo ("PITCH", "A1", true));
    CHECK (pitches() == std::vector<int> { 45, 45, 45, 64 });
    studio.undo();
    CHECK (pitches() == std::vector<int> { 4, 0, 9, 64 });

    // Velocity: by the same amount (102 -> 112: +10).
    REQUIRE (editor.enterInfo ("VELOCITY", "112", false));
    const auto vel = [&] (int i) { return juce::roundToInt (editor.getPart()->notes[(size_t) i].velocity * 127.0f); };
    CHECK (vel (1) == 112);
    CHECK (vel (0) == 64 + 10);
    CHECK (vel (3) == 89);   // (not selected)
    // Start: a beat later for all (2.1.1.0 -> 2.2.1.0).
    REQUIRE (editor.enterInfo ("START", "2.2.1.0", false));
    CHECK (editor.getPart()->notes[1].start == 1.0);
    CHECK (editor.getPart()->notes[0].start == 3.0);
    // Length: each a 16th longer (0.1.0.0 -> 0.1.1.0).
    REQUIRE (editor.enterInfo ("LENGTH", "0.1.1.0", false));
    CHECK (editor.getPart()->notes[1].length == 1.25);
    CHECK (editor.getPart()->notes[2].length == 0.75);
    // End: absolute with Ctrl (all end at bar 4: 8 beats into the part).
    REQUIRE (editor.enterInfo ("END", "4.1.1.0", true));
    for (const auto i : { 0, 1, 2 })
        CHECK (editor.getPart()->notes[(size_t) i].start + editor.getPart()->notes[(size_t) i].length == 8.0);
    CHECK_FALSE (editor.enterInfo ("END", "2.1.1.0", true));   // (before some of their starts: not taken)
    CHECK_FALSE (editor.enterInfo ("LENGTH", "nothing", false));

    // The notes moved down to make room: the ruler sits under the info line.
    CHECK (editor.getNotesArea().getY() == (float) (KeyEditor::infoHeight + KeyEditor::rulerHeight));
    CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
}

TEST_CASE ("0.48.0: keys are black in every finish; on the neon finishes their surrounds are the gradient", "[ui][finish]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    CHECK_FALSE (Finish::neonRings());
    Finish::set (Finish::Type::pearlWhiteNeon, false);
    CHECK (Finish::neonRings());
    Finish::set (Finish::Type::pearlBlack, false);
    CHECK (Finish::neonRings());

    ParagonLookAndFeel lookAndFeel;
    juce::TextButton key;
    key.setLookAndFeel (&lookAndFeel);
    key.getProperties().set (ParagonLookAndFeel::pedalButtonProperty, true);
    key.setSize (80, 30);
    const auto draw = [&] (Finish::Type type)
    {
        Finish::set (type, false);
        juce::Image image (juce::Image::ARGB, 80, 30, true, juce::SoftwareImageType());
        juce::Graphics g (image);
        key.paintEntireComponent (g, true);
        return image;
    };
    for (const auto type : { Finish::Type::pearlWhite, Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon })
    {
        const auto image = draw (type);
        // The key's face: black (no more pink-to-orange keys).
        const auto face = image.getPixelAt (12, 12);   // (beside the LED stripe in the middle)
        CHECK (face.getBrightness() < 0.25f);
        CHECK (std::abs ((int) face.getRed() - (int) face.getBlue()) < 30);
        // The surround's edge, a third of the way down on the left: chrome grey on Pearl White,
        // the gradient (clearly red) on the neon finishes.
        const auto rim = image.getPixelAt (1, 15);
        if (type == Finish::Type::pearlWhite)
            CHECK (std::abs ((int) rim.getRed() - (int) rim.getBlue()) < 40);
        else
            CHECK ((int) rim.getRed() > (int) rim.getBlue() + 60);
    }
    key.setLookAndFeel (nullptr);
    Finish::set (Finish::Type::pearlWhite, false);
}

TEST_CASE ("0.57.4: question boxes have see-through corners and no square shadow (as the progress box)", "[ui]")
{
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    {
        FamilyAlertWindow box ("Create Empty", "Its name:");
        CHECK_FALSE (box.isOpaque());
        CHECK (box.isOnDesktop());
        CHECK_FALSE (box.isDropShadowEnabled());
        std::unique_ptr<juce::AlertWindow> own (lookAndFeel.createAlertWindow ("Delete", "Sure?", "Yes", "No", {}, juce::MessageBoxIconType::QuestionIcon, 2, nullptr));
        REQUIRE (own != nullptr);
        CHECK_FALSE (own->isOpaque());
        CHECK_FALSE (own->isDropShadowEnabled());
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

TEST_CASE ("0.70.0: a track chosen anywhere is shown in the track list and the mixer; Top / Bottom", "[ui][tracks]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    for (int i = 0; i < 30; ++i)
        studio.addTrack (Instrument {});
    // (the views follow the studio's change messages: the message loop runs them)
    const auto settle = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (60); };
    settle();
    auto& mixer = view.getMixer();
    mixer.setSize (900, 400);
    settle();
    mixer.resized();

    const auto shown = studio.getShownTracks();
    // The bottom track, chosen (as from the mixer): the track list scrolls down to it, the mixer across.
    studio.selectTrack (shown.back());
    settle();
    CHECK (view.getTimeline().scrollY > 0);
    auto rowVisible = [&] (int id)
    {
        const auto lanes = view.getArrangeView().getHeight() - Timeline::rulerHeight;
        for (const auto& row : view.getTimeline().layout (studio))
            if (row.kind == Timeline::Row::Kind::track && row.busId == id)
                return row.top >= view.getTimeline().scrollY && row.top + row.height <= view.getTimeline().scrollY + lanes;
        return false;
    };
    CHECK (rowVisible (shown.back()));
    CHECK (mixer.getViewPosition().x > 0);

    // Top: the first track selected, back in view in both.
    view.selectTopTrack();
    settle();
    CHECK (studio.getSelectedTrack() == shown.front());
    CHECK (rowVisible (shown.front()));
    CHECK (mixer.getViewPosition().x == 0);
    // Bottom: the last.
    view.selectBottomTrack();
    settle();
    CHECK (studio.getSelectedTrack() == shown.back());
    CHECK (rowVisible (shown.back()));

    // Both are key commands with no default key (Home stays Go to Start), for KEYS.
    auto& keys = view.getKeyMap();
    CHECK (keys.keysFor ("Navigate.Top").isEmpty());
    CHECK (keys.keysFor ("Navigate.Bottom").isEmpty());
    keys.assign ("Navigate.Top", juce::KeyPress (juce::KeyPress::homeKey, juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0));
    auto swallowed = false;
    CHECK (keys.handle (juce::KeyPress (juce::KeyPress::homeKey, juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0), swallowed));
    CHECK (studio.getSelectedTrack() == shown.front());
}
