#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

// (0.247.0) Delay compensation: a plug-in that reports a delay holds every other path back to meet it.

namespace
{
    // An instrument that makes one click (a single sample at `level`) at sample `when` since it was prepared.
    struct Clicker final : TestPlugin
    {
        Clicker (juce::int64 whenToUse, float levelToUse)
            : TestPlugin (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)), when (whenToUse), level (levelToUse)
        {
            instrument = true;
            name = "Clicker";
        }
        void prepareToPlay (double, int) override { done = 0; }
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                juce::FloatVectorOperations::clear (buffer.getWritePointer (c), buffer.getNumSamples());
            if (when >= done && when < done + buffer.getNumSamples())
                for (int c = 0; c < juce::jmin (2, buffer.getNumChannels()); ++c)
                    buffer.setSample (c, (int) (when - done), level);
            done += buffer.getNumSamples();
        }
        juce::int64 when, done = 0;
        float level;
    };

    // An effect that plays its sound `delay` samples late and says so (as a look-ahead limiter does).
    struct LateEffect final : TestPlugin
    {
        explicit LateEffect (int delayToUse)
            : TestPlugin (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
              delay (delayToUse)
        {
            name = "Late";
            setLatencySamples (delay);
            ring.setSize (2, juce::jmax (1, delay));
            ring.clear();
        }
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            if (delay == 0)
                return;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                for (int c = 0; c < 2; ++c)
                {
                    const auto in = buffer.getSample (c, i);
                    buffer.setSample (c, i, ring.getSample (c, pos));
                    ring.setSample (c, pos, in);
                }
                pos = (pos + 1) % delay;
            }
        }
        int delay, pos = 0;
        juce::AudioBuffer<float> ring;
    };

    struct Desk
    {
        Engine engine;
        Project project;
        Desk() { engine.prepare (48000.0, 512); }
        Track& addTrack (int id, juce::int64 when, float level)
        {
            Track track;
            track.id = id;
            project.tracks.push_back (track);
            engine.setInstrument (id, std::make_unique<Clicker> (when, level));
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
        void late (int busId, int delay)
        {
            engine.insertEffect (busId, 0, std::make_unique<LateEffect> (delay));
            if (busId == Engine::masterBus)
                project.masterInserts.resize (1);
            else
                project.findTrack (busId)->inserts.resize (1);
            engine.update (project);
        }
        Track& track (int id) { return *project.findTrack (id); }
        // The samples where something sounds, with what it is.
        std::vector<std::pair<int, float>> hits (int samples = 8192)
        {
            engine.update (project);
            const auto out = renderSamples (engine, samples);
            std::vector<std::pair<int, float>> found;
            for (int i = 0; i < (int) out.size(); ++i)
                if (std::abs (out[(size_t) i]) > 1.0e-6f)
                    found.push_back ({ i, out[(size_t) i] });
            return found;
        }
    };
}

TEST_CASE ("Delay compensation: a track with a late plug-in, the others held back to meet it", "[engine][delaycomp]")
{
    // Two tracks click at sample 2000; the first through a plug-in 100 samples late.
    Desk desk;
    desk.addTrack (1, 2000, 0.25f);
    desk.addTrack (2, 2000, 0.5f);
    desk.late (1, 100);
    const auto together = desk.hits();
    REQUIRE (together.size() == 1u);
    CHECK (together[0].first == 2100);   // both at 2100: one click of both
    CHECK_THAT (together[0].second, WithinAbs (0.75, 1.0e-6));
    CHECK (desk.engine.getTotalLatency() == 100);
    CHECK (desk.engine.getTrackLatency (1) == std::pair<int, int> { 100, 0 });
    CHECK (desk.engine.getTrackLatency (2) == std::pair<int, int> { 0, 100 });

    // Off: as before (the late one plays late).
    Desk off;
    off.engine.setDelayCompensation (false);
    off.addTrack (1, 2000, 0.25f);
    off.addTrack (2, 2000, 0.5f);
    off.late (1, 100);
    const auto apart = off.hits();
    REQUIRE (apart.size() == 2u);
    CHECK (apart[0].first == 2000);
    CHECK (apart[1].first == 2100);
    CHECK (off.engine.getTotalLatency() == 0);

    // With nothing late, nothing is held back: exactly as before.
    Desk plain;
    plain.addTrack (1, 2000, 0.25f);
    plain.addTrack (2, 2000, 0.5f);
    const auto same = plain.hits();
    REQUIRE (same.size() == 1u);
    CHECK (same[0].first == 2000);
    CHECK (plain.engine.getTotalLatency() == 0);
}

TEST_CASE ("Delay compensation: a bypassed plug-in counts for nothing; the master's own delay is the whole mix's", "[engine][delaycomp]")
{
    Desk desk;
    desk.addTrack (1, 2000, 0.25f);
    desk.addTrack (2, 2000, 0.5f);
    desk.late (1, 64);
    desk.track (1).inserts[0].bypass = true;
    const auto passed = desk.hits();
    REQUIRE (passed.size() == 1u);
    CHECK (passed[0].first == 2000);   // passed by: no delay, nothing held back
    CHECK (desk.engine.getTotalLatency() == 0);

    // A late plug-in on the master: everything later together, the whole mix's delay.
    Desk master;
    master.addTrack (1, 2000, 0.25f);
    master.addTrack (2, 2000, 0.5f);
    master.late (Engine::masterBus, 300);
    const auto later = master.hits();
    REQUIRE (later.size() == 1u);
    CHECK (later[0].first == 2300);
    CHECK (master.engine.getTotalLatency() == 300);
}

