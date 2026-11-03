// The Automation panel (0.28.0): modes (Touch, Auto-Latch, Cross-Over), Fill, Suspend Read and
// Write, Trim (writing, playing, freezing), virgin territory (gaps, terminators), the functions,
// Show, and the panel itself.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/AutomationPanel.h"
#include "ui/Finish.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;
using T = AutomationLane::Target;
using Settings = Studio::AutomationSettings;

namespace
{
    constexpr int beat = 24000;   // samples at 120 BPM, 48 kHz

    struct Writer
    {
        Studio studio { false };
        int track = studio.getProject().tracks[0].id;
        AutomationLane volume = Studio::control (T::volume);
        void playTo (double beatNow)
        {
            while (studio.getEngine().getPosition() < beatNow - 1.0e-9)
                renderSamples (studio.getEngine(), juce::jmin (512, (int) std::ceil ((beatNow - studio.getEngine().getPosition()) * beat)));
        }
        const AutomationLane& lane() { return studio.getAutomation (track)->front(); }
        float dbAt (double b) { return travelToVolume (*lane().valueAt (b)); }
        // A pass: grabbed at `from`, the fader at `db` until `to`, let go there; then on to `stopAt`.
        void pass (double from, float db, double to, double stopAt)
        {
            studio.setAutomationWrite (track, true);
            if (! studio.getEngine().isPlaying())
                studio.play();
            playTo (from);
            studio.grabControl (track, volume);
            studio.setVolume (track, db, false);
            playTo (to);
            studio.setVolume (track, db, true);
            studio.releaseControl (track, volume);
            playTo (stopAt);
            studio.stop();
        }
    };
}

TEST_CASE ("Automation settings, terminators and trims are saved with the song", "[automation][panel]")
{
    Project project;
    auto& a = project.automationSettings;
    a.mode = Settings::Mode::crossOver;
    a.toEnd = Settings::Fill::locked;
    a.loop = Settings::Fill::once;
    a.suspendRead[1] = true;
    a.suspendWrite[2] = true;
    a.trim = true;
    a.freezeTrim = Settings::FreezeTrim::onPassEnd;
    a.virginTerritory = true;
    a.continueOnJump = true;
    a.revealOnWrite = false;
    a.returnSeconds = 0.25;
    a.reductionLevel = 20;
    Track track;
    track.id = 1;
    AutomationLane lane;
    lane.points = { { 0.0, 0.5f, 0.0f, true }, { 4.0, 0.8f, 0.0f, false } };
    lane.trim = { { 0.0, 0.6f, 0.0f, false } };
    track.automation.push_back (lane);
    project.tracks.push_back (track);
    const auto opened = Project::fromXml (*project.toXml());
    REQUIRE (opened.has_value());
    CHECK (opened->automationSettings == a);
    CHECK (opened->tracks[0].automation[0].points == lane.points);
    CHECK (opened->tracks[0].automation[0].trim == lane.trim);
}

TEST_CASE ("Virgin territory: nothing before the first point, after the last, or in a gap; trim plays on top", "[automation][panel]")
{
    AutomationLane lane;
    lane.points = { { 2.0, 0.2f }, { 4.0, 0.6f, 0.0f, true }, { 8.0, 0.4f }, { 10.0, 0.9f } };
    CHECK (*lane.valueAt (1.0) == 0.2f);              // (initial value: the first point's)
    CHECK_FALSE (lane.valueAt (1.0, true).has_value());
    CHECK (lane.valueAt (3.0, true).has_value());
    CHECK_FALSE (lane.valueAt (6.0, true).has_value());   // the gap after the terminator
    CHECK (lane.valueAt (9.0, true).has_value());
    CHECK_FALSE (lane.valueAt (12.0, true).has_value());  // after the last
    CHECK (lane.valueAt (12.0, false).has_value());
    // Trim: +3 dB over -6 dB plays -3 dB.
    AutomationLane volume;
    volume.target = T::volume;
    volume.points = { { 0.0, volumeToTravel (-6.0f) } };
    volume.trim = { { 0.0, 0.5f + 3.0f / (2.0f * AutomationLane::trimRange) } };
    CHECK_THAT (travelToVolume (*volume.playedAt (1.0, false)), WithinAbs (-3.0, 1.0e-3));
    CHECK_THAT (AutomationLane::trimDb (volume.trim, 1.0), WithinAbs (3.0, 1.0e-4));
}

