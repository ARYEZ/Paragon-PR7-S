// Automation editing (0.27.0): bent ramps, copy and paste of automation over the locators, and
// the arrangement's editing: Ctrl-click and box selections, typed values, the event editor's
// handles, bends, DRAW / ERASE / LINE on the lanes.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
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
    void drag (ArrangeView& view, juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods = {})
    {
        view.mouseDown (mouseAt (view, from, from, mods));
        if (to != from)
            for (int step = 1; step <= 8; ++step)
                view.mouseDrag (mouseAt (view, from + (to - from) * ((float) step / 8.0f), from, mods));
        view.mouseUp (mouseAt (view, to, from, mods));
    }
}

TEST_CASE ("Bent ramps: the curve's shape, saved, and followed in between", "[automation][edit]")
{
    CHECK_THAT (AutomationLane::curveShape (0.5f, 0.0f), WithinAbs (0.5, 1.0e-6));
    CHECK_THAT (AutomationLane::curveShape (0.5f, 0.5f), WithinAbs (std::pow (0.5, 0.5), 1.0e-5));   // bowed up
    CHECK_THAT (AutomationLane::curveShape (0.5f, -0.5f), WithinAbs (0.25, 1.0e-5));                 // bowed down
    CHECK_THAT (AutomationLane::curveShape (0.0f, 1.0f), WithinAbs (0.0, 1.0e-6));
    CHECK_THAT (AutomationLane::curveShape (1.0f, -1.0f), WithinAbs (1.0, 1.0e-6));
    AutomationLane lane;
    lane.points = { { 0.0, 0.0f, 0.0f }, { 4.0, 1.0f, -0.5f } };
    CHECK_THAT (*lane.valueAt (2.0), WithinAbs (0.25, 1.0e-5));
    CHECK_THAT (*lane.valueAt (4.0), WithinAbs (1.0, 1.0e-6));
    // Saved with the song.
    Project project;
    Track track;
    track.id = 1;
    track.automation.push_back (lane);
    project.tracks.push_back (track);
    const auto opened = Project::fromXml (*project.toXml());
    REQUIRE (opened.has_value());
    CHECK (opened->tracks[0].automation[0].points == lane.points);
}

TEST_CASE ("Copy and paste automation over the locators: a parameter, all tracks, the selected tracks", "[automation][edit][studio]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    studio.setAutomationOpen (a, true);
    studio.setAutomationOpen (b, true);
    // Track A's volume: up from 0.2 to 0.8 over bars 1 and 2.
    studio.setAutomationPoints (a, 0, { { 0.0, 0.2f, 0.0f }, { 8.0, 0.8f, 0.0f } });
    studio.setCycle (0.0, 8.0);
    REQUIRE (studio.copyAutomation (Studio::AutomationScope::parameter, a, 0));
    CHECK (studio.hasAutomationClip());
    // Onto track B's volume, bars 3 to 4: the same ramp there; outside, B's own (none: the ramp holds its ends).
    studio.setCycle (8.0, 16.0);
    REQUIRE (studio.pasteAutomation (Studio::AutomationScope::parameter, b, 0));
    const auto& laneB = (*studio.getAutomation (b))[0];
    CHECK_THAT (*laneB.valueAt (8.0), WithinAbs (0.2, 1.0e-4));
    CHECK_THAT (*laneB.valueAt (12.0), WithinAbs (0.5, 1.0e-3));
    CHECK_THAT (*laneB.valueAt (16.0), WithinAbs (0.8, 1.0e-4));
    // Pasted over a range half as long: cut where it ends (the value it had reached).
    studio.setCycle (20.0, 24.0);
    REQUIRE (studio.pasteAutomation (Studio::AutomationScope::parameter, b, 0));
    CHECK_THAT (*(*studio.getAutomation (b))[0].valueAt (24.0), WithinAbs (0.5, 1.0e-3));
    // Twice as long: the last value held to the right locator.
    studio.setCycle (32.0, 48.0);
    REQUIRE (studio.pasteAutomation (Studio::AutomationScope::parameter, b, 0));
    CHECK_THAT (*(*studio.getAutomation (b))[0].valueAt (44.0), WithinAbs (0.8, 1.0e-4));
    CHECK_THAT (*(*studio.getAutomation (b))[0].valueAt (36.0), WithinAbs (0.5, 1.0e-3));   // (the ramp again, from bar 9)
    // One undo step each.
    studio.undo();
    CHECK_THAT (*(*studio.getAutomation (b))[0].valueAt (36.0), WithinAbs (0.8, 1.0e-3));
    studio.redo();

    // All tracks: copied from bars 1-2, pasted at bars 9-10 on the same tracks and controls.
    studio.setCycle (0.0, 8.0);
    REQUIRE (studio.copyAutomation (Studio::AutomationScope::allTracks));
    studio.setCycle (64.0, 72.0);
    REQUIRE (studio.pasteAutomation (Studio::AutomationScope::allTracks));
    CHECK_THAT (*(*studio.getAutomation (a))[0].valueAt (68.0), WithinAbs (0.5, 1.0e-3));
    // The selected tracks' onto other selected tracks, in order: A's onto B.
    studio.selectTrack (a);
    studio.setCycle (0.0, 8.0);
    REQUIRE (studio.copyAutomation (Studio::AutomationScope::selectedTracks));
    studio.selectTrack (b);
    studio.setCycle (80.0, 88.0);
    REQUIRE (studio.pasteAutomation (Studio::AutomationScope::selectedTracks));
    CHECK_THAT (*(*studio.getAutomation (b))[0].valueAt (84.0), WithinAbs (0.5, 1.0e-3));
    // Nothing between the locators: nothing copied.
    studio.setCycle (200.0, 208.0);
    const auto c = studio.addTrack ({});
    studio.selectTrack (c);
    CHECK_FALSE (studio.copyAutomation (Studio::AutomationScope::selectedTracks));
}

