// 0.81.0 (the owner's asks): a cycle set from the MIDI editor's ruler (and P / Alt+P round the
// selected notes), the editor's own SNAP key, the SPLIT tool a straight razor, and Render in Place
// of a stereo instrument track keeping both sides.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Engine.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"
#include "ui/ToolIcons.h"

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

    void dragOn (juce::Component& target, juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods = {})
    {
        target.mouseDown (mouseAt (target, from, from, mods));
        for (int step = 1; step <= 4; ++step)
            target.mouseDrag (mouseAt (target, from + (to - from) * ((float) step / 4.0f), from, mods));
        target.mouseUp (mouseAt (target, to, from, mods));
    }

    void clickOn (juce::Component& target, juce::Point<float> at, juce::ModifierKeys mods = {})
    {
        target.mouseDown (mouseAt (target, at, at, mods));
        target.mouseUp (mouseAt (target, at, at, mods));
    }

    struct EditorOpen
    {
        Studio studio { false };
        MainView view { studio };
        int track = 0;

        EditorOpen()
        {
            Finish::set (Finish::Type::pearlWhite, false);
            view.setSize (1500, 900);
            studio.dispatchPendingMessages();
            track = studio.getProject().tracks[0].id;
            studio.addPart (track, { 4.0, 8.0, "Riff", { { 1.0, 1.0, 60, 0.8f }, { 2.5, 0.5, 64, 0.8f }, { 5.0, 1.0, 67, 0.8f } } });
            studio.dispatchPendingMessages();
            view.getArrangeView().select (track, 0);
            view.openKeyEditor();
            studio.dispatchPendingMessages();
            view.setKeyEditorSize (100);
            studio.dispatchPendingMessages();
        }
        KeyEditor& editor() { return view.getKeyEditor(); }
    };
}

TEST_CASE ("0.81.0: the MIDI editor's ruler sets the cycle: a drag in its strip, the handles, Ctrl/Alt-click, a click in it turns it on and off", "[ui][081]")
{
    EditorOpen open;
    REQUIRE (open.view.isKeyEditorOpen());
    auto& editor = open.editor();
    auto& studio = open.studio;
    editor.setGrid (1.0);
    const auto strip = editor.getCycleStrip();
    REQUIRE (! strip.isEmpty());
    const auto y = strip.getCentreY();
    studio.setCycle (40.0, 44.0);   // out of the way
    studio.setCycleOn (false);

    // A drag along the strip: a new range, CYCLE on.
    dragOn (editor, { editor.xForBeat (5.0), y }, { editor.xForBeat (9.0), y });
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (5.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (9.0, 1.0e-9));
    CHECK (studio.getProject().loopOn);

    // A click inside it: off, and on again.
    clickOn (editor, { editor.xForBeat (7.0), y });
    CHECK_FALSE (studio.getProject().loopOn);
    clickOn (editor, { editor.xForBeat (7.0), y });
    CHECK (studio.getProject().loopOn);

    // The right handle dragged: only that end.
    dragOn (editor, { editor.xForBeat (9.0), y }, { editor.xForBeat (11.0), y });
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (5.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (11.0, 1.0e-9));

    // The range dragged along: the same length.
    dragOn (editor, { editor.xForBeat (8.0), y }, { editor.xForBeat (10.0), y });
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (7.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (13.0, 1.0e-9));

    // Ctrl-click: the left locator; Alt-click: the right one (anywhere in the ruler, not the playhead).
    const auto lower = editor.getCycleStrip().getBottom() + 6.0f;
    studio.setPosition (0.0);
    clickOn (editor, { editor.xForBeat (6.0), lower }, juce::ModifierKeys::commandModifier);
    clickOn (editor, { editor.xForBeat (10.0), lower }, juce::ModifierKeys::altModifier);
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (6.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (10.0, 1.0e-9));
    CHECK (studio.getEngine().getPosition() == 0.0);

    // A plain click under the strip still moves the playhead.
    clickOn (editor, { editor.xForBeat (8.0), lower });
    CHECK_THAT (studio.getEngine().getPosition(), WithinAbs (8.0, 1.0e-9));
}

TEST_CASE ("0.81.0: P in the MIDI editor puts the locators round the selected notes (else the part); Alt+P loops and plays", "[ui][081]")
{
    EditorOpen open;
    auto& editor = open.editor();
    auto& studio = open.studio;
    editor.setGrid (1.0);
    // No notes selected: the part (4 to 12).
    REQUIRE (editor.keyPressed (juce::KeyPress ('p')));
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (12.0, 1.0e-9));
    // Two notes (5-6 and 6.5-7): rounded out to the grid, 5 to 7.
    editor.selectNotes ({ 0, 1 });
    REQUIRE (editor.keyPressed (juce::KeyPress ('p')));
    CHECK_THAT (studio.getProject().loopStart, WithinAbs (5.0, 1.0e-9));
    CHECK_THAT (studio.getProject().loopEnd, WithinAbs (7.0, 1.0e-9));
    studio.setCycleOn (false);
    REQUIRE (editor.keyPressed (juce::KeyPress ('p', juce::ModifierKeys::altModifier, 0)));
    CHECK (studio.getProject().loopOn);
    CHECK (studio.getEngine().isPlaying());
    studio.stop();
}

