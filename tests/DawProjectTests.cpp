#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/DawProject.h"
#include "engine/Studio.h"

using namespace pr7s;
using Catch::Matchers::WithinAbs;

namespace
{
    // A .vstpreset as the VST3 SDK writes it: header, chunks, list.
    juce::MemoryBlock vstPreset (const juce::String& component, const juce::String& controller)
    {
        juce::MemoryOutputStream out;
        out.write ("VST3", 4);
        out.writeInt (1);
        out.write ("0123456789ABCDEF0123456789ABCDEF", 32);
        const auto listOffsetAt = out.getPosition();
        out.writeInt64 (0);
        const auto compAt = (juce::int64) out.getPosition();
        out.write (component.toRawUTF8(), component.getNumBytesAsUTF8());
        const auto contAt = (juce::int64) out.getPosition();
        out.write (controller.toRawUTF8(), controller.getNumBytesAsUTF8());
        const auto listAt = (juce::int64) out.getPosition();
        out.write ("List", 4);
        out.writeInt (2);
        out.write ("Comp", 4);
        out.writeInt64 (compAt);
        out.writeInt64 ((juce::int64) component.getNumBytesAsUTF8());
        out.write ("Cont", 4);
        out.writeInt64 (contAt);
        out.writeInt64 ((juce::int64) controller.getNumBytesAsUTF8());
        juce::MemoryBlock block (out.getData(), out.getDataSize());
        const auto offset = juce::ByteOrder::swapIfBigEndian ((juce::uint64) listAt);
        block.copyFrom (&offset, (int) listOffsetAt, 8);
        return block;
    }

