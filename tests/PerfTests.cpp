#include <thread>
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
    // The owner's case (0.9.9): three screens wide, the mixer docked; a click in the ruler, the cycle key.
    view.toggleMixerDocked();
    studio.dispatchPendingMessages();
    std::printf ("(mixer docked: %d)\n", (int) view.getMixer().isShowing());
    auto& self = studio;
    const auto mixer = &view.getMixer();
    time ("5760 wide: setPosition + messages", [&] { self.setPosition (std::fmod (self.getEngine().getPosition() + 3.0, 300.0)); self.dispatchPendingMessages(); });
    time ("5760 wide: setCycleOn + messages", [&] { self.setCycleOn (! self.getProject().loopOn); self.dispatchPendingMessages(); });
    time ("5760 wide: paint the whole window", [&] { juce::Graphics g (wide); view.paintEntireComponent (g, false); });
    time ("5760 wide: paint the mixer", [&] { juce::Graphics g (wide); mixer->paintEntireComponent (g, false); });
    time ("MIDI devices listed", [&] { juce::MidiInput::getAvailableDevices(); });
}

// Hidden: the owner's "sticks for a second" (0.9.9). A heavy plugin (20 ms a
// block) on an audio thread of its own, while the screen follows a click in
// the ruler and the cycle key, with the mixer docked.
TEST_CASE ("Performance: the screen with heavy plugins playing on the audio thread", "[.][perf]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1920, 1000);
    for (int t = 1; t < 6; ++t)
        studio.addTrack ({});
    for (const auto& track : studio.getProject().tracks)
        for (int slot = 0; slot < 3; ++slot)
        {
            Insert insert;
            insert.plugin.name = "Heavy";
            studio.addInsert (track.id, insert, std::make_unique<TestEffect> ([] (juce::AudioBuffer<float>&) { juce::Thread::sleep (1); }, 2, "Heavy"));
        }
    view.toggleMixerDocked();
    studio.dispatchPendingMessages();

    std::atomic<bool> running { true };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> out (2, 512);
        while (running)
        {
            studio.getEngine().render (out, 512);   // 18 effects x 1 ms: about 20 ms a block, under the lock
            juce::Thread::sleep (2);                // the gap before the next block
        }
    });
    juce::Thread::sleep (100);
    const auto time = [&] (const char* what, std::function<void()> action)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < 10; ++i)
            action();
        std::printf ("%-55s %8.2f ms\n", what, (juce::Time::getMillisecondCounterHiRes() - start) / 10.0);
    };
    time ("heavy: setPosition + messages", [&] { studio.setPosition (std::fmod (studio.getEngine().getPosition() + 3.0, 300.0)); studio.dispatchPendingMessages(); });
    time ("heavy: setCycleOn + messages", [&] { studio.setCycleOn (! studio.getProject().loopOn); studio.dispatchPendingMessages(); });
    time ("heavy: timers once (meters)", [&] { juce::Timer::callPendingTimersSynchronously(); });
    running = false;
    audio.join();
}

// Hidden: saving a song whose plugins keep large settings (Kontakt's can be megabytes), 0.9.11.
TEST_CASE ("Performance: saving with large plugin settings", "[.][perf]")
{
    Studio studio (false);
    for (int t = 1; t < 10; ++t)
        studio.addTrack ({});
    const auto megabytes = juce::SystemStats::getEnvironmentVariable ("PERF_MB", "8").getDoubleValue();
    juce::MemoryOutputStream text;
    for (int i = 0; i < (int) (megabytes * 52000); ++i)
        text << "kontakt-patch-data-";
    const auto big = text.toString();
    const auto made = juce::Time::getMillisecondCounterHiRes();
    for (const auto& track : studio.getProject().tracks)
    {
        Insert insert;
        insert.plugin.name = "Heavy";
        auto effect = std::make_unique<TestEffect> ([] (juce::AudioBuffer<float>&) {}, 2, "Heavy");
        effect->state = big;
        studio.addInsert (track.id, insert, std::move (effect));
    }
    std::printf ("setting up                                          %8.1f ms\n", juce::Time::getMillisecondCounterHiRes() - made);
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-perf-save.pr7s");
    for (int i = 0; i < 3; ++i)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        studio.save (file);
        std::printf ("save (10 tracks)                                    %8.1f ms (%lld bytes)\n",
                     juce::Time::getMillisecondCounterHiRes() - start, (long long) file.getSize());
    }
    {
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        const auto xml = studio.getProject().toXml();
        auto t1 = juce::Time::getMillisecondCounterHiRes();
        xml->writeTo (file);
        auto t2 = juce::Time::getMillisecondCounterHiRes();
        std::printf ("  of which toXml %.1f ms, writeTo %.1f ms\n", t1 - t0, t2 - t1);
    }
    // (0.85.2) An auto-save copy: what the screen's thread waits for (the writing goes on in the background).
    studio.setAutoSaveMinutes (5);
    for (int i = 0; i < 3; ++i)
    {
        studio.renameTrack (studio.getProject().tracks[0].id, "Changed " + juce::String (i));
        const auto start = juce::Time::getMillisecondCounterHiRes();
        studio.writeBackup();
        const auto asked = juce::Time::getMillisecondCounterHiRes();
        studio.waitForSaves (60000);
        std::printf ("auto-save copy: the screen waits %.1f ms (written after %.1f ms)\n", asked - start,
                     juce::Time::getMillisecondCounterHiRes() - start);
    }
    studio.backupFolder().deleteRecursively();
    file.deleteFile();
}
