#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    // An engine with tracks (steady levels), FX and group channels, read off the output once settled.
    struct Desk
    {
        Engine engine;
        Project project;
        Desk() { engine.prepare (48000.0, 512); }
        Track& addTrack (int id, float level)
        {
            Track track;
            track.id = id;
            project.tracks.push_back (track);
            engine.setInstrument (id, std::make_unique<SteadyTone> (level, level));
            engine.update (project);
            return project.tracks.back();
        }
        Track& addChannel (int id, bool group)
        {
            Track track;
            track.id = id;
            track.kind = group ? Track::Kind::group : Track::Kind::fx;
            project.tracks.push_back (track);
            engine.addChannel (id, group);
            engine.update (project);
            return project.tracks.back();
        }
        Track& track (int id) { return *project.findTrack (id); }
        float output()
        {
            engine.update (project);
            return renderSamples (engine, 4096).back();
        }
    };
}

TEST_CASE ("Sends: a track's sound to an FX channel, through its effects, back into the master; after the fader or before it", "[engine][sends]")
{
    Desk desk;
    desk.addTrack (1, 0.5f);
    desk.addChannel (2, false);
    desk.engine.insertEffect (2, 0, timesEffect (0.5f));   // the FX channel's "reverb": half
    desk.track (2).inserts.resize (1);
    CHECK_THAT (desk.output(), WithinAbs (0.5, 1.0e-5));   // no send: the track alone

    desk.track (1).sends = { { 2, 0.0f, true, false } };   // a send at 0 dB, after the fader
    CHECK_THAT (desk.output(), WithinAbs (0.75, 1.0e-5)); // 0.5, and 0.5 through the FX's half

    // The send's level: -6 dB is half.
    desk.track (1).sends[0].levelDb = juce::Decibels::gainToDecibels (0.5f);
    CHECK_THAT (desk.output(), WithinAbs (0.625, 1.0e-4));
    desk.track (1).sends[0].levelDb = 0.0f;

    // After the fader: the fader down takes the send with it. Before it: the send goes on.
    desk.track (1).volumeDb = -60.0f;
    CHECK_THAT (desk.output(), WithinAbs (0.0, 1.0e-5));
    desk.track (1).sends[0].preFader = true;
    CHECK_THAT (desk.output(), WithinAbs (0.25, 1.0e-5));
    desk.track (1).volumeDb = 0.0f;

    // Off, or the channel's sends bypassed: just the track.
    desk.track (1).sends[0].on = false;
    CHECK_THAT (desk.output(), WithinAbs (0.5, 1.0e-5));
    desk.track (1).sends[0].on = true;
    desk.track (1).sendsBypass = true;
    CHECK_THAT (desk.output(), WithinAbs (0.5, 1.0e-5));
    desk.track (1).sendsBypass = false;

    // The FX channel's own fader: its return.
    desk.track (2).volumeDb = -60.0f;
    CHECK_THAT (desk.output(), WithinAbs (0.5, 1.0e-5));
    desk.track (2).volumeDb = 0.0f;

    // Two tracks to the same FX channel: it gets both.
    desk.addTrack (3, 0.25f).sends = { { 2, 0.0f, true, false } };
    CHECK_THAT (desk.output(), WithinAbs (0.5 + 0.25 + (0.5 + 0.25) * 0.5, 1.0e-5));

    // Solo a track: the others go quiet, the FX channel it sends to doesn't.
    desk.track (1).solo = true;
    CHECK_THAT (desk.output(), WithinAbs (0.75, 1.0e-5));
    desk.track (1).solo = false;

    // The send's level automated (R on): down to nothing from the lane.
    auto lane = AutomationLane();
    lane.target = AutomationLane::Target::send;
    lane.slot = 0;
    lane.points = { { 0.0, 0.0f } };
    desk.track (1).automation = { lane };
    desk.track (1).automationRead = true;
    CHECK_THAT (desk.output(), WithinAbs (0.5 + 0.25 + 0.25 * 0.5, 1.0e-5));
    CHECK (desk.engine.getSendNowDb (1, 0) == -60.0f);
}

