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
            project.tracks.push_back (track);
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
    track.automation.push_back (lane (AutomationLane::Target::volume, { { 0.0, 0.7f }, { 4.0, 0.2f } }));
    auto knob = lane (AutomationLane::Target::insert, { { 2.0, 0.25f } });
    knob.slot = 3;
    knob.parameter = 7;
    knob.parameterName = "Drive";
    knob.read = false;
    knob.write = true;
    knob.shown = false;
    track.automation.push_back (knob);
    project.tracks.push_back (track);
    project.masterAutomation.push_back (lane (AutomationLane::Target::volume, { { 60.0, 0.8f }, { 64.0, 0.0f } }));

    const auto opened = Project::fromXml (*project.toXml());
    REQUIRE (opened.has_value());
    CHECK (opened->tracks[0].automationOpen);
    CHECK (opened->tracks[0].automation == project.tracks[0].automation);
    CHECK (opened->masterAutomation == project.masterAutomation);
}

TEST_CASE ("Volume, pan and mute automation play as the song goes, to within 32 samples; R off: not heard", "[automation][engine]")
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

    // R off: the fader's own level (0 dB) all through.
    desk.track().automation[0].read = false;
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
