#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/ParagonSynth.h"
#include "engine/Studio.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;

    // An engine with one track (id 1) played by a probe, at 120 BPM: a beat is 24000 samples.
    struct Rig
    {
        Engine engine;
        Project project;
        Probe* probe = nullptr;

        Rig()
        {
            engine.prepare (rate, 512);
            Track track;
            track.id = 1;
            track.name = "Probe";
            project.tracks.push_back (track);
            auto instrument = std::make_unique<Probe>();
            probe = instrument.get();
            engine.setInstrument (1, std::move (instrument));
            engine.update (project);
        }
        void setNotes (const Part& part)
        {
            project.tracks[0].parts = { part };
            engine.update (project);
        }
        juce::int64 beats (double b) const { return (juce::int64) std::llround (b * 60.0 / project.bpm * rate); }
    };
}

TEST_CASE ("Notes play at their beats, in time with the tempo", "[engine]")
{
    Rig rig;
    rig.setNotes ({ 0.0, 4.0, "Riff", { { 0.0, 0.5, 60, 0.8f }, { 1.0, 1.0, 64, 0.8f }, { 2.5, 0.25, 67, 0.8f } } });
    rig.engine.play();
    renderSamples (rig.engine, (int) rig.beats (4.0));

    const auto on = rig.probe->notesOn();
    REQUIRE (on.size() == 3);
    CHECK (on[0].sample == 0);
    CHECK (on[0].message.getNoteNumber() == 60);
    CHECK (on[1].sample == rig.beats (1.0));
    CHECK (on[2].sample == rig.beats (2.5));
    CHECK (on[1].message.getVelocity() == juce::roundToInt (0.8f * 127.0f));
    const auto off = rig.probe->notesOff();
    REQUIRE (off.size() == 3);
    CHECK (off[0].sample == rig.beats (0.5));
    CHECK (off[1].sample == rig.beats (2.0));
    CHECK (off[2].sample == rig.beats (2.75));

    // Half the tempo: twice as far apart.
    Rig slow;
    slow.project.bpm = 60.0;
    slow.setNotes ({ 0.0, 4.0, "Riff", { { 1.0, 0.5, 60, 0.8f } } });
    slow.engine.play();
    renderSamples (slow.engine, 96000);
    REQUIRE (slow.probe->notesOn().size() == 1);
    CHECK (slow.probe->notesOn()[0].sample == 48000);
}

TEST_CASE ("A part plays its notes from where it starts; notes past its end aren't heard", "[engine]")
{
    Rig rig;
    rig.setNotes ({ 4.0, 4.0, "Late", { { 0.0, 1.0, 60, 0.8f }, { 5.0, 1.0, 62, 0.8f } } });
    rig.engine.play();
    renderSamples (rig.engine, (int) rig.beats (12.0));
    const auto on = rig.probe->notesOn();
    REQUIRE (on.size() == 1);
    CHECK (on[0].sample == rig.beats (4.0));

    // A note longer than its part ends with the part.
    Rig cut;
    cut.setNotes ({ 0.0, 2.0, "Short", { { 1.0, 4.0, 60, 0.8f } } });
    cut.engine.play();
    renderSamples (cut.engine, (int) cut.beats (4.0));
    REQUIRE (cut.probe->notesOff().size() == 1);
    CHECK (cut.probe->notesOff()[0].sample == cut.beats (2.0));
}

TEST_CASE ("CYCLE plays round the range, stopping notes held across its end", "[engine]")
{
    // (With 512-sample blocks the cycle's end, 96000 samples, falls exactly
    // on a block boundary every third pass: it must wrap there too.)
    Rig rig;
    rig.project.loopOn = true;
    rig.project.loopStart = 0.0;
    rig.project.loopEnd = 4.0;
    rig.setNotes ({ 0.0, 8.0, "Loop", { { 1.0, 0.5, 60, 0.8f }, { 3.5, 2.0, 67, 0.8f } } });
    rig.engine.play();
    renderSamples (rig.engine, (int) rig.beats (12.0));

    std::vector<juce::int64> c4;
    for (const auto& r : rig.probe->notesOn())
        if (r.message.getNoteNumber() == 60)
            c4.push_back (r.sample);

    REQUIRE (c4.size() == 3);
    CHECK (c4[0] == rig.beats (1.0));
    CHECK (c4[1] == rig.beats (5.0));
    CHECK (c4[2] == rig.beats (9.0));
    // The G held over the cycle's end is stopped there.
    bool stoppedAtEnd = false;
    for (const auto& r : rig.probe->notesOff())
        stoppedAtEnd = stoppedAtEnd || (r.message.getNoteNumber() == 67 && std::abs (r.sample - rig.beats (4.0)) <= 1);
    CHECK (stoppedAtEnd);
    CHECK (rig.engine.getPosition() < 4.0);

    // The same with blocks that never line up with the cycle's end.
    Rig odd;
    odd.project.loopOn = true;
    odd.project.loopEnd = 4.0;
    odd.setNotes ({ 0.0, 8.0, "Loop", { { 1.0, 0.5, 60, 0.8f } } });
    odd.engine.play();
    renderSamples (odd.engine, (int) odd.beats (12.0), 441);
    const auto ons = odd.probe->notesOn();
    REQUIRE (ons.size() == 3);
    CHECK (ons[2].sample == odd.beats (9.0));
}

