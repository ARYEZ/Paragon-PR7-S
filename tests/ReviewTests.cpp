#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/DawProject.h"
#include "engine/MidiImport.h"
#include "engine/ProjectLibrary.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/KeyCommands.h"
#include "ui/TempoEditor.h"

// 0.13.1: the code review's fixes, one test each (each failed before its fix).

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;

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
            project.tracks.push_back (track);
            auto instrument = std::make_unique<Probe>();
            probe = instrument.get();
            engine.setInstrument (1, std::move (instrument));
            engine.update (project);
        }
        juce::int64 beats (double b) const { return (juce::int64) std::llround (b * 60.0 / project.bpm * rate); }
        int offsFor (int pitch) const
        {
            auto n = 0;
            for (const auto& r : probe->notesOff())
                n += r.message.getNoteNumber() == pitch ? 1 : 0;
            return n;
        }
    };
}

TEST_CASE ("A note sounding when its part is edited during playback ends: deleted, muted, transposed; one still ended ahead plays on", "[engine][review]")
{
    for (const auto edit : { 0, 1, 2 })
    {
        Rig rig;
        rig.project.tracks[0].parts = { { 0.0, 8.0, "Long", { { 0.0, 4.0, 60, 0.8f } } } };
        rig.engine.update (rig.project);
        rig.engine.play();
        renderSamples (rig.engine, (int) rig.beats (1.0));
        REQUIRE (rig.probe->notesOn().size() == 1);
        auto& part = rig.project.tracks[0].parts[0];
        if (edit == 0) part.notes.clear();
        if (edit == 1) part.muted = true;
        if (edit == 2) part.notes[0].pitch = 62;
        rig.engine.update (rig.project);
        renderSamples (rig.engine, 2048);
        // Ended at once (not left ringing until stop).
        REQUIRE (rig.offsFor (60) == 1);
        CHECK (rig.probe->notesOff().front().sample < rig.beats (1.0) + 2048);
        rig.engine.stop();
    }
    // An edit elsewhere: the note sounding goes on to its own end.
    Rig rig;
    rig.project.tracks[0].parts = { { 0.0, 8.0, "Long", { { 0.0, 4.0, 60, 0.8f }, { 6.0, 1.0, 64, 0.8f } } } };
    rig.engine.update (rig.project);
    rig.engine.play();
    renderSamples (rig.engine, (int) rig.beats (1.0));
    rig.project.tracks[0].parts[0].notes[1].pitch = 65;
    rig.engine.update (rig.project);
    renderSamples (rig.engine, (int) rig.beats (4.0));
    REQUIRE (rig.offsFor (60) == 1);
    CHECK (std::abs (rig.probe->notesOff().front().sample - rig.beats (4.0)) <= 1);
}

TEST_CASE ("A note started on one MIDI channel ends on it when the track's channel changes", "[engine][review]")
{
    Rig rig;
    rig.project.tracks[0].parts = { { 0.0, 8.0, "Long", { { 0.0, 4.0, 60, 0.8f } } } };
    rig.engine.update (rig.project);
    rig.engine.play();
    renderSamples (rig.engine, (int) rig.beats (1.0));
    rig.project.tracks[0].midiChannel = 5;
    rig.engine.update (rig.project);
    renderSamples (rig.engine, (int) rig.beats (4.0));
    REQUIRE_FALSE (rig.probe->notesOff().empty());
    CHECK (rig.probe->notesOff().front().message.getChannel() == 1);
}

TEST_CASE ("releaseNotes: what was left sounding ends at the next block (a render's start)", "[engine][review]")
{
    Rig rig;
    rig.project.tracks[0].parts = { { 0.0, 8.0, "Long", { { 0.0, 4.0, 60, 0.8f } } } };
    rig.engine.update (rig.project);
    rig.engine.play();
    renderSamples (rig.engine, 1024);
    rig.engine.releaseNotes();
    renderSamples (rig.engine, 512);
    CHECK (rig.offsFor (60) == 1);
}

