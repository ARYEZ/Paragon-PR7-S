// MIDI controller lanes (0.25.0): the events, saved and opened, played (ramps, chasing, the reset
// on stop), recorded, split and glued, imported from MIDI files, and edited in the MIDI editor.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Controllers.h"
#include "engine/MidiImport.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;
namespace K = pr7s::ControllerKinds;

namespace
{
    constexpr double rate = 48000.0;
    constexpr juce::int64 beat = 24000;   // at 120 BPM

    Controller cc (int number, double start, int value, bool ramp = false)
    {
        return { Controller::Type::cc, number, start, value, ramp };
    }
    Controller bend (double start, int value, bool ramp = false)
    {
        return { Controller::Type::pitchbend, 0, start, value, ramp };
    }

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
            track.midiChannel = 3;
            project.tracks.push_back (track);
            auto instrument = std::make_unique<Probe>();
            probe = instrument.get();
            engine.setInstrument (1, std::move (instrument));
            engine.update (project);
        }
        void setPart (const Part& part)
        {
            project.tracks[0].parts = { part };
            engine.update (project);
        }
        std::vector<Probe::Received> controllersOf (int number) const
        {
            std::vector<Probe::Received> found;
            for (const auto& r : probe->received)
                if (r.message.isControllerOfType (number))
                    found.push_back (r);
            return found;
        }
        std::vector<Probe::Received> bends() const
        {
            std::vector<Probe::Received> found;
            for (const auto& r : probe->received)
                if (r.message.isPitchWheel())
                    found.push_back (r);
            return found;
        }
    };

    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {})
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, mods.withFlags (juce::ModifierKeys::leftButtonModifier),
                                 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &target, &target, now, downAt, now, 1, at != downAt);
    }
    void drag (MainView& view, juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods = {})
    {
        auto& editor = view.getKeyEditor();
        view.mouseDown (mouseAt (editor, from, from, mods));
        editor.mouseDown (mouseAt (editor, from, from, mods));
        if (to != from)
            for (int step = 1; step <= 8; ++step)
                editor.mouseDrag (mouseAt (editor, from + (to - from) * ((float) step / 8.0f), from, mods));
        editor.mouseUp (mouseAt (editor, to, from, mods));
    }
}

TEST_CASE ("Controller kinds: codes, names, text, values along ramps", "[controllers]")
{
    CHECK (K::codeOf (cc (1, 0.0, 0)) == 1);
    CHECK (K::codeOf (bend (0.0, 0)) == K::pitchbend);
    CHECK (K::codeOf (Controller { Controller::Type::polyPressure, 60, 0.0, 0, false }) == K::polyBase + 60);
    CHECK (K::typeOf (K::aftertouch) == Controller::Type::aftertouch);
    CHECK (K::numberOf (K::polyBase + 60) == 60);
    CHECK (K::name (1) == "CC 1 MODULATION");
    CHECK (K::name (11) == "CC 11 EXPRESSION");
    CHECK (K::name (K::pitchbend) == "PITCHBEND");
    CHECK (K::shortName (64) == "SUS");
    CHECK (K::maxValue (K::pitchbend) == 16383);
    CHECK (K::defaultValue (K::pitchbend) == 8192);
    CHECK (K::valueText (K::pitchbend, 8192 + 4096) == "+4096");
    CHECK (K::valueText (10, 64) == "C");
    CHECK (K::valueText (10, 52) == "L12");
    CHECK (K::valueText (64, 127) == "127 ON");
    CHECK (K::alwaysStep (64));
    CHECK_FALSE (K::alwaysStep (1));
    for (const auto code : { K::velocity, 0, 1, 74, 127, K::pitchbend, K::aftertouch, K::programChange, K::polyBase, K::polyBase + 127 })
        CHECK (K::fromText (K::toText (code)) == code);
    CHECK (K::fromText ("cc128") == -2);
    CHECK (K::fromText ("nonsense") == -2);
    CHECK (K::lanesFromText (K::lanesToText ({ K::velocity, 1, K::pitchbend })) == std::vector<int> { K::velocity, 1, K::pitchbend });

    // A value holds to the next event; a ramp gets there gradually.
    const std::vector<Controller> events { cc (1, 1.0, 0), cc (1, 3.0, 100, true), cc (1, 4.0, 20), cc (7, 0.0, 90) };
    CHECK_FALSE (K::valueAt (events, 1, 0.5).has_value());
    CHECK (*K::valueAt (events, 1, 1.0) == 0);
    CHECK (*K::valueAt (events, 1, 2.0) == 50);
    CHECK (*K::valueAt (events, 1, 3.5) == 100);
    CHECK (*K::valueAt (events, 1, 4.0) == 20);
    CHECK (*K::valueAt (events, 7, 9.0) == 90);

    // Expanded for the engine: the ramp a value at a time, the steps as they are.
    Part part;
    part.length = 8.0;
    part.controllers = events;
    std::vector<std::tuple<double, int, int>> sent;
    K::expandRamps (part, 1.0 / 128.0, [&] (double b, int code, int value) { sent.push_back ({ b, code, value }); });
    auto rampValues = 0;
    auto last = -1;
    for (const auto& [b, code, value] : sent)
        if (code == 1 && b > 1.0 && b < 3.0)
        {
            CHECK (value > last);   // rising, never the same value twice
            last = value;
            ++rampValues;
        }
    CHECK (rampValues > 90);
    CHECK (rampValues <= 100);
    CHECK (std::is_sorted (sent.begin(), sent.end(), [] (const auto& a, const auto& b) { return std::get<0> (a) < std::get<0> (b); }));
}

