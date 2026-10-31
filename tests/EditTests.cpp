#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"

using Catch::Matchers::WithinAbs;
using namespace pr7s;

namespace
{
    constexpr double rate = 48000.0;

    const Part* partAt (const Studio& studio, int trackId, double start)
    {
        for (const auto& part : studio.getProject().findTrack (trackId)->parts)
            if (std::abs (part.start - start) < 1.0e-9)
                return &part;
        return nullptr;
    }

    int indexAt (const Studio& studio, int trackId, double start)
    {
        const auto& parts = studio.getProject().findTrack (trackId)->parts;
        for (int i = 0; i < (int) parts.size(); ++i)
            if (std::abs (parts[(size_t) i].start - start) < 1.0e-9)
                return i;
        return -1;
    }
}

TEST_CASE ("Split: a part cut in two at a beat; a note across the cut becomes two; one undo step", "[edit]")
{
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    studio.addPart (track, { 4.0, 8.0, "Riff", { { 0.0, 1.0, 60, 0.8f }, { 2.5, 3.0, 64, 0.5f }, { 6.0, 1.0, 67, 0.8f } } });
    CHECK (studio.splitPart (track, 0, 8.0) == 1);   // 4 beats in: through the 64 (2.5 to 5.5)

    const auto* first = partAt (studio, track, 4.0);
    const auto* second = partAt (studio, track, 8.0);
    REQUIRE (first != nullptr);
    REQUIRE (second != nullptr);
    CHECK (first->length == 4.0);
    CHECK (second->length == 4.0);
    CHECK (second->name == "Riff");
    REQUIRE (first->notes.size() == 2);
    CHECK (first->notes[1].pitch == 64);
    CHECK (first->notes[1].length == 1.5);      // up to the cut
    REQUIRE (second->notes.size() == 2);
    CHECK (second->notes[0].pitch == 64);       // the rest of it, from the second part's start
    CHECK (second->notes[0].start == 0.0);
    CHECK (second->notes[0].length == 1.5);
    CHECK (second->notes[0].velocity == 0.5f);
    CHECK (second->notes[1].pitch == 67);
    CHECK (second->notes[1].start == 2.0);
    CHECK (second->id != first->id);

    // At its edges (or outside it), nothing.
    CHECK (studio.splitPart (track, 0, 4.0) == -1);
    CHECK (studio.splitPart (track, 0, 20.0) == -1);

    studio.undo();
    REQUIRE (studio.getProject().findTrack (track)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (track)->parts[0].length == 8.0);
    CHECK (studio.getProject().findTrack (track)->parts[0].notes.size() == 3);
}

TEST_CASE ("Split repeatedly (Alt): equal pieces as long as the first; Split at Cursor: every part the playhead is in", "[edit]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    studio.addPart (a, { 0.0, 7.0, "Long", {} });
    studio.splitPartRepeatedly (a, 0, 2.0);
    const auto& parts = studio.getProject().findTrack (a)->parts;
    REQUIRE (parts.size() == 4);   // 2, 2, 2 and what's left: 1
    CHECK (partAt (studio, a, 2.0)->length == 2.0);
    CHECK (partAt (studio, a, 4.0)->length == 2.0);
    CHECK (partAt (studio, a, 6.0)->length == 1.0);
    studio.undo();
    CHECK (studio.getProject().findTrack (a)->parts.size() == 1);   // one step

    studio.addPart (b, { 2.0, 4.0, "Other", {} });
    studio.addPart (b, { 10.0, 2.0, "Later", {} });
    studio.setPosition (3.0);
    studio.splitAtCursor();
    CHECK (studio.getProject().findTrack (a)->parts.size() == 2);   // 0-7 at 3
    CHECK (studio.getProject().findTrack (b)->parts.size() == 3);   // 2-6 at 3; 10-12 untouched
    CHECK (partAt (studio, b, 3.0)->length == 3.0);
    studio.undo();

    // With a part selected, only that one.
    studio.splitAtCursor (studio.getProject().findTrack (b)->parts[0].id);
    CHECK (studio.getProject().findTrack (a)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (b)->parts.size() == 3);
}

