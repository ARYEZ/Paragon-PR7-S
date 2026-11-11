#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/MainView.h"

// Takes and comping (0.29.0): lanes, the clip in front, the record modes, the Comp tool's
// actions, Clean Up Lanes and Create Tracks from Lanes.

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    // A mouse event on `target`, as JUCE would send it (the button held down since `downAt`).
    juce::MouseEvent mouseAt (juce::Component& target, juce::Point<float> at, juce::Point<float> downAt, juce::ModifierKeys mods = {})
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at,
                                 mods.withFlags (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &target, &target, now, downAt, now, 1, at != downAt);
    }

    constexpr double rate = 48000.0;
    constexpr juce::int64 beat = 24000;   // samples, at 120 BPM

    std::shared_ptr<juce::AudioBuffer<float>> ramp (int samples)
    {
        auto sound = std::make_shared<juce::AudioBuffer<float>> (2, samples);
        for (int i = 0; i < samples; ++i)
        {
            sound->setSample (0, i, (float) i * 1.0e-5f);
            sound->setSample (1, i, (float) i * -1.0e-5f);
        }
        return sound;
    }

    Part clip (double start, double length, const juce::String& file, double offset = 0.0)
    {
        Part p;
        p.start = start;
        p.length = length;
        p.audioFile = file;
        p.audioOffset = offset;
        return p;
    }

    // A studio with an audio track whose clips play `sound` (one file of DC per name: told apart by level).
    struct Takes
    {
        Studio studio { false };
        int trackId = 0;
        Takes()
        {
            trackId = studio.addAudioTrack (false);
        }
        const Track& track() const { return *studio.getProject().findTrack (trackId); }
    };
}

TEST_CASE ("Only the clip in front is heard where audio clips overlap", "[takes]")
{
    Track track;
    track.kind = Track::Kind::audio;
    track.parts = { clip (0.0, 8.0, "a.wav"), clip (2.0, 2.0, "b.wav"), clip (3.0, 4.0, "c.wav") };
    // Later in the list is in front (equal order): b hides a over 2-4, c hides b over 3-4 and a over 3-7.
    auto heard = heardStretches (track, 0);
    REQUIRE (heard.size() == 2);
    CHECK (heard[0] == std::pair<double, double> { 0.0, 2.0 });
    CHECK (heard[1] == std::pair<double, double> { 7.0, 8.0 });
    heard = heardStretches (track, 1);
    REQUIRE (heard.size() == 1);
    CHECK (heard[0] == std::pair<double, double> { 2.0, 3.0 });
    CHECK (heardStretches (track, 2).size() == 1);

    // a brought to the front (order): all of it heard, nothing of b, c from 8 on (nothing: it ends at 7).
    track.parts[0].order = 5;
    CHECK (heardStretches (track, 0).size() == 1);
    CHECK (heardStretches (track, 1).empty());
    CHECK (heardStretches (track, 2).empty());

    // Muted: hides nothing. A lane soloed: only that lane's.
    track.parts[0].muted = true;
    CHECK (heardStretches (track, 0).empty());
    CHECK (heardStretches (track, 1).size() == 1);
    track.parts[0].muted = false;
    track.parts[2].lane = 1;
    track.soloLane = 1;
    CHECK (heardStretches (track, 0).empty());
    REQUIRE (heardStretches (track, 2).size() == 1);
    CHECK (heardStretches (track, 2)[0] == std::pair<double, double> { 3.0, 7.0 });

    // MIDI parts all play.
    Track midi;
    midi.parts = { Part { 0.0, 4.0, "x", {} }, Part { 1.0, 1.0, "y", {} } };
    CHECK (heardStretches (midi, 0).size() == 1);
    CHECK (heardStretches (midi, 1).size() == 1);
}

TEST_CASE ("The engine plays only the front clip, its fades kept at its own ends", "[takes][engine]")
{
    Engine engine;
    engine.prepare (rate, 512);
    const auto low = std::make_shared<juce::AudioBuffer<float>> (2, (int) rate * 4), high = std::make_shared<juce::AudioBuffer<float>> (2, (int) rate * 4);
    for (int c = 0; c < 2; ++c)
    {
        juce::FloatVectorOperations::fill (low->getWritePointer (c), 0.25f, low->getNumSamples());
        juce::FloatVectorOperations::fill (high->getWritePointer (c), 0.5f, high->getNumSamples());
    }
    engine.setAudioSource ([&] (const juce::String& name) -> std::shared_ptr<const juce::AudioBuffer<float>> { return name == "high.wav" ? high : low; });
    Project project;
    Track track;
    track.id = 1;
    track.kind = Track::Kind::audio;
    track.parts = { clip (0.0, 4.0, "low.wav"), clip (1.0, 1.0, "high.wav") };
    project.tracks.push_back (track);
    engine.addAudioTrack (1);
    engine.update (project);
    engine.play();
    const auto heard = renderSamples (engine, (int) beat * 3);
    // (the balance pan at its centre and 0 dB: the clip's level as it is)
    CHECK_THAT (heard[(size_t) beat / 2], WithinAbs (0.25, 1.0e-4));
    CHECK_THAT (heard[(size_t) (beat + beat / 2)], WithinAbs (0.5, 1.0e-4));    // not 0.75: one clip at a time
    CHECK_THAT (heard[(size_t) (beat * 2 + beat / 2)], WithinAbs (0.25, 1.0e-4));

    // The low one in front: it hides the high one.
    engine.stop();
    project.tracks[0].parts[0].order = 1;
    engine.update (project);
    engine.setPosition (0.0);
    engine.play();
    const auto again = renderSamples (engine, (int) beat * 3);
    CHECK_THAT (again[(size_t) (beat + beat / 2)], WithinAbs (0.25, 1.0e-4));

    // A clip's fade-in stays at its own start even where it's partly hidden.
    engine.stop();
    project.tracks[0].parts[0].order = 0;
    project.tracks[0].parts[1].fadeIn = 1.0;   // seconds: two beats, longer than the clip
    project.tracks[0].parts[1].start = 1.0;
    engine.update (project);
    engine.setPosition (0.0);
    engine.play();
    const auto faded = renderSamples (engine, (int) beat * 2);
    CHECK_THAT (faded[(size_t) (beat + beat / 2)], WithinAbs (0.5 * 0.25, 2.0e-3));   // a quarter of the way into its fade
}

