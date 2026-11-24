#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "engine/Composer.h"
#include "engine/RiffPaste.h"
#include "ui/ComposerPage.h"
#include "ui/Finish.h"

// (0.226.0, the owner: "I want the ability to select a different riff per part if I desire... So for instance I
// could use a my riff for the chorus and a different riff for the verse") Each SECTIONS block its own riff.

using namespace pr7s;
using L = ComposerLayer;

namespace
{
    // A bar of eighths on one pitch with lengths and velocities told apart: one riff from another by its pitch.
    ComposerRiff riffOn (int pitch, const juce::String& name, DrumSection section)
    {
        std::vector<Note> notes;
        for (int i = 0; i < 8; ++i)
            notes.push_back ({ i * 0.5, 0.2 + i * 0.02, pitch + (i % 2 == 0 ? 0 : 7), (40.0f + i * 9.0f) / 127.0f });
        return riffFromNotes (notes, 0.0, 4.0, name, section);
    }

    // The pitches the guitar lane plays (keyswitches aside).
    std::vector<int> pitchesOf (const LineIdea& idea)
    {
        std::vector<int> out;
        for (const auto& n : idea.notes)
            if (! n.keyswitch)
                out.push_back (n.pitch);
        return out;
    }

    // The same notes (places and pitches exact, lengths and velocities as the song's text keeps them).
    bool sameNotes (const std::vector<LineNote>& a, const std::vector<LineNote>& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].pitch != b[i].pitch || std::abs (a[i].beat - b[i].beat) > 1.0e-3 || std::abs (a[i].length - b[i].length) > 1.0e-3
                || std::abs (a[i].velocity - b[i].velocity) > 1.0f / 127.0f)
                return false;
        return true;
    }

    bool playsRiff (const ComposerPage& page, const ComposerRiff& riff)
    {
        const auto total = page.getSpans().back().end - page.getSpans().front().start;
        return sameNotes (page.getIdea (L::guitar).notes, tileRiff (riff, total, 4.0).notes);
    }
}

TEST_CASE ("0.226.0: a riff per block: the verse's and the chorus's apart, each played on its own block", "[composer][riff][blockriff]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    studio.appendSongSection ("Verse", 4);
    studio.appendSongSection ("Chorus", 4);
    ComposerPage page (studio);
    page.setSize (1700, 940);
    page.setRandomSeed (5);
    const auto verseId = studio.getSongSections().blocks[0].id, chorusId = studio.getSongSections().blocks[1].id;

    // The verse's riff onto the verse.
    page.targetSection (verseId);
    const auto verseRiff = riffOn (40, "Verse riff", DrumSection::verse);
    REQUIRE (page.pasteRiff (verseRiff, (int) L::guitar));
    const auto verseRiffId = studio.getSongSections().blocks[0].riff;
    CHECK (verseRiffId > 0);
    CHECK (studio.getSongSections().blocks[1].riff == 0);
    CHECK (page.isPlayingMyRiff());
    CHECK (playsRiff (page, *studio.getComposerRiff()));

    // The chorus has none: the guitar writes its own there, not the verse's riff.
    page.targetSection (chorusId);
    CHECK_FALSE (studio.getComposerRiff().has_value());
    CHECK_FALSE (page.isPlayingMyRiff());
    CHECK_FALSE (page.getState().lane (L::guitar).locked);
    CHECK (pitchesOf (page.getIdea (L::guitar)) != pitchesOf (tileRiff (verseRiff, 16.0, 4.0)));

    // The chorus's own riff onto the chorus; the verse keeps its own.
    const auto chorusRiff = riffOn (45, "Chorus riff", DrumSection::chorus);
    REQUIRE (page.pasteRiff (chorusRiff, (int) L::guitar));
    const auto chorusRiffId = studio.getSongSections().blocks[1].riff;
    CHECK (chorusRiffId > 0);
    CHECK (chorusRiffId != verseRiffId);
    CHECK (studio.getSongSections().blocks[0].riff == verseRiffId);
    CHECK (playsRiff (page, chorusRiff));

    // Back on the verse: its riff in hand again, played.
    page.targetSection (verseId);
    REQUIRE (studio.getComposerRiff().has_value());
    CHECK (studio.getComposerRiff()->name == "Verse riff");
    CHECK (page.isPlayingMyRiff());
    CHECK (playsRiff (page, verseRiff));

    // Both listed as the song's riffs, each with the block it's on.
    const auto riffs = studio.getSongRiffs();
    REQUIRE (riffs.size() == 2);
    CHECK (riffs[0].blocks.contains ("Verse"));
    CHECK (riffs[1].blocks.contains ("Chorus"));

    // A new chorus of its own (not a linked copy) starts on the chorus's riff (the owner's choice).
    REQUIRE (studio.insertSongSection (2, "Chorus", 4, false));
    CHECK (studio.getSongSections().blocks[2].link == 0);
    CHECK (studio.getSongSections().blocks[2].riff == chorusRiffId);

    // The verse's riff chosen for the chorus too; then none.
    page.targetSection (chorusId);
    REQUIRE (page.chooseBlockRiff (verseRiffId));
    CHECK (studio.getSongSections().blocks[1].riff == verseRiffId);
    CHECK (playsRiff (page, verseRiff));
    REQUIRE (page.chooseBlockRiff (-1));
    CHECK (studio.getSongSections().blocks[1].riff == 0);
    CHECK_FALSE (page.isPlayingMyRiff());

    // An undo puts the chorus's riff choice back with the row.
    studio.undo();
    CHECK (studio.getSongSections().blocks[1].riff == verseRiffId);
    page.refreshForTests();
    CHECK (studio.getComposerRiff().has_value());

    // Saved and opened again: each block with its riff.
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("blockriffs", ".pr7s");
    REQUIRE (studio.save (file));
    Studio opened (false);
    juce::String problems;
    REQUIRE (opened.open (file, problems));
    const auto& o = opened.getSongSections().blocks;
    REQUIRE (o.size() == 3);
    REQUIRE (o[0].riff > 0);
    REQUIRE (o[2].riff > 0);
    CHECK (opened.getSongSectionRiff (0)->name == "Verse riff");
    CHECK (opened.getSongSectionRiff (1)->name == "Verse riff");
    CHECK (opened.getSongSectionRiff (2)->name == "Chorus riff");
    CHECK (sameNotes (opened.getSongSectionRiff (2)->notes, chorusRiff.notes));
    file.deleteFile();
}