TEST_CASE ("Automation in the arrangement: selecting, typing values, the handles, bends and the tools", "[automation][edit][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setAutomationOpen (trackId, true);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    for (int i = 0; i < 4; ++i)
        arrange.zoomRows (true);   // (taller lanes: room for the handles)
    auto area = arrange.getLaneArea (trackId, 0);
    REQUIRE_FALSE (area.isEmpty());
    const auto& points = [&] () -> const std::vector<AutomationPoint>& { return (*studio.getAutomation (trackId))[0].points; };
    auto& timeline = view.getTimeline();
    const auto at = [&] (double beat, float value) { return juce::Point<float> (timeline.xForBeat (beat), area.getBottom() - value * area.getHeight()); };

    // Clicks add points (on the grid).
    for (const auto& [beat, value] : { std::pair { 1.0, 0.2f }, { 3.0, 0.8f }, { 5.0, 0.4f }, { 7.0, 0.6f } })
        drag (arrange, at (beat, value), at (beat, value));
    REQUIRE (points().size() == 4);

    // Ctrl-click adds a point to the selection, and takes it out again (the owner's way, besides Shift).
    drag (arrange, at (1.0, points()[0].value), at (1.0, points()[0].value));
    CHECK (arrange.getPointSelection().points == std::vector<int> { 0 });
    drag (arrange, at (5.0, points()[2].value), at (5.0, points()[2].value), juce::ModifierKeys::commandModifier);
    CHECK (arrange.getPointSelection().points == std::vector<int> { 0, 2 });
    drag (arrange, at (3.0, points()[1].value), at (3.0, points()[1].value), juce::ModifierKeys::shiftModifier);
    CHECK (arrange.getPointSelection().points == std::vector<int> { 0, 1, 2 });
    drag (arrange, at (5.0, points()[2].value), at (5.0, points()[2].value), juce::ModifierKeys::commandModifier);
    CHECK (arrange.getPointSelection().points == std::vector<int> { 0, 1 });
    // A box round them all.
    drag (arrange, { timeline.xForBeat (0.5), area.getY() + 1.0f }, { timeline.xForBeat (7.5), area.getBottom() - 1.0f });
    CHECK (arrange.getPointSelection().points == std::vector<int> { 0, 1, 2, 3 });

    // Typed values, in the control's units: -6 dB for all of them; then one moved to bar 2.
    REQUIRE (arrange.formatPointValue (trackId, (*studio.getAutomation (trackId))[0], volumeToTravel (-6.0f)) == juce::String ("-6.0 dB"));
    CHECK (arrange.enterPointValue ("-6", ""));
    for (const auto& p : points())
        CHECK_THAT (p.value, WithinAbs (volumeToTravel (-6.0f), 1.0e-4));
    CHECK_FALSE (arrange.enterPointValue ("loud", ""));
    arrange.selectPoints (trackId, 0, { 3 });
    CHECK (arrange.enterPointValue ("+3 dB", "3.1.1.0"));
    REQUIRE (points().size() == 4);
    CHECK_THAT (points()[3].beat, WithinAbs (8.0, 1.0e-9));
    CHECK_THAT (points()[3].value, WithinAbs (volumeToTravel (3.0f), 1.0e-4));
    // The other controls' units.
    AutomationLane pan;
    pan.target = AutomationLane::Target::pan;
    CHECK (arrange.formatPointValue (trackId, pan, panToTravel (-0.3f)) == juce::String ("L30"));
    CHECK_THAT (*arrange.parsePointValue (trackId, pan, "R20"), WithinAbs (panToTravel (0.2f), 1.0e-5));
    AutomationLane mute;
    mute.target = AutomationLane::Target::mute;
    CHECK (*arrange.parsePointValue (trackId, mute, "ON") == 1.0f);
    AutomationLane knob;
    knob.target = AutomationLane::Target::insert;
    CHECK_THAT (*arrange.parsePointValue (trackId, knob, "25%"), WithinAbs (0.25, 1.0e-6));

    // The event editor's handles: two or more selected; the top edge moves them all up.
    studio.setAutomationPoints (trackId, 0, { { 1.0, 0.2f, 0.0f }, { 3.0, 0.6f, 0.0f }, { 5.0, 0.4f, 0.0f }, { 7.0, 0.3f, 0.0f } });
    arrange.selectPoints (trackId, 0, { 0, 1, 2 });
    auto box = arrange.getPointBox();
    REQUIRE_FALSE (box.isEmpty());
    const auto moveAt = juce::Point<float> (box.getX() + box.getWidth() * 0.3f, box.getY());
    REQUIRE (arrange.pointHandleAt (moveAt, false) == ArrangeView::PointHandle::move);
    drag (arrange, moveAt, moveAt + juce::Point<float> (0.0f, -area.getHeight() * 0.1f));
    CHECK_THAT (points()[0].value, WithinAbs (0.3, 0.02));
    CHECK_THAT (points()[1].value, WithinAbs (0.7, 0.02));
    CHECK_THAT (points()[3].value, WithinAbs (0.3, 1.0e-6));   // (not selected: left)
    // Tilt right: the right end moves, the left stays.
    box = arrange.getPointBox();
    const auto tiltAt = box.getTopRight();
    REQUIRE (arrange.pointHandleAt (tiltAt, false) == ArrangeView::PointHandle::tiltRight);
    const auto before0 = points()[0].value, before2 = points()[2].value;
    drag (arrange, tiltAt, tiltAt + juce::Point<float> (0.0f, area.getHeight() * 0.2f));
    CHECK_THAT (points()[0].value, WithinAbs (before0, 0.01));
    CHECK (points()[2].value < before2 - 0.1f);
    // Alt on the corner: compress.
    CHECK (arrange.pointHandleAt (arrange.getPointBox().getTopLeft(), true) == ArrangeView::PointHandle::compressLeft);
    // The bottom edge stretches them along the song.
    {
        juce::String sel;
        for (const auto i : arrange.getPointSelection().points)
            sel << i << " ";
        INFO ("selected before the stretch: " << sel);
        CHECK (arrange.getPointSelection().points == std::vector<int> { 0, 1, 2 });
    }
    box = arrange.getPointBox();
    const auto stretchAt = juce::Point<float> (box.getCentreX(), box.getBottom());
    REQUIRE (arrange.pointHandleAt (stretchAt, false) == ArrangeView::PointHandle::stretch);
    drag (arrange, stretchAt, stretchAt + juce::Point<float> ((float) (timeline.xForBeat (4.0) - timeline.xForBeat (0.0)), 0.0f));
    CHECK_THAT (points()[0].beat, WithinAbs (1.0, 0.05));
    CHECK_THAT (points()[1].beat, WithinAbs (5.0, 0.05));
    CHECK_THAT (points()[2].beat, WithinAbs (7.0, 1.0e-9));   // (the one not selected, passed)
    CHECK_THAT (points()[3].beat, WithinAbs (9.0, 0.05));

    // Bends: the handle in a segment's middle; dragging it up bows the ramp up; a double-click straightens it.
    studio.setAutomationPoints (trackId, 0, { { 2.0, 0.2f, 0.0f }, { 10.0, 0.8f, 0.0f } });
    arrange.selectPoints (trackId, 0, {});
    const auto handle = arrange.getCurveHandle (trackId, 0, 1);
    REQUIRE (arrange.curveHandleAt (trackId, 0, handle) == 1);
    drag (arrange, handle, handle + juce::Point<float> (0.0f, -area.getHeight() * 0.2f));
    CHECK (points()[1].curve > 0.1f);
    CHECK (*(*studio.getAutomation (trackId))[0].valueAt (6.0) > 0.6f);
    const auto bent = arrange.getCurveHandle (trackId, 0, 1);
    arrange.mouseDoubleClick (mouseAt (arrange, bent, bent, {}, 2));
    CHECK_THAT (points()[1].curve, WithinAbs (0.0, 1.0e-6));
    // Quick vertical scaling: the handle at the lane's top over the segment scales its two points.
    const auto top = juce::Point<float> ((timeline.xForBeat (2.0) + timeline.xForBeat (10.0)) * 0.5f, area.getY() + 2.0f);
    REQUIRE (arrange.curveHandleAt (trackId, 0, top) == 1);
    drag (arrange, top, top + juce::Point<float> (0.0f, area.getHeight() * 0.5f));
    CHECK_THAT (points()[0].value, WithinAbs (0.1, 0.01));
    CHECK_THAT (points()[1].value, WithinAbs (0.4, 0.02));

    // DRAW draws; ERASE takes away what it crosses.
    arrange.setTool (ArrangeView::Tool::draw);
    drag (arrange, at (12.0, 0.5f), at (14.0, 0.9f));
    CHECK (points().size() > 2);
    arrange.setTool (ArrangeView::Tool::erase);
    drag (arrange, at (11.5, 0.5f), at (16.0, 0.5f));
    CHECK (points().size() == 2);
    // LINE: a line is its two ends; a sine many points round where it began.
    arrange.setTool (ArrangeView::Tool::line);
    arrange.setLineShape (ArrangeView::LineShape::line);
    drag (arrange, at (16.0, 0.1f), at (20.0, 0.9f));
    REQUIRE (points().size() == 4);
    CHECK_THAT (points()[2].beat, WithinAbs (16.0, 1.0e-6));
    CHECK_THAT (points()[3].beat, WithinAbs (20.0, 1.0e-6));
    arrange.setLineShape (ArrangeView::LineShape::sine);
    drag (arrange, at (24.0, 0.5f), at (26.0, 0.8f));
    CHECK (points().size() > 20);
    auto highest = 0.0f, lowest = 1.0f;
    for (const auto& p : points())
        if (p.beat >= 24.0)
            highest = juce::jmax (highest, p.value), lowest = juce::jmin (lowest, p.value);
    CHECK (highest > 0.75f);
    CHECK (lowest < 0.25f);
    arrange.setTool (ArrangeView::Tool::select);
}