TEST_CASE ("The modes: Touch returns on letting go; Auto-Latch keeps writing the last value; Cross-Over punches out crossing the old curve", "[automation][panel]")
{
    SECTION ("Touch")
    {
        Writer w;
        w.pass (1.0, -12.0f, 2.0, 4.0);
        CHECK_THAT (w.dbAt (1.5), WithinAbs (-12.0, 0.6));
        CHECK_THAT (w.dbAt (3.0), WithinAbs (0.0, 1.0e-3));   // back after letting go
    }
    SECTION ("Auto-Latch")
    {
        Writer w;
        w.studio.setAutomationMode (Settings::Mode::autoLatch);
        w.pass (1.0, -12.0f, 2.0, 4.0);
        CHECK_THAT (w.dbAt (1.5), WithinAbs (-12.0, 0.6));
        CHECK_THAT (w.dbAt (3.5), WithinAbs (-12.0, 0.6));   // still -12 after letting go, to the stop
        CHECK_THAT (w.dbAt (4.5), WithinAbs (0.0, 1.0e-3));  // back after it
    }
    SECTION ("Cross-Over")
    {
        Writer w;
        // An old curve at -6 dB; a pass at -12, let go (latched), touched again and moved up past -6: it punches out there.
        w.studio.setAutomationMode (Settings::Mode::crossOver);
        w.studio.setAutomationOpen (w.track, true);
        w.studio.setAutomationPoints (w.track, 0, { { 0.0, volumeToTravel (-6.0f) }, { 16.0, volumeToTravel (-6.0f) } });   // (the old curve)
        w.studio.setAutomationWrite (w.track, true);
        w.studio.play();
        w.playTo (1.0);
        w.studio.grabControl (w.track, w.volume);
        w.studio.setVolume (w.track, -12.0f, false);
        w.playTo (2.0);
        w.studio.releaseControl (w.track, w.volume);   // let go: latched at -12
        w.playTo (3.0);
        CHECK (w.studio.isWritingAutomation (w.track));
        w.studio.grabControl (w.track, w.volume);     // touched again
        w.studio.setVolume (w.track, -9.0f, false);
        w.playTo (3.5);
        w.studio.setVolume (w.track, -3.0f, false);   // across -6: out
        CHECK_FALSE (w.studio.isWritingAutomation (w.track));
        w.studio.releaseControl (w.track, w.volume);
        w.playTo (5.0);
        w.studio.stop();
        CHECK_THAT (w.dbAt (2.5), WithinAbs (-12.0, 0.6));   // latched after letting go
        CHECK_THAT (w.dbAt (4.5), WithinAbs (-6.0, 0.1));    // the old curve again after the crossing
    }
}

TEST_CASE ("Fill: to punch, to start, to end, loop; once is used up, locked stays", "[automation][panel]")
{
    SECTION ("To Punch")
    {
        Writer w;
        w.studio.stepFill (Studio::FillOption::toPunch);
        CHECK (w.studio.getFill (Studio::FillOption::toPunch) == Settings::Fill::once);
        w.studio.setAutomationWrite (w.track, true);
        w.studio.play();
        w.playTo (1.0);
        w.studio.grabControl (w.track, w.volume);
        w.studio.setVolume (w.track, -20.0f, false);   // searching...
        w.playTo (2.0);
        w.studio.setVolume (w.track, -8.0f, false);    // ...found
        w.playTo (3.0);
        w.studio.releaseControl (w.track, w.volume);
        w.playTo (4.0);
        w.studio.stop();
        CHECK_THAT (w.dbAt (1.5), WithinAbs (-8.0, 0.1));   // the found value, from the punch in
        CHECK_THAT (w.dbAt (2.5), WithinAbs (-8.0, 0.1));
        CHECK (w.studio.getFill (Studio::FillOption::toPunch) == Settings::Fill::off);   // once: used up
    }
    SECTION ("To End and To Start")
    {
        Writer w;
        w.studio.setAutomationPoints (w.track, 0, {});
        w.studio.stepFill (Studio::FillOption::toEnd);
        w.studio.stepFill (Studio::FillOption::toEnd);
        CHECK (w.studio.getFill (Studio::FillOption::toEnd) == Settings::Fill::locked);
        w.pass (2.0, -10.0f, 3.0, 3.5);
        CHECK_THAT (w.dbAt (20.0), WithinAbs (-10.0, 0.1));   // to the end
        CHECK (w.studio.getFill (Studio::FillOption::toEnd) == Settings::Fill::locked);   // locked: stays
        w.studio.stepFill (Studio::FillOption::toEnd);
        CHECK (w.studio.getFill (Studio::FillOption::toEnd) == Settings::Fill::off);
        w.studio.stepFill (Studio::FillOption::toStart);
        w.pass (6.0, -4.0f, 7.0, 7.5);
        CHECK_THAT (w.dbAt (0.0), WithinAbs (-4.0, 0.1));     // from the start
    }
    SECTION ("Loop")
    {
        Writer w;
        w.studio.setCycle (8.0, 12.0);
        w.studio.stepFill (Studio::FillOption::loop);
        w.pass (1.0, -15.0f, 2.0, 2.5);
        CHECK_THAT (w.dbAt (8.5), WithinAbs (-15.0, 0.1));
        CHECK_THAT (w.dbAt (11.5), WithinAbs (-15.0, 0.1));
    }
}