TEST_CASE ("Controller events and lanes are saved and opened", "[controllers][project]")
{
    Project project;
    Track track;
    track.id = 1;
    track.name = "Lead";
    track.controllerLanes = { K::velocity, 1, K::pitchbend };
    track.pitchbendUp = 12;
    track.pitchbendDown = 2;
    track.pitchbendSnap = true;
    Part part;
    part.length = 4.0;
    part.controllers = { cc (1, 0.5, 64), bend (1.0, 12000, true), { Controller::Type::aftertouch, 0, 2.0, 30, false },
                         { Controller::Type::programChange, 0, 0.0, 5, false }, { Controller::Type::polyPressure, 61, 3.0, 77, false } };
    K::sort (part.controllers);
    track.parts.push_back (part);
    project.tracks.push_back (track);

    const auto opened = Project::fromXml (*project.toXml());
    REQUIRE (opened.has_value());
    const auto& t = opened->tracks[0];
    CHECK (t.controllerLanes == track.controllerLanes);
    CHECK (t.pitchbendUp == 12);
    CHECK (t.pitchbendDown == 2);
    CHECK (t.pitchbendSnap);
    CHECK (t.parts[0].controllers == part.controllers);

    // A song from before 0.25.0: the velocity lane alone.
    Project plain;
    plain.tracks.push_back (Track {});
    plain.tracks[0].id = 1;
    const auto again = Project::fromXml (*plain.toXml());
    REQUIRE (again.has_value());
    CHECK (again->tracks[0].controllerLanes == std::vector<int> { K::velocity });
}