TEST_CASE ("Glue: a part to the next (or, with Alt, all after it): one part, their notes in place, the gaps silent", "[edit]")
{
    Studio studio (false);
    const auto track = studio.getProject().tracks[0].id;
    // Out of order in the list: glue goes by where they start.
    studio.addPart (track, { 12.0, 2.0, "C", { { 0.0, 1.0, 72, 0.8f } } });
    studio.addPart (track, { 0.0, 4.0, "A", { { 0.0, 1.0, 60, 0.8f }, { 3.5, 2.0, 62, 0.8f }, { 5.0, 1.0, 50, 0.8f } } });
    studio.addPart (track, { 6.0, 2.0, "B", { { 1.0, 1.0, 64, 0.8f } } });

    REQUIRE (studio.glueParts (track, indexAt (studio, track, 0.0), false));
    REQUIRE (studio.getProject().findTrack (track)->parts.size() == 2);
    const auto* glued = partAt (studio, track, 0.0);
    REQUIRE (glued != nullptr);
    CHECK (glued->name == "A");
    CHECK (glued->length == 8.0);   // to B's end
    REQUIRE (glued->notes.size() == 3);   // the 50 past A's end wasn't heard: gone
    CHECK (glued->notes[1].pitch == 62);
    CHECK (glued->notes[1].length == 0.5);   // as it was heard (A ended at 4)
    CHECK (glued->notes[2].pitch == 64);
    CHECK (glued->notes[2].start == 7.0);
    studio.undo();
    CHECK (studio.getProject().findTrack (track)->parts.size() == 3);

    REQUIRE (studio.glueParts (track, indexAt (studio, track, 0.0), true));
    REQUIRE (studio.getProject().findTrack (track)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (track)->parts[0].length == 14.0);
    CHECK (studio.getProject().findTrack (track)->parts[0].notes.back().start == 12.0);

    // The last part has nothing after it.
    CHECK_FALSE (studio.glueParts (track, 0, false));
}

TEST_CASE ("Audio clips: split, they play exactly as before; glued back, one clip; different recordings don't glue", "[edit][audio]")
{
    Studio studio (false);
    const auto track = studio.addAudioTrack (false);
    Part clip { 1.0, 4.0, "Take", {} };
    clip.audioFile = "take.wav";
    clip.audioOffset = 0.5;
    clip.fadeIn = 0.1;
    clip.fadeOut = 0.2;
    studio.addPart (track, clip);
    const auto before = studio.getProject().findTrack (track)->parts[0];
    REQUIRE (studio.splitPart (track, 0, 2.5) >= 0);
    const auto* first = partAt (studio, track, 1.0);
    const auto* second = partAt (studio, track, 2.5);
    REQUIRE (first != nullptr);
    REQUIRE (second != nullptr);
    CHECK (first->length == 1.5);
    CHECK_THAT (second->audioOffset, WithinAbs (0.5 + 1.5 * 0.5, 1.0e-12));   // 1.5 beats at 120 BPM: 0.75 s further in
    CHECK (second->audioFile == "take.wav");
    CHECK (first->fadeIn == 0.1);
    CHECK (first->fadeOut == 0.0);
    CHECK (second->fadeIn == 0.0);
    CHECK (second->fadeOut == 0.2);

    // Played: every sample as the whole clip played it (a ramp tells them apart).
    const auto render = [&] (std::vector<Part> parts)
    {
        Engine engine;
        engine.prepare (rate, 512);
        auto sound = std::make_shared<juce::AudioBuffer<float>> (2, (int) rate * 4);
        for (int i = 0; i < sound->getNumSamples(); ++i)
            for (int c = 0; c < 2; ++c)
                sound->setSample (c, i, (float) i * 1.0e-5f);
        engine.setAudioSource ([sound] (const juce::String&) -> std::shared_ptr<const juce::AudioBuffer<float>> { return sound; });
        Project project;
        Track t;
        t.id = 1;
        t.kind = Track::Kind::audio;
        t.parts = std::move (parts);
        project.tracks.push_back (t);
        engine.addAudioTrack (1);
        engine.update (project);
        engine.play();
        return renderSamples (engine, (int) rate * 3);
    };
    const auto whole = render ({ before });
    const auto halves = render (studio.getProject().findTrack (track)->parts);
    // (Apart from the fades, which now belong to each half: compare between them.)
    auto same = true;
    for (size_t i = (size_t) (rate * 0.5 + rate * 0.1) + 1; i < (size_t) (rate * 2.5 - rate * 0.2) - 1; ++i)
        same = same && std::abs (whole[i] - halves[i]) < 1.0e-6f;
    CHECK (same);

    // Glued back: the clip as it was.
    REQUIRE (studio.glueParts (track, indexAt (studio, track, 1.0), false));
    REQUIRE (studio.getProject().findTrack (track)->parts.size() == 1);
    const auto& back = studio.getProject().findTrack (track)->parts[0];
    CHECK (back.length == 4.0);
    CHECK_THAT (back.audioOffset, WithinAbs (0.5, 1.0e-12));
    CHECK (back.fadeIn == 0.1);
    CHECK (back.fadeOut == 0.2);

    // A clip from another recording after it: not glued (no audio parts of several clips yet).
    Part other { 5.0, 2.0, "Other", {} };
    other.audioFile = "other.wav";
    studio.addPart (track, other);
    CHECK_FALSE (studio.glueParts (track, indexAt (studio, track, 1.0), false));
    CHECK (studio.getProject().findTrack (track)->parts.size() == 2);
}

