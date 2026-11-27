#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "engine/EditHistory.h"

#include <set>

using Catch::Matchers::WithinAbs;
using namespace pr7s;

// (0.248.0) The CHANNEL STRIP: PR7-C's modules on the studio's channels.

namespace
{
    // An instrument playing a sine on each side (its own phase per side), so levels and harmonics can be read.
    struct Sine final : TestPlugin
    {
        Sine (float hzToUse, float levelToUse, float rightLevel = -1.0f)
            : TestPlugin (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
              hz (hzToUse), left (levelToUse), right (rightLevel < 0.0f ? levelToUse : rightLevel)
        {
            instrument = true;
            name = "Sine";
        }
        void prepareToPlay (double rate, int) override { sampleRate = rate; phase = 0.0; }
        void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
        {
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto v = (float) std::sin (phase);
                phase += juce::MathConstants<double>::twoPi * hz / sampleRate;
                buffer.setSample (0, i, v * left);
                if (buffer.getNumChannels() > 1)
                    buffer.setSample (1, i, v * right);
            }
        }
        double hz, sampleRate = 48000.0, phase = 0.0;
        float left, right;
    };

    struct Desk
    {
        Engine engine;
        Project project;
        Desk() { engine.prepare (48000.0, 512); }
        Track& addTrack (int id, std::unique_ptr<juce::AudioPluginInstance> plugin)
        {
            Track track;
            track.id = id;
            project.tracks.push_back (track);
            engine.setInstrument (id, std::move (plugin));
            engine.update (project);
            return project.tracks.back();
        }
        Track& track (int id) { return *project.findTrack (id); }
        std::pair<std::vector<float>, std::vector<float>> render (int samples)
        {
            engine.update (project);
            std::vector<float> right;
            auto left = renderSamples (engine, samples, 512, &right);
            return { left, right };
        }
    };

    float rmsDb (const std::vector<float>& s, size_t from) { return juce::Decibels::gainToDecibels (rmsOf (s, from), -200.0f); }
}

TEST_CASE ("Channel strip: off it changes nothing; any module on delays the sound its constant 7 samples, told to the compensation", "[engine][strip]")
{
    // Off: exactly the tone.
    Desk plain;
    plain.addTrack (1, std::make_unique<Sine> (440.0, 0.5f));
    const auto clean = plain.render (4096).first;

    Desk off;
    off.addTrack (1, std::make_unique<Sine> (440.0, 0.5f));
    CHECK_FALSE (off.track (1).strip.inUse());
    CHECK (off.render (4096).first == clean);
    CHECK (off.engine.getTotalLatency() == 0);

    // A module on that changes nothing at its settings (STEREO WIDTH at 100 %): the sound 7 samples late.
    Desk on;
    on.addTrack (1, std::make_unique<Sine> (440.0, 0.5f));
    on.track (1).strip.stereo.widthOn = true;
    const auto late = on.render (4096).first;
    CHECK (on.engine.getTotalLatency() == ChannelStrip::latencySamples);
    CHECK (on.engine.getTrackLatency (1).first == ChannelStrip::latencySamples);
    for (size_t i = 2000; i < 4000; ++i)
        CHECK_THAT (late[i], WithinAbs (clean[i - 7], 1.0e-5));
}

TEST_CASE ("Channel strip: two tracks, one through its strip, still meet (the delay compensation lines them up)", "[engine][strip]")
{
    // Track 1 through its strip (7 samples), track 2 without: track 2 held back 7, the sum one tone.
    Desk desk;
    desk.addTrack (1, std::make_unique<Sine> (440.0, 0.25f));
    desk.addTrack (2, std::make_unique<Sine> (440.0, 0.25f));
    desk.track (1).strip.stereo.widthOn = true;
    const auto together = desk.render (8192).first;
    CHECK (desk.engine.getTrackLatency (2).second == ChannelStrip::latencySamples);
    // The two sum to one tone at 0.5: its peak is 0.5, not the lower peak of two out of step.
    float peak = 0.0f;
    for (size_t i = 6000; i < 8000; ++i)
        peak = juce::jmax (peak, std::abs (together[i]));
    CHECK_THAT (peak, WithinAbs (0.5, 2.0e-3));
}

