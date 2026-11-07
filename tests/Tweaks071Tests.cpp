// 0.71.0: the owner's tweaks (the info line's double-click, the faders' half-dB steps, the themed
// windows and menus, removing what's missing, the docked zone's height).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/InfoLine.h"
#include "ui/MainView.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    // A real click through the window's peer: JUCE's own mouse handling (focus, double-clicks), as a person's.
    void peerClick (juce::Component& window, juce::Point<float> at, int64_t& time)
    {
        auto* peer = window.getPeer();
        REQUIRE (peer != nullptr);
        const auto type = juce::Desktop::getInstance().getMainMouseSource().getType();
        const auto down = juce::ModifierKeys().withFlags (juce::ModifierKeys::leftButtonModifier);
        const auto pressure = juce::MouseInputSource::defaultPressure;
        peer->handleMouseEvent (type, at, {}, pressure, juce::MouseInputSource::defaultOrientation, time += 20);
        peer->handleMouseEvent (type, at, down, pressure, juce::MouseInputSource::defaultOrientation, time += 20);
        peer->handleMouseEvent (type, at, {}, pressure, juce::MouseInputSource::defaultOrientation, time += 20);
    }
}

TEST_CASE ("0.71.0: a double-click on the info line's VALUE opens it for typing (the owner: only a point's double-click did)", "[ui][infoline][tweaks071]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    view.setVisible (true);
    view.addToDesktop (0);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setAutomationOpen (trackId, true);
    studio.setAutomationPoints (trackId, 0, { { 4.0, volumeToTravel (-3.0f), 0.0f }, { 5.0, volumeToTravel (-3.0f), 0.0f } });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& info = view.getInfoLine();
    // As the owner does: a point clicked (selected), Shift-click the next, then the info line.
    int64_t time = juce::Time::currentTimeMillis();
    const auto point = [&] (int i) { return view.getLocalPoint (&arrange, arrange.getPointPosition (trackId, 0, i)); };
    peerClick (view, point (0), time);
    time += 1000;
    arrange.selectPoints (trackId, 0, { 0, 1 });
    juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
    info.refresh();
    REQUIRE (info.getField ("VALUE") == "-3.0 dB");
    const auto field = info.getFieldArea ("VALUE");
    REQUIRE (! field.isEmpty());
    const auto at = view.getLocalPoint (&info, field.getCentre()).toFloat();
    time += 1000;
    peerClick (view, at, time);
    peerClick (view, at, time);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
    info.refresh();
    REQUIRE (info.getEditor() != nullptr);
    CHECK (info.getEditor()->getText() == "-3.0 dB");
    CHECK (info.getEditor()->hasKeyboardFocus (true));
}

TEST_CASE ("0.71.0: a single click on the info line's VALUE opens it too", "[ui][infoline][tweaks071]")
{
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    view.setVisible (true);
    view.addToDesktop (0);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setAutomationOpen (trackId, true);
    studio.setAutomationPoints (trackId, 0, { { 4.0, volumeToTravel (-6.0f), 0.0f } });
    studio.dispatchPendingMessages();
    view.getArrangeView().selectPoints (trackId, 0, { 0 });
    auto& info = view.getInfoLine();
    info.refresh();
    int64_t time = juce::Time::currentTimeMillis();
    peerClick (view, view.getLocalPoint (&info, info.getFieldArea ("VALUE").getCentre()).toFloat(), time);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
    REQUIRE (info.getEditor() != nullptr);
    CHECK (info.getEditor()->getText() == "-6.0 dB");
    info.getEditor()->setText ("-9", false);
    CHECK (info.getEditor()->keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
    CHECK_THAT (travelToVolume ((*studio.getAutomation (trackId))[0].points[0].value), WithinAbs (-9.0, 0.05));
}

TEST_CASE ("0.71.0: a mixer fader dragged moves in half-dB steps; typed, any value", "[ui][mixer][tweaks071]")
{
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    view.openMixer();
    studio.dispatchPendingMessages();
    auto& fader = view.getMixer().getStrip (0).getVolume();
    CHECK (fader.snapValue (-3.27, juce::Slider::absoluteDrag) == -3.5);
    CHECK (fader.snapValue (-3.24, juce::Slider::absoluteDrag) == -3.0);
    CHECK (fader.snapValue (5.9, juce::Slider::velocityDrag) == 6.0);
    CHECK (fader.snapValue (-3.27, juce::Slider::notDragging) == -3.27);
    // A real drag through the fader: every value it sets is a multiple of 0.5.
    const auto at = [&] (float y) { return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), { 18.0f, y },
                                                             juce::ModifierKeys().withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                             &fader, &fader, juce::Time::getCurrentTime(), { 18.0f, 100.0f }, juce::Time::getCurrentTime(), 1, true); };
    fader.mouseDown (at (100.0f));
    for (int y = 101; y < 140; y += 3)
    {
        fader.mouseDrag (at ((float) y));
        const auto v = fader.getValue();
        CHECK (std::abs (v * 2.0 - std::round (v * 2.0)) < 1.0e-9);
    }
    fader.mouseUp (at (139.0f));
    // Typed: any value (to the tenth shown).
    REQUIRE (fader.enter ("-3.3"));
    CHECK_THAT (studio.getProject().tracks[0].volumeDb, WithinAbs (-3.3, 1.0e-4));
}