TEST_CASE ("Suspend: read leaves the control by hand; write keeps it out of passes", "[automation][panel]")
{
    Writer w;
    w.studio.setAutomationOpen (w.track, true);
    w.studio.setAutomationPoints (w.track, 0, { { 0.0, volumeToTravel (-20.0f) } });
    w.studio.setPosition (1.0);
    renderSamples (w.studio.getEngine(), 512);
    CHECK_THAT (w.studio.getEngine().getMixNow (w.track).volumeDb, WithinAbs (-20.0, 1.0e-3));
    w.studio.setSuspendRead (Settings::Group::volume, true);
    renderSamples (w.studio.getEngine(), 512);
    CHECK_THAT (w.studio.getEngine().getMixNow (w.track).volumeDb, WithinAbs (0.0, 1.0e-3));   // the fader's own
    w.studio.setSuspendRead (Settings::Group::volume, false);
    // Suspend Write: a pass on volume writes nothing.
    w.studio.setSuspendWrite (Settings::Group::volume, true);
    const auto before = w.lane().points;
    w.pass (1.0, -3.0f, 2.0, 3.0);
    CHECK (w.lane().points == before);
    // SET ALL / Suspend All.
    w.studio.setAllSuspended (true);
    CHECK (w.studio.isAllSuspended());
    w.studio.setAllSuspended (false);
    CHECK_FALSE (w.studio.isAllSuspended());
}

TEST_CASE ("Trim: a pass writes the trim curve; frozen into the points", "[automation][panel]")
{
    Writer w;
    w.studio.setAutomationOpen (w.track, true);
    w.studio.setAutomationPoints (w.track, 0, { { 0.0, volumeToTravel (-6.0f) }, { 16.0, volumeToTravel (-6.0f) } });
    w.studio.setTrim (true);
    w.studio.setAutomationMode (Settings::Mode::autoLatch);
    // The fader at -3 (the curve's -6, trimmed +3) from beat 2 to the stop.
    w.pass (2.0, -3.0f, 3.0, 4.0);
    const auto& lane = w.lane();
    REQUIRE_FALSE (lane.trim.empty());
    CHECK_THAT (AutomationLane::trimDb (lane.trim, 3.0), WithinAbs (3.0, 0.2));
    CHECK_THAT (travelToVolume (*lane.valueAt (3.0)), WithinAbs (-6.0, 1.0e-3));            // the curve as it was
    CHECK_THAT (travelToVolume (*lane.playedAt (3.0, false)), WithinAbs (-3.0, 0.2));        // what plays
    // Freeze: into the points, the trim gone.
    w.studio.freezeTrim (w.track, 0);
    CHECK (w.lane().trim.empty());
    CHECK_THAT (travelToVolume (*w.lane().valueAt (3.0)), WithinAbs (-3.0, 0.2));
    // Freeze Trim "On Leaving Trim Mode".
    auto settings = w.studio.getAutomationSettings();
    settings.freezeTrim = Settings::FreezeTrim::onLeavingTrim;
    w.studio.setAutomationSettings (settings);
    w.studio.setTrimPoints (w.track, 0, { { 0.0, 0.5f - 2.0f / (2.0f * AutomationLane::trimRange) } });
    w.studio.setTrim (false);
    CHECK (w.lane().trim.empty());
    CHECK_THAT (travelToVolume (*w.lane().valueAt (0.0)), WithinAbs (-8.0, 0.2));
}