TEST_CASE ("Exports stop at CANCEL, at the next block; the unfinished file is deleted", "[export][review]")
{
    Studio studio (false);
    const auto synth = studio.getProject().tracks[0].id;
    studio.addPart (synth, { 0.0, 64.0, "Long", { { 0.0, 60.0, 60, 1.0f } } });
    const auto file = juce::File::createTempFile (".wav");
    Studio::ExportOptions options;
    options.start = 0.0;
    options.end = 64.0;
    auto blocks = 0;
    studio.renderRunner = [&] (std::function<bool()> work)
    {
        studio.cancelRender();   // (pressed as it starts)
        ++blocks;
        return work();
    };
    juce::String error;
    CHECK_FALSE (studio.exportMix (file, options, error));
    CHECK (studio.wasRenderCancelled());
    CHECK (error == "Cancelled.");
    CHECK_FALSE (file.existsAsFile());
    CHECK_FALSE (studio.isExporting());
    studio.renderRunner = nullptr;
    // The next one runs (the CANCEL was that render's only).
    CHECK (studio.exportMix (file, options, error));
    file.deleteFile();
}

TEST_CASE ("Undo doesn't bring back the solo of a track hidden or disabled since", "[studio][review]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    studio.setSolo (a, true);
    studio.setTracksHidden ({ a }, true);
    studio.renameTrack (b, "Edited");
    studio.undo();
    CHECK (studio.getProject().findTrack (a)->hidden);
    CHECK_FALSE (studio.getProject().findTrack (a)->solo);
}

TEST_CASE ("A part shorter than a sixteenth keeps its length when the song is saved and opened", "[project][review]")
{
    Project project;
    Track track;
    track.id = 1;
    track.parts.push_back ({ 0.0, 0.03125, "Tiny", {} });
    project.tracks.push_back (track);
    const auto back = Project::fromXml (*project.toXml());
    REQUIRE (back.has_value());
    CHECK_THAT (back->tracks[0].parts[0].length, WithinAbs (0.03125, 1.0e-12));
}

TEST_CASE ("Frozen tracks: a copy is a live track of its own; parts can't be moved onto or off one", "[studio][freeze][review]")
{
    Studio studio (false);
    const auto synth = studio.getProject().tracks[0].id;
    studio.addPart (synth, { 0.0, 4.0, "Riff", { { 0.0, 1.0, 60, 1.0f } } });
    const auto other = studio.addTrack ({});
    studio.addPart (other, { 8.0, 4.0, "Other", { { 0.0, 1.0, 64, 1.0f } } });
    juce::String error;
    REQUIRE (studio.freezeTrack (synth, 0.0, error));

    const auto copy = studio.duplicateTrack (synth);
    REQUIRE (copy != 0);
    CHECK_FALSE (studio.getProject().findTrack (copy)->frozen);
    CHECK (studio.getProject().findTrack (copy)->freezeFile.isEmpty());
    CHECK (studio.getEngine().getInstrument (copy) != nullptr);

    // Dragged onto the frozen track: it stays where it was (it used to vanish).
    const auto otherPart = studio.getProject().findTrack (other)->parts[0].id;
    studio.movePart (other, 0, synth, 8.0);
    CHECK (studio.getProject().findTrack (other)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (synth)->parts.size() == 1);
    // (The tracks next to each other on screen: synth, its copy, other. Two up from other: synth.)
    studio.moveParts ({ otherPart }, 0.0, -2, false);
    CHECK (studio.getProject().findTrack (other)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (synth)->parts.size() == 1);
    // Off it: it stays (a frozen track's parts are locked; it used to be copied).
    studio.moveParts ({ studio.getProject().findTrack (synth)->parts[0].id }, 0.0, 2, false);
    CHECK (studio.getProject().findTrack (synth)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (other)->parts.size() == 1);
    studio.unfreezeTrack (synth, true);
}

TEST_CASE ("A new song from a template brings the template's recordings with it", "[hub][audio][review]")
{
    const auto root = juce::File::createTempFile ("pr7s-template");
    root.createDirectory();
    Studio studio (false);
    const auto trackId = studio.addAudioTrack (false);
    // A second's recording.
    juce::AudioBuffer<float> played (1, (int) rate);
    for (int i = 0; i < played.getNumSamples(); ++i)
        played.setSample (0, i, 0.5f);
    studio.setPosition (0.0);
    studio.record();
    juce::AudioBuffer<float> output (2, (int) rate);
    studio.getEngine().render (output, (int) rate, &played);
    studio.stop();
    REQUIRE (studio.getProject().findTrack (trackId)->parts.size() == 1);
    const auto clipFile = studio.getProject().findTrack (trackId)->parts[0].audioFile;
    REQUIRE (studio.save (root.getChildFile ("Song.pr7s")));

    juce::String error, problems;
    REQUIRE (studio.saveAsTemplate ("PR7 review template", error));
    const auto templateFile = ProjectLibrary::templateFolder().getChildFile ("PR7 review template.pr7s");
    studio.newProject();
    REQUIRE (studio.newFromTemplate (templateFile, problems));
    CHECK (studio.getAudioFolder().getChildFile (clipFile).existsAsFile());
    // Saved: its own "<song> Audio" has it.
    const auto saved = root.getChildFile ("New").getChildFile ("New.pr7s");
    saved.getParentDirectory().createDirectory();
    REQUIRE (studio.save (saved));
    CHECK (saved.getSiblingFile ("New Audio").getChildFile (clipFile).existsAsFile());

    templateFile.deleteFile();
    templateFile.getSiblingFile ("PR7 review template Audio").deleteRecursively();
    studio.getLibrary().removeRecent (saved);
    root.deleteRecursively();
}

