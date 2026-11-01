#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "engine/TempoMap.h"

#include <juce_audio_formats/juce_audio_formats.h>

using Catch::Matchers::WithinAbs;
using namespace pr7s;

TEST_CASE ("The tempo map: beats to seconds and back, steps and ramps exact (0.10.0)", "[tempo]")
{
    // Fixed: 120 BPM, half a second a beat; before the start too.
    const TempoMap fixed (120.0);
    CHECK (fixed.isConstant());
    CHECK_THAT (fixed.secondsAt (8.0), WithinAbs (4.0, 1.0e-12));
    CHECK_THAT (fixed.beatAt (3.0), WithinAbs (6.0, 1.0e-12));
    CHECK_THAT (fixed.secondsAt (-2.0), WithinAbs (-1.0, 1.0e-12));

    // A step to 60 at beat 4: two seconds to it, then a second a beat.
    const TempoMap step ({ { 0.0, 120.0, false }, { 4.0, 60.0, false } });
    CHECK_FALSE (step.isConstant());
    CHECK_THAT (step.secondsAt (4.0), WithinAbs (2.0, 1.0e-12));
    CHECK_THAT (step.secondsAt (6.0), WithinAbs (4.0, 1.0e-12));
    CHECK_THAT (step.beatAt (4.0), WithinAbs (6.0, 1.0e-12));
    CHECK (step.bpmAt (3.99) == 120.0);
    CHECK (step.bpmAt (4.0) == 60.0);

    // A ramp from 120 down to 60 over four beats: the tempo halfway is 90, and
    // the four beats take 60 / k * ln (60 / 120) seconds (k = -15 BPM a beat): 2.7726.
    const TempoMap ramp ({ { 0.0, 120.0, false }, { 4.0, 60.0, true } });
    CHECK_THAT (ramp.bpmAt (2.0), WithinAbs (90.0, 1.0e-9));
    CHECK_THAT (ramp.secondsAt (4.0), WithinAbs (60.0 / -15.0 * std::log (0.5), 1.0e-9));
    CHECK_THAT (ramp.secondsAt (6.0), WithinAbs (ramp.secondsAt (4.0) + 2.0, 1.0e-9));   // 60 after it
    // Slower than 120 throughout, faster than 60: in between.
    CHECK (ramp.secondsAt (4.0) > 2.0);
    CHECK (ramp.secondsAt (4.0) < 4.0);
    // Back and forth exactly, everywhere.
    for (double b = -1.0; b < 12.0; b += 0.37)
        CHECK_THAT (ramp.beatAt (ramp.secondsAt (b)), WithinAbs (b, 1.0e-9));

    // Unsorted, the first moved to the start; two at one beat: the later.
    const TempoMap messy ({ { 8.0, 100.0, false }, { 2.0, 140.0, false }, { 8.0, 80.0, false } });
    CHECK (messy.getEvents().front().beat == 0.0);
    CHECK (messy.bpmAt (9.0) == 80.0);
}

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
            track.name = "Probe";
            project.tracks.push_back (track);
            auto instrument = std::make_unique<Probe>();
            probe = instrument.get();
            engine.setInstrument (1, std::move (instrument));
            engine.update (project);
        }
        juce::int64 at (double beat) const { return (juce::int64) std::llround (project.tempoMap().secondsAt (beat) * rate); }
    };
}