TEST_CASE ("Controllers play on the track's channel at their beats, ramps as runs of values, before notes at the same beat", "[controllers][engine]")
{
    Rig rig;
    Part part;
    part.start = 4.0;
    part.length = 4.0;
    part.notes = { { 1.0, 0.5, 60, 0.8f } };
    part.controllers = { cc (1, 1.0, 100), cc (11, 2.0, 0), cc (11, 3.0, 127, true), bend (1.0, 0) };
    rig.setPart (part);
    rig.engine.play();
    renderSamples (rig.engine, (int) (beat * 9));

    const auto mod = rig.controllersOf (1);
    REQUIRE (mod.size() == 1);
    CHECK (mod[0].sample == beat * 5);
    CHECK (mod[0].message.getChannel() == 3);
    CHECK (mod[0].message.getControllerValue() == 100);
    // At the same beat as the note: the controllers first, then the note.
    size_t modAt = 0, bendAt = 0, noteAt = 0;
    for (size_t i = 0; i < rig.probe->received.size(); ++i)
    {
        const auto& m = rig.probe->received[i].message;
        if (m.isControllerOfType (1)) modAt = i;
        if (m.isPitchWheel() && m.getPitchWheelValue() == 0) bendAt = i;
        if (m.isNoteOn()) noteAt = i;
    }
    CHECK (modAt < noteAt);
    CHECK (bendAt < noteAt);
    // The ramp: from 0 at beat 6 up to 127 at beat 7, a value at a time.
    const auto expr = rig.controllersOf (11);
    REQUIRE (expr.size() > 100);
    CHECK (expr.front().sample == beat * 6);
    CHECK (expr.front().message.getControllerValue() == 0);
    CHECK (expr.back().sample == beat * 7);
    CHECK (expr.back().message.getControllerValue() == 127);
    for (size_t i = 1; i < expr.size(); ++i)
        CHECK (expr[i].message.getControllerValue() > expr[i - 1].message.getControllerValue());

    // Stopped: the bend back to the middle (the reset on stop).
    rig.probe->received.clear();
    rig.engine.stop();
    renderSamples (rig.engine, 1024);
    const auto reset = rig.bends();
    REQUIRE (reset.size() == 1);
    CHECK (reset[0].message.getPitchWheelValue() == 8192);
    CHECK (reset[0].message.getChannel() == 3);
}

TEST_CASE ("Starting in the middle sends each controller's value there first (chasing)", "[controllers][engine]")
{
    Rig rig;
    Part part;
    part.length = 8.0;
    part.controllers = { cc (1, 1.0, 40), cc (1, 2.0, 90), cc (64, 1.5, 127), bend (1.0, 4000), cc (7, 6.0, 10),
                         { Controller::Type::programChange, 0, 0.0, 12, false } };
    rig.setPart (part);
    rig.engine.setPosition (3.0);
    rig.engine.play();
    renderSamples (rig.engine, 1024);
    const auto mod = rig.controllersOf (1);
    REQUIRE (mod.size() == 1);
    CHECK (mod[0].sample == 0);
    CHECK (mod[0].message.getControllerValue() == 90);   // the last before beat 3
    REQUIRE (rig.controllersOf (64).size() == 1);
    CHECK (rig.controllersOf (64)[0].message.getControllerValue() == 127);
    REQUIRE (rig.bends().size() == 1);
    CHECK (rig.bends()[0].message.getPitchWheelValue() == 4000);
    CHECK (rig.controllersOf (7).empty());   // still ahead
    auto programs = 0;
    for (const auto& r : rig.probe->received)
        if (r.message.isProgramChange())
        {
            CHECK (r.message.getProgramChangeNumber() == 12);
            ++programs;
        }
    CHECK (programs == 1);

    // Stopping: the pedal up and the bend back, so nothing rings on or stays bent.
    rig.probe->received.clear();
    rig.engine.stop();
    renderSamples (rig.engine, 1024);
    REQUIRE (rig.controllersOf (64).size() == 1);
    CHECK (rig.controllersOf (64)[0].message.getControllerValue() == 0);
    REQUIRE (rig.bends().size() == 1);
    CHECK (rig.bends()[0].message.getPitchWheelValue() == 8192);

    // A cycle coming round sends the values at its start again.
    Rig cycle;
    Part loop;
    loop.length = 4.0;
    loop.controllers = { cc (1, 0.0, 10), cc (1, 1.0, 120) };
    cycle.project.loopOn = true;
    cycle.project.loopStart = 0.5;
    cycle.project.loopEnd = 2.0;
    cycle.setPart (loop);
    cycle.engine.setPosition (0.5);
    cycle.engine.play();
    renderSamples (cycle.engine, (int) (beat * 2));   // round once
    const auto values = cycle.controllersOf (1);
    REQUIRE (values.size() >= 3);
    CHECK (values[0].message.getControllerValue() == 10);    // chased at the start
    CHECK (values[1].message.getControllerValue() == 120);   // played at beat 1
    CHECK (values[2].message.getControllerValue() == 10);    // chased again at the cycle's start
}

