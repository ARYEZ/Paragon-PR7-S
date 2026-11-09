// 0.78.0 (the owner's four): the ruler dragged down zooms out (up: in), SOLO EDITOR in the MIDI editor,
// Render in Place keeps a mono track mono, and the BOOSTER (PR7-C's OUTPUT TRIM) under the EQ.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Engine.h"
#include "engine/Studio.h"
#include "ui/BoosterView.h"
#include "ui/Finish.h"
#include "ui/Inspector.h"
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

TEST_CASE ("0.78.0: the ruler dragged down zooms out, up zooms in, round the beat clicked; a click still sets the position", "[ui][078]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = arrange.getTimeline();
    timeline.firstBeat = 40.0;   // scrolled along: room to zoom out round the beat clicked
    const auto before = timeline.pixelsPerBeat;
    const float y = (float) Timeline::cycleStripHeight + 10.0f;
    const float x = 400.0f;
    const auto beat = timeline.beatForX (x);

    dragOn (arrange, juce::Point<float> (x, y), juce::Point<float> (x, y + ArrangeView::rulerZoomPixelsPerDouble));   // 80 px down: half
    CHECK_THAT (timeline.pixelsPerBeat, WithinAbs (before / 2.0, 1.0e-6));
    CHECK_THAT (timeline.beatForX (x), WithinAbs (beat, 1.0e-6));
    CHECK_FALSE (arrange.isRulerZooming());
    dragOn (arrange, juce::Point<float> (x, y), juce::Point<float> (x, y - 2.0f * ArrangeView::rulerZoomPixelsPerDouble));   // 160 px up: four times
    CHECK_THAT (timeline.pixelsPerBeat, WithinAbs (before * 2.0, 1.0e-6));
    CHECK_THAT (timeline.beatForX (x), WithinAbs (beat, 1.0e-6));

    // A click (or a drag across) without going up or down: the position, no zoom.
    const auto zoom = timeline.pixelsPerBeat;
    arrange.mouseDown (mouseAt (arrange, juce::Point<float> (600.0f, y), juce::Point<float> (600.0f, y)));
    arrange.mouseUp (mouseAt (arrange, juce::Point<float> (600.0f, y), juce::Point<float> (600.0f, y)));
    CHECK (timeline.pixelsPerBeat == zoom);
    CHECK (studio.getEngine().getPosition() > 0.0);

    // The MIDI editor's ruler too.
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 0.0, 8.0, "Riff", { { 0.0, 1.0, 60, 0.8f } } });
    studio.dispatchPendingMessages();
    arrange.select (track, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    REQUIRE (view.isKeyEditorOpen());
    view.setKeyEditorSize (100);
    auto& editor = view.getKeyEditor();
    const auto editorBefore = editor.getPixelsPerBeat();
    const auto ex = (float) (KeyEditor::sideWidth + KeyEditor::keysWidth) + 200.0f;
    const auto ey = (float) KeyEditor::infoHeight + (float) KeyEditor::rulerHeight * 0.5f;
    dragOn (editor, juce::Point<float> (ex, ey), juce::Point<float> (ex, ey + ArrangeView::rulerZoomPixelsPerDouble));
    CHECK_THAT (editor.getPixelsPerBeat(), WithinAbs (editorBefore / 2.0, 1.0e-6));
}

TEST_CASE ("0.78.0: SOLO EDITOR: only the track in the MIDI editor is heard; the song's solos untouched; off when the editor closes", "[ui][solo][078]")
{
    // The engine: the editor's track alone, whatever the tracks' own solos say.
    {
        Engine engine;
        engine.prepare (48000.0, 512);
        Project project;
        for (int id : { 1, 2 })
        {
            Track t;
            t.id = id;
            project.tracks.push_back (t);
        }
        engine.setInstrument (1, std::make_unique<SteadyTone> (0.25f, 0.25f));
        engine.setInstrument (2, std::make_unique<SteadyTone> (0.125f, 0.125f));
        engine.update (project);
        CHECK_THAT (renderSamples (engine, 4800).back(), WithinAbs (0.375, 1.0e-4));
        engine.setEditorSolo (2);
        engine.update (project);
        CHECK_THAT (renderSamples (engine, 4800).back(), WithinAbs (0.125, 1.0e-4));
        engine.setEditorSolo (0);
        engine.update (project);
        CHECK_THAT (renderSamples (engine, 4800).back(), WithinAbs (0.375, 1.0e-4));
    }

    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 1300);
    view.setVisible (true);
    view.addToDesktop (0);
    const auto first = studio.getProject().tracks[0].id;
    const auto second = studio.addTrack ({});
    studio.addPart (first, { 0.0, 4.0, "A", { { 0.0, 1.0, 60, 0.8f } } });
    studio.addPart (second, { 0.0, 4.0, "B", { { 0.0, 1.0, 64, 0.8f } } });
    studio.dispatchPendingMessages();
    view.getArrangeView().select (first, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    REQUIRE (view.isKeyEditorOpen());
    auto& editor = view.getKeyEditor();
    REQUIRE (editor.isShowing());
    CHECK (editor.getSoloKey().isVisible());

    editor.getSoloKey().onClick();
    CHECK (editor.isSoloEditorOn());
    CHECK (studio.getEditorSolo() == first);
    const auto out = studio.getProject().soloedOutTracks (studio.getEditorSolo());
    CHECK (std::find (out.begin(), out.end(), second) != out.end());
    CHECK (std::find (out.begin(), out.end(), first) == out.end());
    for (const auto& t : studio.getProject().tracks)
        CHECK_FALSE (t.solo);   // nothing saved with the song
    CHECK_FALSE (studio.getProject().toXml()->toString().contains ("editorSolo"));

    // The editor on the other track's part: the solo follows it.
    view.getArrangeView().select (second, 0);
    studio.dispatchPendingMessages();
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    if (editor.getTrackId() == second)
        CHECK (studio.getEditorSolo() == second);

    // Closed: everything heard again; the key stays on for next time.
    view.closeKeyEditor();
    studio.dispatchPendingMessages();
    CHECK (studio.getEditorSolo() == 0);
    CHECK (editor.isSoloEditorOn());
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    CHECK (studio.getEditorSolo() != 0);
    editor.getSoloKey().onClick();
    CHECK (studio.getEditorSolo() == 0);
    view.removeFromDesktop();
}

