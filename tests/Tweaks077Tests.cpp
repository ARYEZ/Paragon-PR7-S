// 0.77.1 (the owner: "why can't I grab the left side of anything? I can only grab the right side"): a MIDI
// part's left edge in the arrangement and a note's left end in the MIDI editor can be dragged (before, only
// audio clips had a left edge; MIDI parts and notes moved instead).

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
