#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr int beat = 24000;   // samples at 48 kHz, 120 BPM

    AutomationLane lane (AutomationLane::Target target, std::vector<AutomationPoint> points)
    {
        AutomationLane l;
        l.target = target;
        l.points = std::move (points);
        return l;
    }

    // A plugin's knob, as a hosted plugin's are.
    struct Knob final : juce::AudioPluginInstance::HostedParameter
    {
        float get() const { return value.load(); }
        float getValue() const override { return value.load(); }
        void setValue (float v) override { value = v; }
        void setValueNotifyingHost (float v) { setValue (v); }
        float getDefaultValue() const override { return 1.0f; }
        juce::String getName (int) const override { return "Level"; }
        juce::String getLabel() const override { return {}; }
        float getValueForText (const juce::String& text) const override { return text.getFloatValue(); }
        juce::String getParameterID() const override { return "level"; }
        std::atomic<float> value { 1.0f };
    };

    // An effect with one knob: it multiplies the sound by it (0 to 1).
    struct KnobEffect final : TestPlugin
    {
        KnobEffect() : TestPlugin (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                                    .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
        {
            name = "Knob";
            auto owned = std::make_unique<Knob>();
            knob = owned.get();
            addHostedParameter (std::move (owned));
        }
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                juce::FloatVectorOperations::multiply (buffer.getWritePointer (c), knob->get(), buffer.getNumSamples());
        }
        Knob* knob;
    };

    // An engine with one track playing a steady 1.0 on both sides.
    struct Desk
    {
        Engine engine;
        Project project;
        Desk()
        {
            engine.prepare (48000.0, 512);
            Track track;
            track.id = 1;
            track.automationRead = true;
            project.tracks.push_back (track);
            project.masterRead = true;
            engine.setInstrument (1, std::make_unique<SteadyTone> (1.0f, 1.0f));
            engine.update (project);
        }
        Track& track() { return project.tracks[0]; }
        std::vector<float> play (int samples, std::vector<float>* right = nullptr)
        {
            engine.update (project);
            engine.setPosition (0.0);
            engine.play();
            auto left = renderSamples (engine, samples, 512, right);
            engine.stop();
            return left;
        }
    };
}

TEST_CASE ("Automation lanes: the value between points, before and after them; mute jumps", "[automation]")
{
    auto volume = lane (AutomationLane::Target::volume, { { 1.0, 0.2f }, { 3.0, 0.6f }, { 3.0, 0.9f } });
    CHECK_FALSE (lane (AutomationLane::Target::volume, {}).valueAt (1.0).has_value());
    CHECK (*volume.valueAt (0.0) == 0.2f);                         // before the first: its value
    CHECK_THAT (*volume.valueAt (2.0), WithinAbs (0.4, 1.0e-6));   // half way: half way
    CHECK (*volume.valueAt (3.0) == 0.9f);                         // two at one beat: a jump, the last from there
    CHECK (*volume.valueAt (10.0) == 0.9f);                        // after the last: its value
    auto mute = lane (AutomationLane::Target::mute, { { 0.0, 0.0f }, { 2.0, 1.0f } });
    CHECK (*mute.valueAt (1.99) == 0.0f);   // no ramp: off until the point
    CHECK (*mute.valueAt (2.0) == 1.0f);

    // The fader's travel: -60 dB at the bottom, -12 half way, +6 at the top (as the mixer's fader).
    CHECK (volumeToTravel (-60.0f) == 0.0f);
    CHECK_THAT (volumeToTravel (-12.0f), WithinAbs (0.5, 1.0e-5));
    CHECK (volumeToTravel (6.0f) == 1.0f);
    CHECK_THAT (travelToVolume (volumeToTravel (-3.5f)), WithinAbs (-3.5, 1.0e-4));
    CHECK (panToTravel (-1.0f) == 0.0f);
    CHECK (travelToPan (0.75f) == 0.5f);
}