    const char* projectXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Project version="1.0">
  <Application name="Cubase" version="15.0.30"/>
  <Transport>
    <Tempo unit="bpm" value="100" id="tempo"/>
    <TimeSignature numerator="3" denominator="4" id="sig"/>
  </Transport>
  <Structure>
    <Track contentType="notes" id="t1" name="Keys" color="#ff1010">
      <Channel role="regular" audioChannels="2" id="c1" solo="false">
        <Devices>
          <Vst3Plugin deviceRole="instrument" deviceName="Test Synth" deviceVendor="Paragon" deviceID="01234567-89AB-CDEF-0123-456789ABCDEF" id="d1">
            <Parameters>
              <RealParameter id="p1" parameterID="1234" name="Cutoff" unit="normalized" min="0" max="1" value="0.5"/>
            </Parameters>
            <State path="plugins/synth.vstpreset"/>
          </Vst3Plugin>
          <Vst3Plugin deviceRole="audioFX" deviceName="Test Delay" deviceVendor="Paragon" id="d2">
            <Enabled value="false" id="e2"/>
          </Vst3Plugin>
        </Devices>
        <Mute value="false" id="m1"/>
        <Pan unit="normalized" min="0" max="1" value="0.25" id="pan1"/>
        <Volume unit="linear" min="0" max="2" value="1.0" id="vol1"/>
        <Sends>
          <Send destination="c4" type="pre" id="s1">
            <Volume unit="decibel" value="-6" id="sv1"/>
            <Enable value="true" id="se1"/>
          </Send>
        </Sends>
      </Channel>
    </Track>
    <Track contentType="audio" id="t2" name="Guitar" color="#e08030">
      <Channel role="regular" audioChannels="1" destination="c3" id="c2">
        <Devices>
          <Equalizer deviceRole="audioFX" deviceName="Channel EQ" id="eq"/>
        </Devices>
        <Volume unit="decibel" value="-3" id="vol2"/>
      </Channel>
    </Track>
    <Track contentType="audio" id="t3" name="Bus">
      <Channel role="submix" id="c3"/>
    </Track>
    <Track contentType="audio" id="t4" name="Reverb">
      <Channel role="effect" id="c4"/>
    </Track>
    <Track contentType="tracks" id="t5" name="Strings">
      <Track contentType="notes" id="t6" name="Pad">
        <Channel role="regular" id="c6"/>
      </Track>
    </Track>
    <Track contentType="audio" id="t7" name="Loop">
      <Channel role="regular" id="c7"/>
    </Track>
    <Track contentType="audio notes" id="t9" name="Master">
      <Channel role="master" id="c9">
        <Volume unit="linear" value="0.5" id="vol9"/>
      </Channel>
    </Track>
  </Structure>
  <Arrangement id="a">
    <Lanes timeUnit="beats" id="l0">
      <Lanes track="t1" id="l1">
        <Clips id="cl1">
          <Clip time="4" duration="8" playStart="0" loopStart="0" loopEnd="2" name="Riff">
            <Notes id="n1">
              <Note time="0" duration="0.5" channel="0" key="60" vel="0.5" rel="0.5"/>
              <Note time="1" duration="0.5" channel="0" key="64" vel="0.9" rel="0.5"/>
            </Notes>
          </Clip>
        </Clips>
        <Points id="pt1" unit="linear">
          <Target parameter="vol1"/>
          <RealPoint time="0" value="1.0"/>
          <RealPoint time="8" value="0.5"/>
        </Points>
        <Points id="pt2" unit="normalized">
          <Target parameter="p1"/>
          <RealPoint time="0" value="0.2"/>
        </Points>
      </Lanes>
      <Lanes track="t2" id="l2">
        <Clips id="cl2">
          <Clip time="2" duration="4" contentTimeUnit="seconds" playStart="0.5" fadeTimeUnit="seconds" fadeInTime="0.1" fadeOutTime="0" name="Take 1">
            <Audio channels="1" duration="10" sampleRate="48000" id="au1">
              <File path="audio/tone.wav"/>
            </Audio>
          </Clip>
        </Clips>
      </Lanes>
      <Lanes track="t6" timeUnit="seconds" id="l6">
        <Clips id="cl6">
          <Clip time="1.2" duration="2.4" name="Pad">
            <Notes id="n6">
              <Note time="0" duration="0.6" key="48" vel="0.7"/>
            </Notes>
          </Clip>
        </Clips>
      </Lanes>
      <Clips track="t7" id="cl7">
        <Clip time="8" duration="6" contentTimeUnit="beats" playStart="2" name="Loop">
          <Warps contentTimeUnit="seconds" id="w7">
            <Audio channels="1" duration="10" sampleRate="48000" id="au7">
              <File path="audio/tone.wav"/>
            </Audio>
            <Warp time="0" contentTime="0"/>
            <Warp time="8" contentTime="4.8"/>
          </Warps>
        </Clip>
      </Clips>
      <Points id="tp">
        <Target parameter="tempo"/>
        <RealPoint time="16" value="140"/>
      </Points>
    </Lanes>
  </Arrangement>
  <Scenes/>
</Project>)";

    juce::File makeDawProject (const juce::File& at)
    {
        juce::ZipFile::Builder builder;
        juce::MemoryBlock wav;
        {
            juce::AudioBuffer<float> tone (1, 48000 * 10);
            for (int i = 0; i < tone.getNumSamples(); ++i)
                tone.setSample (0, i, 0.25f * std::sin (6.283185f * 440.0f * (float) i / 48000.0f));
            std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::MemoryOutputStream> (wav, false);
            auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (48000).withNumChannels (1).withBitsPerSample (24));
            writer->writeFromAudioSampleBuffer (tone, 0, tone.getNumSamples());
        }
        builder.addEntry (new juce::MemoryInputStream (projectXml, std::strlen (projectXml), false), 9, "project.xml", juce::Time());
        const auto preset = vstPreset ("COMPONENT-STATE", "CONTROLLER-STATE");
        builder.addEntry (new juce::MemoryInputStream (preset, true), 9, "plugins/synth.vstpreset", juce::Time());
        builder.addEntry (new juce::MemoryInputStream (wav, true), 9, "audio/tone.wav", juce::Time());
        at.deleteFile();
        juce::FileOutputStream out (at);
        builder.writeToStream (out, nullptr);
        return at;
    }
}

TEST_CASE ("DAWproject: a .vstpreset's states become the VST3 host's", "[dawproject]")
{
    const auto state = DawProject::vstPresetToHostState (vstPreset ("COMP", "CONT"));
    REQUIRE_FALSE (state.isEmpty());
    const auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);
    CHECK (xml->hasTagName ("VST3PluginState"));
    juce::MemoryBlock component, controller;
    REQUIRE (component.fromBase64Encoding (xml->getChildByName ("IComponent")->getAllSubText()));
    REQUIRE (controller.fromBase64Encoding (xml->getChildByName ("IEditController")->getAllSubText()));
    CHECK (component.toString() == "COMP");
    CHECK (controller.toString() == "CONT");
    CHECK (DawProject::vstPresetToHostState (juce::MemoryBlock ("not a preset", 12)).isEmpty());
}