TEST_CASE ("Delay compensation: groups, FX channels and sends all meet at the master", "[engine][delaycomp]")
{
    // Track 1 (on time) into a group with a late plug-in; track 2 (on time) to the master, sending to an
    // FX channel that's late itself; track 3 late on its own. Every path arrives at the latest (300).
    const auto heard = [] (bool preFader, int& total)
    {
        Desk desk;
        desk.addTrack (1, 2000, 0.125f);
        desk.addTrack (2, 2000, 0.25f);
        desk.addTrack (3, 2000, 0.0625f);
        desk.addChannel (10, true);    // the group
        desk.addChannel (11, false);   // the FX channel
        desk.track (1).output = 10;
        desk.late (10, 300);
        desk.late (11, 120);
        desk.late (3, 40);
        desk.track (2).sends = { { 11, 0.0f, true, preFader } };   // 0 dB
        const auto found = desk.hits();
        total = desk.engine.getTotalLatency();
        return found;
    };
    int total = 0;
    const auto found = heard (false, total);
    REQUIRE (found.size() == 1u);
    CHECK (found[0].first == 2300);
    CHECK_THAT (found[0].second, WithinAbs (0.125 + 0.25 + 0.25 + 0.0625, 1.0e-6));   // track 2 and its send's return
    CHECK (total == 300);

    // A send before the fader: its tap meets the FX channel's other inputs too.
    const auto pre = heard (true, total);
    REQUIRE (pre.size() == 1u);
    CHECK (pre[0].first == 2300);
    CHECK_THAT (pre[0].second, WithinAbs (0.125 + 0.25 + 0.25 + 0.0625, 1.0e-6));
}

TEST_CASE ("Delay compensation: an export is put back by the plug-ins' delay, to the sample", "[engine][delaycomp][export]")
{
    // A ramp clip (every sample told apart) from beat 1, a late plug-in on the master: the export still has
    // the clip's first sample exactly on beat 1.
    constexpr int beat = 24000;
    Studio studio { false };
    const auto track = studio.addAudioTrack (false);
    juce::AudioBuffer<float> take (1, beat * 4);
    for (int i = 0; i < take.getNumSamples(); ++i)
        take.setSample (0, i, (float) (i + 1) * 1.0e-6f);
    writeWav (studio.getAudioFolder().getChildFile ("take.wav"), take, 48000.0);
    Part clip { 1.0, 1.0, "Take", {} };
    clip.audioFile = "take.wav";
    studio.addPart (track, clip);
    studio.getEngine().insertEffect (track, 0, std::make_unique<LateEffect> (777));
    studio.getEngine().insertEffect (Engine::masterBus, 0, std::make_unique<LateEffect> (333));
    CHECK (studio.getEngine().refreshLatency() == 777 + 333);

    Studio::ExportOptions options;
    options.start = 0.0;
    options.end = 2.0;
    options.tailSeconds = 0.0;
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-export-delaycomp.wav");
    juce::String error;
    REQUIRE (studio.exportMix (file, options, error));
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    REQUIRE (reader != nullptr);
    juce::AudioBuffer<float> sound ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&sound, 0, (int) reader->lengthInSamples, 0, true, true);
    reader.reset();
    REQUIRE (sound.getNumSamples() == beat * 2);
    CHECK (sound.getMagnitude (0, 0, beat) == 0.0f);
    CHECK_THAT (sound.getSample (0, beat), WithinAbs (take.getSample (0, 0), 2.0e-6));
    CHECK_THAT (sound.getSample (0, beat + 1000), WithinAbs (take.getSample (0, 1000), 2.0e-6));
    file.deleteFile();
    studio.getAudioFolder().deleteRecursively();
}

TEST_CASE ("Delay compensation: the click is held back with the music, recordings put back by it", "[engine][delaycomp]")
{
    // The click at 120 BPM: its first tick 500 samples later with a 500-sample delay than without.
    const auto firstClick = [] (Desk& d)
    {
        d.project.click = true;
        d.engine.update (d.project);
        d.engine.refreshLatency();
        d.engine.play();
        const auto found = d.hits (4096);
        d.engine.stop();
        return found.empty() ? -1 : found[0].first;
    };
    Desk plain;
    plain.addTrack (1, 100000, 0.0f);
    const auto onTime = firstClick (plain);
    REQUIRE (onTime >= 0);
    REQUIRE (onTime < 10);
    Desk desk;
    desk.addTrack (1, 100000, 0.0f);   // (silent: only the click sounds)
    desk.late (1, 500);
    CHECK (firstClick (desk) == onTime + 500);

    // Studio: the recording's latency counts the plug-ins' delay.
    Studio studio { false };
    studio.setLatencyForTests (0.01);
    const auto track = studio.getProject().tracks[0].id;
    studio.getEngine().insertEffect (track, 0, std::make_unique<LateEffect> (480));
    studio.getEngine().refreshLatency();
    CHECK_THAT (studio.getRecordingLatencyForTests(), WithinAbs (0.01 + 480.0 / 48000.0, 1.0e-9));
}
