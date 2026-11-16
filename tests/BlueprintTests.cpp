#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "engine/Studio.h"
#include "engine/Blueprints.h"
#include "engine/Markers.h"
#include "engine/SectionTypes.h"

// (0.152.0, the owner: "choose specific songs and we create their section layout as a template so that way people can
// have presets to start with ... carry chords but give the option to turn the chords off")

using namespace pr7s;

TEST_CASE ("BLUEPRINTS 0.152.0: our blueprints are whole: known sections, types, transitions, keys, chords filling each block", "[blueprints]")
{
    const auto& all = factoryBlueprints();
    REQUIRE (all.size() == 10);
    juce::StringArray names;
    for (const auto& bp : all)
    {
        INFO (bp.name);
        CHECK (bp.factory);
        CHECK (! names.contains (bp.name));
        names.add (bp.name);
        CHECK (bp.keyIndex >= 0);
        CHECK (Key::all()[(size_t) juce::jmax (0, bp.keyIndex)].minor == bp.minor);
        CHECK (bp.bpm >= 40.0);
        CHECK (bp.description.isNotEmpty());
        CHECK (bp.hasChords());
        for (size_t i = 0; i < bp.blocks.size(); ++i)
        {
            const auto& b = bp.blocks[i];
            INFO (b.name << " " << (int) i);
            CHECK (songSectionNames().contains (b.name));
            CHECK (b.bars >= 1);
            CHECK (b.bars <= 64);
            if (b.type.isNotEmpty())
                CHECK (sectionTypeNamed (b.name, b.type).has_value());
            if (b.transition.isNotEmpty())
                CHECK (sectionTransitionNamed (b.transition).has_value());
            double bars = 0.0;
            for (const auto& c : b.chords)
                bars += c.bars;
            if (! b.chords.empty())   // (none: a block with no chords, the chord track left empty there)
                CHECK (bars == Catch::Approx ((double) b.bars).margin (1.0e-6));
            // Linked copies are the same block.
            if (b.link > 0)
                for (const auto& o : bp.blocks)
                    if (o.link == b.link)
                    {
                        CHECK (o.name == b.name);
                        CHECK (o.bars == b.bars);
                        CHECK (o.type == b.type);
                        CHECK (o.transition == b.transition);
                        CHECK (o.chords == b.chords);
                    }
        }
        // Read back from its file form exactly.
        const auto back = Blueprint::fromXml (*bp.toXml());
        REQUIRE (back.has_value());
        CHECK (*back == bp);
    }
}

TEST_CASE ("BLUEPRINTS 0.152.0: using one lays out the row, the key, the chords and the tempo in one undo step", "[blueprints]")
{
    Studio studio { false };
    studio.appendSongSection ("Verse", 4);
    const auto before = studio.getSongSections();
    const auto& bp = factoryBlueprints().front();
    const auto key = Key::fromPitchClass (4, true);   // E minor

    REQUIRE (studio.applyBlueprint (bp, key, true, true));
    const auto& blocks = studio.getSongSections().blocks;
    REQUIRE (blocks.size() == bp.blocks.size());
    for (size_t i = 0; i < blocks.size(); ++i)
    {
        CHECK (blocks[i].name == bp.blocks[i].name);
        CHECK (blocks[i].bars == bp.blocks[i].bars);
        CHECK (blocks[i].type == bp.blocks[i].type);
        CHECK ((blocks[i].link > 0) == (bp.blocks[i].link > 0));
    }
    CHECK (studio.getSongSections().totalBars() == bp.totalBars());
    CHECK (studio.getSongKey() == key);
    CHECK (studio.getProject().bpm == Catch::Approx (bp.bpm));
    // The first block's first chord is on the key's tonic (E), each block's chords from its start.
    const auto chords = studio.getSectionChords (0);
    REQUIRE (! chords.empty());
    CHECK (chords.front().chord.root == 4);
    CHECK (chords.front().chord.type == Chord::Type::minor);
    const auto places = studio.getSongSectionPlaces();
    for (size_t i = 0; i < places.size(); ++i)
    {
        double beats = 0.0;
        for (const auto& c : studio.getSectionChords ((int) i))
            beats += c.beats;
        CHECK (beats == Catch::Approx (places[i].end - places[i].start).margin (1.0e-6));
    }
    // Linked copies hold the same chords.
    for (size_t i = 0; i < blocks.size(); ++i)
        for (size_t j = i + 1; j < blocks.size(); ++j)
            if (blocks[i].link > 0 && blocks[i].link == blocks[j].link)
                CHECK (studio.getSectionChords ((int) i) == studio.getSectionChords ((int) j));

    studio.undo();
    CHECK (studio.getSongSections() == before);
}

