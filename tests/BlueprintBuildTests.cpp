#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "engine/Blueprints.h"
#include "engine/SectionTypes.h"
#include "ui/BlueprintsPage.h"
#include "ui/SectionEditor.h"

// (0.262.0, the owner: "when a blue print is loaded each section should be fully populated with riffs and beats that
// are inspired by the song the blue print is based on. Then the user can hit roll to get more unique riffs and beats")

using namespace pr7s;

namespace
{
    // The notes a track plays inside [from, to), from `from` (rounded to 960ths), with pitches.
    std::vector<std::pair<int, int>> notesIn (const Project& p, int trackId, double from, double to)
    {
        std::vector<std::pair<int, int>> out;
        if (const auto* t = p.findTrack (trackId))
            for (const auto& part : t->parts)
                for (const auto& n : part.notes)
                {
                    const auto at = part.start + n.start;
                    if (at >= from - 1.0e-6 && at < to - 1.0e-6)
                        out.push_back ({ juce::roundToInt ((at - from) * 960.0), n.pitch });
                }
        std::sort (out.begin(), out.end());
        return out;
    }

    const Blueprint& firstWith (const std::function<bool (const Blueprint&)>& fits)
    {
        for (const auto& bp : factoryBlueprints())
            if (fits (bp))
                return bp;
        return factoryBlueprints().front();
    }
}

TEST_CASE ("BLUEPRINTS 0.262.0: the many from the songs: groups, lanes and options that exist, drums to match", "[blueprints262]")
{
    const auto& all = factoryBlueprints();
    REQUIRE (all.size() >= 100);
    juce::StringArray groups;
    auto withParts = 0, withDrums = 0;
    for (const auto& bp : all)
    {
        INFO (bp.name);
        if (bp.group.isNotEmpty())
            groups.addIfNotAlreadyThere (bp.group);
        for (const auto& b : bp.blocks)
        {
            INFO (b.name << " " << b.type << " " << b.parts);
            CHECK (b.parts.indexOfChar ('"') < 0);
            if (b.parts.isEmpty())
                continue;
            ++withParts;
            withDrums += (b.kick != 0 || b.snare != 0) ? 1 : 0;
            ComposerState s;
            applySectionType (s, b.name, b.type);
            for (const auto& [layer, option] : blueprintParts (b.parts))
            {
                CHECK (juce::isPositiveAndBelow (layer, composerLayerCount));
                if (option.isNotEmpty())   // (every option named is one of that lane's under the type)
                    CHECK (sectionApproaches (s, (ComposerLayer) layer).contains (option));
            }
        }
    }
    CHECK (groups.size() >= 10);
    CHECK (withParts > 800);
    CHECK (withDrums > withParts / 2);
}

TEST_CASE ("BLUEPRINTS 0.262.0: loading one builds every section; linked copies alike; roll gives another take", "[blueprints262]")
{
    Studio studio { false };
    const auto& bp = firstWith ([] (const Blueprint& b)
    {
        if (b.group.isEmpty()) return false;
        for (const auto& x : b.blocks)
            if (x.link > 0) return true;
        return false;
    });
    INFO (bp.name);
    REQUIRE (studio.applyBlueprint (bp, Key::all()[(size_t) juce::jmax (0, bp.keyIndex)], true, true));
    const auto built = studio.buildSongSections (&bp, 5);
    const auto& blocks = studio.getSongSections().blocks;
    CHECK (built == (int) blocks.size());
    const auto places = studio.getSongSectionPlaces();
    for (size_t i = 0; i < blocks.size(); ++i)
    {
        INFO (blocks[i].name << " " << (int) i);
        CHECK (blocks[i].recipe.isNotEmpty());
        CHECK (! blocks[i].lanes.empty());
        // The lanes the song played there, and no others.
        const auto parts = blueprintParts (bp.blocks[i].parts);
        if (! parts.empty())
            for (const auto& lane : blocks[i].lanes)
                CHECK (std::any_of (parts.begin(), parts.end(), [&] (const auto& p) { return p.first == lane.layer; }));
        // Something plays in it.
        auto notes = 0;
        for (const auto& lane : blocks[i].lanes)
            notes += (int) notesIn (studio.getProject(), lane.track, places[i].start, places[i].end).size();
        CHECK (notes > 0);
    }
    // Linked copies play the same music.
    for (size_t i = 0; i < blocks.size(); ++i)
        for (size_t j = i + 1; j < blocks.size(); ++j)
            if (blocks[i].link > 0 && blocks[i].link == blocks[j].link)
                for (const auto& lane : blocks[i].lanes)
                    CHECK (notesIn (studio.getProject(), lane.track, places[i].start, places[i].end)
                           == notesIn (studio.getProject(), lane.track, places[j].start, places[j].end));
    // ROLL: another take of one block (and its copies), its lanes kept; Undo puts it back.
    auto target = 0;
    for (int i = 0; i < (int) blocks.size(); ++i)
        if (std::any_of (blocks[(size_t) i].lanes.begin(), blocks[(size_t) i].lanes.end(), [] (const SongSection::Lane& l)
                         { return l.layer == (int) ComposerLayer::guitar || l.layer == (int) ComposerLayer::bass; }))
            target = i;
    const auto lanesBefore = blocks[(size_t) target].lanes.size();
    std::vector<std::vector<std::pair<int, int>>> before;
    for (const auto& lane : blocks[(size_t) target].lanes)
        before.push_back (notesIn (studio.getProject(), lane.track, places[(size_t) target].start, places[(size_t) target].end));
    REQUIRE (studio.rollSongSection (target, 99));
    CHECK (studio.getSongSections().blocks[(size_t) target].lanes.size() == lanesBefore);
    auto changedLanes = 0;
    for (size_t k = 0; k < studio.getSongSections().blocks[(size_t) target].lanes.size(); ++k)
        changedLanes += notesIn (studio.getProject(), studio.getSongSections().blocks[(size_t) target].lanes[k].track,
                                 places[(size_t) target].start, places[(size_t) target].end) != before[k] ? 1 : 0;
    CHECK (changedLanes > 0);
    studio.undo();
    for (size_t k = 0; k < before.size(); ++k)
        CHECK (notesIn (studio.getProject(), studio.getSongSections().blocks[(size_t) target].lanes[k].track,
                        places[(size_t) target].start, places[(size_t) target].end) == before[k]);
}

