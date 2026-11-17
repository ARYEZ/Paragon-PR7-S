// Speed, round five (0.164.0): each change checked to give the same result as before.

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "engine/Pr7dLink.h"
#include "engine/Studio.h"

using namespace pr7s;

TEST_CASE ("0.164.0: a part of thousands of notes, controllers and warp markers is written and read back the same", "[speed164]")
{
    Project song;
    Track track;
    track.id = 1;
    Part part;
    part.start = 4.0;
    part.length = 64.0;
    for (int i = 0; i < 3000; ++i)
    {
        Note n;
        n.start = i * 0.02;
        n.length = 0.1;
        n.pitch = 36 + i % 48;
        n.velocity = (float) ((i % 100) / 100.0);
        part.notes.push_back (n);
        Controller c;
        c.number = 1;
        c.start = i * 0.02;
        c.value = i % 128;
        part.controllers.push_back (c);
    }
    AutomationLane lane;
    for (int i = 0; i < 2000; ++i)
        lane.points.push_back ({ i * 0.03, (float) (i % 7) / 7.0f });
    track.automation.push_back (lane);
    track.parts.push_back (part);
    song.tracks.push_back (track);
    const auto xml = song.toXml();
    REQUIRE (xml != nullptr);
    const auto back = Project::fromXml (*xml);
    REQUIRE (back.has_value());
    REQUIRE (back->tracks.size() == 1);
    const auto& read = back->tracks[0].parts.at (0);
    // In order (the children went in back to front, so they read as written), each as written (to the file's rounding).
    REQUIRE (read.notes.size() == part.notes.size());
    REQUIRE (read.controllers.size() == part.controllers.size());
    const auto& points = back->tracks[0].automation.at (0).points;
    REQUIRE (points.size() == lane.points.size());
    auto notesSame = true, controllersSame = true, pointsSame = true;
    for (size_t i = 0; i < part.notes.size(); ++i)
        notesSame = notesSame && std::abs (read.notes[i].start - part.notes[i].start) < 1.0e-6 && read.notes[i].pitch == part.notes[i].pitch
                    && std::abs (read.notes[i].velocity - part.notes[i].velocity) < 1.0e-5f;
    for (size_t i = 0; i < part.controllers.size(); ++i)
        controllersSame = controllersSame && std::abs (read.controllers[i].start - part.controllers[i].start) < 1.0e-6
                          && read.controllers[i].value == part.controllers[i].value;
    for (size_t i = 0; i < lane.points.size(); ++i)
        pointsSame = pointsSame && std::abs (points[i].beat - lane.points[i].beat) < 1.0e-6 && std::abs (points[i].value - lane.points[i].value) < 1.0e-5f;
    CHECK (notesSame);
    CHECK (controllersSame);
    CHECK (pointsSame);
    // Written twice: the same text.
    CHECK (xml->toString() == song.toXml()->toString());
}

TEST_CASE ("0.164.0: a track clicked changes the selection, not the content the arrangement draws", "[speed164]")
{
    Studio studio (false);
    const auto first = studio.getProject().tracks.at (0).id;
    studio.addTrack ({});
    const auto second = studio.getProject().tracks.back().id;
    const auto change = studio.getChangeVersion(), content = studio.getContentVersion();
    studio.selectTrack (second);
    CHECK (studio.getChangeVersion() > change);   // (the track list, mixer and Inspector still follow it)
    CHECK (studio.getContentVersion() == content);
    studio.toggleTrackSelected (first);
    CHECK (studio.getContentVersion() == content);
    Part part;
    part.notes.push_back ({});
    studio.addPart (first, part);
    CHECK (studio.getContentVersion() > content);   // (an edit is content)
}

TEST_CASE ("0.164.0: PR7-D's fingerprint tells equal copies from changed ones", "[speed164]")
{
    Part p;
    p.start = 8.0;
    p.length = 4.0;
    p.name = "Verse: drums";
    for (int i = 0; i < 16; ++i)
        p.notes.push_back ({ i * 0.25, 0.1, i % 2 == 0 ? 36 : 42, 0.8f, false });
    const std::vector<Part> a { p };
    CHECK (pr7dCopiesFingerprint (a) == pr7dCopiesFingerprint (std::vector<Part> { p }));
    auto louder = p;
    louder.notes[3].velocity = 0.81f;
    CHECK (pr7dCopiesFingerprint (a) != pr7dCopiesFingerprint (std::vector<Part> { louder }));
    auto renamed = p;
    renamed.name = "Chorus: drums";
    CHECK (pr7dCopiesFingerprint (a) != pr7dCopiesFingerprint (std::vector<Part> { renamed }));
}