TEST_CASE ("Save As over a song whose folder already has a file of the same name: ours is copied beside it, not lost", "[studio][audio][review]")
{
    const auto root = juce::File::createTempFile ("pr7s-saveas");
    root.createDirectory();
    Studio studio (false);
    const auto trackId = studio.addAudioTrack (false);
    juce::AudioBuffer<float> played (1, 4800);
    for (int i = 0; i < played.getNumSamples(); ++i)
        played.setSample (0, i, 0.25f);
    studio.setPosition (0.0);
    studio.record();
    juce::AudioBuffer<float> output (2, 4800);
    studio.getEngine().render (output, 4800, &played);
    studio.stop();
    const auto clipFile = studio.getProject().findTrack (trackId)->parts[0].audioFile;
    // Another song's file of that name already where we save.
    const auto target = root.getChildFile ("Song.pr7s");
    target.getSiblingFile ("Song Audio").createDirectory();
    REQUIRE (target.getSiblingFile ("Song Audio").getChildFile (clipFile).replaceWithText ("someone else's"));
    REQUIRE (studio.save (target));
    const auto now = studio.getProject().findTrack (trackId)->parts[0].audioFile;
    CHECK (now != clipFile);
    CHECK (target.getSiblingFile ("Song Audio").getChildFile (clipFile).loadFileAsString() == "someone else's");
    CHECK (target.getSiblingFile ("Song Audio").getChildFile (now).getSize() > 1000);
    studio.getLibrary().removeRecent (target);
    root.deleteRecursively();
}

TEST_CASE ("Key commands: a key on the comma survives saving; macros calling each other stop", "[keys][review]")
{
    KeyCommands::Map map;
    auto runs = 0;
    map.add ({ "Test.A", "Test", "A", {}, [&] { ++runs; } });
    map.assign ("Test.A", KeyCommands::parse ("Ctrl+,"));
    map.assign ("Test.A", KeyCommands::parse ("X"));
    KeyCommands::Map back;
    back.add ({ "Test.A", "Test", "A", {}, [] {} });
    back.fromXml (*map.toXml());
    CHECK (back.keysFor ("Test.A").size() == 2);

    // (0.11.0's files: the keys in one attribute.)
    juce::XmlElement old ("KeyCommands");
    auto* e = old.createNewChildElement ("Command");
    e->setAttribute ("id", "Test.A");
    e->setAttribute ("keys", "Ctrl+,, X");
    back.fromXml (old);
    CHECK (back.keysFor ("Test.A").size() == 2);

    // Two macros of 20 steps calling each other: it ends, after a bounded number of steps.
    const auto m1 = map.addMacro ("M1");
    const auto m2 = map.addMacro ("M2");
    map.addToMacro (m2, "Test.A");
    for (int i = 0; i < 20; ++i)
    {
        map.addToMacro (m1, "Macro.M2");
        map.addToMacro (m2, "Macro.M1");
    }
    map.run ("Macro.M1");
    CHECK (runs <= 1000);
}

namespace
{
    juce::File dawProjectWith (const juce::String& xml, const juce::File& at)
    {
        juce::ZipFile::Builder builder;
        const auto text = xml.toStdString();
        builder.addEntry (new juce::MemoryInputStream (text.data(), text.size(), true), 9, "project.xml", juce::Time());
        at.deleteFile();
        juce::FileOutputStream out (at);
        builder.writeToStream (out, nullptr);
        return at;
    }
}

