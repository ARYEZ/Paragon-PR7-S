#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    // An engine with tracks each playing a steady level; reads the output once
    // the volume ramps (20 ms) have settled.
    struct Desk
    {
        Engine engine;
        Project project;

        Desk()
        {
            engine.prepare (48000.0, 512);
        }
        void addTrack (int id, float left, float right)
        {
            Track track;
            track.id = id;
            project.tracks.push_back (track);
            engine.setInstrument (id, std::make_unique<SteadyTone> (left, right));
            engine.update (project);
        }
        std::pair<float, float> output()
        {
            std::vector<float> right;
            const auto left = renderSamples (engine, 4096, 512, &right);
            return { left.back(), right.back() };
        }
    };
}

TEST_CASE ("Inserts: a track's sound goes through its effects in order, then its volume; bypassed ones are passed by", "[engine][mixer]")
{
    Desk desk;
    desk.addTrack (1, 0.5f, 0.5f);
    CHECK_THAT (desk.output().first, WithinAbs (0.5, 1.0e-5));

    // Times 0.5, then plus 0.1.
    desk.engine.insertEffect (1, 0, timesEffect (0.5f));
    desk.engine.insertEffect (1, 1, plusEffect (0.1f));
    desk.project.tracks[0].inserts.resize (2);
    desk.engine.update (desk.project);
    CHECK_THAT (desk.output().first, WithinAbs (0.35, 1.0e-5));
    CHECK_THAT (desk.output().second, WithinAbs (0.35, 1.0e-5));

    // The other way round: plus 0.1 first, then times 0.5.
    bool timesAlive = false;
    desk.engine.removeEffect (1, 0);
    desk.engine.insertEffect (1, 1, timesEffect (0.5f, "Times", &timesAlive));
    CHECK (desk.engine.getEffect (1, 0)->getName() == "Plus");
    CHECK_THAT (desk.output().first, WithinAbs (0.3, 1.0e-5));

    // The volume comes after the effects: half the volume, half of 0.3
    // (before them it would be (0.25 + 0.1) * 0.5 = 0.175).
    desk.project.tracks[0].volumeDb = juce::Decibels::gainToDecibels (0.5f);
    desk.engine.update (desk.project);
    CHECK_THAT (desk.output().first, WithinAbs (0.15, 1.0e-4));
    desk.project.tracks[0].volumeDb = 0.0f;

    // Bypassing the plus: just times 0.5.
    desk.project.tracks[0].inserts[0].bypass = true;
    desk.engine.update (desk.project);
    CHECK_THAT (desk.output().first, WithinAbs (0.25, 1.0e-5));

    // Removed: gone from the sound, and deleted.
    REQUIRE (timesAlive);
    desk.engine.removeEffect (1, 1);
    CHECK_FALSE (timesAlive);
    CHECK (desk.engine.getNumEffects (1) == 1);
    CHECK_THAT (desk.output().first, WithinAbs (0.5, 1.0e-5));   // the plus is still bypassed
}

TEST_CASE ("A mono effect gets both sides mixed, and its one output goes to both", "[engine][mixer]")
{
    Desk desk;
    desk.addTrack (1, 0.2f, 0.6f);
    auto mono = std::make_unique<TestEffect> ([] (juce::AudioBuffer<float>&) {}, 1, "Mono");
    auto* seen = mono.get();
    desk.engine.insertEffect (1, 0, std::move (mono));
    desk.project.tracks[0].inserts.resize (1);
    desk.engine.update (desk.project);
    const auto [left, right] = desk.output();
    CHECK (seen->channelsSeen == 1);   // given just the one channel it has
    CHECK_THAT (left, WithinAbs (0.4, 1.0e-5));
    CHECK_THAT (right, WithinAbs (0.4, 1.0e-5));
}

TEST_CASE ("A mono instrument is heard on both sides", "[engine][mixer]")
{
    struct MonoTone final : TestPlugin
    {
        MonoTone() : TestPlugin (BusesProperties().withOutput ("Output", juce::AudioChannelSet::mono(), true)) { instrument = true; }
        bool isBusesLayoutSupported (const BusesLayout& layout) const override { return layout.getMainOutputChannels() == 1; }
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (0, i, 0.3f);
        }
    };
    Desk desk;
    Track track;
    track.id = 1;
    desk.project.tracks.push_back (track);
    desk.engine.setInstrument (1, std::make_unique<MonoTone>());
    desk.engine.update (desk.project);
    const auto [left, right] = desk.output();
    CHECK_THAT (left, WithinAbs (0.3, 1.0e-5));
    CHECK_THAT (right, WithinAbs (0.3, 1.0e-5));
}