TEST_CASE ("Splitting and gluing parts carries their controllers", "[controllers][studio]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    Part part;
    part.start = 0.0;
    part.length = 8.0;
    part.controllers = { cc (1, 1.0, 0), cc (1, 5.0, 100, true), cc (7, 6.0, 50) };
    studio.addPart (trackId, part);
    REQUIRE (studio.splitPart (trackId, 0, 3.0) >= 0);
    const auto& parts = studio.getProject().findTrack (trackId)->parts;
    REQUIRE (parts.size() == 2);
    const auto& first = parts[0].start < parts[1].start ? parts[0] : parts[1];
    const auto& second = parts[0].start < parts[1].start ? parts[1] : parts[0];
    // The first half keeps what's before the cut, and the ramp to where it had got.
    REQUIRE (first.controllers.size() == 2);
    CHECK (first.controllers[0] == cc (1, 1.0, 0));
    CHECK (first.controllers[1].ramp);
    CHECK (std::abs (first.controllers[1].value - 50) <= 1);
    // The second starts from that value and ramps on to 100.
    REQUIRE (second.controllers.size() == 3);
    CHECK_THAT (second.controllers[0].start, WithinAbs (0.0, 1.0e-9));
    CHECK (std::abs (second.controllers[0].value - 50) <= 1);
    CHECK_FALSE (second.controllers[0].ramp);
    CHECK_THAT (second.controllers[1].start, WithinAbs (2.0, 1.0e-9));
    CHECK (second.controllers[1].value == 100);
    CHECK (second.controllers[1].ramp);
    CHECK (second.controllers[2] == cc (7, 3.0, 50));

    // Glued back: all of them in their places again.
    const auto firstIndex = parts[0].start < parts[1].start ? 0 : 1;
    REQUIRE (studio.glueParts (trackId, firstIndex, false));
    const auto& glued = studio.getProject().findTrack (trackId)->parts[0];
    CHECK (*K::valueAt (glued.controllers, 1, 1.0) == 0);
    CHECK (std::abs (*K::valueAt (glued.controllers, 1, 4.0) - 75) <= 2);
    CHECK (*K::valueAt (glued.controllers, 1, 5.0) == 100);
    CHECK (*K::valueAt (glued.controllers, 7, 6.0) == 50);
}

TEST_CASE ("Recording keeps what was played on the controllers", "[controllers][record]")
{
    Studio studio (false);
    auto& engine = studio.getEngine();
    const auto trackId = studio.getProject().tracks[0].id;
    studio.setPosition (0.0);
    studio.record();
    REQUIRE (engine.isRecording());
    renderSamples (engine, (int) beat);
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    auto wheel = juce::MidiMessage::controllerEvent (1, 1, 77);
    wheel.setTimeStamp (now);
    engine.getMidiInput().addMessageToQueue (wheel);
    auto bendUp = juce::MidiMessage::pitchWheel (1, 12000);
    bendUp.setTimeStamp (now);
    engine.getMidiInput().addMessageToQueue (bendUp);
    renderSamples (engine, (int) beat);
    studio.stop();
    const auto& parts = studio.getProject().findTrack (trackId)->parts;
    REQUIRE (parts.size() == 1);   // controllers alone make a part
    const auto& events = parts[0].controllers;
    REQUIRE (events.size() == 2);
    const auto modIt = std::find_if (events.begin(), events.end(), [] (const Controller& c) { return c.type == Controller::Type::cc; });
    REQUIRE (modIt != events.end());
    CHECK (modIt->number == 1);
    CHECK (modIt->value == 77);
    CHECK_THAT (modIt->start, WithinAbs (1.0, 0.1));
    const auto bendIt = std::find_if (events.begin(), events.end(), [] (const Controller& c) { return c.type == Controller::Type::pitchbend; });
    REQUIRE (bendIt != events.end());
    CHECK (bendIt->value == 12000);
}