TEST_CASE ("DAWproject: a clip that contains itself, and references nested deep, end (no crash, no hang)", "[dawproject][review]")
{
    const auto temp = juce::File::createTempFile ("pr7s-dawself");
    temp.createDirectory();
    const juce::String head = R"(<?xml version="1.0" encoding="UTF-8"?><Project version="1.0"><Transport><Tempo value="120" unit="bpm"/></Transport>
      <Structure><Track contentType="notes" id="t1" name="Keys"><Channel id="c1" role="regular"/></Track></Structure><Arrangement><Lanes timeUnit="beats"><Lanes track="t1">)";
    const juce::String tail = "</Lanes></Lanes></Arrangement></Project>";
    juce::String error;

    const auto self = dawProjectWith (head + R"(<Clips id="cl"><Clip time="0" duration="4" reference="cl"/></Clips>)" + tail, temp.getChildFile ("self.dawproject"));
    CHECK (DawProject::read (self, temp.getChildFile ("a"), {}, error).has_value());

    // Eight levels, each of eight clips referring to the level below: 16 million notes once.
    juce::String deep = R"(<Notes id="n0"><Note time="0" duration="0.1" key="60" vel="0.8" channel="0"/></Notes>)";
    for (int level = 1; level <= 8; ++level)
    {
        deep << "<Clips id=\"l" << level << "\">";
        for (int i = 0; i < 8; ++i)
            deep << "<Clip time=\"" << i * 4 << "\" duration=\"4\" reference=\"" << (level == 1 ? juce::String ("n0") : "l" + juce::String (level - 1)) << "\"/>";
        deep << "</Clips>";
    }
    const auto started = juce::Time::getMillisecondCounter();
    const auto read = DawProject::read (dawProjectWith (head + R"(<Clips id="top"><Clip time="0" duration="1000000" reference="l8"/></Clips>)" + deep + tail,
                                                        temp.getChildFile ("deep.dawproject")), temp.getChildFile ("b"), {}, error);
    CHECK (read.has_value());
    CHECK (juce::Time::getMillisecondCounter() - started < 20000);
    temp.deleteRecursively();
}

TEST_CASE ("DAWproject: a .vstpreset whose chunk sizes run past its end is refused (no crash)", "[dawproject][review]")
{
    juce::MemoryOutputStream out;
    out.write ("VST3", 4);
    out.writeInt (1);
    out.write ("0123456789ABCDEF0123456789ABCDEF", 32);
    out.writeInt64 (48);   // the list straight after
    out.write ("List", 4);
    out.writeInt (1);
    out.write ("Comp", 4);
    out.writeInt64 (1);
    out.writeInt64 (std::numeric_limits<juce::int64>::max());
    const auto state = DawProject::vstPresetToHostState (out.getMemoryBlock());
    if (! state.isEmpty())
    {
        const auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
        REQUIRE (xml != nullptr);
        CHECK (xml->getChildByName ("IComponent") == nullptr);
    }
}

TEST_CASE ("MIDI import: a file whose first tempo comes later plays at 120 until it", "[midi][review]")
{
    juce::MidiFile file;
    file.setTicksPerQuarterNote (480);
    juce::MidiMessageSequence track;
    auto tempo = juce::MidiMessage::tempoMetaEvent (500000 * 2);   // 60 BPM
    tempo.setTimeStamp (480 * 4);
    track.addEvent (tempo);
    track.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    track.addEvent (juce::MidiMessage::noteOff (1, 60), 480);
    file.addTrack (track);
    const auto path = juce::File::createTempFile (".mid");
    {
        juce::FileOutputStream out (path);
        file.writeTo (out);
    }
    juce::String error;
    const auto read = readMidiFile (path, error);
    REQUIRE (read.has_value());
    CHECK (read->bpm == 120.0);
    REQUIRE (read->tempoChanges.size() == 2);
    CHECK (read->tempoChanges[0].beat == 0.0);
    CHECK_THAT (read->tempoChanges[1].beat, WithinAbs (4.0, 1.0e-9));
    path.deleteFile();
}

TEST_CASE ("The Tempo Track Editor's selection follows its events when one before it goes; PROCESS takes the value typed", "[tempo][ui][review]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    studio.addTempoTrack();
    studio.addTempoEvent (4.0, 100.0);
    studio.addTempoEvent (8.0, 140.0);
    studio.addTempoEvent (12.0, 90.0);
    studio.dispatchPendingMessages();
    TempoEditor editor (studio);
    editor.selectEvents ({ 3 });   // the 90 at beat 12
    studio.removeTempoEvents ({ 1 });   // (on the tempo track: the 100 at beat 4)
    studio.dispatchPendingMessages();
    REQUIRE (editor.getSelection().size() == 1);
    CHECK (studio.getProject().tempoEvents[(size_t) editor.getSelection()[0]].bpm == 90.0);

    // Typed, then PROCESS clicked straight away (the field never lost the focus).
    Studio other (false);
    other.setTempo (120.0);
    other.setCycle (4.0, 12.0);   // 4 seconds
    ProcessTempoPage page (other);
    page.getNewLengthField().setText ("8", juce::sendNotification);
    page.getProcessKey().onClick();
    CHECK_THAT (other.getProject().tempoMap().secondsBetween (4.0, 12.0), WithinAbs (8.0, 1.0e-9));
}

