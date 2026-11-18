#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "engine/KeyCheck.h"
#include "ui/TrackList.h"
#include "ui/ArrangeView.h"

// (0.114.0, the owner: "in the sections lane I need to be able to set the key of the song and all parts need to be in
// that key ... by default all sections must be synced to the same key, but we could have an option to uncheck 'all
// sections are synced to one key' which would allow key changes")

using namespace pr7s;

namespace
{
    Key keyOf (int pc, bool minor) { return Key::fromPitchClass (pc, minor); }
    Chord chord (int root, Chord::Type type = Chord::Type::major) { return { root, type, Chord::Tension::none, -1 }; }

    struct Song
    {
        Studio studio { false };
        int keys = 0, drums = 0;
        Song()
        {
            studio.appendSongSection ("Verse", 4);
            studio.appendSongSection ("Chorus", 4);
            studio.setChordEvents ({ { 0.0, chord (0) }, { 4.0, chord (5) }, { 8.0, chord (7) }, { 12.0, chord (9, Chord::Type::minor) },
                                     { 16.0, chord (0) }, { 24.0, chord (5) } });
            studio.setChordTrackKey (0.0, keyOf (0, false));
            keys = studio.addTrack (Instrument {});
            studio.renameTrack (keys, "Keys");
            Part part;
            part.length = 32.0;
            for (int i = 0; i < 8; ++i)
                part.notes.push_back ({ i * 4.0, 1.0, i % 2 == 0 ? 64 : 60, 0.8f });   // E and C, verse and chorus
            studio.addPart (keys, part);
            drums = studio.addTrack (Instrument {});
            studio.renameTrack (drums, "Drums");
            Part beat;
            beat.length = 32.0;
            beat.notes.push_back ({ 0.0, 0.25, 36, 0.9f });
            beat.notes.push_back ({ 20.0, 0.25, 38, 0.9f });
            studio.addPart (drums, beat);
        }
        const std::vector<Note>& notesOf (int track) const { return studio.getProject().findTrack (track)->parts.front().notes; }
    };
}

TEST_CASE ("SONG KEY: moving music between keys (0.114.0)", "[songkey]")
{
    const auto c = keyOf (0, false), d = keyOf (2, false), cm = keyOf (0, true), am = keyOf (9, true);
    CHECK (keyShiftOf (c, d) == 2);
    CHECK (keyShiftOf (c, keyOf (7, false)) == -5);   // (G: down a fourth, the short way)
    CHECK (movePitchToKey (64, c, d) == 66);
    CHECK (movePitchToKey (64, c, cm) == 63);          // E in C major is Eb in C minor
    CHECK (movePitchToKey (61, c, cm) == 61);          // C#: outside, moves with the tonic (not at all)
    CHECK (movePitchToKey (69, c, am) == 65);          // A, C major's 6th, to A minor's 6th: F
    // A major chord on a major key's own degree takes the minor key's triad there.
    CHECK (moveChordToKey (chord (5), c, cm).type == Chord::Type::minor);       // F -> Fm
    CHECK (moveChordToKey (chord (9, Chord::Type::minor), c, cm).root == 8);    // Am -> Ab
    CHECK (moveChordToKey (chord (9, Chord::Type::minor), c, cm).type == Chord::Type::major);
    CHECK (moveChordToKey (chord (2), c, d).root == 4);                         // D (outside C's triads) -> E, still major
    CHECK (moveChordToKey (chord (2), c, d).type == Chord::Type::major);
}