TEST_CASE ("The engine plays by the tempo track: notes, the click and changes while playing, to the sample (0.10.0)", "[tempo][engine]")
{
    // A step to 60 at beat 4, held to beat 6, then a ramp back up to 120 by beat 8.
    Rig rig;
    rig.project.tempoTrackOn = true;
    rig.project.tempoEvents = { { 0.0, 120.0, false }, { 4.0, 60.0, false }, { 6.0, 60.0, false }, { 8.0, 120.0, true } };
    rig.project.tracks[0].parts = { { 0.0, 12.0, "Riff", { { 2.0, 1.0, 60, 0.8f }, { 5.0, 2.0, 62, 0.8f }, { 7.0, 1.0, 64, 0.8f }, { 10.0, 0.5, 65, 0.8f } } } };
    rig.engine.update (rig.project);
    rig.engine.play();
    renderSamples (rig.engine, (int) rig.at (12.0));
    const auto on = rig.probe->notesOn();
    REQUIRE (on.size() == 4);
    CHECK (on[0].sample == 48000);                 // beat 2 at 120: one second
    CHECK (on[1].sample == 96000 + 48000);         // beat 5: two seconds to beat 4, then a second a beat
    CHECK (on[2].sample == rig.at (7.0));          // into the ramp: exact
    CHECK (on[2].sample > 192000 + 24000);         // (slower than 120 there, faster than 60)
    CHECK (on[2].sample < 192000 + 48000);
    CHECK (on[3].sample == rig.at (10.0));
    CHECK (on[3].sample == rig.at (8.0) + 48000);  // 120 again after the ramp
    // The note from the steady 60 into the ramp: off where the map has its end.
    CHECK (rig.probe->notesOff()[1].sample == rig.at (7.0));

    // A change while playing: the beat carries on from where it was.
    Rig live;
    live.project.tracks[0].parts = { { 0.0, 8.0, "Riff", { { 2.0, 0.5, 60, 0.8f } } } };
    live.engine.update (live.project);
    live.engine.play();
    renderSamples (live.engine, 24064);   // a beat (and a bit of a block) at 120
    live.project.tempoTrackOn = true;
    live.project.tempoEvents = { { 0.0, 60.0, false } };
    live.engine.update (live.project);
    const auto beatNow = live.engine.getPosition();
    CHECK_THAT (beatNow, WithinAbs (24064.0 / 24000.0, 1.0e-9));
    renderSamples (live.engine, 3 * 48000);
    REQUIRE (live.probe->notesOn().size() == 1);
    // From there to beat 2 at 60 BPM: (2 - beat) seconds.
    CHECK (std::abs ((double) live.probe->notesOn()[0].sample - (24064.0 + (2.0 - beatNow) * rate)) <= 1.0);

    // The click on the tempo track's beats: beat 3 at 2 s, beat 4 at 3 s (60 from beat 2).
    Engine engine;
    engine.prepare (rate, 512);
    Project song;
    song.click = true;
    song.tempoTrackOn = true;
    song.tempoEvents = { { 0.0, 120.0, false }, { 2.0, 60.0, false } };
    engine.update (song);
    engine.play();
    const auto out = renderSamples (engine, 4 * 48000);
    const auto window = (size_t) (0.02 * rate);
    CHECK (rmsOf (out, 96000, 96000 + window) > 0.01f);       // beat 3
    CHECK (rmsOf (out, 144000, 144000 + window) > 0.01f);     // beat 4
    CHECK (rmsOf (out, 100000, 140000) < 1.0e-6f);            // nothing between
    CHECK (rmsOf (out, 72000 - 2000, 72000 + 2000) < 1.0e-6f); // (where 120 would have put beat 3)
}