TEST_CASE ("MIDI files bring their controllers in", "[controllers][import]")
{
    juce::MidiMessageSequence seq;
    const auto ppq = 480.0;
    seq.addEvent (juce::MidiMessage::controllerEvent (1, 1, 30), 0.0);
    seq.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0.0);
    seq.addEvent (juce::MidiMessage::noteOff (1, 60), ppq);
    seq.addEvent (juce::MidiMessage::pitchWheel (1, 10000), ppq * 0.5);
    seq.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), ppq * 2.0);
    seq.addEvent (juce::MidiMessage::channelPressureChange (1, 55), ppq * 3.0);
    seq.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote ((int) ppq);
    file.addTrack (seq);
    juce::MemoryOutputStream out;
    REQUIRE (file.writeTo (out, 1));
    juce::MemoryInputStream in (out.getData(), out.getDataSize(), false);
    juce::String error;
    const auto midi = readMidiFile (in, error);
    REQUIRE (midi.has_value());
    REQUIRE (midi->tracks.size() == 1);
    const auto& events = midi->tracks[0].controllers;
    REQUIRE (events.size() == 4);
    CHECK (events[0] == cc (1, 0.0, 30));
    CHECK (events[1] == bend (0.5, 10000));
    CHECK (events[2] == cc (64, 2.0, 127));
    CHECK (events[3] == Controller { Controller::Type::aftertouch, 0, 3.0, 55, false });

    // Into the studio: on the part with the notes.
    const auto temp = juce::File::createTempFile (".mid");
    {
        juce::FileOutputStream stream (temp);
        REQUIRE (file.writeTo (stream, 1));
    }
    Studio studio (false);
    REQUIRE (studio.importMidi (temp, false, 0.0, error) == 1);
    const auto& parts = studio.getProject().tracks.back().parts;
    REQUIRE (parts.size() == 1);
    CHECK (parts[0].controllers.size() == 4);
    temp.deleteFile();
}