TEST_CASE ("Automation is saved and opened with the song: each track's lanes and the master's", "[automation][project]")
{
    Project project;
    Track track;
    track.id = 1;
    track.automationOpen = true;
    track.automationWrite = true;
    track.automation.push_back (lane (AutomationLane::Target::volume, { { 0.0, 0.7f }, { 4.0, 0.2f } }));
    auto knob = lane (AutomationLane::Target::insert, { { 2.0, 0.25f } });
    knob.slot = 3;
    knob.parameter = 7;
    knob.parameterName = "Drive";
    knob.muted = true;
    knob.shown = false;
    track.automation.push_back (knob);
    project.tracks.push_back (track);
    project.masterAutomation.push_back (lane (AutomationLane::Target::volume, { { 60.0, 0.8f }, { 64.0, 0.0f } }));

    const auto opened = Project::fromXml (*project.toXml());
    REQUIRE (opened.has_value());
    CHECK (opened->tracks[0].automationOpen);
    CHECK (opened->tracks[0].automationWrite);
    CHECK_FALSE (opened->tracks[0].automationRead);
    CHECK (opened->masterRead == project.masterRead);
    CHECK (opened->tracks[0].automation == project.tracks[0].automation);
    CHECK (opened->masterAutomation == project.masterAutomation);
}

TEST_CASE ("Volume, pan and mute automation play as the song goes, to within 32 samples; R off or the lane muted: not heard", "[automation][engine]")
{
    Desk desk;
    // The volume down from 0 dB at beat 0 to silent at beat 2 (the fader's travel in a straight line).
    desk.track().automation.push_back (lane (AutomationLane::Target::volume, { { 0.0, volumeToTravel (0.0f) }, { 2.0, 0.0f } }));
    auto heard = desk.play (beat * 3);
    CHECK_THAT (heard[0], WithinAbs (1.0, 1.0e-4));
    const auto halfWay = juce::Decibels::decibelsToGain (travelToVolume (volumeToTravel (0.0f) * 0.5f));
    CHECK_THAT (heard[(size_t) beat], WithinAbs (halfWay, 1.0e-3));
    CHECK (heard[(size_t) beat * 2 + 32] == 0.0f);
    CHECK (desk.engine.getMixNow (1).volumeDb == -60.0f);   // what the fader shows

    // R off, or the lane muted: the fader's own level (0 dB) all through.
    desk.track().automationRead = false;
    heard = desk.play (beat * 3);
    CHECK_THAT (heard[(size_t) beat * 2 + 100], WithinAbs (1.0, 1.0e-4));
    desk.track().automationRead = true;
    desk.track().automation[0].muted = true;
    heard = desk.play (beat * 3);
    CHECK_THAT (heard[(size_t) beat * 2 + 100], WithinAbs (1.0, 1.0e-4));

    // Mute from beat 1: silent from then (within the 32-sample step), heard before.
    desk.track().automation = { lane (AutomationLane::Target::mute, { { 0.0, 0.0f }, { 1.0, 1.0f } }) };
    heard = desk.play (beat * 2);
    CHECK_THAT (heard[(size_t) beat - 33], WithinAbs (1.0, 1.0e-4));
    CHECK (heard[(size_t) beat + 1] == 0.0f);
    CHECK (desk.engine.getMixNow (1).mute);

    // Pan hard right at beat 1: the left side goes.
    desk.track().automation = { lane (AutomationLane::Target::pan, { { 0.0, 0.5f }, { 1.0, 1.0f } }) };
    std::vector<float> right;
    heard = desk.play (beat * 2, &right);
    CHECK_THAT (heard[(size_t) beat / 2], WithinAbs (0.5, 1.0e-3));   // half way to the right: the left at half
    CHECK (heard[(size_t) beat + 100] == 0.0f);
    CHECK_THAT (right[(size_t) beat + 100], WithinAbs (1.0, 1.0e-4));
}