TEST_CASE ("Recording audio in a cycle: Keep History keeps every lap as a take on its own lane, the last in front", "[takes][record]")
{
    Takes t;
    auto& studio = t.studio;
    studio.setCycle (0.0, 2.0);
    studio.setCycleOn (true);
    const auto played = ramp ((int) beat * 5);
    studio.record();
    juce::AudioBuffer<float> output (2, (int) beat * 5);
    studio.getEngine().render (output, (int) beat * 5, played.get());
    studio.stop();
    // Five beats: two whole laps and half of a third (a beat: kept).
    const auto& parts = t.track().parts;
    REQUIRE (parts.size() == 3);
    CHECK (parts[0].lane == 0);
    CHECK (parts[1].lane == 1);
    CHECK (parts[2].lane == 2);
    CHECK_THAT (parts[1].audioOffset, WithinAbs (1.0, 1.0e-9));
    CHECK_THAT (parts[2].audioOffset, WithinAbs (2.0, 1.0e-9));
    CHECK_THAT (parts[2].length, WithinAbs (1.0, 1.0e-6));
    CHECK (parts[0].audioFile == parts[2].audioFile);   // (one file, every lap in it)
    CHECK (parts[2].name.endsWith ("take 3"));
    CHECK (t.track().showLanes);
    // Heard: the third lap's beat, then the second's.
    CHECK (studio.frontPartAt (t.trackId, 0.5) == parts[2].id);
    CHECK (studio.frontPartAt (t.trackId, 1.5) == parts[1].id);
    studio.setPosition (0.0);
    studio.play();
    const auto heard = renderSamples (studio.getEngine(), (int) beat * 2);
    studio.stop();
    CHECK_THAT (heard[10], WithinAbs (played->getSample (0, (int) beat * 4 + 10), 1.0e-6));
    CHECK_THAT (heard[(size_t) beat + 10], WithinAbs (played->getSample (0, (int) beat * 3 + 10), 1.0e-6));
    studio.getAudioFolder().deleteRecursively();
}

TEST_CASE ("Audio record modes: Cycle History + Replace and Replace take the place of what was there", "[takes][record]")
{
    Takes t;
    auto& studio = t.studio;
    const auto played = ramp ((int) beat * 4);
    juce::AudioBuffer<float> output (2, (int) beat * 4);
    // First a straight recording from 0 to 4.
    studio.record();
    studio.getEngine().render (output, (int) beat * 4, played.get());
    studio.stop();
    REQUIRE (t.track().parts.size() == 1);

    // Keep History, over it: on a lane of its own, in front.
    studio.setPosition (1.0);
    studio.record();
    studio.getEngine().render (output, (int) beat, played.get());
    studio.stop();
    REQUIRE (t.track().parts.size() == 2);
    CHECK (t.track().parts[1].lane == 1);
    CHECK (studio.frontPartAt (t.trackId, 1.5) == t.track().parts[1].id);
    studio.undo();

    // Cycle History + Replace: two laps over 1-2 cut out of the first recording; both laps kept.
    auto modes = studio.getProject().recordModes;
    modes.audio = Project::RecordModes::Audio::cycleHistoryReplace;
    studio.setRecordModes (modes);
    studio.setCycle (1.0, 2.0);
    studio.setCycleOn (true);
    studio.setPosition (1.0);
    studio.record();
    studio.getEngine().render (output, (int) beat * 2, played.get());
    studio.stop();
    {
        const auto& parts = t.track().parts;
        REQUIRE (parts.size() == 4);   // 0-1 and 2-4 left of the first, two laps
        auto laps = 0;
        for (const auto& p : parts)
        {
            CHECK_FALSE ((p.start < 1.0 + 1.0e-9 && p.end() > 1.0 + 1.0e-9 && p.lane == 0 && p.length > 1.5));
            if (std::abs (p.start - 1.0) < 1.0e-9 && std::abs (p.length - 1.0) < 1.0e-6)
                ++laps;
        }
        CHECK (laps == 2);
    }
    studio.undo();

    // Replace: only the last lap.
    modes.audio = Project::RecordModes::Audio::replace;
    studio.setRecordModes (modes);
    studio.setPosition (1.0);
    studio.record();
    studio.getEngine().render (output, (int) beat * 2, played.get());
    studio.stop();
    CHECK (t.track().parts.size() == 3);
    CHECK (t.track().numLanes() == 1);
    studio.getAudioFolder().deleteRecursively();
}

TEST_CASE ("MIDI cycle record modes: Mix, Keep Last, Overwrite, Stacked and Mix-Stacked", "[takes][record]")
{
    // Two laps of a four-beat cycle: C3 on beat 1 of the first, G3 on beat 2 of the second.
    const auto playTwoLaps = [] (Studio& studio, Project::RecordModes::MidiCycle mode)
    {
        auto modes = studio.getProject().recordModes;
        modes.midiCycle = mode;
        studio.setRecordModes (modes);
        auto& engine = studio.getEngine();
        studio.setCycle (0.0, 4.0);
        studio.setCycleOn (true);
        studio.setPosition (0.0);
        studio.record();
        renderSamples (engine, 24000);
        engine.getKeyboardState().noteOn (1, 60, 0.8f);
        renderSamples (engine, 12000);
        engine.getKeyboardState().noteOff (1, 60, 0.0f);
        renderSamples (engine, 108000);   // beat 6: beat 2 of the second lap
        engine.getKeyboardState().noteOn (1, 67, 0.8f);
        renderSamples (engine, 12000);
        engine.getKeyboardState().noteOff (1, 67, 0.0f);
        renderSamples (engine, 2048);
    };
    using Cycle = Project::RecordModes::MidiCycle;
    {
        Studio studio (false);
        playTwoLaps (studio, Cycle::mix);
        studio.stop();
        const auto& parts = studio.getProject().tracks[0].parts;
        REQUIRE (parts.size() == 1);
        CHECK (parts[0].notes.size() == 2);
    }
    {
        // Keep Last: stopped during the second lap, so the first is kept.
        Studio studio (false);
        playTwoLaps (studio, Cycle::keepLast);
        studio.stop();
        const auto& parts = studio.getProject().tracks[0].parts;
        REQUIRE (parts.size() == 1);
        REQUIRE (parts[0].notes.size() == 1);
        CHECK (parts[0].notes[0].pitch == 60);
    }
    {
        // Keep Last, the second lap played to its end: it's kept.
        Studio studio (false);
        playTwoLaps (studio, Cycle::keepLast);
        renderSamples (studio.getEngine(), 24000 * 2 + 4096);   // into the third lap (nothing played in it)
        studio.stop();
        const auto& parts = studio.getProject().tracks[0].parts;
        REQUIRE (parts.size() == 1);
        REQUIRE (parts[0].notes.size() == 1);
        CHECK (parts[0].notes[0].pitch == 67);
    }
    {
        // Overwrite: the second lap's G3 at beat 2 overwrites from there on (the C3 at beat 1 stays).
        Studio studio (false);
        playTwoLaps (studio, Cycle::overwrite);
        studio.stop();
        const auto& parts = studio.getProject().tracks[0].parts;
        REQUIRE (parts.size() == 1);
        CHECK (parts[0].notes.size() == 2);
    }
    {
        // Stacked: a part per lap, on lanes, the first muted.
        Studio studio (false);
        playTwoLaps (studio, Cycle::stacked);
        studio.stop();
        const auto& track = studio.getProject().tracks[0];
        REQUIRE (track.parts.size() == 2);
        CHECK (track.parts[0].lane == 0);
        CHECK (track.parts[1].lane == 1);
        CHECK (track.parts[0].muted);
        CHECK_FALSE (track.parts[1].muted);
        REQUIRE (track.parts[0].notes.size() == 1);
        CHECK (track.parts[0].notes[0].pitch == 60);
        REQUIRE (track.parts[1].notes.size() == 1);
        CHECK (track.parts[1].notes[0].pitch == 67);
        CHECK_THAT (track.parts[1].notes[0].start, WithinAbs (2.0, 0.01));
        CHECK (track.showLanes);
    }
    {
        Studio studio (false);
        playTwoLaps (studio, Cycle::mixStacked);
        studio.stop();
        const auto& track = studio.getProject().tracks[0];
        REQUIRE (track.parts.size() == 2);
        CHECK_FALSE (track.parts[0].muted);
        CHECK_FALSE (track.parts[1].muted);
    }
}

