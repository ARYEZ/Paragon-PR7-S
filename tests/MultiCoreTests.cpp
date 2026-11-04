#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/AudioWorkers.h"
#include "engine/Studio.h"

#include <mutex>
#include <set>
#include <thread>

// 0.34.5, multi-core audio: tracks processed side by side on helper threads must give the
// same output as one after another, to the last bit (their sums are made in the same order).

using namespace pr7s;

namespace
{
    // Which threads ran the effects (the helpers should take a share).
    std::mutex threadsLock;
    std::set<std::thread::id> threadsSeen;

    // An instrument playing noise and a tone of its own (every track sounds different, so any
    // mix-up of tracks, or sums made in another order, shows in the output's bits).
    struct Noise final : TestPlugin
    {
        explicit Noise (int seed)
            : TestPlugin (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)), random (seed), step (0.01 + seed * 0.003)
        {
            instrument = true;
            name = "Noise";
        }
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                phase += step;
                buffer.setSample (0, i, 0.3f * (random.nextFloat() * 2.0f - 1.0f) + 0.2f * (float) std::sin (phase));
                buffer.setSample (1, i, 0.3f * (random.nextFloat() * 2.0f - 1.0f) + 0.2f * (float) std::cos (phase * 1.3));
            }
        }
        juce::Random random;
        double phase = 0.0, step;
    };

    // A stateful effect (a filter remembering its last sample), with some work in it, noting its thread.
    std::unique_ptr<TestEffect> smear (float amount, const juce::String& name = "Smear")
    {
        auto memory = std::make_shared<std::array<float, 2>>();
        return std::make_unique<TestEffect> ([amount, memory] (juce::AudioBuffer<float>& b)
        {
            {
                const std::scoped_lock noted (threadsLock);
                threadsSeen.insert (std::this_thread::get_id());
            }
            for (int c = 0; c < juce::jmin (2, b.getNumChannels()); ++c)
                for (int i = 0; i < b.getNumSamples(); ++i)
                {
                    auto v = b.getSample (c, i);
                    for (int k = 0; k < 6; ++k)   // (work: something for the helpers to share)
                        v = std::tanh (v * 1.05f) * 0.99f;
                    (*memory)[(size_t) c] = (*memory)[(size_t) c] * amount + v * (1.0f - amount);
                    b.setSample (c, i, (*memory)[(size_t) c]);
                }
        }, 2, name);
    }

    struct Song
    {
        Engine engine;
        Project project;

        Song (int helpers, bool fxIntoFx)
        {
            engine.prepare (48000.0, 512);
            engine.setHelperThreads (helpers);
            const auto add = [&] (int id, Track::Kind kind)
            {
                Track track;
                track.id = id;
                track.kind = kind;
                project.tracks.push_back (track);
                return id;
            };
            for (int id = 1; id <= 24; ++id)
            {
                add (id, Track::Kind::instrument);
                if (id != 9)   // (track 9: no instrument at all)
                    engine.setInstrument (id, std::make_unique<Noise> (id));
            }
            add (30, Track::Kind::group);
            add (31, Track::Kind::group);
            add (40, Track::Kind::fx);
            add (41, Track::Kind::fx);
            for (const auto id : { 30, 31 })
                engine.addChannel (id, true);
            for (const auto id : { 40, 41 })
                engine.addChannel (id, false);
            engine.update (project);

            for (int id = 1; id <= 24; ++id)
            {
                auto& track = *project.findTrack (id);
                track.volumeDb = -2.0f - (float) (id % 7);
                track.pan = (float) ((id % 5) - 2) * 0.3f;
                track.mute = id == 7;
                if (id % 3 == 0)
                    track.output = id % 2 == 0 ? 30 : 31;
                track.sends = { { 40, -6.0f, true, true }, { 41, -3.0f - (float) (id % 4), id % 4 != 0, false } };
                engine.insertEffect (id, 0, smear (0.2f + 0.02f * (float) id));
                engine.insertEffect (id, 1, timesEffect (0.9f));
                track.inserts.resize (2);
                track.preFaderSlots = 1;
            }
            // Automation: track 5's volume ramps down and up (read every 32 samples).
            auto& five = *project.findTrack (5);
            AutomationLane lane;
            lane.points = { { 0.0, 0.8f }, { 4.0, 0.2f }, { 8.0, 0.9f } };
            five.automation = { lane };
            five.automationRead = true;

            project.findTrack (30)->sends = { { 41, -4.0f, true, false } };
            if (fxIntoFx)
                project.findTrack (40)->sends = { { 41, -8.0f, true, false } };   // (the studio never makes these: the engine's fallback)
            for (const auto id : { 30, 31, 40, 41 })
            {
                engine.insertEffect (id, 0, smear (0.5f, "Bus"));
                project.findTrack (id)->inserts.resize (1);
            }
            engine.insertEffect (0, 0, smear (0.3f, "Master"));
            project.masterInserts.resize (1);
            engine.update (project);
            engine.setPosition (0.0);
            engine.play();
        }
    };

    // The output of a song, block by block; halfway, faders move (they glide: the smoothed gains).
    std::vector<float> renderSong (int helpers, bool fxIntoFx)
    {
        Song song (helpers, fxIntoFx);
        std::vector<float> out;
        juce::AudioBuffer<float> buffer (2, 512);
        for (int block = 0; block < 240; ++block)
        {
            if (block == 120)
            {
                for (int id = 1; id <= 24; ++id)
                    song.project.findTrack (id)->volumeDb += 3.0f;
                song.engine.update (song.project);
            }
            const auto length = block % 3 == 0 ? 512 : (block % 3 == 1 ? 333 : 97);   // (blocks of all sizes)
            song.engine.render (buffer, length);
            for (int c = 0; c < 2; ++c)
                out.insert (out.end(), buffer.getReadPointer (c), buffer.getReadPointer (c) + length);
        }
        return out;
    }
}