TEST_CASE ("Mute: a muted part isn't played (MIDI or audio), stays in the song, is saved; Erase with Alt: the part and all after it", "[edit]")
{
    Engine engine;
    engine.prepare (rate, 512);
    Project project;
    Track track;
    track.id = 1;
    track.parts = { { 0.0, 1.0, "Heard", { { 0.0, 0.5, 60, 0.8f } } }, { 1.0, 1.0, "Muted", { { 0.0, 0.5, 64, 0.8f } } } };
    track.parts[1].muted = true;
    project.tracks.push_back (track);
    auto probe = std::make_unique<Probe>();
    auto* heard = probe.get();
    engine.setInstrument (1, std::move (probe));
    engine.update (project);
    engine.play();
    renderSamples (engine, (int) rate * 2);
    REQUIRE (heard->notesOn().size() == 1);
    CHECK (heard->notesOn()[0].message.getNoteNumber() == 60);

    // Saved and opened again.
    const auto again = Project::fromXml (*project.toXml());
    REQUIRE (again.has_value());
    CHECK_FALSE (again->tracks[0].parts[0].muted);
    CHECK (again->tracks[0].parts[1].muted);

    // In the studio: one undo step each; several at once.
    Studio studio (false);
    const auto id = studio.getProject().tracks[0].id;
    studio.addPart (id, { 0.0, 2.0, "A", {} });
    studio.addPart (id, { 4.0, 2.0, "B", {} });
    studio.addPart (id, { 8.0, 2.0, "C", {} });
    studio.setPartMuted (id, 1, true);
    CHECK (studio.getProject().findTrack (id)->parts[1].muted);
    studio.undo();
    CHECK_FALSE (studio.getProject().findTrack (id)->parts[1].muted);
    const auto& parts = studio.getProject().findTrack (id)->parts;
    studio.setPartsMuted ({ parts[0].id, parts[2].id }, true);
    CHECK (studio.getProject().findTrack (id)->parts[0].muted);
    CHECK_FALSE (studio.getProject().findTrack (id)->parts[1].muted);
    CHECK (studio.getProject().findTrack (id)->parts[2].muted);

    studio.deletePartsFrom (id, indexAt (studio, id, 4.0));
    REQUIRE (studio.getProject().findTrack (id)->parts.size() == 1);
    CHECK (studio.getProject().findTrack (id)->parts[0].name == "A");
    studio.undo();
    CHECK (studio.getProject().findTrack (id)->parts.size() == 3);
}