TEST_CASE ("0.81.0: the MIDI editor's SNAP key: off, a dragged note lands where it's dropped; on, on the grid; J toggles it", "[ui][081]")
{
    EditorOpen open;
    auto& editor = open.editor();
    auto& studio = open.studio;
    editor.setSize (1271, 900);   // (docked in a 900-high test window it's short: room for the notes)
    editor.fitToPart();
    editor.setGrid (1.0);
    editor.setTool (KeyEditor::Tool::select);
    CHECK (editor.isSnapOn());
    const auto startOf = [&] { return studio.findPart (studio.getProject().tracks[0].parts[0].id) != std::nullopt
                                          ? studio.getProject().tracks[0].parts[0].notes[0].start : -1.0; };
    const auto dragFirstBy = [&] (double beats)
    {
        const auto box = editor.getNoteBounds (0);
        const auto from = box.getCentre();
        INFO ("note " << box.toString() << " notes area " << editor.getNotesArea().toString() << " editor " << editor.getLocalBounds().toString());
        REQUIRE (editor.getNotesArea().contains (from));
        dragOn (editor, from, from + juce::Point<float> ((float) (beats * editor.getPixelsPerBeat()), 0.0f));
    };

    // On: a third of a beat rounds to nothing.
    dragFirstBy (0.3);
    CHECK_THAT (startOf(), WithinAbs (1.0, 1.0e-6));

    // Off (J): it goes where it's dropped.
    REQUIRE (editor.keyPressed (juce::KeyPress ('j')));
    CHECK_FALSE (editor.isSnapOn());
    CHECK_FALSE (editor.getSnapKey().getToggleState());
    dragFirstBy (0.3);
    CHECK_THAT (startOf(), WithinAbs (1.3, 0.02));

    // On again (the key): back to the grid's steps.
    editor.getSnapKey().onClick();
    CHECK (editor.isSnapOn());
    dragFirstBy (0.8);
    CHECK_THAT (startOf(), WithinAbs (2.0, 1.0e-6));
}

TEST_CASE ("0.81.0: the SPLIT tool is a razor: a solid blade and an outlined handle; the pointer's hot spot on the blade", "[ui][081]")
{
    const auto icon = ToolIcons::make (ArrangeView::Tool::split);
    REQUIRE (! icon.fill.isEmpty());
    REQUIRE (! icon.stroke.isEmpty());
    const auto blade = icon.fill.getBounds(), handle = icon.stroke.getBounds();
    CHECK (blade.getCentreX() < handle.getCentreX());   // the blade on the left, the handle up to the right
    CHECK (blade.getCentreY() > handle.getCentreY());
    CHECK (juce::Rectangle<float> (0.0f, 0.0f, 24.0f, 24.0f).contains (blade.getUnion (handle)));
    // The hot spot (9, 16) close to the blade's edge.
    CHECK (icon.fill.getBounds().expanded (2.0f).contains (9.0f, 16.0f));
}

TEST_CASE ("0.81.0: Render in Place of a stereo instrument keeps its two sides apart (a stereo file on a stereo track)", "[render][081]")
{
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    studio.getEngine().setInstrument (track, std::make_unique<SteadyTone> (0.4f, 0.1f));
    studio.addPart (track, { 0.0, 4.0, "Riff", { { 0.0, 4.0, 60, 1.0f } } });
    Studio::RenderOptions options;
    options.tail = Studio::RenderOptions::Tail::off;
    juce::String error;
    const auto made = studio.renderInPlace ({ { track, 0.0, 4.0, true } }, options, error);
    REQUIRE (made.size() == 1);
    const auto* rendered = studio.getProject().findTrack (made[0]);
    REQUIRE (rendered != nullptr);
    CHECK (rendered->stereoInput);
    REQUIRE (! rendered->parts.empty());
    std::unique_ptr<juce::AudioFormatReader> reader (juce::WavAudioFormat().createReaderFor (
        studio.getAudioFolder().getChildFile (rendered->parts[0].audioFile).createInputStream().release(), true));
    REQUIRE (reader != nullptr);
    CHECK (reader->numChannels == 2);
    juce::AudioBuffer<float> back (2, (int) reader->lengthInSamples);
    reader->read (&back, 0, back.getNumSamples(), 0, true, true);
    const auto middle = back.getNumSamples() / 2;
    CHECK_THAT (back.getSample (0, middle), WithinAbs (0.4, 0.02));
    CHECK_THAT (back.getSample (1, middle), WithinAbs (0.1, 0.02));

    // Drawn as two sides: the studio knows the file is stereo and keeps each side's peaks.
    const auto data = studio.getAudio (rendered->parts[0].audioFile);
    REQUIRE (data != nullptr);
    CHECK (data->stereo);
    REQUIRE (data->leftPeaks.size() == data->peaks.size());
    REQUIRE (data->rightPeaks.size() == data->peaks.size());
    const auto at = data->peaks.size() / 2;
    CHECK_THAT (data->leftPeaks[at], WithinAbs (0.4, 0.02));
    CHECK_THAT (data->rightPeaks[at], WithinAbs (0.1, 0.02));
}
