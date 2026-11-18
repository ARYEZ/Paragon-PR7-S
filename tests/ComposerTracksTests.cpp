#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "engine/Composer.h"
#include "ui/ComposerPage.h"
#include "ui/ComposerTracksPage.h"

// COMPOSER TRACKS (0.112.0, the owner asked): each composer lane's track names (several, any case), and a
// lane on EVERY matching track (guitar left and right), the copies after the first a little looser.

using namespace pr7s;

namespace
{
    using L = ComposerLayer;

    int addNamed (Studio& studio, const juce::String& name)
    {
        const auto id = studio.addTrack (Instrument {});
        studio.renameTrack (id, name);
        return id;
    }

    std::vector<Note> notesOf (const Studio& studio, int trackId)
    {
        std::vector<Note> all;
        for (const auto& p : studio.getProject().findTrack (trackId)->parts)
            all.insert (all.end(), p.notes.begin(), p.notes.end());
        return all;
    }
}

TEST_CASE ("0.112.0: COMPOSER TRACKS: the names each lane looks for", "[composer][composertracks]")
{
    Studio studio (false);
    // The usual names by default, one track each.
    CHECK (studio.getComposerTracks (LineIdea::Part::bass).names == juce::StringArray ("Bass"));
    CHECK (studio.getComposerTracks (LineIdea::Part::tune).names == juce::StringArray ("Melody"));
    CHECK (studio.getComposerTracks (LineIdea::Part::melody).names == juce::StringArray ("Melody"));
    CHECK_FALSE (studio.getComposerTracks (LineIdea::Part::rhythm).every);

    // Any case, any of the names, the first name's track first.
    const auto di = addNamed (studio, "bass di");
    CHECK (studio.findIdeaTrack (LineIdea::Part::bass) == 0);
    studio.setComposerTracks (LineIdea::Part::bass, { juce::StringArray ("Bass", "Bass DI"), false });
    CHECK (studio.findIdeaTrack (LineIdea::Part::bass) == di);
    const auto bass = addNamed (studio, "BASS");
    CHECK (studio.findIdeaTrack (LineIdea::Part::bass) == bass);   // ("Bass" is named first)
    CHECK (studio.newIdeaTrackName (LineIdea::Part::bass) == "Bass");

    // Cleaned: spaces, repeats, empty names; none left: the usual one.
    studio.setComposerTracks (LineIdea::Part::lead, { juce::StringArray::fromTokens ("  Solo ,solo,, Lead Guitar ", ",", ""), false });
    CHECK (studio.getComposerTracks (LineIdea::Part::lead).names == juce::StringArray ("Solo", "Lead Guitar"));
    studio.setComposerTracks (LineIdea::Part::lead, { juce::StringArray (" , "), false });
    CHECK (studio.getComposerTracks (LineIdea::Part::lead).names == juce::StringArray ("Lead Guitar"));

    // EVERY: all matches in the track list's order; off: the first.
    const auto left = addNamed (studio, "Guitar L"), right = addNamed (studio, "Guitar R");
    studio.setComposerTracks (LineIdea::Part::rhythm, { juce::StringArray ("Guitar R", "Guitar L"), true });
    CHECK (studio.findIdeaTracks (LineIdea::Part::rhythm) == std::vector<int> { left, right });
    studio.setComposerTracks (LineIdea::Part::rhythm, { juce::StringArray ("Guitar R", "Guitar L"), false });
    CHECK (studio.findIdeaTracks (LineIdea::Part::rhythm) == std::vector<int> { right });

    // A new track BUILD makes takes the first name.
    studio.setComposerTracks (LineIdea::Part::chords, { juce::StringArray ("Keys", "Piano"), false });
    const std::vector<ChordSpan> spans { { 0.0, 4.0, Chord { 0 }, Key {} } };
    LineIdea idea;
    idea.part = LineIdea::Part::chords;
    idea.notes.push_back ({ 0.0, 1.0, 60, 0.8f });
    const auto made = studio.buildSection ({ idea }, spans, "Verse");
    REQUIRE (made.size() == 1);
    CHECK (studio.getProject().findTrack (made[0])->name == "Keys");

    studio.resetComposerTracks();
    CHECK (studio.getComposerTracks (LineIdea::Part::rhythm).names == juce::StringArray ("Guitar"));
    CHECK_FALSE (studio.getComposerTracks (LineIdea::Part::rhythm).every);
}