TEST_CASE ("DAWproject: a song from Cubase: tempo, tracks, mix, routing, plug-ins, parts (loops, seconds), audio, automation; what can't come is listed", "[dawproject]")
{
    const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("pr7s-dawproject");
    temp.deleteRecursively();
    temp.createDirectory();
    const auto file = makeDawProject (temp.getChildFile ("Song.dawproject"));
    const auto audioFolder = temp.getChildFile ("Song Audio");
    juce::String error;
    const auto imported = DawProject::read (file, audioFolder, {}, error);
    REQUIRE (imported.has_value());
    CHECK (error.isEmpty());
    const auto& p = imported->project;
    CHECK (imported->application == "Cubase 15.0.30");
    CHECK (p.bpm == 100.0);
    CHECK (p.numerator == 3);
    CHECK_THAT (p.masterVolumeDb, WithinAbs (-6.02, 0.01));

    // The tracks, in order: the folder flattened (its Pad after it), the master not a track.
    REQUIRE (p.tracks.size() == 6);
    const auto& keys = p.tracks[0];
    const auto& guitar = p.tracks[1];
    const auto& bus = p.tracks[2];
    const auto& reverb = p.tracks[3];
    const auto& pad = p.tracks[4];
    CHECK (keys.name == "Keys");
    CHECK (keys.kind == Track::Kind::instrument);
    CHECK (guitar.kind == Track::Kind::audio);
    CHECK_FALSE (guitar.stereoInput);
    CHECK (bus.isGroup());
    CHECK (reverb.isFx());
    CHECK (pad.name == "Pad");
    CHECK (keys.colour == FamilyColours::red);
    CHECK (guitar.colour == FamilyColours::orange);

    // The mix and routing.
    CHECK_THAT (keys.volumeDb, WithinAbs (0.0, 0.01));
    CHECK_THAT (keys.pan, WithinAbs (-0.5, 0.001));
    CHECK_THAT (guitar.volumeDb, WithinAbs (-3.0, 0.01));
    CHECK (guitar.output == bus.id);
    REQUIRE (keys.sends.size() == 1);
    CHECK (keys.sends[0].target == reverb.id);
    CHECK (keys.sends[0].preFader);
    CHECK_THAT (keys.sends[0].levelDb, WithinAbs (-6.0, 0.01));

    // Plug-ins: the instrument with its settings (converted), the disabled effect bypassed; the EQ listed as not coming.
    CHECK (keys.instrument.kind == Instrument::Kind::plugin);
    CHECK (keys.instrument.plugin.name == "Test Synth");
    const auto state = juce::AudioProcessor::getXmlFromBinary (keys.instrument.state.getData(), (int) keys.instrument.state.getSize());
    REQUIRE (state != nullptr);
    CHECK (state->hasTagName ("VST3PluginState"));
    REQUIRE (keys.inserts.size() == 1);
    CHECK (keys.inserts[0].plugin.name == "Test Delay");
    CHECK (keys.inserts[0].bypass);
    CHECK (guitar.inserts.empty());
    CHECK (pad.instrument.kind == Instrument::Kind::paragonSynth);   // none came: PR7-K

    // The parts: the looped clip (2 beats of content over 8: four passes), the seconds-timed one (1.2 s at 100 bpm: beat 2).
    REQUIRE (keys.parts.size() == 1);
    CHECK (keys.parts[0].start == 4.0);
    CHECK (keys.parts[0].length == 8.0);
    CHECK (keys.parts[0].name == "Riff");
    REQUIRE (keys.parts[0].notes.size() == 8);
    CHECK (keys.parts[0].notes[2].start == 2.0);
    CHECK (keys.parts[0].notes[2].pitch == 60);
    CHECK_THAT (keys.parts[0].notes[1].velocity, WithinAbs (0.9, 0.001));
    REQUIRE (pad.parts.size() == 1);
    CHECK_THAT (pad.parts[0].start, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (pad.parts[0].length, WithinAbs (4.0, 1.0e-9));
    REQUIRE (pad.parts[0].notes.size() == 1);
    CHECK_THAT (pad.parts[0].notes[0].length, WithinAbs (1.0, 1.0e-9));

    // Audio: at beat 2 for 4 beats, from 0.5 s into the file, faded in 0.1 s; the file copied out.
    REQUIRE (guitar.parts.size() == 1);
    const auto& clip = guitar.parts[0];
    CHECK (clip.start == 2.0);
    CHECK_THAT (clip.length, WithinAbs (4.0, 1.0e-9));
    CHECK_THAT (clip.audioOffset, WithinAbs (0.5, 1.0e-9));
    CHECK_THAT (clip.fadeIn, WithinAbs (0.1, 1.0e-9));
    CHECK (clip.name == "Take 1");
    CHECK (audioFolder.getChildFile (clip.audioFile).existsAsFile());

    // Clips that say their track themselves (as the official library writes them), audio through warps:
    // beat 8 for 6 beats, from 2 beats into the warped content (1.2 s at 100 bpm, the warps not stretching it).
    const auto& loop = p.tracks[5];
    CHECK (loop.name == "Loop");
    REQUIRE (loop.parts.size() == 1);
    CHECK (loop.parts[0].start == 8.0);
    CHECK_THAT (loop.parts[0].length, WithinAbs (6.0, 1.0e-9));
    CHECK_THAT (loop.parts[0].audioOffset, WithinAbs (1.2, 1.0e-9));
    CHECK (loop.parts[0].audioFile == clip.audioFile);   // the same file, copied once

    // Automation: volume (two points, on the fader's travel), R on; the plug-in's knob waits for the plug-in.
    REQUIRE (keys.automation.size() == 1);
    CHECK (keys.automation[0].target == AutomationLane::Target::volume);
    REQUIRE (keys.automation[0].points.size() == 2);
    CHECK_THAT (keys.automation[0].points[0].value, WithinAbs (volumeToTravel (0.0f), 0.001));
    CHECK_THAT (keys.automation[0].points[1].value, WithinAbs (volumeToTravel (-6.0206f), 0.001));
    CHECK (keys.automationRead);
    REQUIRE (imported->pluginLanes.size() == 1);
    CHECK (imported->pluginLanes[0].parameterId == "1234");
    CHECK (imported->pluginLanes[0].lane.target == AutomationLane::Target::instrument);

    // What didn't come across, said.
    const auto notes = imported->notes.joinIntoString ("\n");
    // Its tempo change (0.10.0): on the tempo track, 100 to 140 at beat 16.
    CHECK_FALSE (notes.contains ("Tempo"));
    REQUIRE (p.tempoEvents.size() == 2);
    CHECK (p.tempoTrackOn);
    CHECK (p.tempoTrackShown);
    CHECK (p.tempoEvents[0].bpm == 100.0);
    CHECK (p.tempoEvents[1].beat == 16.0);
    CHECK (p.tempoEvents[1].bpm == 140.0);
    CHECK_FALSE (p.tempoEvents[1].ramp);
    CHECK (notes.contains ("folder \"Strings\""));
    CHECK (notes.contains ("Channel EQ"));
    CHECK (notes.contains ("Test Synth isn't among the plug-ins"));
    CHECK_FALSE (notes.contains ("Time-stretched"));   // (the loop's warps play it at its own speed: nothing lost)

    // Into the studio: a new song, not saved, its audio playing from the clip.
    Studio studio (false);
    juce::String report;
    REQUIRE (studio.importDawProject (file, report));
    CHECK (studio.getProject().tracks.size() == 6);
    CHECK (studio.getFile() == juce::File());
    CHECK (studio.isModified());
    CHECK (report.contains ("Test Synth"));
    const auto* inStudio = studio.getProject().findTrack (studio.getProject().tracks[1].id);
    REQUIRE (inStudio->parts.size() == 1);
    const auto audio = studio.getAudio (inStudio->parts[0].audioFile);
    REQUIRE (audio != nullptr);
    CHECK (audio->sound.getMagnitude (0, audio->sound.getNumSamples()) > 0.2f);

    // Not a DAWproject: an error, the song as it was.
    const auto notOne = temp.getChildFile ("not.dawproject");
    notOne.replaceWithText ("hello");
    CHECK_FALSE (studio.importDawProject (notOne, report));
    CHECK (report.isNotEmpty());
    CHECK (studio.getProject().tracks.size() == 6);
    studio.getAudioFolder().deleteRecursively();
    temp.deleteRecursively();
}
