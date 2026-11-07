// 0.73.0: the owner's tweaks (TAP, M and S on the header's top row, the output meter as an LCD beside
// the CPU's, the tracks' numbers, a track moved by typing a number, the track's colour in the track list).

#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/MainView.h"
#include "ui/TrackColours.h"
#include "ui/TrackList.h"
#include "ui/TransportBar.h"

using namespace pr7s;

namespace
{
    std::vector<int> order (const Studio& studio)
    {
        std::vector<int> ids;
        for (const auto& t : studio.getProject().tracks)
            ids.push_back (t.id);
        return ids;
    }
}

TEST_CASE ("0.73.0: TAP, M and S are on the header; the output meter is an LCD beside the CPU's", "[ui][tweaks073]")
{
    Studio studio (false);
    MainView view (studio);
    view.setSize (1900, 1000);
    studio.dispatchPendingMessages();
    view.resized();
    CHECK (view.getTapButton().isVisible());
    CHECK (view.getMuteAllButton().isVisible());
    CHECK (view.getSoloAllButton().isVisible());
    // The keys sit on the header's row (above the transport).
    CHECK (view.getTapButton().getBottom() <= view.getTransport().getY());
    // The OUT screen is the CPU screen's size, right after it.
    auto& transport = view.getTransport();
    const auto cpu = transport.getCpuArea(), out = transport.getOutArea();
    CHECK (! out.isEmpty());
    CHECK (out.getWidth() == cpu.getWidth());
    CHECK (out.getY() == cpu.getY());
    CHECK (out.getX() > cpu.getRight());
    CHECK (out.getX() - cpu.getRight() < 20.0f);

    // M and S on the header turn every mute and solo off, as the track list's did.
    const auto id = studio.getProject().tracks[0].id;
    studio.setMute (id, true);
    studio.setSolo (id, true);
    studio.dispatchPendingMessages();
    view.getMuteAllButton().onClick();
    view.getSoloAllButton().onClick();
    CHECK_FALSE (studio.anyMute());
    CHECK_FALSE (studio.anySolo());
    // TAP taps.
    const auto before = studio.getProject().bpm;
    for (int i = 0; i < 4; ++i)
    {
        view.getTapButton().onClick();
        juce::Thread::sleep (500);
    }
    CHECK (std::abs (studio.getProject().bpm - 120.0) < 15.0);
    juce::ignoreUnused (before);
}

TEST_CASE ("0.73.0: tracks are numbered by place; a number typed moves the track there", "[studio][ui][tweaks073]")
{
    Studio studio (false);
    for (int i = 0; i < 9; ++i)
        studio.addTrack ({});
    const auto start = order (studio);   // ten tracks
    REQUIRE (start.size() == 10);
    // The eighth to the third: the third to the seventh move down one.
    REQUIRE (studio.moveTrackToSlot (start[7], 3));
    auto now = order (studio);
    CHECK (now == std::vector<int> { start[0], start[1], start[7], start[2], start[3], start[4], start[5], start[6], start[8], start[9] });
    // Down: the second to the ninth.
    REQUIRE (studio.moveTrackToSlot (now[1], 9));
    auto then = order (studio);
    CHECK (then[8] == now[1]);
    CHECK (then[1] == now[2]);
    CHECK (then[9] == now[9]);
    // Past the end: the last slot. Its own slot: nothing.
    REQUIRE (studio.moveTrackToSlot (then[0], 99));
    CHECK (order (studio).back() == then[0]);
    CHECK_FALSE (studio.moveTrackToSlot (then[0], 10));
    // One undo step puts it back.
    studio.undo();
    CHECK (order (studio) == then);

    // On screen: the numbers follow the places, and typing one moves the track.
    MainView view (studio);
    view.setSize (1500, 2400);
    studio.dispatchPendingMessages();
    view.resized();
    auto& list = view.getTrackList();
    const auto ids = order (studio);
    CHECK (list.getTrackNumber (ids[0]) == 1);
    CHECK (list.getTrackNumber (ids[4]) == 5);
    auto* label = list.getNumberLabel (ids[6]);
    REQUIRE (label != nullptr);
    CHECK (label->getText() == "7");
    label->setText ("2", juce::sendNotificationSync);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    studio.dispatchPendingMessages();
    CHECK (order (studio)[1] == ids[6]);
    CHECK (list.getTrackNumber (ids[6]) == 2);
    CHECK (list.getTrackNumber (ids[1]) == 3);
}