TEST_CASE ("Automation follows the playhead when stopped, and round the cycle", "[automation][engine]")
{
    Desk desk;
    desk.track().automation.push_back (lane (AutomationLane::Target::volume, { { 0.0, volumeToTravel (0.0f) }, { 4.0, volumeToTravel (-20.0f) } }));
    desk.engine.update (desk.project);
    desk.engine.setPosition (4.0);
    const auto stopped = renderSamples (desk.engine, 512);
    (void) stopped;
    CHECK_THAT (desk.engine.getMixNow (1).volumeDb, WithinAbs (-20.0, 1.0e-3));

    // A cycle over beats 0 to 2: back at the start, back to 0 dB.
    desk.project.loopOn = true;
    desk.project.loopStart = 0.0;
    desk.project.loopEnd = 2.0;
    const auto heard = desk.play (beat * 3);
    CHECK_THAT (heard[(size_t) beat * 2 + 10], WithinAbs (1.0, 1.0e-3));
}

TEST_CASE ("Plugin knobs and the master's volume are automated too", "[automation][engine]")
{
    Desk desk;
    auto effect = std::make_unique<KnobEffect>();
    auto* knob = effect->knob;
    desk.engine.insertEffect (1, 0, std::move (effect));
    desk.track().inserts.resize (1);
    auto level = lane (AutomationLane::Target::insert, { { 0.0, 1.0f }, { 1.0, 1.0f }, { 1.0, 0.25f } });
    level.slot = 0;
    desk.track().automation.push_back (level);
    auto heard = desk.play (beat * 2);
    CHECK_THAT (heard[100], WithinAbs (1.0, 1.0e-5));
    CHECK_THAT (heard[(size_t) beat + 512], WithinAbs (0.25, 1.0e-5));   // from the block after beat 1
    CHECK_THAT (knob->get(), WithinAbs (0.25, 1.0e-5));

    // The master fading out over beats 0 to 1.
    desk.track().automation.clear();
    knob->setValueNotifyingHost (1.0f);
    desk.project.masterAutomation.push_back (lane (AutomationLane::Target::volume, { { 0.0, volumeToTravel (0.0f) }, { 1.0, 0.0f } }));
    heard = desk.play (beat * 2);
    CHECK_THAT (heard[0], WithinAbs (1.0, 1.0e-4));
    CHECK (heard[(size_t) beat + 32] == 0.0f);
    CHECK (desk.engine.getMixNow (Engine::masterBus).volumeDb == -60.0f);
}