TEST_CASE ("Channel strip: the compressor turns a loud tone down by its ratio over the threshold; MAKE UP adds", "[engine][strip]")
{
    // A tone peaking at 0.707 (-3 dBFS; RMS -6 dB), VCA 4:1 at -26 dB.
    Desk desk;
    desk.addTrack (1, std::make_unique<Sine> (1000.0, 0.70710678f));
    auto& c = desk.track (1).strip.comp;
    c.on = true;
    c.thresholdDb = -26.0f;
    c.ratio = 4.0f;
    c.character = 0;
    c.attackMs = 1.0f;
    c.releaseMs = 100.0f;
    const auto out = desk.render (48000).first;
    const auto level = rmsDb (out, 24000);
    INFO ("level " << level);
    // PR7-C reads a sine as its peak (-3 dBFS here): 23 dB over, 4:1 takes 17.25 dB off the -6 dB RMS.
    CHECK_THAT (level, WithinAbs (-6.0 - 23.0 * 0.75, 1.0));
    CHECK (desk.engine.getStripReduction (1, true) > 10.0f);
    // MAKE UP 6 dB: 6 dB louder.
    desk.track (1).strip.comp.makeUpDb = 6.0f;
    const auto louder = rmsDb (desk.render (48000).first, 24000);
    CHECK_THAT (louder - level, WithinAbs (6.0, 0.5));
}

TEST_CASE ("Channel strip: the gate shuts a quiet tone down by its range, leaves a loud one alone", "[engine][strip]")
{
    Desk quiet;
    quiet.addTrack (1, std::make_unique<Sine> (1000.0, 0.01f));   // about -43 dBFS
    auto& g = quiet.track (1).strip.gate;
    g.on = true;
    g.thresholdDb = -20.0f;
    g.rangeDb = 40.0f;
    const auto shut = rmsDb (quiet.render (48000).first, 24000);
    CHECK (shut < -43.0f - 35.0f);

    Desk loud;
    loud.addTrack (1, std::make_unique<Sine> (1000.0, 0.5f));
    loud.track (1).strip.gate = g;
    const auto open = rmsDb (loud.render (48000).first, 24000);
    CHECK_THAT (open, WithinAbs (juce::Decibels::gainToDecibels (0.5f / std::sqrt (2.0f)), 0.2));
}

TEST_CASE ("Channel strip: saturation adds harmonics; stereo width 0 makes it mono; the console colour's noise is there", "[engine][strip]")
{
    // SATURATION pushed: a 500 Hz tone gains its 2nd and 3rd harmonics.
    Desk sat;
    sat.addTrack (1, std::make_unique<Sine> (500.0, 0.5f));
    sat.track (1).strip.sat = { true, 24.0f, 50.0f, false, 100.0f };
    const auto driven = sat.render (24000).first;
    const auto fundamental = levelAt (driven, 500.0, 48000.0, 8000, 24000);
    CHECK (levelAt (driven, 1000.0, 48000.0, 8000, 24000) / fundamental > 0.01f);
    CHECK (levelAt (driven, 1500.0, 48000.0, 8000, 24000) / fundamental > 0.01f);

    // STEREO WIDTH 0: the sides gone, left = right.
    Desk wide;
    wide.addTrack (1, std::make_unique<Sine> (300.0, 0.6f, 0.1f));
    wide.track (1).strip.stereo.widthOn = true;
    wide.track (1).strip.stereo.width = 0.0f;
    const auto [l, r] = wide.render (24000);
    for (size_t i = 12000; i < 24000; i += 97)
        CHECK_THAT (l[i], WithinAbs (r[i], 1.0e-4));

    // V GAIN: the noise floor on a silent track (an instrument playing nothing).
    Desk noise;
    noise.addTrack (1, std::make_unique<Sine> (300.0, 0.0f));
    noise.track (1).strip.colour = { true, 0.0f, 100.0f, 9, false };
    const auto hiss = rmsDb (noise.render (48000).first, 24000);
    INFO ("hiss " << hiss);
    CHECK (hiss > -75.0f);
    CHECK (hiss < -55.0f);
}