TEST_CASE ("The functions: delete (all, selected, range), fill gaps; Show", "[automation][panel]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    studio.setAutomationOpen (a, true);
    studio.setAutomationOpen (b, true);
    studio.setAutomationPoints (a, 0, { { 0.0, 0.2f }, { 8.0, 0.8f } });
    studio.setAutomationPoints (b, 0, { { 0.0, 0.3f }, { 8.0, 0.6f } });
    // In range: between the locators gone, the curve either side kept.
    studio.setCycle (2.0, 6.0);
    studio.deleteAutomationInRange();
    for (const auto& p : studio.getAutomation (a)->front().points)
        CHECK ((p.beat <= 2.0 + 1.0e-9 || p.beat > 6.0));
    // Of the selected tracks.
    studio.selectTrack (b);
    studio.deleteAutomationOfSelectedTracks();
    CHECK (studio.getAutomation (b)->front().points.empty());
    CHECK_FALSE (studio.getAutomation (a)->front().points.empty());
    // Fill gaps: a terminator's gap filled with its section's value.
    auto settings = studio.getAutomationSettings();
    settings.virginTerritory = true;
    studio.setAutomationSettings (settings);
    studio.setAutomationPoints (a, 0, { { 0.0, 0.2f, 0.0f, true }, { 8.0, 0.8f } });
    CHECK_FALSE (studio.getAutomation (a)->front().valueAt (4.0, true).has_value());
    studio.selectTrack (a);
    studio.fillGaps (false);
    CHECK_THAT (*studio.getAutomation (a)->front().valueAt (4.0, true), WithinAbs (0.2, 1.0e-4));
    // All.
    studio.deleteAllAutomation();
    CHECK (studio.getAutomation (a)->front().points.empty());
    // Show: every track's pan lane; Hide All.
    studio.hideAllAutomation();
    CHECK_FALSE (studio.isAutomationOpen (a));
    studio.showAutomationGroup (Settings::Group::pan, false);
    CHECK (studio.isAutomationOpen (a));
    CHECK (studio.isAutomationOpen (b));
    auto hasPan = false;
    for (const auto& l : *studio.getAutomation (b))
        hasPan = hasPan || (l.target == T::pan && l.shown);
    CHECK (hasPan);
}

TEST_CASE ("The Automation panel: its keys follow and set the studio's settings", "[automation][panel][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    AutomationPanel panel (studio);
    REQUIRE (panel.keyNamed ("AUTO-LATCH") != nullptr);
    panel.keyNamed ("AUTO-LATCH")->onClick();
    CHECK (studio.getAutomationSettings().mode == Settings::Mode::autoLatch);
    CHECK (panel.keyNamed ("AUTO-LATCH")->getToggleState());
    panel.keyNamed ("TO END")->onClick();
    CHECK (studio.getFill (Studio::FillOption::toEnd) == Settings::Fill::once);
    panel.keyNamed ("TO END")->onClick();
    CHECK (studio.getFill (Studio::FillOption::toEnd) == Settings::Fill::locked);
    CHECK (panel.keyNamed ("TO END (LOCKED)") != nullptr);
    panel.keyNamed ("TRIM")->onClick();
    CHECK (studio.getAutomationSettings().trim);
    panel.keyNamed ("SUSPEND")->onClick();
    CHECK (studio.isAllSuspended());
    panel.setTab (AutomationPanel::Tab::settings);
    panel.getReductionLevel().setValue (10.0, juce::sendNotificationSync);
    CHECK (studio.getAutomationSettings().reductionLevel == 10);
    panel.getReturnTime().setValue (250.0, juce::sendNotificationSync);
    CHECK_THAT (studio.getAutomationSettings().returnSeconds, WithinAbs (0.25, 1.0e-9));
}