TEST_CASE ("0.34.5: the worker pool runs every task of every job exactly once, however the work falls", "[multicore]")
{
    AudioWorkers workers;
    workers.setHelpers (3);
    CHECK (workers.getHelpers() == 3);
    juce::Random random (7);
    std::array<std::atomic<int>, 64> hits {};
    for (int job = 0; job < 20000; ++job)
    {
        const auto count = 1 + random.nextInt (64);
        for (auto& hit : hits)
            hit = 0;
        auto task = [&] (int i)
        {
            hits[(size_t) i].fetch_add (1);
            for (volatile int k = 0; k < (i * 7) % 50; ++k) {}   // (tasks of different lengths)
        };
        workers.run (count, task);
        for (int i = 0; i < 64; ++i)
            REQUIRE (hits[(size_t) i].load() == (i < count ? 1 : 0));
    }
    workers.setHelpers (0);
    auto ran = 0;
    auto alone = [&] (int) { ++ran; };
    workers.run (10, alone);
    CHECK (ran == 10);
}

TEST_CASE ("0.34.5: a song mixed on four cores is the same, to the last bit, as on one", "[multicore][engine]")
{
    const auto one = renderSong (0, false);
    {
        const std::scoped_lock clear (threadsLock);
        threadsSeen.clear();
    }
    const auto four = renderSong (3, false);
    REQUIRE (one.size() == four.size());
    CHECK (std::memcmp (one.data(), four.data(), one.size() * sizeof (float)) == 0);
    auto loud = 0.0f;
    for (const auto v : one)
        loud = juce::jmax (loud, std::abs (v));
    CHECK (loud > 0.05f);   // (something was heard)
    // The helpers took a share (the audio thread and at least one more ran effects).
    const std::scoped_lock seen (threadsLock);
    CHECK (threadsSeen.size() >= 2);
}

TEST_CASE ("0.34.5: an FX channel sending into another keeps their order (they wait for each other): the same as on one core", "[multicore][engine]")
{
    const auto one = renderSong (0, true);
    const auto four = renderSong (3, true);
    REQUIRE (one.size() == four.size());
    CHECK (std::memcmp (one.data(), four.data(), one.size() * sizeof (float)) == 0);
    CHECK (one != renderSong (0, false));   // (the FX send is heard)
}

TEST_CASE ("0.34.5 timing: the 24-track song on one core and on four", "[.][perf-multicore]")
{
    for (const auto helpers : { 0, 1, 3 })
    {
        renderSong (helpers, false);   // (warmed up)
        const auto started = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < 3; ++i)
            renderSong (helpers, false);
        const auto ms = (juce::Time::getMillisecondCounterHiRes() - started) / 3.0;
        // 240 blocks, 80 x 512 + 80 x 333 + 80 x 97 samples: 1.9 s of sound at 48 kHz.
        WARN ((helpers + 1) << " core(s): " << juce::String (ms, 1) << " ms for 1.9 s of sound (the song set up included)");
    }
}