TEST_CASE ("Freeze and Render in Place hold only their own track: another track's notes aren't in the file", "[render][freeze][review]")
{
    const auto check = [] (bool freeze, bool threaded)
    {
        Studio studio (false);
        const auto a = studio.getProject().tracks[0].id;
        studio.addPart (a, { 0.0, 8.0, "A", { { 0.0, 1.0, 60, 1.0f } } });
        const auto b = studio.addTrack ({});
        studio.addPart (b, { 0.0, 8.0, "B", { { 4.0, 3.0, 67, 1.0f } } });
        studio.selectTrack (a);
        if (threaded)
            studio.renderRunner = [] (std::function<bool()> work)
            {
                auto result = false;
                std::thread worker ([&] { result = work(); });
                worker.join();
                return result;
            };
        juce::String error;
        juce::String file;
        if (freeze)
        {
            REQUIRE (studio.freezeTrack (a, 0.0, error));
            file = studio.getProject().findTrack (a)->freezeFile;
        }
        else
        {
            Studio::RenderOptions options;
            options.tail = Studio::RenderOptions::Tail::off;
            const auto made = studio.renderInPlace ({ { a, 0.0, 8.0, true } }, options, error);
            REQUIRE (made.size() == 1);
            REQUIRE (studio.getProject().findTrack (made[0])->parts.size() == 1);
            file = studio.getProject().findTrack (made[0])->parts[0].audioFile;
        }
        const auto data = studio.getAudio (file);
        REQUIRE (data != nullptr);
        const auto beat = (int) (data->sampleRate * 0.5);   // 120 BPM
        REQUIRE (data->sound.getNumSamples() >= beat * 7);
        CHECK (data->sound.getMagnitude (0, 0, beat) > 0.01f);              // A sounds
        CHECK (data->sound.getMagnitude (0, beat * 4 + 200, beat * 2) < 1.0e-4f);   // B doesn't
    };
    check (true, false);
    check (false, false);
    check (true, true);
    check (false, true);
}

TEST_CASE ("A track frozen earlier isn't heard in the next freeze or render (its freeze file played into them)", "[render][freeze][review]")
{
    for (const auto freeze : { true, false })
    {
        Studio studio (false);
        const auto a = studio.getProject().tracks[0].id;
        studio.addPart (a, { 0.0, 8.0, "A", { { 0.0, 1.0, 60, 1.0f } } });
        const auto b = studio.addTrack ({});
        studio.addPart (b, { 0.0, 8.0, "B", { { 4.0, 3.0, 67, 1.0f } } });
        juce::String error;
        REQUIRE (studio.freezeTrack (b, 0.0, error));   // B first: a file of its own
        juce::String file;
        if (freeze)
        {
            REQUIRE (studio.freezeTrack (a, 0.0, error));
            file = studio.getProject().findTrack (a)->freezeFile;
        }
        else
        {
            Studio::RenderOptions options;
            options.tail = Studio::RenderOptions::Tail::off;
            const auto made = studio.renderInPlace ({ { a, 0.0, 8.0, true } }, options, error);
            REQUIRE (made.size() == 1);
            file = studio.getProject().findTrack (made[0])->parts[0].audioFile;
        }
        const auto data = studio.getAudio (file);
        REQUIRE (data != nullptr);
        const auto beat = (int) (data->sampleRate * 0.5);
        CHECK (data->sound.getMagnitude (0, 0, beat) > 0.01f);
        CHECK (data->sound.getMagnitude (0, beat * 4 + 200, beat * 2) < 1.0e-4f);   // B's frozen sound isn't in it
    }
}

#include "ui/TransportBar.h"

TEST_CASE ("The POSITION screen's value is always as wide as it counts (it pulsed)", "[ui][review]")
{
    const auto cells = [] (const juce::String& text)
    {
        auto n = 0.0;
        for (auto c : text)
            n += juce::CharacterFunctions::isDigit (c) || c == 0x2007 ? 1.0 : 0.5;
        return n;
    };
    const auto a = TransportBar::screenPosition ("1. 1. 1.   0", 4), b = TransportBar::screenPosition ("12. 4. 3. 119", 4);
    CHECK (cells (a) == cells (b));
}
