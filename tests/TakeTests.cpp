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