TEST_CASE ("0.78.0: Render in Place: a mono track renders a mono file onto a mono track; a stereo track stays stereo", "[render][078]")
{
    for (const bool stereo : { false, true })
    {
        Studio studio (false);
        const auto folder = juce::File::createTempFile ("").getSiblingFile ("pr7s-078-" + juce::String (stereo ? "st" : "mono"));
        folder.deleteRecursively();
        const auto file = folder.getChildFile ("take.wav");
        juce::AudioBuffer<float> sound (stereo ? 2 : 1, 48000);
        for (int c = 0; c < sound.getNumChannels(); ++c)
            for (int i = 0; i < sound.getNumSamples(); ++i)
                sound.setSample (c, i, 0.3f * (float) std::sin (i * 0.05));
        REQUIRE (writeWav (file, sound, 48000.0));
        const auto track = studio.addAudioTrack (stereo);
        juce::String error;
        REQUIRE (studio.importAudio ({ file }, track, 0.0, error) == track);
        REQUIRE (studio.getProject().findTrack (track)->stereoInput == stereo);
        Studio::RenderOptions options;
        options.tail = Studio::RenderOptions::Tail::off;
        const auto made = studio.renderInPlace ({ { track, 0.0, 1.0, true } }, options, error);
        REQUIRE (made.size() == 1);
        const auto* rendered = studio.getProject().findTrack (made[0]);
        REQUIRE (rendered != nullptr);
        CHECK (rendered->stereoInput == stereo);
        REQUIRE (! rendered->parts.empty());
        std::unique_ptr<juce::AudioFormatReader> reader (juce::WavAudioFormat().createReaderFor (
            studio.getAudioFolder().getChildFile (rendered->parts[0].audioFile).createInputStream().release(), true));
        REQUIRE (reader != nullptr);
        CHECK ((int) reader->numChannels == (stereo ? 2 : 1));
        juce::AudioBuffer<float> back ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&back, 0, back.getNumSamples(), 0, true, true);
        CHECK (back.getMagnitude (0, 0, back.getNumSamples()) > 0.05f);
        folder.deleteRecursively();
    }
}