TEST_CASE ("The master: every track through its effects and volume; the click joins after them", "[engine][mixer]")
{
    Desk desk;
    desk.addTrack (1, 0.1f, 0.1f);
    desk.addTrack (2, 0.15f, 0.05f);
    auto [left, right] = desk.output();
    CHECK_THAT (left, WithinAbs (0.25, 1.0e-5));
    CHECK_THAT (right, WithinAbs (0.15, 1.0e-5));

    // A master effect works on the sum.
    desk.engine.insertEffect (Engine::masterBus, 0, timesEffect (2.0f));
    desk.project.masterInserts.resize (1);
    desk.engine.update (desk.project);
    std::tie (left, right) = desk.output();
    CHECK_THAT (left, WithinAbs (0.5, 1.0e-5));
    CHECK_THAT (right, WithinAbs (0.3, 1.0e-5));

    // Then the master's volume: -6 dB, half.
    desk.project.masterVolumeDb = juce::Decibels::gainToDecibels (0.5f);
    desk.engine.update (desk.project);
    CHECK_THAT (desk.output().first, WithinAbs (0.25, 1.0e-4));

    // A master effect that silences everything: the click still sounds (it doesn't go through the master).
    desk.engine.removeEffect (Engine::masterBus, 0);
    desk.engine.insertEffect (Engine::masterBus, 0, timesEffect (0.0f));
    desk.project.click = true;
    desk.engine.update (desk.project);
    desk.engine.play();
    const auto withClick = renderSamples (desk.engine, 2048);
    CHECK (rmsOf (withClick, 0, 1440) > 0.01f);       // the first beat's tick
    CHECK (rmsOf (withClick, 1500, 2048) < 1.0e-6f);  // and nothing else
}

TEST_CASE ("The studio's mixer: effects added, bypassed and removed (windows told first); master volume; saved and opened", "[studio][mixer]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    std::vector<juce::AudioProcessor*> going;
    bool effectAliveWhenTold = false;
    studio.onEffectGoing = [&] (juce::AudioProcessor* effect)
    {
        going.push_back (effect);
        effectAliveWhenTold = effect == nullptr || dynamic_cast<TestEffect*> (effect) != nullptr;
    };

    Insert insert;
    insert.plugin.name = "Test Delay";
    insert.plugin.pluginFormatName = "Test";
    auto effect = timesEffect (0.5f, "Test Delay");
    effect->state = "time=250";
    auto* instance = effect.get();
    REQUIRE (studio.addInsert (trackId, insert, std::move (effect)));
    REQUIRE (studio.getInserts (trackId)->size() == 1);
    CHECK (studio.getEngine().getEffect (trackId, 0) == instance);
    CHECK (studio.getInserts (Engine::masterBus)->empty());
    CHECK (studio.getInserts (999) == nullptr);

    studio.setInsertBypass (trackId, 0, true);
    CHECK (studio.getInserts (trackId)->at (0).bypass);
    studio.setInsertBypass (trackId, 0, false);

    // There's room for 8.
    for (int i = 1; i < Project::maxInserts; ++i)
        REQUIRE (studio.addInsert (trackId, insert, timesEffect (1.0f)));
    CHECK_FALSE (studio.addInsert (trackId, insert, timesEffect (1.0f)));
    for (int i = Project::maxInserts - 1; i >= 1; --i)
        studio.removeInsert (trackId, i);
    CHECK (going.size() == 7);
    CHECK (effectAliveWhenTold);
    going.clear();

    // The master's volume is undoable.
    studio.setMasterVolume (-6.0f);
    CHECK (studio.getProject().masterVolumeDb == -6.0f);
    studio.undo();
    CHECK (studio.getProject().masterVolumeDb == 0.0f);
    studio.redo();
    REQUIRE (studio.addInsert (Engine::masterBus, insert, timesEffect (1.0f, "Test Delay")));

    // Saved with each effect's settings; opened on a computer without the
    // effect, it says so and keeps the insert (and its settings) for later.
    const auto file = juce::File::createTempFile (Project::fileExtension);
    REQUIRE (studio.save (file));
    CHECK (studio.getProject().tracks[0].inserts[0].state.toString() == "time=250");
    Studio other (false);
    juce::String problems;
    REQUIRE (other.open (file, problems));
    CHECK (problems.contains ("Test Delay"));
    CHECK (problems.contains ("Master"));
    REQUIRE (other.getInserts (trackId)->size() == 1);
    CHECK (other.getInserts (trackId)->at (0).state.toString() == "time=250");
    CHECK (other.getInserts (Engine::masterBus)->size() == 1);
    CHECK (other.getProject().masterVolumeDb == -6.0f);
    CHECK (other.getEngine().getEffect (trackId, 0) == nullptr);   // not loaded: the sound goes past it
    file.deleteFile();

    // Deleting the track: its effects' windows are told first.
    const auto second = studio.addTrack ({});
    REQUIRE (studio.addInsert (second, insert, timesEffect (1.0f)));
    auto* secondEffect = studio.getEngine().getEffect (second, 0);
    studio.removeTrack (second);
    REQUIRE (going.size() == 1);
    CHECK (going[0] == secondEffect);

    // A new song: every effect's window (nullptr: all).
    studio.newProject();
    CHECK (going.back() == nullptr);
    CHECK (studio.getEngine().getNumEffects (Engine::masterBus) == 0);
    CHECK (studio.getProject().masterInserts.empty());
}