TEST_CASE ("Channel strip: saved with the song, undone, in the history, in track presets; RANDOM CH gives each its own", "[strip]")
{
    Studio studio (false);
    const auto id = studio.getProject().tracks[0].id;
    auto strip = *studio.getChannelStrip (id);
    strip.comp.on = true;
    strip.comp.character = 3;
    strip.comp.thresholdDb = -22.0f;
    strip.colour.on = true;
    strip.colour.channel = 14;
    studio.setChannelStrip (id, strip);
    CHECK (studio.getChannelStrip (id)->comp.character == 3);
    CHECK (studio.getHistory()[(size_t) studio.getHistoryPosition()].action == "Channel Strip");

    // The song's file: there and back.
    const auto xml = studio.getProject().toXml();
    const auto back = Project::fromXml (*xml);
    REQUIRE (back.has_value());
    CHECK (back->findTrack (id)->strip == strip.limited());

    // Undo.
    studio.undo();
    CHECK (studio.getChannelStrip (id)->isDefault());
    studio.redo();
    CHECK (studio.getChannelStrip (id)->comp.on);

    // A track preset carries it.
    const auto preset = studio.makeTrackPreset (id);
    REQUIRE (preset != nullptr);
    const auto other = studio.addTrack ({});
    juce::String error;
    REQUIRE (studio.applyTrackPreset (other, *preset, error));
    CHECK (studio.getChannelStrip (other)->comp.character == 3);

    // The master has one; a folder none.
    CHECK (studio.getChannelStrip (Engine::masterBus) != nullptr);

    // RANDOM CH: every strip with its colour on gets a channel of its own (and the next, ANALOG).
    for (int n = 0; n < 6; ++n)
    {
        const auto t = studio.addTrack ({});
        studio.setChannelStrip (t, strip);
    }
    studio.randomiseConsoleChannels (std::nullopt);
    std::set<int> used;
    auto clashes = 0;
    for (const auto& t : studio.getProject().tracks)
        if (t.strip.colour.on)
            for (const auto c : { t.strip.colour.channel, t.strip.colour.channel % ChannelStrip::consoleChannels + 1 })
                clashes += used.insert (c).second ? 0 : 1;
    CHECK (clashes == 0);
    CHECK (used.size() >= 14u);
}

#include "ui/StripEditor.h"

TEST_CASE ("Channel strip: the window's knobs and keys are the studio's settings, a knob let go one undo step", "[strip][ui]")
{
    Studio studio (false);
    const auto id = studio.getProject().tracks[0].id;
    StripEditor editor (studio, id);
    editor.setSize (StripEditor::preferredWidth, StripEditor::preferredHeight);
    for (int m = 0; m < StripEditor::numModules; ++m)
        CHECK_FALSE (editor.getModuleArea (m).isEmpty());

    editor.clickKey ("comp.on");
    CHECK (studio.getChannelStrip (id)->comp.on);
    CHECK (editor.isKeyLit ("comp.on"));
    editor.clickKey ("comp.fet");
    CHECK (studio.getChannelStrip (id)->comp.character == 2);
    CHECK_FALSE (editor.findControl ("comp.distortion1")->isEnabled());   // (MODERN only)
    editor.clickKey ("comp.modern");
    CHECK (editor.findControl ("comp.distortion1")->isEnabled());

    const auto before = studio.getHistoryPosition();
    editor.setKnob ("comp.threshold", -31.4);
    CHECK_THAT (studio.getChannelStrip (id)->comp.thresholdDb, WithinAbs (-31.4, 1.0e-4));
    CHECK (studio.getHistoryPosition() == before + 1);
    editor.setKnob ("colour.channel", 40.0);
    CHECK (studio.getChannelStrip (id)->colour.channel == 40);

    // An undo shows in the window.
    studio.undo();
    editor.setBus (id);
    CHECK (editor.getKnob ("colour.channel") == 1.0);

    // Every control has a tooltip.
    for (const auto* control : { "gate.on", "gate.threshold", "sc.listen", "comp.ratio", "sat.drive", "colour.random", "stereo.amount" })
    {
        INFO (control);
        auto* c = dynamic_cast<juce::SettableTooltipClient*> (editor.findControl (control));
        REQUIRE (c != nullptr);
        CHECK (c->getTooltip().isNotEmpty());
    }
}

