#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/KeyCheck.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/KeyCheckPage.h"
#include "ui/KeyEditor.h"
#include "ui/MainView.h"

// 0.84.0, KEY CHECK: notes and chords outside the key, by the chord track's own rule.

using namespace pr7s;

namespace
{
    const Key aMinor { 5, 0, true }, cMajor { 0, 0, false };
}

TEST_CASE ("0.84.0: a note is in the key exactly when the chord track would call its one-note chord in the key", "[keycheck]")
{
    // The chord track's rule (Chord::isInKey): every note in the scale, a minor key's raised seventh allowed.
    // A chord is in the key exactly when each of its notes passes the note rule, for every chord in every key.
    using T = Chord::Type;
    using X = Chord::Tension;
    for (const auto& key : Key::all())
        for (int root = 0; root < 12; ++root)
            for (const auto type : { T::major, T::minor, T::diminished, T::augmented, T::sus2, T::sus4 })
                for (const auto tension : { X::none, X::seven, X::maj7, X::nine })
                {
                    const Chord chord { root, type, tension };
                    auto all = true, both7ths = false;
                    const auto pcs = chord.pitchClasses();
                    for (const auto pc : pcs)
                        all = all && isPitchClassInKey (pc, key);
                    // The one difference: a chord holding a minor key's seventh and its raised seventh together
                    // (the chord track: outside; a note is checked alone, and each of those is in the key).
                    const auto has = [&pcs] (int p) { return std::find (pcs.begin(), pcs.end(), ((p % 12) + 12) % 12) != pcs.end(); };
                    if (key.minor)
                        both7ths = has (key.pitchClass() + 10) && has (key.pitchClass() + 11);
                    CHECK (chord.isInKey (key) == (all && ! both7ths));
                }
    CHECK (isPitchClassInKey (8, aMinor));          // G#: A minor's raised seventh
    CHECK_FALSE (isPitchClassInKey (8, cMajor));
}

TEST_CASE ("0.84.0: notes outside the key are found on pitched tracks only, as heard, with the nearest in-key note", "[keycheck]")
{
    Studio studio (false);
    const auto lead = studio.getProject().tracks[0].id;
    studio.addPart (lead, { 0.0, 4.0, "Lead", { { 0.0, 1.0, 69, 0.8f }, { 1.0, 1.0, 70, 0.8f }, { 2.0, 1.0, 68, 0.8f }, { 3.0, 1.0, 66, 0.8f } } });
    const auto drums = studio.addTrack (Instrument {});
    studio.setTrackDrumMap (drums, "GM Map");
    studio.addPart (drums, { 0.0, 4.0, "Beat", { { 0.0, 0.5, 37, 0.8f }, { 1.0, 0.5, 39, 0.8f } } });
    const auto issues = studio.checkSongKey (aMinor);
    // Bb (70) and F# (66) are out; G# (68) is A minor's raised seventh, as the chord track allows; the drums skipped.
    REQUIRE (issues.size() == 2);
    CHECK (issues[0].pitch == 70);
    CHECK (issues[0].suggested == 69);   // a half step down to A (ties go down)
    CHECK (issues[0].noteIndex == 1);
    CHECK (issues[1].pitch == 66);
    CHECK (issues[1].suggested == 65);   // F (down on the tie with G)
    for (const auto& i : issues)
        CHECK (i.trackId == lead);

    // A part transposed: what's heard is checked (the part's transpose is playback only).
    auto& project = const_cast<Project&> (studio.getProject());
    project.findTrack (lead)->parts[0].transpose = 2;   // 71 72 70 68: B C Bb G#
    const auto heard = checkKey (project, [] (double) { return aMinor; });
    REQUIRE (heard.size() == 1);
    CHECK (heard[0].pitch == 70);   // 68 + 2: Bb heard (B, C and G# are in A minor)
    project.findTrack (lead)->parts[0].transpose = 0;
}

TEST_CASE ("0.84.0: FIX ALL is one undo step, and chords outside the key are listed", "[keycheck]")
{
    Studio studio (false);
    const auto lead = studio.getProject().tracks[0].id;
    studio.addPart (lead, { 0.0, 4.0, "Lead", { { 0.0, 1.0, 61, 0.8f }, { 1.0, 1.0, 63, 0.8f }, { 2.0, 1.0, 64, 0.8f } } });
    studio.addChordEvent (0.0, Chord { 0, Chord::Type::major });
    studio.addChordEvent (4.0, Chord { 1, Chord::Type::major });   // Db: outside C major
    studio.setScaleEvents ({ ScaleEvent { 0.0, cMajor } });
    auto issues = studio.checkSongKey();   // automatic: the chord track's key
    int notes = 0, chords = 0;
    for (const auto& i : issues)
        (i.kind == KeyIssue::Kind::note ? notes : chords)++;
    CHECK (notes == 2);
    CHECK (chords == 1);
    CHECK (studio.fixOutOfKeyNotes (issues));
    const auto& fixed = studio.getProject().findTrack (lead)->parts[0].notes;
    CHECK (fixed[0].pitch == 60);
    CHECK (fixed[1].pitch == 62);
    CHECK (fixed[2].pitch == 64);
    issues = studio.checkSongKey();
    REQUIRE (issues.size() == 1);
    CHECK (issues[0].kind == KeyIssue::Kind::chord);
    studio.undo();
    CHECK (studio.getProject().findTrack (lead)->parts[0].notes[0].pitch == 61);
    CHECK (studio.getProject().findTrack (lead)->parts[0].notes[1].pitch == 63);
}

TEST_CASE ("0.84.0: the KEY CHECK window lists, fixes and shows a note in the MIDI editor", "[keycheck]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    const auto lead = studio.getProject().tracks[0].id;
    studio.addPart (lead, { 0.0, 4.0, "Lead", { { 0.0, 1.0, 60, 0.8f }, { 1.0, 1.0, 61, 0.8f }, { 2.0, 1.0, 64, 0.8f } } });
    studio.dispatchPendingMessages();
    view.openKeyCheck();
    REQUIRE (view.getKeyCheckWindow() != nullptr);
    auto& page = view.getKeyCheckWindow()->getPage();
    page.setKey (cMajor);
    REQUIRE (page.getIssues().size() == 1);
    CHECK (page.getRowText (0).contains ("C#3"));
    CHECK (page.getSummary().contains ("1 NOTE"));
    CHECK (view.getKeyEditor().hasKeyMarks());

    page.onShowIssue (page.getIssues()[0]);
    studio.dispatchPendingMessages();
    REQUIRE (view.getKeyEditor().getPart() != nullptr);
    CHECK (view.getKeyEditor().getSelectedNotes() == std::vector<int> { 1 });
    CHECK (view.getKeyEditor().createComponentSnapshot (view.getKeyEditor().getLocalBounds()).isValid());

    CHECK (page.fix (0));
    CHECK (page.getIssues().empty());
    CHECK (page.getSummary().startsWith ("EVERYTHING IS IN THE KEY"));
    CHECK (studio.getProject().findTrack (lead)->parts[0].notes[1].pitch == 60);
    CHECK (page.createComponentSnapshot (page.getLocalBounds()).isValid());
}