TEST_CASE ("Stopping or jumping stops the notes sounding", "[engine]")
{
    Rig rig;
    rig.setNotes ({ 0.0, 8.0, "Held", { { 0.0, 8.0, 60, 0.8f } } });
    rig.engine.play();
    renderSamples (rig.engine, (int) rig.beats (1.0));
    REQUIRE (rig.probe->notesOn().size() == 1);
    CHECK (rig.probe->notesOff().empty());
    rig.engine.stop();
    renderSamples (rig.engine, 512);
    REQUIRE (rig.probe->notesOff().size() == 1);
    CHECK (rig.probe->notesOff()[0].message.getNoteNumber() == 60);

    // Jumping while playing: the note stops, and plays again from the new place when reached.
    Rig jump;
    jump.setNotes ({ 0.0, 8.0, "Held", { { 0.0, 8.0, 60, 0.8f } } });
    jump.engine.play();
    renderSamples (jump.engine, (int) jump.beats (1.0));
    jump.engine.setPosition (0.0);
    renderSamples (jump.engine, 1024);
    CHECK (jump.probe->notesOff().size() == 1);
    CHECK (jump.probe->notesOn().size() == 2);
}

TEST_CASE ("What's played live goes to the live track's instrument", "[engine]")
{
    Rig rig;
    auto second = std::make_unique<Probe>();
    auto* other = second.get();
    Track track;
    track.id = 2;
    rig.project.tracks.push_back (track);
    rig.engine.setInstrument (2, std::move (second));
    rig.engine.update (rig.project);

    rig.engine.setLiveTrack (2);
    rig.engine.getKeyboardState().noteOn (1, 62, 0.7f);
    renderSamples (rig.engine, 512);
    CHECK (rig.probe->notesOn().empty());
    REQUIRE (other->notesOn().size() == 1);
    CHECK (other->notesOn()[0].message.getNoteNumber() == 62);
}

TEST_CASE ("The Paragon Synth plays the note's pitch, louder the harder it's played", "[engine][synth]")
{
    ParagonSynth synth;
    synth.prepareToPlay (rate, 512);
    const auto play = [&synth] (int note, float velocity)
    {
        synth.reset();
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, note, velocity), 0);
        std::vector<float> out;
        for (int block = 0; block < 40; ++block)
        {
            synth.processBlock (buffer, midi);
            midi.clear();
            out.insert (out.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + 512);
        }
        // Let it go, so the next note starts afresh.
        midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
        for (int block = 0; block < 60; ++block)
        {
            synth.processBlock (buffer, midi);
            midi.clear();
        }
        return out;
    };
    const auto a4 = play (69, 0.8f);
    CHECK (levelAt (a4, 440.0, rate, 4800) > 4.0f * levelAt (a4, 415.3, rate, 4800));
    CHECK (levelAt (a4, 440.0, rate, 4800) > 4.0f * levelAt (a4, 466.2, rate, 4800));
    const auto soft = play (69, 0.2f), hard = play (69, 1.0f);
    CHECK (rmsOf (hard, 4800) > 2.0f * rmsOf (soft, 4800));
    CHECK (rmsOf (hard, 4800) < 0.5f);   // headroom with a chord on top
}