TEST_CASE ("0.71.0: what a song opened without can be removed (effects, recordings, instruments)", "[studio][tweaks071]")
{
    const auto folder = juce::File::createTempFile ("pr7s-missing");
    folder.createDirectory();
    const auto song = folder.getChildFile ("Missing.pr7s");
    {
        Studio studio (false);
        const auto synthTrack = studio.getProject().tracks[0].id;
        const auto audio = studio.addAudioTrack (false);
        auto project = studio.getProject();
        auto* track = project.findTrack (synthTrack);
        track->instrument.kind = Instrument::Kind::plugin;
        track->instrument.plugin.name = "Nowhere Synth";
        track->instrument.plugin.pluginFormatName = "VST3";
        track->instrument.plugin.fileOrIdentifier = "/nowhere/Nowhere Synth.vst3";
        Insert gone;
        gone.plugin.name = "Nowhere Booster";
        gone.plugin.pluginFormatName = "VST3";
        gone.plugin.fileOrIdentifier = "/nowhere/Nowhere Booster.vst3";
        track->inserts = { gone };
        project.masterInserts = { {}, gone };
        Part clip;
        clip.start = 0.0;
        clip.length = 4.0;
        clip.audioFile = "never recorded.wav";
        project.findTrack (audio)->parts.push_back (clip);
        Part other = clip;
        other.start = 8.0;
        project.findTrack (audio)->parts.push_back (other);
        REQUIRE (project.toXml()->writeTo (song));
    }
    Studio studio (false);
    juce::String problems;
    REQUIRE (studio.open (song, problems));
    CHECK (problems.contains ("Nowhere Synth"));
    CHECK (problems.contains ("Nowhere Booster"));
    CHECK (problems.contains ("never recorded.wav"));
    CHECK (studio.getMissing().size() == 5);   // the instrument, two effects, a recording on two clips (one each)
    CHECK (studio.removeMissing() >= 4);
    CHECK (studio.getMissing().empty());
    const auto& project = studio.getProject();
    CHECK (project.tracks[0].instrument.kind == Instrument::Kind::paragonSynth);
    CHECK (project.tracks[0].parts.size() == studio.getProject().tracks[0].parts.size());
    CHECK (project.tracks[0].inserts.empty());
    CHECK (project.masterInserts.empty());
    for (const auto& track : project.tracks)
        for (const auto& part : track.parts)
            CHECK (part.audioFile != "never recorded.wav");
    // One undo step puts the clips back (the plug-ins aren't undo steps, as everywhere).
    studio.undo();
    auto clips = 0;
    for (const auto& track : studio.getProject().tracks)
        for (const auto& part : track.parts)
            clips += part.audioFile == "never recorded.wav" ? 1 : 0;
    CHECK (clips == 2);
    folder.deleteRecursively();
}

TEST_CASE ("0.71.0: a docked zone opens leaving the master and five whole tracks above it", "[ui][zone][tweaks071]")
{
    Studio studio (false);
    for (int i = 0; i < 8; ++i)
        studio.addTrack ({});
    MainView view (studio);
    view.setSize (1600, 1200);
    for (const auto open : { 0, 1 })
    {
        if (open == 0)
            view.openMixer();
        else
        {
            view.closeMixer();
            view.openChordPads();
        }
        studio.dispatchPendingMessages();
        view.resized();
        auto& arrange = view.getArrangeView();
        const auto& timeline = view.getTimeline();
        const auto rows = arrange.getHeight() - Timeline::rulerHeight;
        CHECK (rows >= timeline.masterRowHeight() + 5 * timeline.trackRowHeight());
        CHECK (rows < timeline.masterRowHeight() + 5 * timeline.trackRowHeight() + 4);
    }
}

TEST_CASE ("0.71.0: the EQ's starting points are all valid; the Inspector has PRESETS", "[eq][tweaks071]")
{
    const auto& points = eqStartingPoints();
    CHECK (points.size() >= 15);
    for (const auto& p : points)
    {
        CHECK (p.eq == p.eq.limited());
        CHECK_FALSE (p.eq.isFlat());
        CHECK (p.name.isNotEmpty());
    }
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setChannelEq (trackId, points[0].eq);
    CHECK (*studio.getChannelEq (trackId) == points[0].eq);
}