TEST_CASE ("BLUEPRINTS 0.262.0: the picker: groups, search, length, tempo, mode, favourites; BUILD THE MUSIC", "[blueprints262]")
{
    Studio studio { false };
    BlueprintsPage page (studio);
    page.setSize (BlueprintsPage::preferredWidth, BlueprintsPage::preferredHeight);
    const auto& all = page.getBlueprints();
    CHECK (page.getShown().size() == all.size());
    CHECK (page.getGroups().size() >= 10);
    page.setMode (BlueprintsPage::Mode::major);
    for (const auto i : page.getShown())
        CHECK (! all[(size_t) i].minor);
    CHECK (! page.getShown().empty());
    page.setMode (BlueprintsPage::Mode::any);
    page.setTempo (BlueprintsPage::Tempo::fast);
    for (const auto i : page.getShown())
        CHECK (all[(size_t) i].bpm > 140.0);
    page.setTempo (BlueprintsPage::Tempo::any);
    page.setLength (BlueprintsPage::Length::shortSong);
    for (const auto i : page.getShown())
        CHECK (all[(size_t) i].totalBars() < 64);
    page.setLength (BlueprintsPage::Length::any);
    const auto group = page.getGroups()[0];
    page.setGroup (group);
    CHECK (! page.getShown().empty());
    page.setGroup ({});
    page.setSearch ("breakdown");
    for (const auto i : page.getShown())
    {
        auto has = all[(size_t) i].name.containsIgnoreCase ("breakdown") || all[(size_t) i].description.containsIgnoreCase ("breakdown");
        for (const auto& b : all[(size_t) i].blocks)
            has = has || b.name.containsIgnoreCase ("breakdown") || b.type.containsIgnoreCase ("breakdown");
        CHECK (has);
    }
    page.setSearch ({});
    // Favourites: kept, listed in their own group.
    page.setFavourite (3, true);
    CHECK (page.isFavourite (3));
    page.setGroup (BlueprintsPage::favouritesGroup);
    REQUIRE (page.getShown().size() == 1);
    CHECK (page.getShown()[0] == 3);
    page.setFavourite (3, false);
    CHECK (page.getShown().empty());
    page.setGroup ({});
    // USE IT with BUILD THE MUSIC: the row and its music.
    CHECK (page.isWithMusic());
    page.choose (12);
    REQUIRE (page.use());
    for (const auto& b : studio.getSongSections().blocks)
        CHECK (b.recipe.isNotEmpty());
}

TEST_CASE ("BLUEPRINTS 0.262.0: the block editor's ROLL MUSIC builds an unbuilt block too", "[blueprints262]")
{
    Studio studio { false };
    const auto& bp = factoryBlueprints().front();
    REQUIRE (studio.applyBlueprint (bp, Key::all()[(size_t) bp.keyIndex], true, true));
    REQUIRE (studio.getSongSections().blocks[1].recipe.isEmpty());
    SectionEditor editor (studio, studio.getSongSections().blocks[1].id);
    REQUIRE (editor.rollMusic (3));
    CHECK (studio.getSongSections().blocks[1].recipe.isNotEmpty());
    CHECK (! studio.getSongSections().blocks[1].lanes.empty());
}