namespace
{
    const StripPreset* factoryNamed (const juce::String& name)
    {
        for (const auto& p : factoryStripPresets())
            if (p.name == name)
                return &p;
        return nullptr;
    }
}

TEST_CASE ("STRIP PRESETS 0.249.0: PR7-C's factory presets as strips, what the strip hasn't named, EQ-only ones left out", "[strip][strippresets]")
{
    const auto& presets = factoryStripPresets();
    CHECK (presets.size() > 100);
    for (const auto& p : presets)
    {
        CHECK (p.strip.inUse());
        CHECK (p.strip == p.strip.limited());
        CHECK (stripPresetGroups().contains (p.group));
        CHECK (p.slot.length() == 3);
    }

    // A01 Kick Punch: hpf (30) + gate (-30, 30, fast, 50, 150) + comp (fet, -14, 4, 15, 200, 2) + three EQ bells.
    const auto* kick = factoryNamed ("Kick Punch");
    REQUIRE (kick != nullptr);
    CHECK (kick->slot == "A01");
    CHECK (kick->group == "Drums");
    CHECK (kick->strip.gate.on);
    CHECK (kick->strip.gate.thresholdDb == -30.0f);
    CHECK (kick->strip.gate.rangeDb == 30.0f);
    CHECK (kick->strip.gate.attack == 0);
    CHECK (kick->strip.gate.holdMs == 50.0f);
    CHECK (kick->strip.gate.releaseMs == 150.0f);
    CHECK (kick->strip.comp.on);
    CHECK (kick->strip.comp.character == 2);
    CHECK (kick->strip.comp.thresholdDb == -14.0f);
    CHECK (kick->strip.comp.ratio == 4.0f);
    CHECK (kick->strip.comp.attackMs == 15.0f);
    CHECK (kick->strip.comp.releaseMs == 200.0f);
    CHECK (kick->strip.comp.makeUpDb == 2.0f);
    CHECK_FALSE (kick->strip.sat.on);
    CHECK_FALSE (kick->strip.colour.on);
    CHECK (kick->leftOut == "EQ, HPF");

    // A09 Drum Bus Glue: the HPF on SC becomes the SC FILTER; saturation and colour carried.
    const auto* glue = factoryNamed ("Drum Bus Glue");
    REQUIRE (glue != nullptr);
    CHECK (glue->strip.sc.on);
    CHECK (glue->strip.sc.hpf == 60.0f);
    CHECK (glue->strip.comp.autoRelease);
    CHECK (glue->strip.sat.on);
    CHECK (glue->strip.sat.driveDb == 6.0f);
    CHECK (glue->strip.sat.harmonics == 50.0f);
    CHECK (glue->strip.sat.mix == 70.0f);
    CHECK (glue->strip.colour.on);
    CHECK (glue->strip.colour.thd == 30.0f);
    CHECK (glue->leftOut.isEmpty());

    // Width and mono maker; the limiter named.
    const auto* wide = factoryNamed ("Master Wide");
    REQUIRE (wide != nullptr);
    CHECK (wide->strip.stereo.monoOn);
    CHECK (wide->strip.stereo.monoHz == 120.0f);
    CHECK (wide->strip.stereo.widthOn);
    CHECK (wide->strip.stereo.width == 120.0f);
    CHECK (wide->leftOut == "LIMITER");

    // Only a limiter: nothing the strip has, not listed. Filters, EQ and saturation: the saturation and width.
    CHECK (factoryNamed ("Master Transparent Limit") == nullptr);
    const auto* phone = factoryNamed ("Telephone Vocal");
    REQUIRE (phone != nullptr);
    CHECK (phone->strip.sat.vcaClip);
    CHECK (phone->strip.stereo.widthOn);
    CHECK (phone->strip.stereo.width == 0.0f);
    CHECK (phone->leftOut == "EQ, HPF, LPF");
    // Every name once.
    std::set<juce::String> names;
    for (const auto& p : presets)
        CHECK (names.insert (p.name).second);
}