TEST_CASE ("MIDI record modes: Merge into the part there, Replace what's under the recording", "[takes][record]")
{
    const auto recordOver = [] (Studio& studio, int pitch)
    {
        auto& engine = studio.getEngine();
        studio.setPosition (0.0);
        studio.record();
        renderSamples (engine, 12000);
        engine.getKeyboardState().noteOn (1, pitch, 0.8f);
        renderSamples (engine, 12000);
        engine.getKeyboardState().noteOff (1, pitch, 0.0f);
        renderSamples (engine, 12000);
        studio.stop();
    };
    using Midi = Project::RecordModes::Midi;
    for (const auto mode : { Midi::newParts, Midi::merge, Midi::replace })
    {
        Studio studio (false);
        auto modes = studio.getProject().recordModes;
        modes.midi = mode;
        studio.setRecordModes (modes);
        recordOver (studio, 60);
        recordOver (studio, 64);
        const auto& parts = studio.getProject().tracks[0].parts;
        if (mode == Midi::newParts)
        {
            REQUIRE (parts.size() == 2);   // both, the second on a lane of its own
            CHECK (parts[1].lane == 1);
        }
        else if (mode == Midi::merge)
        {
            REQUIRE (parts.size() == 1);
            CHECK (parts[0].notes.size() == 2);
        }
        else
        {
            // The first part's note was in the recording's bar: gone, and its part with it.
            REQUIRE (parts.size() == 1);
            REQUIRE (parts[0].notes.size() == 1);
            CHECK (parts[0].notes[0].pitch == 64);
        }
    }
}

TEST_CASE ("To Front, the Comp tool's range, lanes, Clean Up Lanes and Create Tracks from Lanes", "[takes][comp]")
{
    Takes t;
    auto& studio = t.studio;
    // Three takes of one stretch (0-8), one file each, cut end to end from it.
    for (int i = 0; i < 3; ++i)
    {
        auto take = clip (0.0, 8.0, "take" + juce::String (i) + ".wav");
        take.lane = i;
        take.order = i + 1;
        studio.addPart (t.trackId, take);
    }
    const auto idOf = [&] (int lane, double at)
    {
        for (const auto& p : t.track().parts)
            if (p.lane == lane && at >= p.start && at < p.end())
                return p.id;
        return 0;
    };
    CHECK (studio.frontPartAt (t.trackId, 1.0) == idOf (2, 1.0));

    // To Front: the first take.
    studio.bringToFront (idOf (0, 1.0));
    CHECK (studio.frontPartAt (t.trackId, 1.0) == idOf (0, 1.0));
    // Clicked again (toggle): the one it hid (the take that was in front) comes back.
    studio.bringToFront (idOf (0, 1.0), true);
    CHECK (studio.frontPartAt (t.trackId, 1.0) == idOf (2, 1.0));

    // The Comp tool: lane 1 over 2-4. Everything's cut at 2 and 4; lane 1's piece in front there.
    studio.compRange (t.trackId, 1, 2.0, 4.0);
    CHECK (t.track().parts.size() == 9);
    CHECK (studio.frontPartAt (t.trackId, 3.0) == idOf (1, 3.0));
    CHECK (studio.frontPartAt (t.trackId, 1.0) == idOf (2, 1.0));
    CHECK (studio.frontPartAt (t.trackId, 5.0) == idOf (2, 5.0));
    // Lane 1 again over 3-6: its pieces from 2 to 6 in front, the adjacent ones of one file glued back.
    studio.compRange (t.trackId, 1, 3.0, 6.0);
    CHECK (studio.frontPartAt (t.trackId, 5.5) == idOf (1, 5.5));
    CHECK (studio.frontPartAt (t.trackId, 2.5) == idOf (1, 2.5));
    {
        const auto* piece = [&]() -> const Part* { for (const auto& p : t.track().parts) if (p.id == idOf (1, 4.0)) return &p; return nullptr; }();
        REQUIRE (piece != nullptr);
        CHECK_THAT (piece->start, WithinAbs (3.0, 1.0e-9));   // 3-6 glued (2-3 stays apart: in front from the first comp)
        CHECK_THAT (piece->length, WithinAbs (3.0, 1.0e-9));
        CHECK_THAT (piece->audioOffset, WithinAbs (1.5, 1.0e-6));   // 3 beats at 120 BPM
    }

    // Lane solo: only that lane heard.
    studio.setLaneSolo (t.trackId, 0);
    CHECK (studio.frontPartAt (t.trackId, 3.0) == idOf (0, 3.0));
    studio.setLaneSolo (t.trackId, 0);   // (again: off)
    CHECK (t.track().soloLane == -1);

    // Moving a part onto another lane.
    const auto moved = idOf (0, 7.0);
    studio.setPartLane (moved, 3);
    CHECK (t.track().numLanes() == 4);

    // Clean Up Lanes: only what's heard is left, no overlaps, lanes from 0 again.
    studio.cleanUpLanes (t.trackId);
    {
        const auto& track = t.track();
        for (size_t a = 0; a < track.parts.size(); ++a)
            for (size_t b = a + 1; b < track.parts.size(); ++b)
                CHECK_FALSE ((track.parts[a].start < track.parts[b].end() - 1.0e-9 && track.parts[b].start < track.parts[a].end() - 1.0e-9));
        double covered = 0.0;
        for (const auto& p : track.parts)
            covered += p.length;
        CHECK_THAT (covered, WithinAbs (8.0, 1.0e-6));
        CHECK (track.numLanes() <= 3);
    }
    // The moved piece was hidden (moving to a lane doesn't bring it to the front): gone.
    CHECK (std::none_of (t.track().parts.begin(), t.track().parts.end(), [moved] (const Part& p) { return p.id == moved; }));

    // Create Tracks from Lanes: each lane after the first a track of its own.
    const auto lanes = t.track().numLanes();
    const auto before = studio.getProject().tracks.size();
    const auto made = studio.createTracksFromLanes (t.trackId);
    CHECK ((int) made.size() == lanes - 1);
    CHECK (studio.getProject().tracks.size() == before + made.size());
    CHECK (t.track().numLanes() == 1);
    for (const auto id : made)
    {
        const auto* track = studio.getProject().findTrack (id);
        REQUIRE (track != nullptr);
        CHECK (track->isAudio());
        CHECK_FALSE (track->parts.empty());
        CHECK (track->numLanes() == 1);
    }
}