TEST_CASE ("0.34.4: automation and tempo lanes drawn strip by strip (as the playhead redraws them) give the same pixels as drawn whole", "[automation][ui][performance]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setAutomationOpen (trackId, true);
    studio.appendAutomationLane (trackId, 0);
    // Many points over a long song: ramps, bent ramps both ways, a terminator (a gap with virgin territory).
    std::vector<AutomationPoint> volume, pan;
    for (int i = 0; i < 400; ++i)
    {
        AutomationPoint p;
        p.beat = i * 1.37;
        p.value = 0.5f + 0.45f * (float) std::sin (i * 0.7);
        p.curve = i % 3 == 0 ? 0.0f : (i % 3 == 1 ? 0.6f : -0.8f);
        p.terminator = i == 150;
        volume.push_back (p);
        p.value = 0.5f + 0.4f * (float) std::cos (i * 0.3);
        p.curve = 0.0f;
        p.terminator = false;
        pan.push_back (p);
    }
    studio.setAutomationPoints (trackId, 0, volume);
    studio.setAutomationPoints (trackId, 1, pan);
    studio.addTempoTrack();
    studio.setTempoTrackOn (false);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = view.getTimeline();

    const auto whole = [&]
    {
        juce::Image image (juce::Image::ARGB, arrange.getWidth(), arrange.getHeight(), true, juce::SoftwareImageType());   // (JUCE's own renderer everywhere: a Mac's shades a cut edge differently)
        juce::Graphics g (image);
        arrange.paintEntireComponent (g, true);
        return image;
    };
    const auto inStrips = [&] (int width, int height)
    {
        juce::Image image (juce::Image::ARGB, arrange.getWidth(), arrange.getHeight(), true, juce::SoftwareImageType());   // (JUCE's own renderer everywhere: a Mac's shades a cut edge differently)
        juce::Graphics g (image);
        for (int x = 0; x < arrange.getWidth(); x += width)
            for (int y = 0; y < arrange.getHeight(); y += height)
            {
                juce::Graphics::ScopedSaveState saved (g);
                g.reduceClipRegion (x, y, width, height);
                arrange.paintEntireComponent (g, true);
            }
        return image;
    };
    // JUCE's edges where a redraw cuts a line differ by a shade from the line drawn whole, so
    // strips are checked against the whole within a few levels (the culled curve would be off by
    // whole pixels); PARAGON_STRIPS=<folder> saves them, to compare two builds exactly.
    const auto close = [] (const juce::Image& a, const juce::Image& b)
    {
        auto far = 0;
        for (int y = 0; y < a.getHeight(); ++y)
            for (int x = 0; x < a.getWidth(); ++x)
            {
                const auto p = a.getPixelAt (x, y), q = b.getPixelAt (x, y);
                const auto d = juce::jmax (std::abs (p.getRed() - q.getRed()), std::abs (p.getGreen() - q.getGreen()), std::abs (p.getBlue() - q.getBlue()));
                far += d > 24 ? 1 : 0;
            }
        return far;
    };
    auto pass = 0;
    const auto check = [&]
    {
        const auto all = whole();
        const juce::Image strips[] = { inStrips (7, all.getHeight()), inStrips (37, 23), inStrips (all.getWidth(), 31) };
        for (const auto& image : strips)
            CHECK (close (all, image) == 0);
        if (const auto folder = juce::SystemStats::getEnvironmentVariable ("PARAGON_STRIPS", {}); folder.isNotEmpty())
            for (int i = 0; i < 3; ++i)
            {
                juce::FileOutputStream out (juce::File (folder).getChildFile ("strips" + juce::String (pass) + "-" + juce::String (i) + ".png"));
                out.setPosition (0);
                out.truncate();
                juce::PNGImageFormat().writeImageToStream (strips[i], out);
            }
        ++pass;
    };
    check();                         // the song's start, zoomed out
    timeline.firstBeat = 150.0;      // in the middle, the gap's edge in view
    for (int i = 0; i < 3; ++i)
        arrange.zoom (true);
    check();
    studio.setTrim (true);           // trimming: the played curve sampled every 3 px
    std::vector<AutomationPoint> trim { { 160.0, 0.7f }, { 190.0, 0.3f } };
    studio.setTrimPoints (trackId, 0, trim);
    studio.dispatchPendingMessages();
    check();
}