TEST_CASE ("0.112.0: a lane on two tracks: the second a looser copy", "[composer][composertracks]")
{
    Studio studio (false);
    const auto left = addNamed (studio, "Guitar L"), right = addNamed (studio, "Guitar R");
    studio.setComposerTracks (LineIdea::Part::rhythm, { juce::StringArray ("Guitar L", "Guitar R"), true });

    ComposerPage page (studio);
    page.setRandomSeed (5);
    for (const auto l : composerLayers())
        page.setOn (l, l == L::guitar);
    CHECK (page.tracksFor (L::guitar, false) == std::vector<int> { left, right });
    CHECK (page.trackText (L::guitar) == "TRACKS: GUITAR L + GUITAR R");

    const auto used = page.useAll();
    CHECK (used == std::vector<int> { left, right });
    const auto a = notesOf (studio, left), b = notesOf (studio, right);
    REQUIRE_FALSE (a.empty());
    REQUIRE (a.size() == b.size());
    // The same riff (pitches), played a little differently (timing or velocity), each velocity in its layer.
    auto differs = false, beatsClose = true;
    const auto layerOf = [] (float v)
    {
        const auto m = juce::roundToInt (v * 127.0f);
        return m <= 59 ? 0 : m <= 119 ? 1 : m <= 126 ? 2 : 3;
    };
    for (size_t i = 0; i < a.size(); ++i)
    {
        CHECK (a[i].pitch == b[i].pitch);
        CHECK (layerOf (a[i].velocity) == layerOf (b[i].velocity));
        differs = differs || std::abs (a[i].start - b[i].start) > 1.0e-6 || a[i].velocity != b[i].velocity;
        beatsClose = beatsClose && std::abs (a[i].start - b[i].start) < 0.06;
    }
    CHECK (differs);
    CHECK (beatsClose);

    // LOOSE COPIES off: the same notes on each.
    studio.setLooseCopies (false);
    studio.setPosition (64.0);
    page.useAll();
    std::vector<Note> later[2];
    for (int t = 0; t < 2; ++t)
        for (const auto& p : studio.getProject().findTrack (t == 0 ? left : right)->parts)
            if (p.start >= 64.0)
                later[t] = p.notes;
    REQUIRE_FALSE (later[0].empty());
    CHECK (later[0] == later[1]);

    // A track chosen in the lane's TRACK menu: that one alone.
    page.setTrack (L::guitar, right);
    CHECK (page.tracksFor (L::guitar, false) == std::vector<int> { right });
}

TEST_CASE ("0.112.0: a block built on two tracks is rebuilt on both when its chords change", "[composer][composertracks][sections]")
{
    Studio studio (false);
    const auto left = addNamed (studio, "Guitar L"), right = addNamed (studio, "Guitar R");
    studio.setComposerTracks (LineIdea::Part::rhythm, { juce::StringArray ("Guitar L", "Guitar R"), true });
    const auto index = studio.appendSongSection ("Verse", 4);
    REQUIRE (index >= 0);
    REQUIRE (studio.setSectionChords (index, { { Chord { 9, Chord::Type::minor }, 8.0 }, { Chord { 5 }, 8.0 } }));

    ComposerPage page (studio);
    page.setRandomSeed (9);
    for (const auto l : composerLayers())
        page.setOn (l, l == L::guitar);
    page.setFromChordTrack (true);
    page.targetSection (studio.getSongSections().blocks[(size_t) index].id);
    REQUIRE_FALSE (page.buildSection().empty());
    const auto& lanes = studio.getSongSections().blocks[(size_t) index].lanes;
    REQUIRE (lanes.size() == 2);
    CHECK (lanes[0].track == left);
    CHECK (lanes[1].track == right);
    const auto beforeLeft = notesOf (studio, left), beforeRight = notesOf (studio, right);

    // New chords: both rebuilt, the second still its own looser copy of the first.
    REQUIRE (studio.setSectionChords (index, { { Chord { 0 }, 8.0 }, { Chord { 7 }, 8.0 } }));
    const auto afterLeft = notesOf (studio, left), afterRight = notesOf (studio, right);
    CHECK_FALSE (afterLeft == beforeLeft);
    CHECK_FALSE (afterRight == beforeRight);
    REQUIRE (afterLeft.size() == afterRight.size());
    auto samePitches = true;
    for (size_t i = 0; i < afterLeft.size(); ++i)
        samePitches = samePitches && afterLeft[i].pitch == afterRight[i].pitch;
    CHECK (samePitches);
    CHECK_FALSE (afterLeft == afterRight);
}

TEST_CASE ("0.112.0: the Hub's COMPOSER TRACKS page", "[composertracks][ui]")
{
    Studio studio (false);
    addNamed (studio, "Guitar L");
    addNamed (studio, "Guitar R");
    ComposerTracksPage page (studio);
    page.setSize (900, 640);
    CHECK (page.getRow (1).names.getText() == "Guitar");
    CHECK (page.getFoundText (1).contains ("NONE YET"));
    page.setNames (1, "guitar l, Guitar R");
    page.setEvery (1, true);
    CHECK (studio.getComposerTracks (LineIdea::Part::rhythm).names == juce::StringArray ("guitar l", "Guitar R"));
    CHECK (page.getRow (1).every.getToggleState());
    CHECK (page.getFoundText (1) == "IN THIS SONG: GUITAR L + GUITAR R");
    page.setLooseCopies (false);
    CHECK_FALSE (studio.getLooseCopies());
    page.resetAll();
    CHECK (page.getRow (1).names.getText() == "Guitar");
    CHECK_FALSE (page.getRow (1).every.getToggleState());
    CHECK (ComposerTracksPage::laneName (5) == "DRUMS");
}