TEST_CASE ("The tempo track in the studio: add, events by type, lock, the cursor, undo, saved (0.10.0)", "[tempo][studio]")
{
    Studio studio (false);
    studio.setTempo (100.0);
    CHECK_FALSE (studio.getProject().followsTempoTrack());

    // Added: on, one event at the start with the song's tempo.
    studio.addTempoTrack();
    const auto& song = studio.getProject();
    CHECK (song.tempoTrackShown);
    CHECK (song.followsTempoTrack());
    REQUIRE (song.tempoEvents.size() == 1);
    CHECK (song.tempoEvents[0].beat == 0.0);
    CHECK (song.tempoEvents[0].bpm == 100.0);

    // New points: Ramp (at first), Step, Automatic (as the one before).
    CHECK (studio.addTempoEvent (8.0, 140.0) == 1);
    CHECK (song.tempoEvents[1].ramp);
    studio.setNewTempoType (Project::NewTempoType::step);
    CHECK (studio.addTempoEvent (4.0, 80.0) == 1);   // (sorted in before it)
    CHECK_FALSE (song.tempoEvents[1].ramp);
    studio.setNewTempoType (Project::NewTempoType::automatic);
    studio.addTempoEvent (12.0, 90.0);
    CHECK (song.tempoEvents[3].ramp);                 // the one before it (beat 8) ramps
    CHECK (song.tempoEvents.size() == 4);
    // At an event's beat: that one's tempo.
    CHECK (studio.addTempoEvent (8.0, 150.0) == 2);
    CHECK (song.tempoEvents.size() == 4);
    CHECK (song.tempoEvents[2].bpm == 150.0);
    CHECK_THAT (song.bpmAt (6.0), WithinAbs (80.0 + (150.0 - 80.0) * 0.5, 1.0e-9));   // the ramp from 4 to 8

    // The first can't be removed; the others can.
    studio.removeTempoEvents ({ 0, 3 });
    CHECK (song.tempoEvents.size() == 3);
    CHECK (song.tempoEvents[0].beat == 0.0);

    // Undo takes back the last edit.
    studio.undo();
    CHECK (song.tempoEvents.size() == 4);

    // Locked: nothing changes.
    studio.setTempoLocked (true);
    CHECK (studio.addTempoEvent (2.0, 70.0) == -1);
    studio.removeTempoEvents ({ 1 });
    CHECK (song.tempoEvents.size() == 4);
    studio.setTempoLocked (false);

    // The tempo at the cursor: the event in effect there.
    studio.getEngine().setPosition (5.0);
    studio.setTempoAtCursor (85.0);
    CHECK (song.tempoEvents[1].bpm == 85.0);
    CHECK (studio.insertTempoEventAtCursor (95.0) == 2);
    CHECK (song.tempoEvents[2].beat == 5.0);

    // Off: the fixed tempo again (the events kept); the cursor's tempo is the fixed one.
    studio.setTempoTrackOn (false);
    CHECK_FALSE (song.followsTempoTrack());
    CHECK (song.bpmAt (9.0) == 100.0);
    studio.setTempoAtCursor (110.0);
    CHECK (song.bpm == 110.0);
    studio.setTempoTrackOn (true);

    // Saved and opened as it was.
    const auto xml = song.toXml();
    const auto loaded = Project::fromXml (*xml);
    REQUIRE (loaded.has_value());
    CHECK (loaded->tempoTrackOn);
    CHECK (loaded->tempoTrackShown);
    CHECK (loaded->tempoEvents == song.tempoEvents);
    CHECK (loaded->newTempoType == Project::NewTempoType::automatic);

    // Removed: hidden and off, its events kept for when it's added again.
    studio.removeTempoTrack();
    CHECK_FALSE (song.tempoTrackShown);
    CHECK_FALSE (song.followsTempoTrack());
    CHECK (song.tempoEvents.size() == 5);
    studio.addTempoTrack();
    CHECK (song.followsTempoTrack());
    CHECK (song.tempoEvents.size() == 5);
}

TEST_CASE ("Audio by the tempo track: an imported file's length in beats, cutting a clip (0.10.0)", "[tempo][audio]")
{
    Studio studio (false);
    studio.setTempo (120.0);
    studio.addTempoTrack();
    studio.setNewTempoType (Project::NewTempoType::step);
    studio.addTempoEvent (4.0, 60.0);

    // Three seconds of audio at beat 2: a second to beat 4 (120), then two beats at 60.
    const auto folder = juce::File::createTempFile ("pr7s-tempo-audio");
    folder.createDirectory();
    const auto wav = folder.getChildFile ("three.wav");
    {
        juce::AudioBuffer<float> buffer (1, 3 * 48000);
        buffer.clear();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (wav);
        auto writer = format.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (48000.0).withNumChannels (1).withBitsPerSample (24));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
    juce::String error;
    const auto track = studio.importAudio ({ wav }, 0, 2.0, error);
    REQUIRE (track != 0);
    const auto& clip = studio.getProject().findTrack (track)->parts.front();
    CHECK_THAT (clip.length, WithinAbs (4.0, 1.0e-9));

    // Cut at beat 5: the second half starts two seconds into the file (1 s to beat 4, 1 s to beat 5).
    studio.splitPart (track, 0, 5.0);
    const auto& parts = studio.getProject().findTrack (track)->parts;
    REQUIRE (parts.size() == 2);
    CHECK_THAT (parts[1].audioOffset, WithinAbs (2.0, 1.0e-9));
    folder.deleteRecursively();
}