TEST_CASE ("MIDI takes: To Front mutes the others there; the Comp tool mutes the other lanes over its range", "[takes][comp]")
{
    Studio studio (false);
    const auto trackId = studio.getProject().tracks[0].id;
    for (int i = 0; i < 2; ++i)
    {
        Part take { 0.0, 4.0, "take", {} };
        take.notes = { Note { 0.5, 0.5, 60 + i, 0.8f }, Note { 2.5, 0.5, 60 + i, 0.8f } };
        take.lane = i;
        take.muted = i == 0;
        studio.addPart (trackId, take);
    }
    const auto& track = *studio.getProject().findTrack (trackId);
    studio.bringToFront (track.parts[0].id);
    CHECK_FALSE (track.parts[0].muted);
    CHECK (track.parts[1].muted);
    studio.bringToFront (track.parts[0].id, true);   // again: muted
    CHECK (track.parts[0].muted);

    studio.bringToFront (track.parts[1].id);
    studio.compRange (trackId, 0, 2.0, 4.0);
    // Lane 0 heard over 2-4, lane 1 over 0-2.
    for (const auto& p : track.parts)
    {
        const auto inRange = p.start >= 2.0 - 1.0e-9;
        CHECK (p.muted == (p.lane == 0 ? ! inRange : inRange));
    }
}

TEST_CASE ("Lanes and record modes are saved with the song", "[takes][project]")
{
    Project project;
    Track track;
    track.id = 1;
    track.showLanes = true;
    track.soloLane = 2;
    Part part { 0.0, 4.0, "take", {} };
    part.lane = 2;
    part.order = 7;
    track.parts.push_back (part);
    project.tracks.push_back (track);
    project.recordModes.audio = Project::RecordModes::Audio::replace;
    project.recordModes.midi = Project::RecordModes::Midi::merge;
    project.recordModes.midiCycle = Project::RecordModes::MidiCycle::stacked;
    const auto again = Project::fromXml (*project.toXml());
    REQUIRE (again.has_value());
    CHECK (again->recordModes == project.recordModes);
    CHECK (again->tracks[0].showLanes);
    CHECK (again->tracks[0].soloLane == 2);
    CHECK (again->tracks[0].parts[0].lane == 2);
    CHECK (again->tracks[0].parts[0].order == 7);
}

TEST_CASE ("On screen: LANES shows a row per lane; the Comp tool brings a take forward and comps a stretch", "[takes][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    // Two MIDI takes of bars 1-2, on lanes 0 and 1 (the second heard: the first muted, as Stacked leaves them).
    for (int i = 0; i < 2; ++i)
    {
        Part take { 0.0, 8.0, "Take " + juce::String (i + 1), { { 1.0, 0.5, 60 + i * 4, 0.8f }, { 5.0, 0.5, 60 + i * 4, 0.8f } } };
        take.lane = i;
        take.muted = i == 0;
        studio.addPart (trackId, take);
    }
    studio.setShowLanes (trackId, true);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();

    // The layout: the track, then its two lanes and an empty one.
    const auto rows = arrange.getTimeline().layout (studio);
    auto takeRows = 0;
    for (const auto& row : rows)
        takeRows += row.kind == Timeline::Row::Kind::take && row.busId == trackId ? 1 : 0;
    CHECK (takeRows == 3);
    // Each part sits on its lane's row, under the track's.
    const auto first = arrange.getPartBounds (trackId, 0), second = arrange.getPartBounds (trackId, 1);
    CHECK (first.getY() > arrange.trackTop (trackId) + 20.0f);
    CHECK (second.getY() > first.getY());
    CHECK (arrange.laneAtY (first.getCentreY()) == 0);
    CHECK (arrange.laneAtY (second.getCentreY()) == 1);
    CHECK (arrange.trackAtY (second.getCentreY()) == trackId);
    // On the track's own row, what's heard there: the second take.
    const auto onTrack = juce::Point<float> (first.getCentreX(), arrange.trackTop (trackId) + 20.0f);
    REQUIRE (arrange.partAt (onTrack).has_value());
    CHECK (arrange.partAt (onTrack)->second == 1);

    // COMP: a click on the first take brings it forward (MIDI: unmuted, the other muted there).
    arrange.setTool (ArrangeView::Tool::comp);
    const auto at = first.getCentre();
    arrange.mouseDown (mouseAt (arrange, at, at));
    arrange.mouseUp (mouseAt (arrange, at, at));
    const auto& parts = studio.getProject().findTrack (trackId)->parts;
    CHECK_FALSE (parts[0].muted);
    CHECK (parts[1].muted);

    // A drag across the second lane over bar 2: both takes cut at 4 (and 8, their end), lane 2 heard there.
    const auto x4 = arrange.getTimeline().xForBeat (4.0), x8 = arrange.getTimeline().xForBeat (7.99);
    const juce::Point<float> from (x4 + 1.0f, second.getCentreY()), to (x8, second.getCentreY());
    arrange.mouseDown (mouseAt (arrange, from, from));
    for (int step = 1; step <= 4; ++step)
        arrange.mouseDrag (mouseAt (arrange, from + (to - from) * ((float) step / 4.0f), from));
    CHECK (arrange.getCompBand().first < arrange.getCompBand().second);
    arrange.mouseUp (mouseAt (arrange, to, from));
    REQUIRE (parts.size() == 4);
    for (const auto& p : parts)
    {
        const auto late = p.start >= 4.0 - 1.0e-9;
        CHECK (p.muted == (p.lane == 0 ? late : ! late));
    }

    // Dragging a take (SELECT) onto the empty lane under them puts it on a lane of its own.
    arrange.setTool (ArrangeView::Tool::select);
    studio.dispatchPendingMessages();
    auto lastLaneY = 0.0f;
    for (const auto& row : arrange.getTimeline().layout (studio))
        if (row.kind == Timeline::Row::Kind::take && row.busId == trackId && row.laneIndex == 2)
            lastLaneY = (float) (Timeline::rulerHeight + row.top - arrange.getTimeline().scrollY + row.height / 2);
    REQUIRE (lastLaneY > 0.0f);
    int index = 0;
    for (int i = 0; i < (int) parts.size(); ++i)
        if (parts[(size_t) i].lane == 0 && parts[(size_t) i].start < 1.0)
            index = i;
    const auto grab = arrange.getPartBounds (trackId, index).getCentre();
    const juce::Point<float> drop (grab.x, lastLaneY);
    arrange.mouseDown (mouseAt (arrange, grab, grab));
    for (int step = 1; step <= 4; ++step)
        arrange.mouseDrag (mouseAt (arrange, grab + (drop - grab) * ((float) step / 4.0f), grab));
    arrange.mouseUp (mouseAt (arrange, drop, grab));
    CHECK (studio.getProject().findTrack (trackId)->numLanes() == 3);

    // It draws in every finish, the lanes and the comp on the track's row.
    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
    }
}