TEST_CASE ("0.78.0: the BOOSTER: trim and level add, after the EQ; the bottom is silence; the master's too", "[booster][engine][078]")
{
    // PR7-C's fader law: its marks evenly spaced.
    CHECK (BoosterView::faderProportion (-60.0f) == 0.0f);
    CHECK (BoosterView::faderProportion (10.0f) == 1.0f);
    CHECK_THAT (BoosterView::faderProportion (0.0f), WithinAbs (0.75, 1.0e-6));
    CHECK_THAT (BoosterView::faderProportion (-15.0f), WithinAbs ((3.5) / 8.0, 1.0e-6));
    for (const auto db : { -60.0f, -35.0f, -10.0f, -2.5f, 0.0f, 7.5f, 10.0f })
        CHECK_THAT (BoosterView::faderDb (BoosterView::faderProportion (db)), WithinAbs (db, 0.051));
    CHECK (BoosterView::dbText (-60.0f, false) == "-INF DB");
    CHECK (BoosterView::dbText (3.0f, true) == "+3.0 DB");

    Booster flat;
    CHECK (flat.gain() == 1.0f);
    CHECK (Booster { 0.0f, -60.0f }.gain() == 0.0f);
    CHECK (Booster { 30.0f, 40.0f }.limited() == Booster { 12.0f, 10.0f });

    Engine engine;
    engine.prepare (48000.0, 512);
    Project project;
    Track track;
    track.id = 1;
    project.tracks.push_back (track);
    engine.setInstrument (1, std::make_unique<SteadyTone> (0.25f, 0.25f));
    engine.update (project);
    const auto level = [&engine] { return renderSamples (engine, 9600, 512).back(); };
    CHECK_THAT (level(), WithinAbs (0.25, 1.0e-5));
    const auto x2 = juce::Decibels::gainToDecibels (2.0f);
    project.tracks[0].boost = { x2 / 2.0f, x2 / 2.0f };   // the two add: x2
    engine.update (project);
    CHECK_THAT (level(), WithinAbs (0.5, 1.0e-4));
    // After the EQ: a low shelf +6 dB (x2 at DC) then the booster x2: x4.
    project.tracks[0].eq.bands[0] = { true, EqBand::Type::lowShelf, juce::Decibels::gainToDecibels (2.0), 1000.0, 0.71 };
    engine.update (project);
    CHECK_THAT (level(), WithinAbs (1.0, 1.0e-3));
    project.tracks[0].eq = {};
    project.tracks[0].boost = { 0.0f, Booster::leastLevel };   // the bottom: silence
    engine.update (project);
    CHECK_THAT (level(), WithinAbs (0.0, 1.0e-6));
    project.tracks[0].boost = {};
    project.masterBoost = { -x2, 0.0f };   // the master's: x0.5
    engine.update (project);
    CHECK_THAT (level(), WithinAbs (0.125, 1.0e-4));
    // A move ramps over a block: no jump in the first sample.
    project.masterBoost = {};
    engine.update (project);
    const auto ramp = renderSamples (engine, 512, 512);
    CHECK (ramp.front() < 0.13f);
    CHECK_THAT (ramp.back(), WithinAbs (0.25, 1.0e-4));
}

TEST_CASE ("0.78.0: the BOOSTER in the studio and the Inspector: a drag one undo step, saved, in presets; under EQUALIZERS", "[booster][ui][078]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    const auto id = studio.getProject().tracks[0].id;
    studio.setBooster (id, { 2.0f, -5.0f }, false);
    studio.setBooster (id, { 3.5f, -6.0f }, true);
    CHECK (*studio.getBooster (id) == Booster { 3.5f, -6.0f });
    studio.undo();
    CHECK (studio.getBooster (id)->isFlat());
    studio.redo();
    CHECK (*studio.getBooster (id) == Booster { 3.5f, -6.0f });
    studio.setBooster (0, { -1.0f, 4.0f });
    CHECK (studio.getProject().masterBoost == Booster { -1.0f, 4.0f });

    const auto back = Project::fromXml (*studio.getProject().toXml());
    REQUIRE (back);
    CHECK (back->findTrack (id)->boost == Booster { 3.5f, -6.0f });
    CHECK (back->masterBoost == Booster { -1.0f, 4.0f });
    // A flat booster writes nothing.
    Studio plain (false);
    CHECK_FALSE (plain.getProject().toXml()->toString().contains ("Booster"));

    const auto preset = studio.makeTrackPreset (id);
    REQUIRE (preset != nullptr);
    const auto other = studio.addTrack ({});
    juce::String error;
    REQUIRE (studio.applyTrackPreset (other, *preset, error));
    CHECK (*studio.getBooster (other) == Booster { 3.5f, -6.0f });

    // The Inspector: BOOSTER right after EQUALIZERS, its fader and knob writing to the selected track.
    MainView view (studio);
    view.setSize (1500, 1300);
    studio.selectTrack (id);
    studio.dispatchPendingMessages();
    auto& inspector = view.getInspector();
    auto& eqHeader = inspector.getHeader (Inspector::Section::eq);
    auto& boostHeader = inspector.getHeader (Inspector::Section::boost);
    REQUIRE (boostHeader.isVisible());
    CHECK (boostHeader.getY() > eqHeader.getY());
    auto& booster = inspector.getBooster();
    CHECK (booster.getBus() == id);
    CHECK_THAT (booster.getTrim().getValue(), WithinAbs (3.5, 1.0e-6));
    booster.getFader().setValue (BoosterView::faderProportion (-10.0f), juce::sendNotificationSync);
    CHECK_THAT (studio.getBooster (id)->levelDb, WithinAbs (-10.0f, 0.05));
    booster.getTrim().setValue (-4.0, juce::sendNotificationSync);
    CHECK_THAT (studio.getBooster (id)->trimDb, WithinAbs (-4.0f, 1.0e-6));
    studio.undo();
    CHECK_THAT (studio.getBooster (id)->trimDb, WithinAbs (3.5f, 1.0e-6));
    // The master's.
    studio.selectMaster();
    studio.dispatchPendingMessages();
    CHECK (booster.getBus() == Engine::masterBus);
    CHECK_THAT (booster.getTrim().getValue(), WithinAbs (-1.0, 1.0e-6));
}
