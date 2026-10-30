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
    Insert delay;
    delay.plugin.name = "PR7-C";
    delay.plugin.pluginFormatName = "VST3";
    delay.plugin.fileOrIdentifier = "C:/VST3/Paragon PR7-C.vst3";
    delay.state.append ("strip!", 6);
    delay.bypass = true;
    synth.inserts.push_back (delay);
    project.tracks.push_back (synth);
    project.masterVolumeDb = -3.5f;
    project.masterInserts.push_back (delay);

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
    // The mixer: the track's inserts and the master's.
    REQUIRE (lead.inserts.size() == 1);
    CHECK (lead.inserts[0].plugin.name == "PR7-C");
    CHECK (lead.inserts[0].plugin.fileOrIdentifier == "C:/VST3/Paragon PR7-C.vst3");
    CHECK (lead.inserts[0].state.toString() == "strip!");
    CHECK (lead.inserts[0].bypass);
    CHECK (keys.inserts.empty());
    CHECK (loaded->masterVolumeDb == -3.5f);
    REQUIRE (loaded->masterInserts.size() == 1);
    CHECK (loaded->masterInserts[0].plugin.name == "PR7-C");

    // A song from before the mixer: master at 0 dB, no effects.
    auto old = project.toXml();
    old->removeChildElement (old->getChildByName ("Master"), true);
    CHECK (Project::fromXml (*old)->masterVolumeDb == 0.0f);

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

TEST_CASE ("Quantize moves note starts to the song's grid, keeping their lengths", "[project][editor]")
{
    // A part starting half a beat into the song: the grid is the song's, not the part's.
    Part part { 0.5, 8.0, "Riff", { { 0.1, 0.3, 60, 0.8f }, { 1.4, 1.0, 62, 0.8f }, { 2.2, 0.5, 64, 0.8f } } };
    CHECK (quantize (part, 1.0));
    REQUIRE (part.notes.size() == 3);
    CHECK_THAT (part.notes[0].start, WithinAbs (0.5, 1.0e-9));   // song beat 0.6 -> 1.0
    CHECK_THAT (part.notes[1].start, WithinAbs (1.5, 1.0e-9));   // 1.9 -> 2.0
    CHECK_THAT (part.notes[2].start, WithinAbs (2.5, 1.0e-9));   // 2.7 -> 3.0
    CHECK (part.notes[0].length == 0.3);                          // lengths stay
    CHECK (part.notes[1].length == 1.0);

    // Only the chosen notes; a note never goes before the part's start.
    Part some { 0.0, 4.0, {}, { { 0.1, 0.25, 60, 0.8f }, { 1.1, 0.25, 62, 0.8f } } };
    CHECK (quantize (some, 0.25, { 1 }));
    CHECK (some.notes[0].start == 0.1);
    CHECK_THAT (some.notes[1].start, WithinAbs (1.0, 1.0e-9));
    Part early { 1.0, 4.0, {}, { { 0.05, 0.25, 60, 0.8f } } };
    quantize (early, 4.0);   // the nearest bar line is 0, before the part
    CHECK (early.notes[0].start == 0.0);

    // Triplets: thirds of a beat.
    Part triplets { 0.0, 4.0, {}, { { 0.30, 0.1, 60, 0.8f }, { 0.70, 0.1, 62, 0.8f } } };
    quantize (triplets, 1.0 / 3.0);
    CHECK_THAT (triplets.notes[0].start, WithinAbs (1.0 / 3.0, 1.0e-9));
    CHECK_THAT (triplets.notes[1].start, WithinAbs (2.0 / 3.0, 1.0e-9));

    // Two of the same pitch landing together become one: the longer.
    Part twins { 0.0, 4.0, {}, { { 0.9, 0.25, 60, 0.8f }, { 1.1, 0.75, 60, 0.6f }, { 1.05, 0.5, 64, 0.8f } } };
    CHECK_FALSE (quantize (twins, 1.0));
    REQUIRE (twins.notes.size() == 2);
    CHECK (twins.notes[0].pitch == 60);
    CHECK (twins.notes[0].length == 0.75);
    CHECK (twins.notes[1].pitch == 64);
}

TEST_CASE ("Note names count middle C as C3", "[project][editor]")
{
    CHECK (noteName (60) == "C3");
    CHECK (noteName (61) == "C#3");
    CHECK (noteName (69) == "A3");
    CHECK (noteName (72) == "C4");
    CHECK (noteName (0) == "C-2");
    CHECK (noteName (127) == "G8");
}