//==============================================================================
// Comping, part two (0.30.0)

namespace
{
    // A WAV of one steady level (both sides) in the studio's audio folder: takes told apart by level.
    void writeLevel (Studio& studio, const juce::String& name, float level, double seconds)
    {
        auto folder = studio.getAudioFolder();
        folder.createDirectory();
        juce::AudioBuffer<float> sound (2, (int) (rate * seconds));
        for (int c = 0; c < 2; ++c)
            juce::FloatVectorOperations::fill (sound.getWritePointer (c), level, sound.getNumSamples());
        std::unique_ptr<juce::OutputStream> stream (folder.getChildFile (name).createOutputStream());
        auto writer = juce::WavAudioFormat().createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2)
                                                                           .withBitsPerSample (32)
                                                                           .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
        REQUIRE (writer != nullptr);
        writer->writeFromAudioSampleBuffer (sound, 0, sound.getNumSamples());
    }

    const Part& partWithId (const Studio& studio, int trackId, int id)
    {
        for (const auto& p : studio.getProject().findTrack (trackId)->parts)
            if (p.id == id)
                return p;
        FAIL ("no part " << id);
        return studio.getProject().findTrack (trackId)->parts.front();
    }
}

TEST_CASE ("Moving a split or a take's edge moves it on every take sharing it, held by the others", "[takes][comp]")
{
    Takes t;
    auto& studio = t.studio;
    writeLevel (studio, "a.wav", 0.25f, 8.0);
    writeLevel (studio, "b.wav", 0.5f, 8.0);
    auto first = clip (0.0, 8.0, "a.wav");
    auto second = clip (0.0, 8.0, "b.wav");
    second.lane = 1;
    studio.addPart (t.trackId, first);
    studio.addPart (t.trackId, second);
    studio.compRange (t.trackId, 0, 4.0, 8.0);   // a over 4-8: every take cut at 4
    REQUIRE (t.track().parts.size() == 4);

    // The split at 4 to 5: every take's piece ending there ends at 5, every one starting there starts at 5,
    // what they play staying where it was (the second half's recording from 2.5 s, at 120 BPM).
    CHECK_THAT (studio.moveTakeEdge (t.trackId, 4.0, 5.0), WithinAbs (5.0, 1.0e-9));
    for (const auto& p : t.track().parts)
    {
        CHECK ((std::abs (p.start) < 1.0e-9 || std::abs (p.start - 5.0) < 1.0e-9));
        if (p.start > 1.0)
            CHECK_THAT (p.audioOffset, WithinAbs (2.5, 1.0e-6));
    }
    // Past the end: held a 64th short of it.
    CHECK_THAT (studio.moveTakeEdge (t.trackId, 5.0, 9.0), WithinAbs (8.0 - 1.0 / 64.0, 1.0e-9));
    studio.moveTakeEdge (t.trackId, 8.0 - 1.0 / 64.0, 4.0);

    // The takes' end (8) earlier, to 7: on every take; later than the recordings: held at their end (8 s = 16 beats).
    CHECK_THAT (studio.moveTakeEdge (t.trackId, 8.0, 7.0), WithinAbs (7.0, 1.0e-9));
    for (const auto& p : t.track().parts)
        CHECK (p.end() <= 7.0 + 1.0e-9);
    CHECK_THAT (studio.moveTakeEdge (t.trackId, 7.0, 40.0), WithinAbs (16.0, 1.0e-6));
    // A start earlier than its recording's beginning: held at it.
    CHECK_THAT (studio.moveTakeEdge (t.trackId, 0.0, -3.0), WithinAbs (0.0, 1.0e-9));

    // Another take on the lane, not at the edge: an end can't pass it.
    Takes u;
    auto& other = u.studio;
    other.addPart (u.trackId, clip (0.0, 4.0, "x.wav"));
    other.addPart (u.trackId, clip (6.0, 2.0, "y.wav"));
    CHECK_THAT (other.moveTakeEdge (u.trackId, 4.0, 7.5), WithinAbs (6.0, 1.0e-9));
    studio.getAudioFolder().deleteRecursively();
}