TEST_CASE ("0.34.4: the transport drawn screen by screen (as its timer redraws them) gives the pixels it gave", "[ui][performance]")
{
    for (const auto finish : { Finish::Type::pearlWhite, Finish::Type::pearlBlack })
    {
        Finish::set (finish, false);
        Studio studio (false);
        MainView view (studio);
        for (const auto width : { 1500, 3000 })   // (the wide layout too)
        {
            view.setSize (width, 900);
            auto& bar = view.getTransport();
            juce::Image whole (juce::Image::ARGB, bar.getWidth(), bar.getHeight(), true, juce::SoftwareImageType());   // (JUCE's own renderer everywhere: a Mac's shades a cut edge differently)
            {
                juce::Graphics g (whole);
                bar.paintEntireComponent (g, true);
            }
            juce::Image strips (juce::Image::ARGB, bar.getWidth(), bar.getHeight(), true, juce::SoftwareImageType());   // (JUCE's own renderer everywhere: a Mac's shades a cut edge differently)
            {
                juce::Graphics g (strips);
                for (int x = 0; x < bar.getWidth(); x += 29)
                {
                    juce::Graphics::ScopedSaveState saved (g);
                    g.reduceClipRegion (x, 0, 29, bar.getHeight());
                    bar.paintEntireComponent (g, true);
                }
            }
            auto far = 0;
            for (int y = 0; y < whole.getHeight(); ++y)
                for (int x = 0; x < whole.getWidth(); ++x)
                {
                    const auto p = whole.getPixelAt (x, y), q = strips.getPixelAt (x, y);
                    far += juce::jmax (std::abs (p.getRed() - q.getRed()), std::abs (p.getGreen() - q.getGreen()), std::abs (p.getBlue() - q.getBlue())) > 24 ? 1 : 0;
                }
            CHECK (far == 0);
            if (const auto folder = juce::SystemStats::getEnvironmentVariable ("PARAGON_STRIPS", {}); folder.isNotEmpty())
            {
                juce::FileOutputStream out (juce::File (folder).getChildFile ("transport-" + juce::String ((int) finish) + "-" + juce::String (width) + ".png"));
                out.setPosition (0);
                out.truncate();
                juce::PNGImageFormat().writeImageToStream (strips, out);
            }
        }
    }
    Finish::set (Finish::Type::pearlWhite, false);
}