TEST_CASE ("Group channels: tracks routed to one go through its effects and fader, then the master; a group sends too", "[engine][sends]")
{
    Desk desk;
    desk.addTrack (1, 0.5f);
    desk.addTrack (2, 0.25f);
    desk.addChannel (3, true);
    desk.track (1).output = 3;
    desk.track (2).output = 3;
    CHECK_THAT (desk.output(), WithinAbs (0.75, 1.0e-5));   // the group at 0 dB: the same

    // The group's fader: both tracks with it.
    desk.track (3).volumeDb = juce::Decibels::gainToDecibels (0.5f);
    CHECK_THAT (desk.output(), WithinAbs (0.375, 1.0e-4));
    desk.track (3).volumeDb = 0.0f;

    // Its effects: on the two together.
    desk.engine.insertEffect (3, 0, plusEffect (0.1f));
    desk.track (3).inserts.resize (1);
    CHECK_THAT (desk.output(), WithinAbs (0.85, 1.0e-5));

    // A group sends to an FX channel as a track does.
    desk.addChannel (4, false);
    desk.track (3).sends = { { 4, 0.0f, true, false } };
    CHECK_THAT (desk.output(), WithinAbs (0.85 * 2.0, 1.0e-5));

    // Muting the group mutes what goes through it.
    desk.track (3).mute = true;
    CHECK_THAT (desk.output(), WithinAbs (0.0, 1.0e-5));
}

TEST_CASE ("The studio's FX and group channels: added (named as the manual's), sends and outputs set, undone, saved, removed", "[studio][sends]")
{
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    Insert reverb;
    reverb.plugin.name = "Reverb";
    const auto fx = studio.addFxChannel (reverb, timesEffect (1.0f, "Reverb"));
    REQUIRE (fx != 0);
    const auto* fxTrack = studio.getProject().findTrack (fx);
    CHECK (fxTrack->isFx());
    CHECK (fxTrack->name == "FX 1-Reverb");
    CHECK (fxTrack->inserts.size() == 1);
    CHECK (studio.getEngine().getEffect (fx, 0)->getName() == "Reverb");
    const auto group = studio.addGroupChannel();
    CHECK (studio.getProject().findTrack (group)->name == "Group 1");

    // A channel can't be armed (it records nothing).
    studio.setArmed (fx);
    CHECK_FALSE (studio.getProject().findTrack (fx)->armed);

    // A send and an output; each one undo step.
    studio.setSend (track, 0, { fx, -6.0f, true, false });
    REQUIRE (studio.getProject().findTrack (track)->sends.size() == 1);
    studio.setOutput (track, group);
    CHECK (studio.getProject().findTrack (track)->output == group);
    studio.setOutput (track, fx);   // an FX channel isn't an output
    CHECK (studio.getProject().findTrack (track)->output == group);
    studio.undo();
    CHECK (studio.getProject().findTrack (track)->output == 0);
    studio.redo();
    studio.setSendLevel (track, 0, -12.0f);
    CHECK (studio.getProject().findTrack (track)->sends[0].levelDb == -12.0f);

    // The send's level is an automatable control, named after its FX channel.
    const auto controls = studio.getAutomationControls (track);
    CHECK (std::any_of (controls.begin(), controls.end(), [] (const auto& c) { return c.target == AutomationLane::Target::send && c.name == "Send 1: FX 1-Reverb"; }));

    // Saved and opened.
    const auto saved = Project::fromXml (*studio.getProject().toXml());
    REQUIRE (saved.has_value());
    CHECK (saved->findTrack (fx)->isFx());
    CHECK (saved->findTrack (group)->isGroup());
    CHECK (saved->findTrack (track)->sends == studio.getProject().findTrack (track)->sends);
    CHECK (saved->findTrack (track)->output == group);

    // Removing the FX channel takes the send; removing the group, the routing.
    studio.removeTrack (fx);
    CHECK (studio.getProject().findTrack (track)->sends.empty());
    studio.removeTrack (group);
    CHECK (studio.getProject().findTrack (track)->output == 0);
}