TEST_CASE ("Slip Event, Bounce, Delete Overlaps and Merge MIDI in Loop", "[takes][comp]")
{
    {
        // Slip: an audio clip's recording slides under it (held within it); MIDI notes slide.
        Takes t;
        auto& studio = t.studio;
        writeLevel (studio, "a.wav", 0.25f, 8.0);
        studio.addPart (t.trackId, clip (2.0, 4.0, "a.wav", 1.0));
        const auto id = t.track().parts[0].id;
        studio.slipPart (id, 0.5);   // later: a quarter of a second less offset
        CHECK_THAT (partWithId (studio, t.trackId, id).audioOffset, WithinAbs (0.75, 1.0e-9));
        CHECK_THAT (partWithId (studio, t.trackId, id).start, WithinAbs (2.0, 1.0e-9));
        studio.slipPart (id, 10.0);
        CHECK_THAT (partWithId (studio, t.trackId, id).audioOffset, WithinAbs (0.0, 1.0e-9));
        studio.slipPart (id, -100.0);   // (the recording's 8 s: 6 s at most, so 2 s of it still play)
        CHECK_THAT (partWithId (studio, t.trackId, id).audioOffset, WithinAbs (6.0, 1.0e-6));
        studio.getAudioFolder().deleteRecursively();

        Studio midi (false);
        const auto trackId = midi.getProject().tracks[0].id;
        midi.addPart (trackId, { 0.0, 4.0, "riff", { { 1.0, 0.5, 60, 0.8f } } });
        midi.slipPart (midi.getProject().tracks[0].parts[0].id, 0.25);
        CHECK_THAT (midi.getProject().tracks[0].parts[0].notes[0].start, WithinAbs (1.25, 1.0e-9));
    }
    {
        // Bounce (audio): three takes comped become one clip of what was heard, in a new file.
        Takes t;
        auto& studio = t.studio;
        writeLevel (studio, "a.wav", 0.1f, 4.0);
        writeLevel (studio, "b.wav", 0.2f, 4.0);
        writeLevel (studio, "c.wav", 0.3f, 4.0);
        int lane = 0;
        for (const auto* name : { "a.wav", "b.wav", "c.wav" })
        {
            auto take = clip (0.0, 4.0, name);
            take.lane = lane++;
            studio.addPart (t.trackId, take);
        }
        studio.compRange (t.trackId, 0, 1.0, 2.0);
        studio.compRange (t.trackId, 1, 2.0, 3.0);
        // Heard: c 0-1, a 1-2, b 2-3, c 3-4.
        std::vector<int> all;
        for (const auto& p : t.track().parts)
            all.push_back (p.id);
        juce::String error;
        const auto bounced = studio.bounceTakes (all, error);
        REQUIRE (bounced != 0);
        REQUIRE (t.track().parts.size() == 1);
        const auto& result = t.track().parts[0];
        CHECK_THAT (result.start, WithinAbs (0.0, 1.0e-9));
        CHECK_THAT (result.length, WithinAbs (4.0, 1.0e-9));
        const auto data = studio.getAudio (result.audioFile);
        REQUIRE (data != nullptr);
        const auto at = [&] (double beatsIn) { return data->sound.getSample (0, (int) (beatsIn * (double) beat)); };
        CHECK_THAT (at (0.5), WithinAbs (0.3, 1.0e-5));
        CHECK_THAT (at (1.5), WithinAbs (0.1, 1.0e-5));
        CHECK_THAT (at (2.5), WithinAbs (0.2, 1.0e-5));
        CHECK_THAT (at (3.5), WithinAbs (0.3, 1.0e-5));
        studio.undo();
        CHECK (t.track().parts.size() > 1);

        // Delete Overlaps: only what's heard, all on the first lane, nothing overlapping.
        all.clear();
        for (const auto& p : t.track().parts)
            all.push_back (p.id);
        studio.deleteOverlaps (all);
        double covered = 0.0;
        for (const auto& p : t.track().parts)
        {
            CHECK (p.lane == 0);
            covered += p.length;
        }
        CHECK_THAT (covered, WithinAbs (4.0, 1.0e-6));
        CHECK (studio.frontPartAt (t.trackId, 1.5) != 0);
        studio.getAudioFolder().deleteRecursively();
    }
    {
        // Bounce MIDI: the heard takes' notes in one part; Merge MIDI in Loop: onto a new track.
        Studio studio (false);
        const auto trackId = studio.getProject().tracks[0].id;
        for (int i = 0; i < 2; ++i)
        {
            Part take { 0.0, 4.0, "take", { { 0.5, 0.5, 60 + i, 0.8f }, { 2.5, 0.5, 60 + i, 0.8f } } };
            take.lane = i;
            take.muted = i == 0;   // (as Stacked records them)
            studio.addPart (trackId, take);
        }
        studio.compRange (trackId, 0, 2.0, 4.0);   // lane 1 heard over 0-2, lane 0 over 2-4
        studio.setCycle (0.0, 4.0);
        const auto merged = studio.mergeMidiInLoop ({ trackId });
        REQUIRE (merged != 0);
        const auto& copy = *studio.getProject().findTrack (merged);
        REQUIRE (copy.parts.size() == 1);
        REQUIRE (copy.parts[0].notes.size() == 2);
        CHECK (copy.parts[0].notes[0].pitch == 61);
        CHECK (copy.parts[0].notes[1].pitch == 60);

        std::vector<int> all;
        for (const auto& p : studio.getProject().findTrack (trackId)->parts)
            all.push_back (p.id);
        juce::String error;
        REQUIRE (studio.bounceTakes (all, error) != 0);
        const auto& track = *studio.getProject().findTrack (trackId);
        REQUIRE (track.parts.size() == 1);
        REQUIRE (track.parts[0].notes.size() == 2);
        CHECK (track.parts[0].notes[0].pitch == 61);
        CHECK (track.parts[0].notes[1].pitch == 60);
        CHECK (track.parts[0].lane == 0);
    }
}

TEST_CASE ("Audition plays only the track (and lane) without changing the song", "[takes][comp]")
{
    Takes t;
    auto& studio = t.studio;
    const auto before = studio.getProject().toXml()->toString();
    studio.startAudition (t.trackId, 1);
    CHECK (studio.isAuditioning());
    CHECK (studio.getProject().toXml()->toString() == before);
    studio.stopAudition();
    CHECK_FALSE (studio.isAuditioning());
}

TEST_CASE ("On screen: COMP moves a split, splits every take, slips; the comp handle brings a take forward", "[takes][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    for (int i = 0; i < 2; ++i)
    {
        Part take { 0.0, 8.0, "Take " + juce::String (i + 1), { { 1.0, 0.5, 60 + i * 4, 0.8f }, { 5.0, 0.5, 60 + i * 4, 0.8f } } };
        take.lane = i;
        studio.addPart (trackId, take);
    }
    studio.setShowLanes (trackId, true);
    studio.compRange (trackId, 0, 4.0, 8.0);   // cut at 4
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    auto& timeline = arrange.getTimeline();
    arrange.setTool (ArrangeView::Tool::comp);
    const auto laneY = [&] (int lane) { const auto r = arrange.partRow (trackId, lane); return r.getCentreY(); };
    const auto dragFrom = [&] (juce::Point<float> from, juce::Point<float> to, juce::ModifierKeys mods = {})
    {
        arrange.mouseDown (mouseAt (arrange, from, from, mods));
        for (int step = 1; step <= 4; ++step)
            arrange.mouseDrag (mouseAt (arrange, from + (to - from) * ((float) step / 4.0f), from, mods));
        arrange.mouseUp (mouseAt (arrange, to, from, mods));
        studio.dispatchPendingMessages();
    };

    // The split at 4, dragged to 6 on lane 1: every take's split there moves.
    REQUIRE (arrange.takeEdgeAt ({ timeline.xForBeat (4.0) + 1.0f, laneY (1) }).has_value());
    dragFrom ({ timeline.xForBeat (4.0) + 1.0f, laneY (1) }, { timeline.xForBeat (6.0), laneY (1) });
    for (const auto& p : studio.getProject().findTrack (trackId)->parts)
        CHECK ((std::abs (p.start) < 1.0e-9 || std::abs (p.start - 6.0) < 1.0e-9));

    // Alt-click: every take cut there (bar 1's beat 2).
    const auto before = studio.getProject().findTrack (trackId)->parts.size();
    const juce::Point<float> at2 (timeline.xForBeat (2.0), laneY (0));
    arrange.mouseDown (mouseAt (arrange, at2, at2, juce::ModifierKeys (juce::ModifierKeys::altModifier)));
    arrange.mouseUp (mouseAt (arrange, at2, at2, juce::ModifierKeys (juce::ModifierKeys::altModifier)));
    CHECK (studio.getProject().findTrack (trackId)->parts.size() == before + 2);

    // Alt+Shift drag: the take's notes slide a beat later.
    const auto& parts = studio.getProject().findTrack (trackId)->parts;
    int index = -1;
    for (int i = 0; i < (int) parts.size(); ++i)
        if (parts[(size_t) i].lane == 0 && parts[(size_t) i].start < 1.0)
            index = i;
    REQUIRE (index >= 0);
    const auto noteBefore = parts[(size_t) index].notes.at (0).start;
    const auto id = parts[(size_t) index].id;
    const juce::ModifierKeys slip (juce::ModifierKeys::altModifier | juce::ModifierKeys::shiftModifier);
    const juce::Point<float> grab (timeline.xForBeat (1.2), laneY (0));
    dragFrom (grab, grab + juce::Point<float> ((float) timeline.pixelsPerBeat, 0.0f), slip);
    CHECK_THAT (partWithId (studio, trackId, id).notes.at (0).start, WithinAbs (noteBefore + 1.0, 1.0e-6));

    // The comp handle (lanes shown) of the take not heard there (lane 2's): it comes to the front.
    for (int i = 0; i < (int) parts.size(); ++i)
        if (parts[(size_t) i].lane == 1 && parts[(size_t) i].start > 6.0 - 1.0e-9)
            index = i;
    const auto handle = arrange.compHandleBounds (trackId, index);
    REQUIRE_FALSE (handle.isEmpty());
    const auto front = parts[(size_t) index].id;
    arrange.setTool (ArrangeView::Tool::select);
    arrange.mouseDown (mouseAt (arrange, handle.getCentre(), handle.getCentre()));
    arrange.mouseUp (mouseAt (arrange, handle.getCentre(), handle.getCentre()));
    // (MIDI: heard = unmuted, the other lane's take there muted)
    CHECK_FALSE (partWithId (studio, trackId, front).muted);
    for (const auto& p : studio.getProject().findTrack (trackId)->parts)
        if (p.lane == 0 && p.start > 6.0 - 1.0e-9)
            CHECK (p.muted);
    // Clicked again: it goes back (muted: MIDI's toggle).
    const auto again = arrange.compHandleBounds (trackId, index).getCentre();
    arrange.mouseDown (mouseAt (arrange, again, again));
    arrange.mouseUp (mouseAt (arrange, again, again));
    CHECK (partWithId (studio, trackId, front).muted);

    for (const auto type : { Finish::Type::pearlBlack, Finish::Type::pearlWhiteNeon, Finish::Type::pearlWhite })
    {
        Finish::set (type, false);
        CHECK (view.createComponentSnapshot (view.getLocalBounds()).isValid());
    }
}

