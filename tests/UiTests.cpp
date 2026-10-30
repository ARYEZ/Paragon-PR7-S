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
    CHECK (editor.getBottom() <= view.getHeight() - MainView::keyboardHeight);
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
    window->setSize (5760, 1000);
    CHECK (editor.getWidth() >= 5700);
    CHECK (editor.getHeight() > 900);
    editor.fitToPart();
    CHECK (editor.getNotesArea().contains (editor.getNoteBounds (1).getCentre()));   // the last note, 15 beats in
    CHECK (editor.getNotesArea().getHeight() > 700);                                // and many more rows

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