TEST_CASE ("Range editing: split at its edges, crop, delete (and Delete Time), cut and paste, insert silence, move, copy, duplicate", "[edit][range]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    const auto reset = [&]
    {
        // a: one 8-beat part (a note across beat 4 and one at 6); b: 0-4 and 6-10.
        for (const auto id : { a, b })
            while (! studio.getProject().findTrack (id)->parts.empty())
                studio.deletePart (id, 0);
        studio.addPart (a, { 0.0, 8.0, "A", { { 3.0, 2.0, 60, 0.8f }, { 6.0, 1.0, 62, 0.8f } } });
        studio.addPart (b, { 0.0, 4.0, "B1", {} });
        studio.addPart (b, { 6.0, 4.0, "B2", {} });
    };
    const auto starts = [&] (int id)
    {
        std::vector<std::pair<double, double>> out;
        for (const auto& part : studio.getProject().findTrack (id)->parts)
            out.push_back ({ part.start, part.length });
        std::sort (out.begin(), out.end());
        return out;
    };
    using Spans = std::vector<std::pair<double, double>>;
    const Studio::Range range { 2.0, 7.0, { a, b } };

    SECTION ("Split at the range's edges (Shift+X), on its tracks only")
    {
        reset();
        studio.splitRange (range);
        CHECK (starts (a) == Spans { { 0.0, 2.0 }, { 2.0, 5.0 }, { 7.0, 1.0 } });
        CHECK (starts (b) == Spans { { 0.0, 2.0 }, { 2.0, 2.0 }, { 6.0, 1.0 }, { 7.0, 3.0 } });
        studio.undo();
        CHECK (starts (a) == Spans { { 0.0, 8.0 } });   // one step
        studio.splitRange ({ 2.0, 7.0, { b } });
        CHECK (starts (a) == Spans { { 0.0, 8.0 } });
    }

    SECTION ("Crop: parts across the edges cut down to the range; parts wholly inside or out untouched")
    {
        reset();
        studio.addPart (a, { 20.0, 2.0, "Far", {} });
        studio.cropRange (range);
        CHECK (starts (a) == Spans { { 2.0, 5.0 }, { 20.0, 2.0 } });
        CHECK (starts (b) == Spans { { 2.0, 2.0 }, { 6.0, 1.0 } });
        // The note across beat 4 is now 1 beat into the cropped part, still 2 long.
        const auto& cropped = studio.getProject().findTrack (a)->parts[0];
        REQUIRE (cropped.notes.size() == 2);
        CHECK (cropped.notes[0].start == 1.0);
        CHECK (cropped.notes[0].length == 2.0);
    }

    SECTION ("Delete leaves the gap; Delete Time closes it (what's after moves back)")
    {
        reset();
        studio.deleteRange (range, false);
        CHECK (starts (a) == Spans { { 0.0, 2.0 }, { 7.0, 1.0 } });
        CHECK (starts (b) == Spans { { 0.0, 2.0 }, { 7.0, 3.0 } });
        studio.undo();
        studio.deleteRange (range, true);
        CHECK (starts (a) == Spans { { 0.0, 2.0 }, { 2.0, 1.0 } });
        CHECK (starts (b) == Spans { { 0.0, 2.0 }, { 2.0, 3.0 } });
    }

    SECTION ("Cut, then Paste at the playhead's beat onto the selected track down; Paste Time makes room; Insert Silence")
    {
        reset();
        const auto clip = studio.cutRange (range, false);
        CHECK (clip.length == 5.0);
        REQUIRE (clip.tracks.size() == 2);
        REQUIRE (clip.tracks[0].size() == 1);
        CHECK (clip.tracks[0][0].start == 0.0);
        CHECK (clip.tracks[0][0].length == 5.0);
        CHECK (clip.tracks[0][0].notes[0].start == 1.0);   // the note at beat 3: a beat into the range
        CHECK (starts (a) == Spans { { 0.0, 2.0 }, { 7.0, 1.0 } });
        studio.pasteRange (clip, 20.0, a, false);
        CHECK (starts (a) == Spans { { 0.0, 2.0 }, { 7.0, 1.0 }, { 20.0, 5.0 } });
        CHECK (starts (b) == Spans { { 0.0, 2.0 }, { 7.0, 3.0 }, { 20.0, 2.0 }, { 24.0, 1.0 } });
        studio.undo();

        studio.pasteRange (clip, 0.0, a, true);   // Paste Time: everything from 0 moves on by 5 first
        CHECK (starts (a) == Spans { { 0.0, 5.0 }, { 5.0, 2.0 }, { 12.0, 1.0 } });

        reset();
        studio.insertSilence ({ 4.0, 6.0, { a } });
        CHECK (starts (a) == Spans { { 0.0, 4.0 }, { 6.0, 4.0 } });   // split at 4, the rest 2 later
        CHECK (starts (b) == Spans { { 0.0, 4.0 }, { 6.0, 4.0 } });   // not in the range: as it was
    }

    SECTION ("Move (drag) and copy (Alt-drag) the contents; Duplicate right after; onto tracks of the wrong kind, not")
    {
        reset();
        studio.moveRange ({ 2.0, 4.0, { a } }, 10.0, a, false);
        CHECK (starts (a) == Spans { { 0.0, 2.0 }, { 4.0, 4.0 }, { 10.0, 2.0 } });
        studio.undo();
        CHECK (starts (a) == Spans { { 0.0, 8.0 } });   // one step
        studio.moveRange ({ 2.0, 4.0, { a } }, 10.0, b, true);   // copied onto the track below
        CHECK (starts (a) == Spans { { 0.0, 8.0 } });
        CHECK (starts (b) == Spans { { 0.0, 4.0 }, { 6.0, 4.0 }, { 10.0, 2.0 } });
        studio.undo();
        studio.duplicateRange ({ 6.0, 10.0, { b } });
        CHECK (starts (b) == Spans { { 0.0, 4.0 }, { 6.0, 4.0 }, { 10.0, 4.0 } });

        const auto audio = studio.addAudioTrack (false);
        studio.moveRange ({ 0.0, 2.0, { a } }, 0.0, audio, true);   // MIDI onto an audio track: left out
        CHECK (studio.getProject().findTrack (audio)->parts.empty());
    }
}