TEST_CASE ("BLUEPRINTS 0.152.0: chords and tempo can be left out; another mode plays from the relative key", "[blueprints]")
{
    Studio studio { false };
    const auto bpm = studio.getProject().bpm;
    const auto& bp = factoryBlueprints().front();
    REQUIRE (bp.minor);
    REQUIRE (studio.applyBlueprint (bp, Key::fromPitchClass (7, false), false, false));   // G major
    CHECK (studio.getProject().chordTrack.events.empty());
    CHECK (studio.getProject().bpm == Catch::Approx (bpm));

    Studio other { false };
    REQUIRE (other.applyBlueprint (bp, Key::fromPitchClass (7, false), true, true));
    const auto chords = other.getSectionChords (0);
    REQUIRE (! chords.empty());
    CHECK (chords.front().chord.root == 4);   // (G major's relative minor: E minor's i)
    CHECK (chords.front().chord.type == Chord::Type::minor);
}

TEST_CASE ("BLUEPRINTS 0.152.0: a song's row saved as the owner's own, listed, read back and deleted", "[blueprints]")
{
    juce::TemporaryFile folderFile;
    const auto folder = folderFile.getFile();
    folder.createDirectory();
    Studio studio { false };
    studio.setBlueprintFolderForTests (folder);
    const auto& bp = factoryBlueprints()[1];
    REQUIRE (studio.applyBlueprint (bp, Key::fromPitchClass (9, true), true, true));   // A minor (its own key)

    const auto made = studio.makeBlueprint ("My Form");
    REQUIRE (made.blocks.size() == bp.blocks.size());
    for (size_t i = 0; i < made.blocks.size(); ++i)
    {
        INFO ((int) i);
        CHECK (made.blocks[i].name == bp.blocks[i].name);
        CHECK (made.blocks[i].bars == bp.blocks[i].bars);
        CHECK (made.blocks[i].type == bp.blocks[i].type);
        CHECK (made.blocks[i].transition == bp.blocks[i].transition);
        CHECK (made.blocks[i].chords == bp.blocks[i].chords);
    }
    CHECK (made.bpm == Catch::Approx (bp.bpm));

    juce::String error;
    const auto file = studio.saveSongAsBlueprint ("My Form", error);
    REQUIRE (file.existsAsFile());
    const auto second = studio.saveSongAsBlueprint ("My Form", error);
    CHECK (second.getFileNameWithoutExtension() == "My Form 2");
    const auto list = studio.getBlueprints();
    CHECK (list.size() == factoryBlueprints().size() + 2);
    const auto mine = loadBlueprint (file);
    REQUIRE (mine.has_value());
    CHECK (! mine->factory);
    CHECK (mine->blocks == made.blocks);

    // Used in another song, it gives that song the same row.
    Studio again { false };
    REQUIRE (again.applyBlueprint (*mine, Key::fromPitchClass (2, true), true, true));
    CHECK (again.getSongSections().blocks.size() == made.blocks.size());

    CHECK (studio.deleteBlueprint (*mine));
    CHECK (! file.existsAsFile());
    CHECK (! studio.deleteBlueprint (factoryBlueprints().front()));
    folder.deleteRecursively();
}

#include "ui/BlueprintsPage.h"

TEST_CASE ("BLUEPRINTS 0.152.0: the window uses the chosen one, asks before replacing a filled row, chords off when unticked", "[blueprints]")
{
    Studio studio { false };
    BlueprintsPage page (studio);
    page.setSize (BlueprintsPage::preferredWidth, BlueprintsPage::preferredHeight);
    REQUIRE (page.getBlueprints().size() == factoryBlueprints().size());
    page.choose (2);
    // Its own key to start.
    CHECK (page.getKey() == Key::all()[(size_t) factoryBlueprints()[2].keyIndex]);
    CHECK (! page.getDeleteKey().isEnabled());   // (ours can't be deleted)
    REQUIRE (page.use());
    CHECK (studio.getSongSections().blocks.size() == factoryBlueprints()[2].blocks.size());

    // The row is filled now: it asks; "no" leaves it.
    auto asked = 0;
    page.askToReplace = [&asked] (std::function<void (bool)> answer) { ++asked; answer (false); };
    page.choose (0);
    page.use();
    CHECK (asked == 1);
    CHECK (studio.getSongSections().blocks.size() == factoryBlueprints()[2].blocks.size());
    // "Yes", chords off: the row replaced, the chord track as it was.
    const auto chordsBefore = studio.getProject().chordTrack.events;
    page.askToReplace = [&asked] (std::function<void (bool)> answer) { ++asked; answer (true); };
    page.setWithChords (false);
    page.use();
    CHECK (asked == 2);
    CHECK (studio.getSongSections().blocks.size() == factoryBlueprints()[0].blocks.size());
    CHECK (studio.getProject().chordTrack.events == chordsBefore);
}
