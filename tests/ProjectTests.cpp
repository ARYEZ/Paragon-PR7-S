#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/Project.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

TEST_CASE ("The position reads as bars, beats, sixteenths and ticks, as in Cubase", "[project]")
{
    Project project;   // 4/4
    CHECK (formatBarsBeats (0.0, project) == "1. 1. 1.   0");
    CHECK (formatBarsBeats (1.0, project) == "1. 2. 1.   0");
    CHECK (formatBarsBeats (4.0, project) == "2. 1. 1.   0");
    CHECK (formatBarsBeats (4.25, project) == "2. 1. 2.   0");
    CHECK (formatBarsBeats (4.0 + 1.0 / 480.0, project) == "2. 1. 1.   1");
    CHECK (formatBarsBeats (4.0 - 1.0 / 480.0, project) == "1. 4. 4. 119");

    // 6/8: a bar is three quarter notes, counted in six eighths.
    project.numerator = 6;
    project.denominator = 8;
    CHECK (project.beatsPerBar() == 3.0);
    CHECK (formatBarsBeats (0.5, project) == "1. 2. 1.   0");
    CHECK (formatBarsBeats (3.0, project) == "2. 1. 1.   0");

    CHECK (formatTime (0.0) == "0:00.000");
    CHECK (formatTime (61.5) == "1:01.500");
}

TEST_CASE ("A project saves and opens with everything in it", "[project]")
{
    Project project;
    project.bpm = 97.5;
    project.numerator = 7;
    project.denominator = 8;
    project.loopOn = true;
    project.loopStart = 8.0;
    project.loopEnd = 24.0;
    project.click = true;

    Track synth;
    synth.id = 3;
    synth.name = "Lead";
    synth.colour = trackColour (2);
    synth.volumeDb = -6.0f;
    synth.pan = 0.25f;
    synth.mute = true;
    synth.armed = true;
    Part part { 4.0, 8.0, "Riff", { { 0.0, 0.5, 64, 0.9f }, { 1.5, 0.25, 67, 0.5f } } };
    synth.parts.push_back (part);
    project.tracks.push_back (synth);

    Track plugin;
    plugin.id = 7;
    plugin.name = "Keys";
    plugin.instrument.kind = Instrument::Kind::plugin;
    plugin.instrument.plugin.name = "Some Piano";
    plugin.instrument.plugin.pluginFormatName = "VST3";
    plugin.instrument.plugin.fileOrIdentifier = "C:/VST3/Some Piano.vst3";
    plugin.instrument.state.append ("settings!", 9);
    project.tracks.push_back (plugin);
    project.nextTrackId = 8;

    const auto xml = project.toXml();
    const auto loaded = Project::fromXml (*xml);
    REQUIRE (loaded.has_value());
    CHECK (loaded->bpm == 97.5);
    CHECK (loaded->numerator == 7);
    CHECK (loaded->denominator == 8);
    CHECK (loaded->loopOn);
    CHECK (loaded->loopStart == 8.0);
    CHECK (loaded->loopEnd == 24.0);
    CHECK (loaded->click);
    REQUIRE (loaded->tracks.size() == 2);
    const auto& lead = loaded->tracks[0];
    CHECK (lead.id == 3);
    CHECK (lead.name == "Lead");
    CHECK (lead.colour == trackColour (2));
    CHECK (lead.volumeDb == -6.0f);
    CHECK (lead.pan == 0.25f);
    CHECK (lead.mute);
    CHECK (lead.armed);
    CHECK (lead.instrument.kind == Instrument::Kind::paragonSynth);
    REQUIRE (lead.parts.size() == 1);
    CHECK (lead.parts[0] == part);
    const auto& keys = loaded->tracks[1];
    CHECK (keys.instrument.kind == Instrument::Kind::plugin);
    CHECK (keys.instrument.plugin.name == "Some Piano");
    CHECK (keys.instrument.plugin.fileOrIdentifier == "C:/VST3/Some Piano.vst3");
    CHECK (keys.instrument.state.toString() == "settings!");
    CHECK (loaded->nextTrackId == 8);

    // Not a project: nothing.
    CHECK_FALSE (Project::fromXml (juce::XmlElement ("Something")).has_value());
}

TEST_CASE ("Snapping and the song's length", "[project]")
{
    CHECK (snap (3.4, 1.0) == 3.0);
    CHECK (snap (3.6, 1.0) == 4.0);
    CHECK (snap (5.0, 4.0) == 4.0);
    CHECK (snap (3.37, 0.0) == 3.37);

    Project project;
    CHECK (project.lastBeat() == 0.0);
    Track track;
    track.parts.push_back ({ 8.0, 4.0, {}, {} });
    track.parts.push_back ({ 0.0, 4.0, {}, {} });
    project.tracks.push_back (track);
    CHECK (project.lastBeat() == 12.0);
}