TEST_CASE ("Tracks: several selected (Ctrl, Shift); moved in the list (undo puts the order back); duplicated with everything; removed; empty ones found", "[edit][tracks]")
{
    Studio studio (false);
    const auto a = studio.getProject().tracks[0].id;
    const auto b = studio.addTrack ({});
    const auto c = studio.addTrack ({});
    const auto d = studio.addAudioTrack (true);
    const auto order = [&]
    {
        std::vector<int> ids;
        for (const auto& track : studio.getProject().tracks)
            ids.push_back (track.id);
        return ids;
    };
    using Ids = std::vector<int>;

    SECTION ("Selecting: one; Ctrl adds and removes; Shift a run from the one clicked last, or drops a selected one")
    {
        studio.selectTrack (a);
        CHECK (studio.getSelectedTracks() == Ids { a });
        studio.toggleTrackSelected (c);
        CHECK (studio.getSelectedTracks() == Ids { a, c });
        CHECK (studio.getSelectedTrack() == c);
        studio.toggleTrackSelected (a);
        CHECK (studio.getSelectedTracks() == Ids { c });
        studio.selectTrack (a);
        studio.selectTrackRun (c);
        CHECK (studio.getSelectedTracks() == Ids { a, b, c });
        studio.selectTrackRun (b);   // a selected one: dropped
        CHECK (studio.getSelectedTracks() == Ids { a, c });
        studio.selectTrack (d);
        CHECK (studio.getSelectedTracks() == Ids { d });
    }

    SECTION ("Moving tracks: before a place in the list, several keeping their order; undo puts the order back")
    {
        studio.addPart (b, { 0.0, 4.0, "B", {} });
        studio.moveTracks ({ c }, 0);
        CHECK (order() == Ids { c, a, b, d });
        studio.moveTracks ({ b, c }, 4);   // to the end, in the order they're in the list (c, then b)
        CHECK (order() == Ids { a, d, c, b });
        studio.undo();
        CHECK (order() == Ids { c, a, b, d });
        studio.undo();
        CHECK (order() == Ids { a, b, c, d });
        CHECK (studio.getProject().findTrack (b)->parts.size() == 1);   // (the parts as they were)
        studio.moveTracks ({ a }, 3);   // before d
        CHECK (order() == Ids { b, c, a, d });
    }

    SECTION ("Duplicating: straight under it, with its parts, effects, sends, automation and mix; selected")
    {
        studio.addPart (b, { 2.0, 4.0, "Riff", { { 0.0, 1.0, 60, 0.8f } } });
        studio.setVolume (b, -6.0f);
        studio.setPan (b, 0.25f);
        studio.setAutomationOpen (b, true);   // a volume lane
        studio.setAutomationPoints (b, 0, { { 0.0, 0.5f }, { 4.0, 0.8f } });
        REQUIRE (studio.getProject().findTrack (b)->automation.size() == 1);
        const auto copy = studio.duplicateTrack (b);
        REQUIRE (copy != 0);
        CHECK (order() == Ids { a, b, copy, c, d });
        const auto* t = studio.getProject().findTrack (copy);
        REQUIRE (t != nullptr);
        CHECK (t->name == studio.getProject().findTrack (b)->name + " copy");
        REQUIRE (t->parts.size() == 1);
        CHECK (t->parts[0].name == "Riff");
        CHECK (t->parts[0].id != studio.getProject().findTrack (b)->parts[0].id);
        CHECK (t->volumeDb == -6.0f);
        CHECK (t->pan == 0.25f);
        REQUIRE (t->automation.size() == 1);
        CHECK (t->automation[0].points.size() == 2);
        CHECK (studio.getEngine().getInstrument (copy) != nullptr);   // its own instrument
        CHECK (studio.getEngine().getInstrument (copy) != studio.getEngine().getInstrument (b));
        CHECK (studio.getSelectedTracks() == Ids { copy });
        // An audio track: an audio track again, its clips the same recordings.
        const auto audioCopy = studio.duplicateTrack (d);
        CHECK (studio.getProject().findTrack (audioCopy)->isAudio());
        CHECK (studio.getProject().findTrack (audioCopy)->stereoInput);
        // Several: each under its own, all selected.
        studio.duplicateTracks ({ a, c });
        CHECK (studio.getSelectedTracks().size() == 2);
        CHECK (studio.getProject().tracks.size() == 8);
    }

    SECTION ("Removing several; the empty ones (no parts, no automation points)")
    {
        studio.addPart (b, { 0.0, 4.0, "B", {} });
        CHECK (studio.getEmptyTracks() == Ids { a, c, d });
        studio.removeTracks (studio.getEmptyTracks());
        CHECK (order() == Ids { b });
        CHECK (studio.getSelectedTrack() == b);
    }
}