#include "ui/Finish.h"
#include "ui/MainView.h"

namespace
{
    juce::MouseEvent mouseOn (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {})
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, 1, at != downAt);
    }
}

TEST_CASE ("The tempo track on screen: its row under the master's, a click on the curve adds an event, drag, Delete, Erase (0.10.0)", "[tempo][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    studio.setTempo (120.0);
    studio.addTempoTrack();
    studio.dispatchPendingMessages();

    // Its row: right under the master's, in the track list and the arrangement.
    auto& arrange = view.getArrangeView();
    auto& timeline = view.getTimeline();
    const auto rows = timeline.layout (studio);
    REQUIRE (rows.size() >= 3);
    CHECK (rows[0].kind == Timeline::Row::Kind::master);
    CHECK (rows[1].kind == Timeline::Row::Kind::tempo);
    CHECK (rows[2].kind == Timeline::Row::Kind::track);
    const auto area = arrange.getTempoArea();
    REQUIRE (! area.isEmpty());

    // A click on the curve at bar 3 (beat 8), at the height of 150 BPM: an event there.
    timeline.firstBeat = 0.0;
    const juce::Point<float> at (timeline.xForBeat (8.0), arrange.yForBpm (area, 150.0));
    arrange.mouseDown (mouseOn (arrange, at, at));
    arrange.mouseUp (mouseOn (arrange, at, at));
    const auto& events = studio.getProject().tempoEvents;
    REQUIRE (events.size() == 2);
    CHECK (events[1].beat == 8.0);
    CHECK_THAT (events[1].bpm, WithinAbs (150.0, 1.0));
    CHECK (arrange.getTempoSelection() == std::vector<int> { 1 });

    // Dragged a bar later and down: it follows (snapped), one undo step.
    const auto from = arrange.getTempoEventPosition (1);
    const juce::Point<float> to (timeline.xForBeat (12.0), arrange.yForBpm (area, 100.0));
    arrange.mouseDown (mouseOn (arrange, from, from));
    for (int step = 1; step <= 4; ++step)
        arrange.mouseDrag (mouseOn (arrange, from + (to - from) * ((float) step / 4.0f), from));
    arrange.mouseUp (mouseOn (arrange, to, from));
    REQUIRE (events.size() == 2);
    CHECK (events[1].beat == 12.0);
    CHECK_THAT (events[1].bpm, WithinAbs (100.0, 1.0));
    studio.undo();
    CHECK (events[1].beat == 8.0);
    studio.redo();

    // The first event can't be dragged along the song.
    {
        const auto first = arrange.getTempoEventPosition (0);
        const auto later = first + juce::Point<float> (200.0f, 0.0f);
        arrange.mouseDown (mouseOn (arrange, first, first));
        arrange.mouseDrag (mouseOn (arrange, later, first));
        arrange.mouseUp (mouseOn (arrange, later, first));
        CHECK (events[0].beat == 0.0);
    }

    // Delete removes the selected (never the first).
    arrange.selectAllTempoEvents();
    CHECK (view.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    REQUIRE (events.size() == 1);

    // Erase: the event clicked goes.
    studio.addTempoEvent (4.0, 140.0);
    arrange.setTool (ArrangeView::Tool::erase);
    const auto e = arrange.getTempoEventPosition (1);
    arrange.mouseDown (mouseOn (arrange, e, e));
    arrange.mouseUp (mouseOn (arrange, e, e));
    CHECK (events.size() == 1);
    arrange.setTool (ArrangeView::Tool::select);

    // Locked: a click adds nothing.
    studio.setTempoLocked (true);
    arrange.mouseDown (mouseOn (arrange, at, at));
    arrange.mouseUp (mouseOn (arrange, at, at));
    CHECK (events.size() == 1);
    studio.setTempoLocked (false);

    // The transport: the tempo track's where the playhead is (a step, so before it the tempo is still 120).
    studio.setNewTempoType (Project::NewTempoType::step);
    studio.addTempoEvent (4.0, 140.0);
    studio.getEngine().setPosition (6.0);
    CHECK (view.getTransport().getTempoText() == "140.00");
    studio.getEngine().setPosition (1.0);
    CHECK (view.getTransport().getTempoText() == "120.00");
}
