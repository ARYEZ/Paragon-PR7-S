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