TEST_CASE ("Volume, pan, mute and solo", "[engine][mix]")
{
    // Two synth tracks: A3 (220 Hz) and G5 (784 Hz, clear of the A's harmonics), held.
    Engine engine;
    engine.prepare (rate, 512);
    Project project;
    for (int id : { 1, 2 })
    {
        Track track;
        track.id = id;
        track.parts.push_back ({ 0.0, 16.0, {}, { { 0.0, 16.0, id == 1 ? 57 : 79, 0.8f } } });
        project.tracks.push_back (track);
        engine.setInstrument (id, std::make_unique<ParagonSynth>());
    }
    const auto measure = [&] (std::vector<float>* rightSide = nullptr)
    {
        engine.update (project);
        engine.stop();
        renderSamples (engine, 24000);   // the last notes die away
        engine.setPosition (0.0);
        engine.play();
        return renderSamples (engine, 24000, 512, rightSide);
    };
    const auto both = measure();
    const auto low = levelAt (both, 220.0, rate, 4800), high = levelAt (both, 783.99, rate, 4800);
    CHECK (low > 0.01f);
    CHECK (high > 0.01f);

    project.tracks[0].mute = true;
    const auto muted = measure();
    CHECK (levelAt (muted, 220.0, rate, 4800) < low * 0.01f);
    CHECK_THAT (levelAt (muted, 783.99, rate, 4800), WithinAbs (high, high * 0.05));

    project.tracks[0].mute = false;
    project.tracks[0].solo = true;
    const auto soloed = measure();
    CHECK (levelAt (soloed, 783.99, rate, 4800) < high * 0.01f);
    CHECK_THAT (levelAt (soloed, 220.0, rate, 4800), WithinAbs (low, low * 0.05));

    project.tracks[0].solo = false;
    project.tracks[0].volumeDb = -6.0f;
    const auto quieter = measure();
    CHECK_THAT (levelAt (quieter, 220.0, rate, 4800) / low, WithinAbs (0.501, 0.03));

    project.tracks[0].volumeDb = 0.0f;
    project.tracks[0].pan = -1.0f;   // hard left: none of it on the right
    std::vector<float> right;
    measure (&right);
    CHECK (levelAt (right, 220.0, rate, 4800) < low * 0.01f);
}

TEST_CASE ("The click ticks on every beat, higher on the bar's first", "[engine][click]")
{
    Engine engine;
    engine.prepare (rate, 512);
    Project project;
    project.click = true;
    engine.update (project);
    engine.play();
    const auto out = renderSamples (engine, 48000);   // two beats at 120 BPM
    const auto window = (size_t) (0.02 * rate);
    CHECK (levelAt (out, 1760.0, rate, 0, window) > 4.0f * levelAt (out, 1320.0, rate, 0, window));
    CHECK (levelAt (out, 1320.0, rate, 24000, 24000 + window) > 4.0f * levelAt (out, 1760.0, rate, 24000, 24000 + window));
    CHECK (rmsOf (out, 3000, 23000) < 1.0e-6f);   // silent between ticks

    project.click = false;
    engine.update (project);
    CHECK (rmsOf (renderSamples (engine, 48000)) < 1.0e-6f);
}

TEST_CASE ("Recording: what's played becomes a part on the armed track, on its beats", "[engine][record]")
{
    Studio studio (false);
    auto& engine = studio.getEngine();
    REQUIRE (studio.getProject().tracks.size() == 1);   // a new song starts with one synth track
    const auto trackId = studio.getProject().tracks[0].id;
    CHECK (studio.getProject().tracks[0].armed);

    studio.setPosition (4.0);   // from bar 2
    studio.record();
    REQUIRE (engine.isRecording());
    renderSamples (engine, 24000);                       // to beat 5
    engine.getKeyboardState().noteOn (1, 60, 0.75f);
    renderSamples (engine, 48000);                       // held two beats
    engine.getKeyboardState().noteOff (1, 60, 0.0f);
    engine.getKeyboardState().noteOn (1, 64, 1.0f);      // still held when recording stops
    renderSamples (engine, 12000);
    studio.stop();
    CHECK_FALSE (engine.isRecording());

    const auto& parts = studio.getProject().findTrack (trackId)->parts;
    REQUIRE (parts.size() == 1);
    const auto& part = parts[0];
    CHECK (part.start == 4.0);    // from the bar recording started in...
    CHECK (part.length == 4.0);   // ...to the end of the bar it stopped in
    REQUIRE (part.notes.size() == 2);
    CHECK (part.notes[0].pitch == 60);
    CHECK_THAT (part.notes[0].start, WithinAbs (1.0, 0.01));
    CHECK_THAT (part.notes[0].length, WithinAbs (2.0, 0.01));
    CHECK_THAT (part.notes[0].velocity, WithinAbs (0.75, 0.01));
    CHECK (part.notes[1].pitch == 64);
    CHECK_THAT (part.notes[1].start, WithinAbs (3.0, 0.01));
    CHECK_THAT (part.notes[1].length, WithinAbs (0.5, 0.01));   // ended where recording stopped
    engine.getKeyboardState().noteOff (1, 64, 0.0f);

    // Undo takes the recording back.
    studio.undo();
    CHECK (studio.getProject().findTrack (trackId)->parts.empty());
    studio.redo();
    CHECK (studio.getProject().findTrack (trackId)->parts.size() == 1);

    // Recording with nothing played makes no part.
    studio.setPosition (0.0);
    studio.record();
    renderSamples (engine, 24000);
    studio.stop();
    CHECK (studio.getProject().findTrack (trackId)->parts.size() == 1);
}

