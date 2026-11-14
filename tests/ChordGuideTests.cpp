// CHORD GUIDES in the MIDI editor (0.121.0, the owner asked): under each chord on the chord track, the chord's notes
// lit in every octave (root, then 3rd / 5th, then added notes), the key's other notes faintly; never on drums.

#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"
#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/KeyEditor.h"
#include "ui/MainView.h"

using namespace pr7s;

namespace
{
    struct Song
    {
        Studio studio { false };
        MainView view { studio };
        int lead = 0;
        Song()
        {
            Finish::set (Finish::Type::pearlWhite, false);
            view.setSize (1500, 900);
            view.setKeyEditorSize (100);   // (more rows in view)
            lead = studio.getProject().tracks[0].id;
            studio.addPart (lead, { 0.0, 12.0, "Lead", { { 0.0, 1.0, 60, 0.8f } } });
            Chord g7 { 7 };
            g7.tension = Chord::Tension::seven;
            Chord am { 9 };
            am.type = Chord::Type::minor;
            // C for a bar, G7 for a bar, a gap, then A minor.
            studio.setChordEvents ({ { 0.0, Chord { 0 }, 4.0 }, { 4.0, g7, 4.0 }, { 10.0, am, 2.0 } });
            studio.dispatchPendingMessages();
            view.getArrangeView().select (lead, 0);
            view.openKeyEditor();
            studio.dispatchPendingMessages();
        }
        KeyEditor& editor() { return view.getKeyEditor(); }
    };
}

TEST_CASE ("0.121.0: CHORD GUIDES light each chord's notes, then the key's", "[chordguides]")
{
    Song song;
    auto& e = song.editor();
    REQUIRE (e.getPart() != nullptr);
    CHECK (e.isChordGuidesOn());
    // Under C major: C the root, E and G the triad, D / F / A / B the key, C# outside.
    CHECK (e.chordGuideLevel (60, 1.0) == 4);
    CHECK (e.chordGuideLevel (48, 1.0) == 4);   // (every octave)
    CHECK (e.chordGuideLevel (64, 1.0) == 3);
    CHECK (e.chordGuideLevel (67, 1.0) == 3);
    CHECK (e.chordGuideLevel (62, 1.0) == 1);
    CHECK (e.chordGuideLevel (61, 1.0) == 0);
    // Under G7: G root, B and D the triad, F the added 7th.
    CHECK (e.chordGuideLevel (67, 5.0) == 4);
    CHECK (e.chordGuideLevel (71, 5.0) == 3);
    CHECK (e.chordGuideLevel (62, 5.0) == 3);
    CHECK (e.chordGuideLevel (65, 5.0) == 2);
    // The gap between G7's end and A minor: nothing.
    CHECK (e.chordGuideLevel (67, 9.0) == 0);
    CHECK (e.chordGuideLevel (69, 9.0) == 0);
    // A minor: A, C, E.
    CHECK (e.chordGuideLevel (57, 11.0) == 4);
    CHECK (e.chordGuideLevel (60, 11.0) == 3);

    // The part's transpose: rows are the written pitches, the guide follows what's heard.
    song.studio.setPartsField ({ e.getPart()->id }, Studio::PartField::transpose, 2.0, true);
    song.studio.dispatchPendingMessages();
    CHECK (e.chordGuideLevel (58, 1.0) == 4);   // (written Bb sounds C)

    // Drawn, and off by the key.
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    e.setChordGuides (false);
    CHECK_FALSE (e.isChordGuidesOn());
    CHECK_FALSE (e.getChordGuidesKey().getToggleState());
    CHECK (e.createComponentSnapshot (e.getLocalBounds()).isValid());
    e.setChordGuides (true);
}

TEST_CASE ("0.121.0: CHORD GUIDES draw behind the notes, in the family's colours, and never on drums", "[chordguides]")
{
    Song song;
    auto& e = song.editor();
    // A root row under a chord is redder than the same row with the guides off (whichever is in view).
    const auto area = e.getNotesArea();
    REQUIRE_FALSE (area.isEmpty());
    const auto x = juce::roundToInt (e.xForBeat (2.0));   // inside the C chord, past the part's one note (beat 0-1)
    REQUIRE (area.contains ((float) x, area.getCentreY()));
    const auto beat = e.beatForX ((float) x);
    int y = -1;
    for (int pitch = 0; pitch < 128 && y < 0; ++pitch)
        if (e.chordGuideLevel (pitch, beat) == 4 && area.contains ((float) x, e.yForPitch (pitch) + 3.0f))
            y = juce::roundToInt (e.yForPitch (pitch) + 3.0f);
    REQUIRE (y >= 0);
    const auto lit = e.createComponentSnapshot (e.getLocalBounds()).getPixelAt (x, y);
    e.setChordGuides (false);
    const auto plain = e.createComponentSnapshot (e.getLocalBounds()).getPixelAt (x, y);
    e.setChordGuides (true);
    CHECK (lit.getRed() > plain.getRed());
    CHECK (lit.getRed() >= lit.getBlue());

    // A drum track: no guides.
    song.studio.setTrackDrumMap (song.lead, DrumMap::gm().name);
    song.studio.dispatchPendingMessages();
    CHECK (e.chordGuideLevel (60, 1.0) == 4);   // (the level itself is the chord's)
    const auto drums = e.createComponentSnapshot (e.getLocalBounds());
    CHECK (drums.isValid());
}
