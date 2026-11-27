// 0.263.0: a whole optimization pass (three reviewers: engine, message thread, screen; each finding checked).
#include <catch2/catch_test_macros.hpp>

#include "engine/Composer.h"
#include "engine/SectionTypes.h"
#include "engine/Studio.h"
#include "ui/ComposerPage.h"
#include "ui/OptionBrowser.h"

using namespace pr7s;
using L = ComposerLayer;

namespace
{
    bool sameNotes (const LineIdea& a, const LineIdea& b)
    {
        if (a.notes.size() != b.notes.size())
            return false;
        for (size_t i = 0; i < a.notes.size(); ++i)
        {
            const auto& x = a.notes[i];
            const auto& y = b.notes[i];
            if (x.beat != y.beat || x.length != y.length || x.pitch != y.pitch || x.velocity != y.velocity)
                return false;
        }
        return a.style == b.style;
    }
}

TEST_CASE ("0.263.0: a type's options listed by name, their parts made one at a time, as before", "[speed263]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (5);
    page.setSection (DrumSection::chorus);
    page.setSectionType ("SIXTEENTH WALL, HALF-TIME KIT");
    for (const auto l : { L::drums, L::bass })
    {
        const auto whole = page.laneOptions (l, true);
        auto named = page.laneOptions (l, false);
        REQUIRE (whole.size() == named.size());
        REQUIRE (! whole.empty());
        auto browser = page.makeOptionBrowser (l);
        for (size_t i = 0; i < whole.size(); ++i)
        {
            CHECK (named[i].name == whole[i].name);
            CHECK (named[i].group == whole[i].group);
            CHECK (named[i].id == whole[i].id);
            if (named[i].id < ComposerPage::approachId)
                continue;
            CHECK (named[i].pending);
            REQUIRE (browser->onFill != nullptr);
            CHECK (browser->onFill (named[i]));
            CHECK_FALSE (named[i].pending);
            CHECK (sameNotes (named[i].idea, whole[i].idea));
            CHECK (named[i].measure.feel == whole[i].measure.feel);
            CHECK (named[i].description == whole[i].description);
        }
        browser->fillAllNow();
        CHECK (browser->countPending() == 0);
    }
}

TEST_CASE ("0.263.0: the section types' option lists kept, the same every time", "[speed263]")
{
    ComposerState s;
    s.sectionType = "SIXTEENTH WALL, HALF-TIME KIT";
    const auto first = sectionApproaches (s, L::guitar);
    const auto again = sectionApproaches (s, L::guitar);
    CHECK (first == again);
    CHECK (! first.isEmpty());
    // Another type, another list; back again, the first.
    s.sectionType = "STACCATO CHUG";
    const auto other = sectionApproaches (s, L::guitar);
    CHECK (other != first);
    s.writtenRiffs = true;
    CHECK (sectionApproaches (s, L::guitar).isEmpty());
    s.writtenRiffs = false;
    s.sectionType = "SIXTEENTH WALL, HALF-TIME KIT";
    CHECK (sectionApproaches (s, L::guitar) == first);
    CHECK (baseSectionApproaches (s, L::guitar).size() <= first.size());
    // The keys' world changes the keys' list only.
    const auto keys = sectionApproaches (s, L::chords);
    s.lane (L::chords).world = 1;
    CHECK (sectionApproaches (s, L::chords) != keys);
    CHECK (sectionApproaches (s, L::guitar) == first);
}

TEST_CASE ("0.263.0: a lane's tracks found again when a track's name or the lane's names change", "[speed263]")
{
    Studio studio (false);
    const auto a = studio.addTrack ({});
    studio.renameTrack (a, "Something");
    CHECK (studio.findIdeaTracks (LineIdea::Part::bass, false).empty());
    studio.renameTrack (a, "Bass");
    CHECK (studio.findIdeaTracks (LineIdea::Part::bass, false) == std::vector<int> { a });
    CHECK (studio.findIdeaTracks (LineIdea::Part::bass, false) == std::vector<int> { a });
    auto tracks = studio.getComposerTracks (LineIdea::Part::bass);
    tracks.names = juce::StringArray ("Low End");
    studio.setComposerTracks (LineIdea::Part::bass, tracks);
    CHECK (studio.findIdeaTracks (LineIdea::Part::bass, false).empty());
    studio.renameTrack (a, "Low End");
    CHECK (studio.findIdeaTracks (LineIdea::Part::bass, false) == std::vector<int> { a });
    studio.setTracksDisabled ({ a }, true);
    CHECK (studio.findIdeaTracks (LineIdea::Part::bass, false).empty());
}

TEST_CASE ("0.263.0: favourites read once, and again when they change", "[speed263]")
{
    Studio studio (false);
    CHECK_FALSE (studio.getOptionMemory().isFavourite ("blueprint", "X"));
    studio.setOptionFavourite ("blueprint", "X", true);
    CHECK (studio.getOptionMemory().isFavourite ("blueprint", "X"));
    studio.setOptionFavourite ("blueprint", "X", false);
    CHECK_FALSE (studio.getOptionMemory().isFavourite ("blueprint", "X"));
}

TEST_CASE ("0.263.0: delay compensation off: no delay anywhere", "[speed263]")
{
    Studio studio (false);
    studio.addTrack ({});
    auto& engine = studio.getEngine();
    engine.setDelayCompensation (false);
    CHECK (engine.refreshLatency() == 0);
    CHECK (engine.getTotalLatency() == 0);
    engine.setDelayCompensation (true);
    CHECK (engine.refreshLatency() == 0);
}

#include "ui/MainView.h"

TEST_CASE ("0.263.0: ARCHITECT opened again goes to the block the playhead is in, with its own lanes", "[architect263]")
{
    Studio studio (false);
    REQUIRE (studio.appendSongSection ("Verse", 4) == 0);
    REQUIRE (studio.appendSongSection ("Bridge", 4) == 1);
    const auto verse = studio.getSongSections().blocks[0].id;
    const auto bridge = studio.getSongSections().blocks[1].id;
    // The bridge built: its recipe has the guitar on another style.
    auto built = ComposerState::fromString (studio.getComposerState());
    const auto looseStyle = built.lane (L::guitar).style;
    built.lane (L::guitar).style = looseStyle + 2;
    studio.noteSectionBuilt (bridge, built.toString(), {});

    MainView view (studio);
    view.setSize (1600, 1000);
    view.openSectionInComposer (1);
    REQUIRE (view.getComposerWindow() != nullptr);
    auto& page = view.getComposerWindow()->getPage();
    CHECK (page.getTargetBlock() == bridge);
    CHECK (page.getStyle (L::guitar) == looseStyle + 2);

    // Closed (hidden), the playhead put in the verse, opened again: the verse, with the lanes from before the bridge.
    view.getComposerWindow()->setVisible (false);
    studio.setPosition (studio.getSongSectionPlace (0)->start + 1.0);
    view.openComposer();
    CHECK (page.getTargetBlock() == verse);
    CHECK (page.getStyle (L::guitar) == looseStyle);
    CHECK (page.typeSectionName() == "Verse");

    // And back to the bridge: its own lanes again.
    view.getComposerWindow()->setVisible (false);
    studio.setPosition (studio.getSongSectionPlace (1)->start + 1.0);
    view.openComposer();
    CHECK (page.getTargetBlock() == bridge);
    CHECK (page.getStyle (L::guitar) == looseStyle + 2);
    view.getComposerWindow()->setVisible (false);
}