TEST_CASE ("Recording in a cycle: every pass's notes go into one part, in the cycle's place", "[engine][record]")
{
    Studio studio (false);
    auto& engine = studio.getEngine();
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setCycle (0.0, 4.0);
    studio.setCycleOn (true);
    studio.setPosition (0.0);
    studio.record();
    renderSamples (engine, 24000);                    // beat 1, first pass
    engine.getKeyboardState().noteOn (1, 60, 0.8f);
    renderSamples (engine, 12000);
    engine.getKeyboardState().noteOff (1, 60, 0.0f);
    renderSamples (engine, 108000);                   // on to beat 6: beat 2 of the second pass
    engine.getKeyboardState().noteOn (1, 67, 0.8f);
    renderSamples (engine, 12000);
    engine.getKeyboardState().noteOff (1, 67, 0.0f);
    renderSamples (engine, 2048);
    studio.stop();

    const auto& parts = studio.getProject().findTrack (trackId)->parts;
    REQUIRE (parts.size() == 1);
    CHECK (parts[0].start == 0.0);
    CHECK (parts[0].length == 4.0);
    REQUIRE (parts[0].notes.size() == 2);
    CHECK_THAT (parts[0].notes[0].start, WithinAbs (1.0, 0.01));
    CHECK (parts[0].notes[1].pitch == 67);
    CHECK_THAT (parts[0].notes[1].start, WithinAbs (2.0, 0.01));
}

TEST_CASE ("Editing parts, undo and redo, and a song saved and opened", "[studio]")
{
    Studio studio (false);
    const auto first = studio.getProject().tracks[0].id;
    const auto second = studio.addTrack ({});
    REQUIRE (second != 0);
    CHECK (studio.getProject().tracks[1].name == "Synth 2");
    CHECK (studio.getSelectedTrack() == second);

    studio.addPart (first, { 0.0, 4.0, "A", { { 0.0, 1.0, 60, 0.8f } } });
    studio.movePart (first, 0, second, 8.0);
    CHECK (studio.getProject().findTrack (first)->parts.empty());
    REQUIRE (studio.getProject().findTrack (second)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (second)->parts[0].start == 8.0);

    studio.copyPart (second, 0, second, 12.0);
    CHECK (studio.getProject().findTrack (second)->parts.size() == 2);
    studio.resizePart (second, 1, 2.0);
    CHECK (studio.getProject().findTrack (second)->parts[1].length == 2.0);
    studio.deletePart (second, 0);
    CHECK (studio.getProject().findTrack (second)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (second)->parts[0].start == 12.0);

    // Undo walks it all back, step by step; redo forward again.
    studio.undo();   // the delete
    CHECK (studio.getProject().findTrack (second)->parts.size() == 2);
    studio.undo();   // the resize
    CHECK (studio.getProject().findTrack (second)->parts[1].length == 4.0);
    studio.undo();   // the copy
    studio.undo();   // the move
    CHECK (studio.getProject().findTrack (first)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (second)->parts.empty());
    studio.redo();
    CHECK (studio.getProject().findTrack (second)->parts.size() == 1);

    // A track just added: the first change to it can be undone.
    const auto added = studio.addTrack ({});
    studio.setVolume (added, -12.0f);
    studio.undo();
    CHECK (studio.getProject().findTrack (added)->volumeDb == 0.0f);
    studio.redo();
    CHECK (studio.getProject().findTrack (added)->volumeDb == -12.0f);

    studio.setTempo (140.0);
    studio.setSignature (6, 8);
    studio.renameTrack (second, "Bass");
    studio.setMute (first, true);

    const auto file = juce::File::createTempFile (Project::fileExtension);
    REQUIRE (studio.save (file));
    CHECK_FALSE (studio.isModified());

    Studio other (false);
    juce::String problems;
    REQUIRE (other.open (file, problems));
    CHECK (problems.isEmpty());
    const auto& project = other.getProject();
    CHECK (project.bpm == 140.0);
    CHECK (project.numerator == 6);
    CHECK (project.denominator == 8);
    REQUIRE (project.tracks.size() == 3);
    CHECK (project.tracks[0].mute);
    CHECK (project.tracks[1].name == "Bass");
    CHECK (project.tracks[1].parts.size() == 1);
    CHECK (other.getEngine().getInstrument (project.tracks[1].id) != nullptr);   // its synth is loaded
    file.deleteFile();

    // A track whose VST3 instrument isn't on this computer: the song opens, and says so.
    {
        auto xml = project.toXml();
        auto* missing = xml->getChildByName ("Track");
        missing->setAttribute ("instrument", "plugin");
        juce::PluginDescription absent;
        absent.name = "Not Installed";
        absent.pluginFormatName = "VST3";
        absent.fileOrIdentifier = "/nowhere/Not Installed.vst3";
        missing->addChildElement (absent.createXml().release());
        REQUIRE (xml->writeTo (file));
    }
    Studio third (false);
    REQUIRE (third.open (file, problems));
    CHECK (problems.contains ("Not Installed"));
    CHECK (third.getProject().tracks.size() == 3);
    file.deleteFile();
}