TEST_CASE ("Render in Place: a part to an audio clip on a new track under its own; through the effects or not (Dry); modes; the source kept, muted or removed", "[edit][render]")
{
    Studio studio (false);
    const auto synth = studio.getProject().tracks[0].id;   // the Paragon Synth: real sound
    studio.addPart (synth, { 4.0, 4.0, "Riff", { { 0.0, 1.0, 60, 1.0f }, { 2.0, 1.0, 64, 1.0f } } });
    studio.addPart (synth, { 8.0, 4.0, "Riff 2", { { 0.0, 1.0, 67, 1.0f } } });
    studio.addPart (synth, { 16.0, 2.0, "Later", { { 0.0, 1.0, 72, 1.0f } } });
    const auto peakOf = [&] (const Part& clip)
    {
        const auto data = studio.getAudio (clip.audioFile);
        REQUIRE (data != nullptr);
        return data->sound.getMagnitude (0, data->sound.getNumSamples());
    };
    const auto spb = studio.getProject().secondsPerBeat();
    Studio::RenderOptions options;
    options.tail = Studio::RenderOptions::Tail::off;

    SECTION ("As separate events: a clip for each part, where it was; a new audio track straight under the source")
    {
        juce::String error;
        const auto made = studio.renderInPlace ({ { synth, 0.0, 32.0, true } }, options, error);
        CHECK (error.isEmpty());
        REQUIRE (made.size() == 1);
        const auto& tracks = studio.getProject().tracks;
        REQUIRE (tracks.size() == 2);
        CHECK (tracks[1].id == made[0]);
        CHECK (tracks[1].isAudio());
        CHECK (tracks[1].name.endsWith ("render"));
        const auto& clips = tracks[1].parts;
        REQUIRE (clips.size() == 3);
        CHECK (clips[0].start == 4.0);
        CHECK_THAT (clips[0].length, WithinAbs (4.0, 0.01));   // no tail: as long as the part
        CHECK (clips[1].start == 8.0);
        CHECK (clips[2].start == 16.0);
        CHECK (peakOf (clips[0]) > 0.01f);   // the synth's sound, not silence
        CHECK (studio.getAudioFolder().getChildFile (clips[0].audioFile).existsAsFile());
        CHECK_FALSE (studio.getProject().findTrack (synth)->mute);   // kept as it was
    }

    SECTION ("As block events: parts end to end are one clip; as one event: one clip over them all")
    {
        options.mode = Studio::RenderOptions::Mode::blocks;
        juce::String error;
        auto made = studio.renderInPlace ({ { synth, 0.0, 32.0, true } }, options, error);
        REQUIRE (made.size() == 1);
        const auto* blocks = studio.getProject().findTrack (made[0]);
        REQUIRE (blocks->parts.size() == 2);   // 4 to 12 (two parts end to end), then 16 to 18
        CHECK (blocks->parts[0].start == 4.0);
        CHECK_THAT (blocks->parts[0].length, WithinAbs (8.0, 0.01));
        options.mode = Studio::RenderOptions::Mode::one;
        made = studio.renderInPlace ({ { synth, 0.0, 32.0, true } }, options, error);
        const auto* one = studio.getProject().findTrack (made[0]);
        REQUIRE (one->parts.size() == 1);
        CHECK (one->parts[0].start == 4.0);
        CHECK_THAT (one->parts[0].length, WithinAbs (14.0, 0.01));
    }

    SECTION ("A range: only what's in it (the parts cut at its edges); its source parts muted, the rest not")
    {
        options.source = Studio::RenderOptions::Source::mute;
        juce::String error;
        const auto made = studio.renderInPlace ({ { synth, 6.0, 10.0, false } }, options, error);
        REQUIRE (made.size() == 1);
        const auto& clips = studio.getProject().findTrack (made[0])->parts;
        REQUIRE (clips.size() == 2);   // 6 to 8 of Riff, 8 to 10 of Riff 2
        CHECK (clips[0].start == 6.0);
        CHECK_THAT (clips[0].length, WithinAbs (2.0, 0.01));
        // The source: cut at 6 and 10, what's between muted.
        for (const auto& part : studio.getProject().findTrack (synth)->parts)
            CHECK (part.muted == (part.start >= 6.0 - 1.0e-9 && part.end() <= 10.0 + 1.0e-9));
    }

    SECTION ("Channel Settings goes through the inserts; Dry doesn't, and the new track gets them; the fader goes to the new track")
    {
        Insert half;
        half.plugin.name = "Half";
        REQUIRE (studio.addInsert (synth, half, timesEffect (0.5f, "Half")));
        studio.setVolume (synth, -6.0f);
        juce::String error;
        options.processing = Studio::RenderOptions::Processing::channelSettings;
        const auto through = studio.renderInPlace ({ { synth, 4.0, 8.0, false } }, options, error);
        options.processing = Studio::RenderOptions::Processing::dry;
        const auto dry = studio.renderInPlace ({ { synth, 4.0, 8.0, false } }, options, error);
        REQUIRE (through.size() == 1);
        REQUIRE (dry.size() == 1);
        const auto wet = peakOf (studio.getProject().findTrack (through[0])->parts.at (0));
        const auto plain = peakOf (studio.getProject().findTrack (dry[0])->parts.at (0));
        CHECK_THAT (wet / plain, WithinAbs (0.5, 0.02));   // the insert's half, and not the fader's -6 dB on top
        CHECK (studio.getProject().findTrack (through[0])->volumeDb == -6.0f);   // the fader: on the new track
        CHECK (studio.getProject().findTrack (through[0])->inserts.empty());
        CHECK (studio.getProject().findTrack (dry[0])->inserts.size() == 1);    // Dry: the effects go with it
        // Complete Signal Path: the fader rendered in too, the new track at 0 dB.
        options.processing = Studio::RenderOptions::Processing::completeSignalPath;
        const auto complete = studio.renderInPlace ({ { synth, 4.0, 8.0, false } }, options, error);
        const auto all = peakOf (studio.getProject().findTrack (complete[0])->parts.at (0));
        CHECK_THAT (all / wet, WithinAbs (std::pow (10.0, -6.0 / 20.0), 0.02));
        CHECK (studio.getProject().findTrack (complete[0])->volumeDb == 0.0f);
    }

    SECTION ("A tail (in bars): the clip runs on after the part; a whole track removed after rendering")
    {
        options.tail = Studio::RenderOptions::Tail::bars;
        options.tailSize = 1.0;
        options.source = Studio::RenderOptions::Source::remove;
        juce::String error;
        const auto made = studio.renderInPlace ({ { synth, 16.0, 18.0, true } }, options, error);
        REQUIRE (made.size() == 1);
        const auto& clip = studio.getProject().findTrack (made[0])->parts.at (0);
        CHECK (clip.length >= 2.0);                 // the part, and what rang on (up to a bar: it stops early once silent)
        CHECK (clip.length <= 2.0 + 4.0 + 0.01);
        CHECK (studio.getProject().findTrack (synth) == nullptr);   // removed
        juce::ignoreUnused (spb);
    }

    studio.getAudioFolder().deleteRecursively();
}