TEST_CASE ("0.73.0: a folder moved by number takes its tracks", "[studio][tweaks073]")
{
    Studio studio (false);
    for (int i = 0; i < 4; ++i)
        studio.addTrack ({});
    const auto start = order (studio);   // five
    const auto folder = studio.moveTracksToNewFolder ({ start[3], start[4] }, false);
    REQUIRE (folder != 0);
    const auto shown = studio.getShownTracks();
    const auto folderSlot = (int) (std::find (shown.begin(), shown.end(), folder) - shown.begin()) + 1;
    REQUIRE (studio.moveTrackToSlot (folder, 1));
    const auto now = order (studio);
    CHECK (now[0] == folder);
    CHECK (now[1] == start[3]);
    CHECK (now[2] == start[4]);
    CHECK (studio.getProject().findTrack (start[3])->folder == folder);
    juce::ignoreUnused (folderSlot);
}

TEST_CASE ("0.73.0: a track's colour in the track list: per track, 1-100 %, saved", "[studio][ui][tweaks073]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    TrackColourMenu menu (studio, { a });
    menu.setListColour (true, 40);
    CHECK (studio.getProject().findTrack (a)->listColourOn);
    CHECK (studio.getProject().findTrack (a)->listColourAmount == 40);
    CHECK_FALSE (studio.getProject().findTrack (b)->listColourOn);
    menu.setListColour (true, 500);
    CHECK (studio.getProject().findTrack (a)->listColourAmount == 100);
    menu.setListColour (true, 0);
    CHECK (studio.getProject().findTrack (a)->listColourAmount == 1);
    menu.setListColour (true, 65);
    // Saved and read back.
    const auto xml = studio.getProject().toXml();
    const auto back = Project::fromXml (*xml);
    REQUIRE (back.has_value());
    CHECK (back->findTrack (a)->listColourOn);
    CHECK (back->findTrack (a)->listColourAmount == 65);
    CHECK_FALSE (back->findTrack (b)->listColourOn);
    menu.setListColour (false, 65);
    CHECK_FALSE (studio.getProject().findTrack (a)->listColourOn);
}

TEST_CASE ("0.73.1: OUT shows one number, the loudest peak of the last second, refreshed about four times a second", "[ui][tweaks073]")
{
    Studio studio (false);
    TransportBar transport (studio);
    auto& engine = studio.getEngine();
    const auto push = [&] (float level)
    {
        const float left[] = { level }, right[] = { level * 0.5f };
        engine.getOutputMeter (0).pushSamples (left, 1);
        engine.getOutputMeter (1).pushSamples (right, 1);
    };
    CHECK (transport.getShownOutDb() <= LevelMeter::minDb);
    // A -6 dB peak: nothing changes until the window ends (8 ticks), then the number reads it.
    push (0.5f);
    for (int i = 0; i < TransportBar::outRefreshTicks - 1; ++i)
    {
        transport.followOutputForTests();
        CHECK (transport.getShownOutDb() <= LevelMeter::minDb);
    }
    transport.followOutputForTests();
    CHECK (std::abs (transport.getShownOutDb() + 6.02f) < 0.05f);
    // Quieter peaks after it: the -6 holds for the rest of the second (4 windows), then it shows -20.
    for (int window = 1; window < TransportBar::outWindows; ++window)
        for (int i = 0; i < TransportBar::outRefreshTicks; ++i)
        {
            push (0.1f);
            transport.followOutputForTests();
        }
    CHECK (std::abs (transport.getShownOutDb() + 6.02f) < 0.05f);
    for (int i = 0; i < TransportBar::outRefreshTicks; ++i)
    {
        push (0.1f);
        transport.followOutputForTests();
    }
    CHECK (std::abs (transport.getShownOutDb() + 20.0f) < 0.05f);
}