TEST_CASE ("Edited notes play where they are now; each edit is one undo step; parts keep their ids", "[studio][editor]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    auto instrument = std::make_unique<Probe>();
    auto* probe = instrument.get();
    studio.getEngine().setInstrument (trackId, std::move (instrument));

    studio.addPart (trackId, { 4.0, 4.0, "A", { { 0.0, 1.0, 60, 0.8f } } });
    const auto before = studio.getProject().tracks[0].parts[0].notes;
    // Moved to the part's third beat, up to E, played softer.
    studio.setNotes (trackId, 0, { { 2.0, 0.5, 64, 0.5f } });

    studio.play();
    const auto beat = (juce::int64) (60.0 / 120.0 * 48000.0);   // 24000 samples at 120 BPM
    renderSamples (studio.getEngine(), (int) (beat * 8));
    studio.stop();
    const auto on = probe->notesOn();
    REQUIRE (on.size() == 1);
    CHECK (on[0].sample == beat * 6);   // the part's start (4) + 2
    CHECK (on[0].message.getNoteNumber() == 64);
    CHECK (on[0].message.getVelocity() == juce::roundToInt (0.5f * 127.0f));
    REQUIRE (probe->notesOff().size() == 1);
    CHECK (probe->notesOff()[0].sample == beat * 6 + beat / 2);

    // One undo puts the old notes back; redo the new ones.
    studio.undo();
    CHECK (studio.getProject().tracks[0].parts[0].notes == before);
    studio.redo();
    CHECK (studio.getProject().tracks[0].parts[0].notes[0].pitch == 64);
    // The same notes again isn't an edit: undo goes back past it to the old notes.
    studio.setNotes (trackId, 0, studio.getProject().tracks[0].parts[0].notes);
    studio.undo();
    CHECK (studio.getProject().tracks[0].parts[0].notes == before);
    studio.redo();

    // Out-of-range values are kept in range.
    studio.setNotes (trackId, 0, { { -1.0, 0.0, 200, 3.0f } });
    const auto& kept = studio.getProject().tracks[0].parts[0].notes[0];
    CHECK (kept.start == 0.0);
    CHECK (kept.length > 0.0);
    CHECK (kept.pitch == 127);
    CHECK (kept.velocity == 1.0f);

    // A part keeps its id when moved (to another track too); a copy gets its own.
    const auto id = studio.getProject().tracks[0].parts[0].id;
    CHECK (id != 0);
    const auto other = studio.addTrack ({});
    studio.movePart (trackId, 0, other, 12.0);
    REQUIRE (studio.findPart (id).has_value());
    CHECK (studio.findPart (id)->first == other);
    studio.copyPart (other, 0, other, 20.0);
    const auto& parts = studio.getProject().findTrack (other)->parts;
    REQUIRE (parts.size() == 2);
    CHECK (parts[1].id != 0);
    CHECK (parts[1].id != id);
    studio.deletePart (other, 0);
    CHECK_FALSE (studio.findPart (id).has_value());
    studio.undo();   // back again, with the same id
    CHECK (studio.findPart (id).has_value());
}