TEST_CASE ("0.82.6: Glue joins clips of different recordings into one new file, as they were heard", "[takes][glue]")
{
    Takes t;
    auto& studio = t.studio;
    writeLevel (studio, "a.wav", 0.25f, 4.0);
    writeLevel (studio, "b.wav", 0.5f, 4.0);
    auto first = clip (0.0, 2.0, "a.wav");
    auto second = clip (3.0, 2.0, "b.wav");   // a beat's gap between them
    second.volumeDb = -6.0f;
    studio.addPart (t.trackId, first);
    studio.addPart (t.trackId, second);
    REQUIRE (t.track().parts.size() == 2);
    REQUIRE (studio.glueParts (t.trackId, 0, false));
    REQUIRE (t.track().parts.size() == 1);
    const auto& glued = t.track().parts[0];
    CHECK (glued.audioFile.contains ("glued"));
    CHECK_THAT (glued.start, WithinAbs (0.0, 1.0e-9));
    CHECK_THAT (glued.length, WithinAbs (5.0, 1.0e-9));
    CHECK (glued.volumeDb == 0.0f);
    CHECK (glued.fadeIn == 0.0);
    const auto data = studio.getAudio (glued.audioFile);
    REQUIRE (data != nullptr);
    const auto at = [&] (double beatsIn) { return data->sound.getSample (0, (int) (beatsIn * (double) beat)); };
    CHECK_THAT (at (1.0), WithinAbs (0.25, 1.0e-4));
    CHECK_THAT (at (2.5), WithinAbs (0.0, 1.0e-6));                   // the gap: silence
    CHECK_THAT (at (4.0), WithinAbs (0.5 * std::pow (10.0, -6.0 / 20.0), 1.0e-3));   // its volume printed in
    // The recordings are left as they were; Undo brings the two clips back.
    CHECK (studio.getAudioFolder().getChildFile ("a.wav").existsAsFile());
    studio.undo();
    CHECK (t.track().parts.size() == 2);
}

