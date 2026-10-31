#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

using namespace pr7s;

// Hidden ("[.]"): how long the screen takes to follow a change, on a big song
// (12 tracks, 40 parts each, 64 notes a part). Run:
//   xvfb-run -a build/tests/ParagonPR7STests "[perf]"
TEST_CASE ("Performance: the screen following changes on a big song", "[.][perf]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1920, 1000);
    for (int t = 1; t < 12; ++t)
        studio.addTrack ({});
    for (const auto& track : studio.getProject().tracks)
        for (int p = 0; p < 40; ++p)
        {
            Part part { p * 8.0, 8.0, "Part", {} };
            for (int n = 0; n < 64; ++n)
                part.notes.push_back ({ n * 0.125, 0.1, 40 + (n * 7) % 40, 0.8f });
            studio.addPart (track.id, part);
        }
    studio.dispatchPendingMessages();
    view.openKeyEditor();
    studio.dispatchPendingMessages();

    const auto time = [&] (const char* what, std::function<void()> action)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < 10; ++i)
            action();
        const auto ms = (juce::Time::getMillisecondCounterHiRes() - start) / 10.0;
        std::printf ("%-55s %8.2f ms\n", what, ms);
    };
    auto image = juce::Image (juce::Image::ARGB, 1920, 1000, true);
    time ("setPosition + messages", [&] { studio.setPosition (std::fmod (studio.getEngine().getPosition() + 3.0, 300.0)); studio.dispatchPendingMessages(); });
    time ("setNotes (one edit) + messages", [&]
    {
        auto notes = studio.getProject().tracks[0].parts[0].notes;
        notes[0].pitch = notes[0].pitch == 60 ? 61 : 60;
        studio.setNotes (studio.getProject().tracks[0].id, 0, notes);
        studio.dispatchPendingMessages();
    });
    time ("selectTrack + messages", [&] { studio.selectTrack (studio.getProject().tracks[(size_t) (juce::Random::getSystemRandom().nextInt (12))].id); studio.dispatchPendingMessages(); });
    time ("paint the whole window", [&] { juce::Graphics g (image); view.paintEntireComponent (g, false); });
    time ("paint the arrangement", [&] { juce::Graphics g (image); view.getArrangeView().paintEntireComponent (g, false); });
    time ("paint the track list", [&] { juce::Graphics g (image); view.getTrackList().paintEntireComponent (g, false); });
    time ("paint the key editor", [&] { juce::Graphics g (image); view.getKeyEditor().paintEntireComponent (g, false); });
    time ("paint the Inspector", [&] { juce::Graphics g (image); view.getInspector().paintEntireComponent (g, false); });
    time ("timers once", [&] { juce::Timer::callPendingTimersSynchronously(); });
    // Three screens wide (the owner's): a whole redraw of the arrangement, against the playhead's strip alone.
    view.setSize (5760, 1000);
    auto wide = juce::Image (juce::Image::ARGB, 5760, 1000, true);
    auto& arrange = view.getArrangeView();
    time ("5760 wide: the whole arrangement", [&] { juce::Graphics g (wide); arrange.paintEntireComponent (g, false); });
    time ("5760 wide: the playhead's strip (17 px)", [&]
    {
        juce::Graphics g (wide);
        g.reduceClipRegion (juce::Rectangle<int> (1000, 0, 17, arrange.getHeight()));
        arrange.paintEntireComponent (g, false);
    });
    time ("MIDI devices listed", [&] { juce::MidiInput::getAvailableDevices(); });
}