TEST_CASE ("The studio's automation: show/hide, append, a lane switched to another control keeps its points, remove, mute, R and W", "[automation][studio]")
{
    using T = AutomationLane::Target;
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    CHECK (studio.getAutomation (track)->empty());

    // Show Automation the first time: a lane for the first control (volume).
    studio.setAutomationOpen (track, true);
    REQUIRE (studio.getAutomation (track)->size() == 1);
    CHECK (studio.getAutomation (track)->at (0).target == T::volume);
    CHECK (studio.isAutomationOpen (track));
    CHECK_THAT (studio.getControlValue (track, studio.getAutomation (track)->at (0)), WithinAbs (volumeToTravel (0.0f), 1.0e-6));

    // + : the next control (pan), then mute.
    studio.appendAutomationLane (track, 0);
    studio.appendAutomationLane (track, 1);
    REQUIRE (studio.getAutomation (track)->size() == 3);
    CHECK (studio.getAutomation (track)->at (1).target == T::pan);
    CHECK (studio.getAutomation (track)->at (2).target == T::mute);

    // Points: R comes on; undoable.
    CHECK_FALSE (studio.getAutomationRead (track));
    studio.setAutomationPoints (track, 0, { { 4.0, 0.2f }, { 0.0, 0.8f } });
    CHECK (studio.getAutomationRead (track));
    CHECK (studio.getAutomation (track)->at (0).points.front().beat == 0.0);   // in order
    studio.undo();
    CHECK (studio.getAutomation (track)->at (0).points.empty());
    studio.redo();
    CHECK (studio.getAutomation (track)->at (0).points.size() == 2);

    // The volume lane switched to mute: mute's lane comes here, volume's points are kept, hidden.
    const auto controls = studio.getAutomationControls (track);
    REQUIRE (controls.size() >= 3);
    studio.setLaneControl (track, 0, controls[2]);
    const auto* lanes = studio.getAutomation (track);
    REQUIRE (lanes->size() == 3);
    CHECK (lanes->at (0).target == T::mute);
    CHECK (lanes->at (1).target == T::pan);
    CHECK (lanes->at (2).target == T::volume);
    CHECK_FALSE (lanes->at (2).shown);
    CHECK (lanes->at (2).points.size() == 2);
    // And back: volume's lane, with its points, shown again in place.
    studio.setLaneControl (track, 0, controls[0]);
    lanes = studio.getAutomation (track);
    CHECK (lanes->at (0).target == T::volume);
    CHECK (lanes->at (0).shown);
    CHECK (lanes->at (0).points.size() == 2);

    // Mute Automation; Remove Unused Parameters; Remove Parameter.
    studio.setLaneMuted (track, 0, true);
    CHECK (studio.getAutomation (track)->at (0).muted);
    studio.removeUnusedAutomation (track);
    CHECK (studio.getAutomation (track)->size() == 1);
    studio.removeAutomationParameter (track, 0);
    CHECK (studio.getAutomation (track)->empty());
    CHECK_FALSE (studio.isAutomationOpen (track));

    // W turns R on; R and W for all (the master too).
    studio.setAutomationRead (track, false);
    studio.setAutomationWrite (track, true);
    CHECK (studio.getAutomationRead (track));
    CHECK (studio.anyAutomationWrite());
    studio.setAutomationWriteAll (false);
    CHECK_FALSE (studio.anyAutomationWrite());
    CHECK (studio.anyAutomationRead());
    studio.setAutomationReadAll (false);
    CHECK_FALSE (studio.anyAutomationRead());
    studio.setAutomationWriteAll (true);
    CHECK (studio.getAutomationRead (Engine::masterBus));

    // Show All Used Automation opens every track's lanes with points.
    studio.setAutomationPoints (Engine::masterBus, 0, {});   // no lane yet: nothing happens
    studio.setAutomationOpen (Engine::masterBus, true);
    studio.setAutomationPoints (Engine::masterBus, 0, { { 8.0, 0.5f } });
    studio.setAutomationOpen (Engine::masterBus, false);
    studio.showAllUsedAutomation();
    CHECK (studio.isAutomationOpen (Engine::masterBus));
    CHECK (studio.getAutomationControls (Engine::masterBus).size() == 1);   // the master: volume (no effects)
}

TEST_CASE ("An insert's automation goes with it; the ones after it move up a slot", "[automation][studio]")
{
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    for (const auto* name : { "One", "Two" })
    {
        Insert insert;
        insert.plugin.name = name;
        studio.addInsert (track, insert, timesEffect (1.0f, name));
    }
    studio.setAutomationOpen (track, true);
    auto lane = [&] (int slot)
    {
        Studio::AutomationControl control;
        control.target = AutomationLane::Target::insert;
        control.slot = slot;
        return control;
    };
    studio.appendAutomationLane (track, 0);   // pan
    studio.setLaneControl (track, 1, lane (0));
    studio.appendAutomationLane (track, 1);   // mute
    studio.setLaneControl (track, 2, lane (1));
    studio.setAutomationPoints (track, 1, { { 0.0, 0.5f } });
    studio.setAutomationPoints (track, 2, { { 0.0, 0.25f } });
    REQUIRE (studio.getAutomation (track)->size() == 3);

    studio.removeInsert (track, 0);
    const auto* lanes = studio.getAutomation (track);
    REQUIRE (lanes->size() == 2);
    CHECK (lanes->at (1).target == AutomationLane::Target::insert);
    CHECK (lanes->at (1).slot == 0);   // "Two", now in the first slot
    CHECK (lanes->at (1).points.front().value == 0.25f);
    studio.undo();   // the points' undo doesn't bring the removed effect's lane back
    CHECK (studio.getAutomation (track)->size() == 2);
}