TEST_CASE ("0.84.1: several takes selected move together when one is dragged", "[takes][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    for (int i = 0; i < 2; ++i)
    {
        Part take { 4.0, 8.0, "Take " + juce::String (i + 1), { { 1.0, 0.5, 60 + i * 4, 0.8f } } };
        take.lane = i;
        take.muted = i == 0;
        studio.addPart (trackId, take);
    }
    studio.setShowLanes (trackId, true);
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    arrange.setTool (ArrangeView::Tool::select);
    const auto first = arrange.getPartBounds (trackId, 0), second = arrange.getPartBounds (trackId, 1);

    SECTION ("chosen by a box")
    {
        const juce::Point<float> from (first.getX() - 20.0f, first.getY() + 2.0f), to (first.getCentreX(), second.getCentreY());
        arrange.mouseDown (mouseAt (arrange, from, from));
        for (int step = 1; step <= 4; ++step)
            arrange.mouseDrag (mouseAt (arrange, from + (to - from) * ((float) step / 4.0f), from));
        arrange.mouseUp (mouseAt (arrange, to, from));
    }
    SECTION ("chosen by Shift-click")
    {
        arrange.mouseDown (mouseAt (arrange, first.getCentre(), first.getCentre()));
        arrange.mouseUp (mouseAt (arrange, first.getCentre(), first.getCentre()));
        arrange.mouseDown (mouseAt (arrange, second.getCentre(), second.getCentre(), juce::ModifierKeys::shiftModifier));
        arrange.mouseUp (mouseAt (arrange, second.getCentre(), second.getCentre(), juce::ModifierKeys::shiftModifier));
    }
    SECTION ("chosen by Ctrl-click (the other DAW's way)")
    {
        arrange.mouseDown (mouseAt (arrange, first.getCentre(), first.getCentre()));
        arrange.mouseUp (mouseAt (arrange, first.getCentre(), first.getCentre()));
        arrange.mouseDown (mouseAt (arrange, second.getCentre(), second.getCentre(), juce::ModifierKeys::commandModifier));
        arrange.mouseUp (mouseAt (arrange, second.getCentre(), second.getCentre(), juce::ModifierKeys::commandModifier));
    }
    REQUIRE (arrange.getSelectedParts().size() == 2);
    for (const auto grabIndex : { 0, 1 })
    {
        const auto grab = arrange.getPartBounds (trackId, grabIndex).getCentre();
        const juce::Point<float> drop (grab.x + (float) (arrange.getTimeline().pixelsPerBeat * 4.0), grab.y);
        arrange.mouseDown (mouseAt (arrange, grab, grab));
        for (int step = 1; step <= 4; ++step)
            arrange.mouseDrag (mouseAt (arrange, grab + (drop - grab) * ((float) step / 4.0f), grab));
        // PARAGON_SNAPSHOT_TAKEDRAG=<file>: the takes on their lanes while they're dragged.
        if (const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_TAKEDRAG", {}); path.isNotEmpty() && grabIndex == 0)
        {
            juce::File (path).deleteFile();
            juce::FileOutputStream out { juce::File (path) };
            juce::PNGImageFormat().writeImageToStream (arrange.createComponentSnapshot (arrange.getLocalBounds()), out);
        }
        arrange.mouseUp (mouseAt (arrange, drop, grab));
        const auto& parts = studio.getProject().findTrack (trackId)->parts;
        REQUIRE (parts.size() == 2);
        for (const auto& p : parts)
            CHECK (p.start == (grabIndex == 0 ? 8.0 : 12.0));
        CHECK (arrange.getSelectedParts().size() == 2);
    }
}

TEST_CASE ("0.84.1: Ctrl-click on a selected part takes it out of the selection", "[takes][ui]")
{
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto trackId = studio.getProject().tracks[0].id;
    studio.addPart (trackId, { 0.0, 4.0, "A", {} });
    studio.addPart (trackId, { 8.0, 4.0, "B", {} });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    arrange.setTool (ArrangeView::Tool::select);
    const auto a = arrange.getPartBounds (trackId, 0).getCentre(), b = arrange.getPartBounds (trackId, 1).getCentre();
    const auto ctrl = juce::ModifierKeys::commandModifier;
    arrange.mouseDown (mouseAt (arrange, a, a));
    arrange.mouseUp (mouseAt (arrange, a, a));
    arrange.mouseDown (mouseAt (arrange, b, b, ctrl));
    arrange.mouseUp (mouseAt (arrange, b, b, ctrl));
    CHECK (arrange.getSelectedParts().size() == 2);
    arrange.mouseDown (mouseAt (arrange, a, a, ctrl));
    arrange.mouseUp (mouseAt (arrange, a, a, ctrl));
    REQUIRE (arrange.getSelectedParts().size() == 1);
    CHECK (arrange.getSelectedParts()[0] == studio.getProject().findTrack (trackId)->parts[1].id);
}

TEST_CASE ("0.84.1: the select box scrolls at the view's edges and redraws only what it changed", "[ui]")
{
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto first = studio.getProject().tracks[0].id;
    for (int i = 0; i < 30; ++i)
        studio.addTrack (Instrument {});
    for (const auto& track : studio.getProject().tracks)
        studio.addPart (track.id, { 0.0, 16.0, "Part", { { 0.0, 1.0, 60, 0.8f } } });
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    arrange.setTool (ArrangeView::Tool::select);
    const auto w = (float) arrange.getWidth(), h = (float) arrange.getHeight();
    const juce::Point<float> from (arrange.getTimeline().xForBeat (17.0), (float) Timeline::rulerHeight + 10.0f);

    // A big box grown by a few pixels redraws a few strips, not the whole view.
    arrange.mouseDown (mouseAt (arrange, from, from));
    const juce::Point<float> mid (w * 0.6f, h * 0.6f);
    arrange.mouseDrag (mouseAt (arrange, mid, from));
    arrange.mouseDrag (mouseAt (arrange, mid + juce::Point<float> (6.0f, 5.0f), from));
    CHECK (arrange.getBandRepaintArea() > 0.0);
    CHECK (arrange.getBandRepaintArea() < (double) w * h * 0.1);

    // Held inside the right and bottom edges (where a window at the screen's edge stops the mouse), it scrolls.
    const auto beatBefore = arrange.getTimeline().firstBeat;
    const auto yBefore = arrange.getTimeline().scrollY;
    const juce::Point<float> corner (w - 4.0f, h - 4.0f);
    for (int i = 0; i < 10; ++i)
        arrange.mouseDrag (mouseAt (arrange, corner, from));
    CHECK (arrange.getTimeline().firstBeat > beatBefore);
    CHECK (arrange.getTimeline().scrollY > yBefore);
    arrange.mouseUp (mouseAt (arrange, corner, from));
    // What it swept over is selected, the first track's part among them.
    CHECK (arrange.isPartSelected (studio.getProject().findTrack (first)->parts[0].id) == false);   // (the box began after it)
    CHECK (arrange.getSelectedParts().empty());   // (every part ends at beat 16; the box starts at 17)
}

TEST_CASE ("0.84.1: three clips on three tracks selected move together (the owner's bass and guitar renders)", "[ui]")
{
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    std::vector<int> tracks;
    for (int i = 0; i < 3; ++i)
    {
        tracks.push_back (studio.addAudioTrack (true));
        studio.addPart (tracks.back(), clip (4.0, 8.0, "render " + juce::String (i) + ".wav"));
    }
    studio.dispatchPendingMessages();
    auto& arrange = view.getArrangeView();
    arrange.setTool (ArrangeView::Tool::select);
    const auto at = [&] (int i) { return arrange.getPartBounds (tracks[(size_t) i], 0).getCentre(); };

    SECTION ("chosen by a box")
    {
        const juce::Point<float> from (arrange.getTimeline().xForBeat (2.0), at (0).y - 5.0f), to (at (2).x, at (2).y);
        arrange.mouseDown (mouseAt (arrange, from, from));
        for (int step = 1; step <= 4; ++step)
            arrange.mouseDrag (mouseAt (arrange, from + (to - from) * ((float) step / 4.0f), from));
        arrange.mouseUp (mouseAt (arrange, to, from));
    }
    SECTION ("chosen by Shift-click")
        for (int i = 0; i < 3; ++i)
        {
            const auto mods = i == 0 ? juce::ModifierKeys() : juce::ModifierKeys (juce::ModifierKeys::shiftModifier);
            arrange.mouseDown (mouseAt (arrange, at (i), at (i), mods));
            arrange.mouseUp (mouseAt (arrange, at (i), at (i), mods));
        }
    SECTION ("chosen by Ctrl-click")
        for (int i = 0; i < 3; ++i)
        {
            const auto mods = i == 0 ? juce::ModifierKeys() : juce::ModifierKeys (juce::ModifierKeys::commandModifier);
            arrange.mouseDown (mouseAt (arrange, at (i), at (i), mods));
            arrange.mouseUp (mouseAt (arrange, at (i), at (i), mods));
        }
    REQUIRE (arrange.getSelectedParts().size() == 3);
    const auto grab = at (0);
    const juce::Point<float> drop (grab.x + (float) (arrange.getTimeline().pixelsPerBeat * 4.0), grab.y + 3.0f);
    arrange.mouseDown (mouseAt (arrange, grab, grab));
    for (int step = 1; step <= 4; ++step)
        arrange.mouseDrag (mouseAt (arrange, grab + (drop - grab) * ((float) step / 4.0f), grab));
    arrange.mouseUp (mouseAt (arrange, drop, grab));
    for (const auto id : tracks)
    {
        REQUIRE (studio.getProject().findTrack (id)->parts.size() == 1);
        CHECK (studio.getProject().findTrack (id)->parts[0].start == 8.0);
    }
}