TEST_CASE ("STRIP PRESETS 0.249.0: applied as one undo step keeping the console channel; saved, listed, menu, deleted", "[strip][strippresets]")
{
    juce::TemporaryFile folderFile;
    const auto folder = folderFile.getFile();
    Studio studio (false);
    studio.setStripPresetFolderForTests (folder);
    const auto id = studio.getProject().tracks[0].id;

    auto strip = *studio.getChannelStrip (id);
    strip.colour.channel = 31;
    strip.colour.digital = true;
    studio.setChannelStrip (id, strip);
    const auto before = studio.getHistoryPosition();

    const auto* glue = factoryNamed ("Drum Bus Glue");
    REQUIRE (glue != nullptr);
    REQUIRE (studio.applyStripPreset (id, *glue));
    const auto& now = *studio.getChannelStrip (id);
    CHECK (now.comp.on);
    CHECK (now.colour.on);
    CHECK (now.colour.channel == 31);   // the channel's own
    CHECK (now.colour.digital);
    CHECK (studio.getHistoryPosition() == before + 1);
    studio.undo();
    CHECK_FALSE (studio.getChannelStrip (id)->comp.on);
    studio.redo();

    // Saved: a file in the folder, listed after ours, its settings back; a taken name gets " 2".
    const auto file = studio.saveStripPreset (id, "My Bus");
    REQUIRE (file.has_value());
    CHECK (file->getFileName() == "My Bus.pr7strip");
    CHECK (studio.saveStripPreset (id, "My Bus")->getFileName() == "My Bus 2.pr7strip");
    auto presets = studio.getStripPresets();
    CHECK (presets.size() == factoryStripPresets().size() + 2);
    const auto& mine = presets[factoryStripPresets().size()];
    CHECK_FALSE (mine.factory);
    CHECK (mine.name == "My Bus");
    CHECK (mine.strip == *studio.getChannelStrip (id));

    // The menu: ours and yours chosen by their ids.
    auto menu = StripEditor::makeMenu (studio, presets);
    CHECK (menu.getNumItems() > 5);
    studio.setChannelStrip (id, {});
    const auto* kickIndex = factoryNamed ("Kick Punch");
    const auto kickAt = (int) (kickIndex - factoryStripPresets().data());
    StripEditor::menuChosen (studio, id, StripEditor::presetItems + kickAt, presets);
    CHECK (studio.getChannelStrip (id)->gate.on);
    CHECK (studio.getChannelStrip (id)->comp.character == 2);
    StripEditor::menuChosen (studio, id, StripEditor::presetItems + (int) factoryStripPresets().size(), presets);
    CHECK (studio.getChannelStrip (id)->sat.on);

    // Deleted (only yours, only in the folder).
    StripEditor::menuChosen (studio, id, StripEditor::deleteItems + (int) factoryStripPresets().size(), presets);
    CHECK_FALSE (file->exists());
    CHECK (studio.getStripPresets().size() == factoryStripPresets().size() + 1);
    CHECK_FALSE (studio.deleteStripPreset (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("x.pr7strip")));
}