TEST_CASE ("SONG KEY: the song's key set on the SECTIONS row moves every part into it, one undo step (0.114.0)", "[songkey]")
{
    Song song;
    auto& studio = song.studio;
    CHECK (studio.isOneKey());
    CHECK (studio.getSongKey() == keyOf (0, false));
    studio.setSongKey (keyOf (2, false));   // D major
    CHECK (studio.getSongKey() == keyOf (2, false));
    const auto& events = studio.getProject().chordTrack.events;
    REQUIRE (events.size() == 6);
    CHECK (events[0].chord.root == 2);
    CHECK (events[3].chord.root == 11);
    CHECK (events[3].chord.type == Chord::Type::minor);
    for (const auto& n : song.notesOf (song.keys))
        CHECK ((n.pitch == 66 || n.pitch == 62));
    CHECK (song.notesOf (song.drums)[0].pitch == 36);   // (drums aren't in a key)
    CHECK (studio.chordTrackKeyAt (20.0) == keyOf (2, false));
    for (const auto& b : studio.getSongSections().blocks)
        CHECK (studio.getSongSectionKey (studio.getSongSections().indexOf (b.id)) == keyOf (2, false));
    studio.undo();
    CHECK (studio.getProject().chordTrack.events[0].chord.root == 0);
    CHECK (song.notesOf (song.keys)[0].pitch == 64);
}

TEST_CASE ("SONG KEY: with ALL SECTIONS IN ONE KEY off, a section has its own key; back on, it returns (0.114.0)", "[songkey]")
{
    Song song;
    auto& studio = song.studio;
    // On: a block's key is the song's: changing it changes the song.
    studio.setSongSectionKey (1, keyOf (2, false));
    CHECK (studio.getSongKey() == keyOf (2, false));
    studio.setSongKey (keyOf (0, false));
    studio.setOneKey (false);
    CHECK_FALSE (studio.isOneKey());
    studio.setSongSectionKey (1, keyOf (4, false));   // the chorus in E
    CHECK (studio.getSongSectionKey (0) == keyOf (0, false));
    CHECK (studio.getSongSectionKey (1) == keyOf (4, false));
    CHECK (studio.getSongKey() == keyOf (0, false));
    const auto& events = studio.getProject().chordTrack.events;
    CHECK (events[0].chord.root == 0);   // (the verse kept)
    CHECK (events[4].chord.root == 4);   // (the chorus moved)
    CHECK (song.notesOf (song.keys)[0].pitch == 64);
    CHECK (song.notesOf (song.keys)[4].pitch == 68);
    CHECK (studio.chordTrackKeyAt (2.0) == keyOf (0, false));
    CHECK (studio.chordTrackKeyAt (18.0) == keyOf (4, false));
    // Saved and opened.
    const auto reread = Project::fromXml (*studio.getProject().toXml());
    REQUIRE (reread.has_value());
    CHECK_FALSE (reread->songSections.oneKey);
    CHECK (reread->songSections.blocks[1].key >= 0);
    // The song's key moved: the chorus with its own key stays.
    studio.setSongKey (keyOf (2, false));
    CHECK (studio.getSongSectionKey (1) == keyOf (4, false));
    CHECK (events[0].chord.root == 2);
    CHECK (events[4].chord.root == 4);
    // Back to one key: the chorus into the song's.
    studio.setOneKey (true);
    CHECK (studio.getSongSectionKey (1) == keyOf (2, false));
    CHECK (studio.getProject().chordTrack.events[4].chord.root == 2);
    CHECK (song.notesOf (song.keys)[4].pitch == 66);
}

TEST_CASE ("SONG KEY: the KEY button on the SECTIONS row (0.114.0)", "[songkey][ui]")
{
    Song song;
    Timeline timeline;
    TrackList list (song.studio, timeline);
    list.setSize (400, 800);
    song.studio.dispatchPendingMessages();
    auto* key = list.getSongKeyButton();
    REQUIRE (key != nullptr);
    CHECK (key->getButtonText() == "KEY: C MAJOR");
    song.studio.setSongKey (Key::fromPitchClass (9, true));
    song.studio.dispatchPendingMessages();
    CHECK (key->getButtonText() == "KEY: A MINOR");
    song.studio.setOneKey (false);
    song.studio.dispatchPendingMessages();
    CHECK (key->getButtonText() == "KEY: A MINOR +");
}