TEST_CASE ("The MIDI editor's controller lanes: add, draw, line, select, move, type, copy, remove", "[controllers][ui][editor]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 1000);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, { 4.0, 8.0, "Lead", { { 0.0, 1.0, 60, 0.8f }, { 2.0, 1.0, 62, 0.5f }, { 4.0, 1.0, 64, 0.5f } } });
    auto& arrange = view.getArrangeView();
    studio.dispatchPendingMessages();
    arrange.select (trackId, 0);
    view.openKeyEditor();
    studio.dispatchPendingMessages();
    REQUIRE (view.isKeyEditorOpen());
    view.setKeyEditorSize (100);
    auto& editor = view.getKeyEditor();
    editor.setSize (1300, 700);
    REQUIRE (editor.getPart() != nullptr);
    const auto part = [&] { return *editor.getPart(); };
    const auto& track = [&] () -> const Track& { return *studio.getProject().findTrack (trackId); };

    // At first: the velocity lane alone.
    CHECK (editor.getLanes() == std::vector<int> { K::velocity });
    const auto notesHeight = editor.getNotesArea().getHeight();
    editor.addLane (1);
    editor.addLane (K::pitchbend);
    CHECK (track().controllerLanes == std::vector<int> { K::velocity, 1, K::pitchbend });
    CHECK (editor.getNotesArea().getHeight() < notesHeight);   // the notes make room
    CHECK (editor.getNotesArea().getBottom() <= editor.getLaneArea (0).getY() + 1.0f);
    editor.addLane (1);   // shown already: no second one
    CHECK (editor.getNumLanes() == 3);
    const auto modLane = editor.laneOf (1), bendLane = editor.laneOf (K::pitchbend);
    REQUIRE (modLane == 1);
    REQUIRE (bendLane == 2);
    CHECK (editor.getVelocityArea() == editor.getLaneArea (0));
    // A value's height and back.
    CHECK (editor.valueForY (modLane, editor.yForValue (modLane, 100)) == 100);
    CHECK (editor.valueForY (bendLane, editor.yForValue (bendLane, 8192)) == 8192);

    // DRAW: a click puts an event on the grid step, at the value under the mouse.
    editor.setTool (KeyEditor::Tool::draw);
    const auto lane = editor.getLaneArea (modLane);
    drag (view, { editor.xForBeat (5.1), editor.yForValue (modLane, 90) }, { editor.xForBeat (5.1), editor.yForValue (modLane, 90) });
    REQUIRE (part().controllers.size() == 1);
    CHECK (part().controllers[0].type == Controller::Type::cc);
    CHECK (part().controllers[0].number == 1);
    CHECK_THAT (part().controllers[0].start, WithinAbs (1.0, 1.0e-9));   // song beat 5 = the part's beat 1
    CHECK (std::abs (part().controllers[0].value - 90) <= 1);
    // A drag draws a curve: an event a grid step (a sixteenth) apart.
    drag (view, { editor.xForBeat (6.0) + 1.0f, lane.getBottom() - 8.0f }, { editor.xForBeat (6.95), lane.getY() + 8.0f });
    CHECK (part().controllers.size() == 5);
    // One undo step for the drag.
    studio.undo();
    CHECK (part().controllers.size() == 1);
    studio.redo();
    CHECK (part().controllers.size() == 5);

    // ERASE across them.
    editor.setTool (KeyEditor::Tool::erase);
    drag (view, { editor.xForBeat (5.9), lane.getCentreY() }, { editor.xForBeat (7.1), lane.getCentreY() });
    CHECK (part().controllers.size() == 1);

    // LINE: a ramp needs only its two ends (new events ramp, at first).
    CHECK (studio.getNewControllersRamp());
    editor.setTool (KeyEditor::Tool::line);
    editor.setLineMode (KeyEditor::LineMode::line);
    drag (view, { editor.xForBeat (8.0) + 1.0f, editor.yForValue (modLane, 0) }, { editor.xForBeat (10.0) + 1.0f, editor.yForValue (modLane, 127) });
    REQUIRE (part().controllers.size() == 3);
    CHECK_THAT (part().controllers[1].start, WithinAbs (4.0, 1.0e-9));
    CHECK (part().controllers[1].value <= 2);
    CHECK_THAT (part().controllers[2].start, WithinAbs (6.0, 0.26));
    CHECK (part().controllers[2].value >= 125);
    CHECK (part().controllers[2].ramp);
    // As steps: an event per grid step.
    studio.setNewControllersRamp (false);
    editor.setLineMode (KeyEditor::LineMode::parabola);
    drag (view, { editor.xForBeat (8.0) + 1.0f, editor.yForValue (modLane, 0) }, { editor.xForBeat (9.0) + 1.0f, editor.yForValue (modLane, 127) });
    auto parabola = 0;
    for (const auto& c : part().controllers)
        if (c.start >= 4.0 - 1.0e-9 && c.start <= 5.0 + 1.0e-9)
        {
            ++parabola;
            CHECK_FALSE (c.ramp);
        }
    CHECK (parabola == 5);
    studio.setNewControllersRamp (true);
    // Sine on the pitchbend lane: up and down round where it began.
    editor.setLineMode (KeyEditor::LineMode::sine);
    drag (view, { editor.xForBeat (4.0) + 1.0f, editor.yForValue (bendLane, 8192) }, { editor.xForBeat (5.0), editor.yForValue (bendLane, 12000) });
    auto highest = 0, lowest = 16383;
    for (const auto& c : part().controllers)
        if (c.type == Controller::Type::pitchbend)
            highest = juce::jmax (highest, c.value), lowest = juce::jmin (lowest, c.value);
    CHECK (highest > 11000);
    CHECK (lowest < 5000);

    // SELECT: a box round events selects them; Delete removes them (the lanes have the keys after a click in them).
    editor.setTool (KeyEditor::Tool::select);
    drag (view, { editor.xForBeat (7.95), lane.getY() + 2.0f }, { editor.xForBeat (9.05), lane.getBottom() - 2.0f });
    CHECK (editor.getSelectedControllers().size() == 5);
    CHECK (editor.areLanesFocused());
    CHECK_FALSE (editor.getControllerBox().isEmpty());   // the handles, for two or more
    editor.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey));
    for (const auto& c : part().controllers)
        if (c.type == Controller::Type::cc)
            CHECK ((c.start < 4.0 - 1.0e-9 || c.start > 5.0 + 1.0e-9));

    // Typing a value: the one at beat 1 to 64, and moved to beat 2 (bar 2, beat 3: 2.3.1.0).
    auto modAt = [&] (double b) { for (int i = 0; i < (int) part().controllers.size(); ++i) if (part().controllers[(size_t) i].type == Controller::Type::cc && std::abs (part().controllers[(size_t) i].start - b) < 1.0e-6) return i; return -1; };
    REQUIRE (modAt (1.0) >= 0);
    editor.selectControllers ({ modAt (1.0) });
    CHECK (editor.enterControllerValue ("64", "", false));
    CHECK (part().controllers[(size_t) modAt (1.0)].value == 64);
    CHECK_FALSE (editor.enterControllerValue ("loud", "", std::nullopt));   // not a value: nothing changes
    CHECK (editor.enterControllerValue ("", "2.3.1.0", std::nullopt));
    REQUIRE (modAt (2.0) >= 0);
    CHECK (part().controllers[(size_t) modAt (2.0)].value == 64);
    // Pitchbend in semitones (the grid's range: 2 up), pan as it reads.
    CHECK (editor.parseControllerValue (K::pitchbend, "2st") == 16383);
    CHECK (editor.parseControllerValue (K::pitchbend, "-1 st") == 4096);
    CHECK (editor.parseControllerValue (K::pitchbend, "+4096") == 12288);
    CHECK (editor.parseControllerValue (10, "C") == 64);
    CHECK (editor.parseControllerValue (64, "ON") == 127);
    CHECK (editor.parseControllerValue (1, "200") == 127);

    // Moving one: a drag on it, a beat later and higher.
    editor.selectControllers ({});
    const auto point = editor.getControllerPoint (modAt (2.0));
    drag (view, point, point + juce::Point<float> ((float) (editor.xForBeat (7.0) - editor.xForBeat (6.0)), -10.0f));
    REQUIRE (modAt (3.0) >= 0);
    CHECK (part().controllers[(size_t) modAt (3.0)].value > 64);

    // Copy and paste onto another lane (expression), at the playhead.
    editor.addLane (11);
    editor.selectControllers ({ modAt (3.0) });
    editor.copySelectedControllers();
    studio.setPosition (10.0);
    editor.selectAllOnLane (editor.laneOf (11));   // (the lane last clicked: expression)
    editor.pasteControllers();
    auto pasted = 0;
    for (const auto& c : part().controllers)
        if (c.type == Controller::Type::cc && c.number == 11)
        {
            CHECK_THAT (c.start, WithinAbs (6.0, 1.0e-9));
            ++pasted;
        }
    CHECK (pasted == 1);

    // Pitchbend's grid: SNAP keeps values on its semitones.
    studio.setPitchbendGrid (trackId, 2, 2, true, true);
    const auto snappedValue = editor.valueForY (bendLane, editor.yForValue (bendLane, 12000));
    CHECK ((snappedValue == 12288 || snappedValue == 8192 + 8191 / 2 + 1 || snappedValue == 12288 - 1));

    // The velocity lane with LINE: the notes along it get its values.
    editor.setTool (KeyEditor::Tool::line);
    editor.setLineMode (KeyEditor::LineMode::line);
    const auto vel = editor.getVelocityArea().reduced (0.0f, 6.0f);
    drag (view, { editor.xForBeat (4.0) + 0.5f, vel.getBottom() - vel.getHeight() * 0.2f }, { editor.xForBeat (8.0) + 2.0f, vel.getY() });
    CHECK (part().notes[0].velocity < part().notes[1].velocity);
    CHECK (part().notes[1].velocity < part().notes[2].velocity);
    CHECK (part().notes[2].velocity > 0.95f);

    // Show Used Controllers adds a lane for each kind with events; Velocity Lane Only goes back.
    editor.removeLane (editor.laneOf (K::pitchbend));
    CHECK (editor.laneOf (K::pitchbend) < 0);
    editor.showUsedControllers();
    CHECK (editor.laneOf (K::pitchbend) >= 0);
    editor.showVelocityLaneOnly();
    CHECK (editor.getLanes() == std::vector<int> { K::velocity });
    // Hide all, and back.
    editor.toggleLanesShown();
    CHECK (editor.getNumLanes() == 0);
    CHECK (editor.getVelocityArea().getHeight() == 0.0f);
    editor.toggleLanesShown();
    CHECK (editor.getLanes() == std::vector<int> { K::velocity });
}