TEST_CASE ("0.226.0: a riff pasted with no block chosen goes on the blocks of its section", "[composer][riff][blockriff]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    studio.appendSongSection ("Verse", 4);
    studio.appendSongSection ("Chorus", 4);
    studio.insertSongSection (2, "Verse", 4, false);   // (a second verse of its own)
    ComposerPage page (studio);
    page.setSize (1700, 940);
    page.clearTargetSection();
    REQUIRE (page.pasteRiff (riffOn (40, "Verse riff", DrumSection::verse), (int) L::guitar));
    const auto& blocks = studio.getSongSections().blocks;
    CHECK (blocks[0].riff > 0);
    CHECK (blocks[2].riff == blocks[0].riff);
    CHECK (blocks[1].riff == 0);
    CHECK (page.getTargetPlace().has_value());   // (the first verse chosen)
}

TEST_CASE ("0.226.0: a block's chords changed: the rebuild plays its riff, not a new part", "[composer][riff][blockriff][rewrite]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    studio.appendSongSection ("Verse", 4);
    studio.appendSongSection ("Chorus", 4);
    ComposerPage page (studio);
    page.setSize (1700, 940);
    page.setRandomSeed (8);
    page.targetSection (studio.getSongSections().blocks[1].id);
    const auto riff = riffOn (45, "Chorus riff", DrumSection::chorus);
    REQUIRE (page.pasteRiff (riff, (int) L::guitar));
    // Its chords set, then built.
    REQUIRE (studio.setSectionChords (1, { { Chord { 9, Chord::Type::minor }, 16.0 } }));
    page.targetSection (studio.getSongSections().blocks[1].id);
    const auto tracks = page.buildSection();
    REQUIRE_FALSE (tracks.empty());
    const auto place = *studio.getSongSectionPlace (1);
    const auto* guitar = studio.getProject().findTrack (studio.findIdeaTrack (LineIdea::Part::rhythm));
    REQUIRE (guitar != nullptr);
    const auto guitarPitches = [&]
    {
        std::vector<int> out;
        for (const auto& p : guitar->parts)
            for (const auto& n : p.notes)
                if (p.start + n.start >= place.start - 1.0e-9 && p.start + n.start < place.end - 1.0e-9 && n.pitch >= 24)
                    out.push_back (n.pitch);
        std::sort (out.begin(), out.end());
        out.erase (std::unique (out.begin(), out.end()), out.end());
        return out;
    };
    const std::vector<int> riffPitches { 45, 52 };
    REQUIRE (guitarPitches() == riffPitches);

    // The chords change: the block is built again, its guitar still the riff as played (0.225.0 wrote a new part).
    REQUIRE (studio.setSectionChords (1, { { Chord { 2, Chord::Type::minor }, 16.0 } }));
    guitar = studio.getProject().findTrack (guitar->id);
    REQUIRE (guitar != nullptr);
    CHECK (guitarPitches() == riffPitches);
}

TEST_CASE ("0.226.0: a song from before keeps its riff on the blocks of the riff's section", "[composer][riff][blockriff]")
{
    Project song;
    SongSection verse;
    verse.id = 1;
    verse.name = "Verse";
    verse.bars = 4;
    auto chorus = verse;
    chorus.id = 2;
    chorus.name = "Chorus";
    song.songSections.blocks = { verse, chorus };
    Project::ArchitectRiff loose;
    loose.notes = lineNotesToText (riffOn (45, "Old riff", DrumSection::chorus).notes);
    loose.bars = 1;
    loose.section = (int) DrumSection::chorus;
    loose.name = "Old riff";
    loose.played = true;
    song.architectRiff = loose;
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("oldriff", ".pr7s");
    REQUIRE (song.toXml()->writeTo (file));

    Studio studio (false);
    juce::String problems;
    REQUIRE (studio.open (file, problems));
    const auto& blocks = studio.getSongSections().blocks;
    CHECK (blocks[0].riff == 0);
    REQUIRE (blocks[1].riff > 0);
    CHECK (studio.getSongSectionRiff (1)->name == "Old riff");
    CHECK (studio.isSongSectionRiffPlayed (1));
    // The loose riff stays the riff with no block chosen.
    CHECK (studio.getComposerRiff().has_value());
    file.deleteFile();
}
