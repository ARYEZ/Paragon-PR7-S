// 0.123.0 (the owner: dragging the cycle lagged, in the MIDI editor and in the arrangement). A drag's
// steps used to go through Studio::changed(): every track rebuilt for the engine, a whole copy of the
// song for undo, and every open view rebuilt and redrawn, on every mouse move. Now a step hands the
// engine the locators only and the views redraw their cycle strips; the release is one change, one undo step.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Engine.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt)
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, 1, at != downAt);
    }

    struct Open
    {
        Studio studio { false };
        MainView view { studio };
        int track = 0;
        Open()
        {
            Finish::set (Finish::Type::pearlWhite, false);
            view.setSize (1500, 900);
            studio.dispatchPendingMessages();
            track = studio.getProject().tracks[0].id;
            studio.addPart (track, { 4.0, 8.0, "Riff", { { 1.0, 1.0, 60, 0.8f }, { 2.5, 0.5, 64, 0.8f } } });
            studio.setCycle (8.0, 16.0);
            studio.setCycleOn (true);
            studio.dispatchPendingMessages();
        }
    };

    // Drags from `from` to `to` in steps; after every step: the song's change version unchanged, the
    // undo history unchanged, the engine's locators where the song's are.
    void dragChecked (Studio& studio, juce::Component& target, juce::Point<float> from, juce::Point<float> to)
    {
        const auto version = studio.getChangeVersion();
        const auto steps = studio.getHistory().size();
        target.mouseDown (mouseAt (target, from, from));
        for (int step = 1; step <= 6; ++step)
        {
            target.mouseDrag (mouseAt (target, from + (to - from) * ((float) step / 6.0f), from));
            studio.dispatchPendingMessages();
            CHECK (studio.getChangeVersion() == version);
            CHECK (studio.getHistory().size() == steps);
            const auto [start, end] = studio.getEngine().getCycleRange();
            CHECK_THAT (start, WithinAbs (studio.getProject().loopStart, 1.0e-9));
            CHECK_THAT (end, WithinAbs (studio.getProject().loopEnd, 1.0e-9));
        }
        target.mouseUp (mouseAt (target, to, from));
        studio.dispatchPendingMessages();
        CHECK (studio.getChangeVersion() != version);
        CHECK (studio.getHistory().size() == steps + 1);   // one undo step for the whole drag
    }
}

TEST_CASE ("0.123.0: a cycle drag in the arrangement: the engine follows each step, one undo step at the end, only the strips redrawn", "[speed0123][cycle]")
{
    Open open;
    auto& studio = open.studio;
    auto& arrange = open.view.getArrangeView();
    const auto& timeline = arrange.getTimeline();
    const auto y = (float) Timeline::cycleStripHeight * 0.5f;
    const auto repaintsBefore = arrange.getCycleRepaints();

    // The right handle (16) dragged to 20.
    dragChecked (studio, arrange, { (float) timeline.xForBeat (16.0), y }, { (float) timeline.xForBeat (20.0), y });
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (8.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (20.0, 1.0e-9));
    CHECK (arrange.getCycleRepaints() > repaintsBefore);

    // The range moved along by 4 beats: the same length.
    dragChecked (studio, arrange, { (float) timeline.xForBeat (12.0), y }, { (float) timeline.xForBeat (16.0), y });
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (12.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (24.0, 1.0e-9));

    // Undo takes the move back in one go; again, the handle's drag.
    studio.undo();
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (8.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (20.0, 1.0e-9));
    studio.undo();
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (16.0, 1.0e-9));
    CHECK_THAT (studio.getEngine().getCycleRange().second, WithinAbs (16.0, 1.0e-9));
}

TEST_CASE ("0.123.0: a cycle drag in the MIDI editor: the engine follows each step, one undo step at the end", "[speed0123][cycle]")
{
    Open open;
    auto& studio = open.studio;
    open.view.getArrangeView().select (open.track, 0);
    open.view.openKeyEditor();
    studio.dispatchPendingMessages();
    open.view.setKeyEditorSize (100);
    studio.dispatchPendingMessages();
    auto& editor = open.view.getKeyEditor();
    editor.setGrid (1.0);
    const auto y = editor.getCycleStrip().getCentreY();

    dragChecked (studio, editor, { editor.xForBeat (8.0), y }, { editor.xForBeat (6.0), y });   // the left handle
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (6.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (16.0, 1.0e-9));
    studio.undo();
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (8.0, 1.0e-9));
}

TEST_CASE ("0.123.0: a cycle drag's step against a whole change, on a heavy song (timings)", "[.perf-cycle]")
{
    Studio studio { false };
    for (int t = 0; t < 30; ++t)
    {
        const auto id = t == 0 ? studio.getProject().tracks[0].id : studio.addTrack (Instrument {});
        std::vector<Note> notes;
        for (int n = 0; n < 2000; ++n)
            notes.push_back ({ n * 0.25, 0.2, 36 + (n * 7 + t) % 48, 0.8f });
        studio.addPart (id, { 0.0, 500.0, "Part", notes });
    }
    const auto time = [&] (bool undoable)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < 40; ++i)
            studio.setCycle (8.0 + (i % 8), 24.0 + (i % 8), undoable);
        studio.finishCycleDrag();
        return (juce::Time::getMillisecondCounterHiRes() - start) / 40.0;
    };
    const auto whole = time (true), step = time (false);
    UNSCOPED_INFO ("a whole change " << whole << " ms, a drag's step " << step << " ms");
    CHECK (step < whole);
}
