#include <catch2/catch_test_macros.hpp>

#include "engine/OptionFeel.h"
#include "engine/Studio.h"
#include "engine/SectionTypes.h"
#include "ui/ComposerPage.h"
#include "ui/OptionBrowser.h"
#include "ui/ParagonLookAndFeel.h"
#include "ui/Finish.h"
#include <iostream>

using namespace pr7s;
using L = ComposerLayer;

namespace
{
    // Two bars of drums: a kick and snare pattern by sixteenths ('x'), closed hats on the eighths.
    LineIdea drumsOf (const char* kick, const char* snare, float velocity = 0.9f, bool hats = true)
    {
        LineIdea idea;
        idea.part = LineIdea::Part::drums;
        for (int bar = 0; bar < 2; ++bar)
            for (int s = 0; s < 16; ++s)
            {
                const auto beat = bar * 4.0 + s * 0.25;
                if (kick[s] == 'x')
                    idea.notes.push_back ({ beat, 0.25, 36, velocity });
                if (snare[s] == 'x')
                    idea.notes.push_back ({ beat, 0.25, 38, velocity });
                if (hats && s % 2 == 0)
                    idea.notes.push_back ({ beat, 0.25, 42, velocity * 0.7f });
            }
        return idea;
    }

    LineIdea lineOf (std::initializer_list<double> starts, double length, float velocity = 0.8f)
    {
        LineIdea idea;
        for (const auto b : starts)
            idea.notes.push_back ({ b, length, 48, velocity });
        return idea;
    }
}

TEST_CASE ("0.156.0: an option's feel and energy, measured from its part", "[options]")
{
    CHECK (measureOption (drumsOf ("x-------x-------", "----x-------x---"), 4.0, true).feel == OptionFeel::straight);
    CHECK (measureOption (drumsOf ("x-----x---x-----", "--------x-------"), 4.0, true).feel == OptionFeel::half);
    CHECK (measureOption (drumsOf ("x-x-x-x-x-x-x-x-", "x---x---x---x---"), 4.0, true).feel == OptionFeel::doubleTime);
    CHECK (measureOption (drumsOf ("x--x---x--x-----", "-----x-------x--"), 4.0, true).feel == OptionFeel::broken);
    // Hats on every sixteenth don't make a beat double time: the kick and snare do.
    {
        auto idea = drumsOf ("x-------x-------", "----x-------x---");
        for (int s = 0; s < 32; ++s)
            idea.notes.push_back ({ s * 0.25, 0.25, 44, 0.6f });
        CHECK (measureOption (idea, 4.0, true).feel == OptionFeel::straight);
    }
    // Energy: a few soft hits calm, a full hard beat heavy.
    const auto calm = measureOption (drumsOf ("x-------x-------", "--------x-------", 0.4f, false), 4.0, true);
    const auto heavy = measureOption (drumsOf ("xxxxxxxxxxxxxxxx", "----x-------x---", 1.0f), 4.0, true);
    CHECK (calm.energy == 1);
    CHECK (heavy.energy == 3);
    CHECK (heavy.hitsPerBeat > calm.hitsPerBeat);

    // A line: held notes half time and calm, pushed ones broken, sixteenths double time.
    CHECK (measureOption (lineOf ({ 0.0, 4.0 }, 4.0), 4.0, false).feel == OptionFeel::half);
    CHECK (measureOption (lineOf ({ 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0 }, 1.0), 4.0, false).feel == OptionFeel::straight);
    CHECK (measureOption (lineOf ({ 0.25, 0.75, 1.75, 2.25, 3.75, 4.25, 5.75, 6.25 }, 0.25), 4.0, false).feel == OptionFeel::broken);
    {
        std::vector<double> starts;
        for (int s = 0; s < 32; ++s)
            starts.push_back (s * 0.25);
        LineIdea fast;
        for (const auto b : starts)
            fast.notes.push_back ({ b, 0.25, 40, 0.9f });
        const auto m = measureOption (fast, 4.0, false);
        CHECK (m.feel == OptionFeel::doubleTime);
        CHECK (m.energy == 3);
    }
    CHECK (measureOption (LineIdea {}, 4.0, true).feel == OptionFeel::unknown);
    CHECK (optionFeelName (OptionFeel::half) == "HALF TIME");
    CHECK (energyName (2) == "MEDIUM");
}

TEST_CASE ("0.156.0: favourites and the recent, per list, kept by the studio", "[options]")
{
    OptionMemory m;
    m.setFavourite ("lane:drums", "BREAKBEAT", true);
    m.setFavourite ("lane:drums", "BREAKBEAT", true);
    CHECK (m.favouritesOf ("lane:drums").size() == 1);
    CHECK_FALSE (m.isFavourite ("lane:bass", "BREAKBEAT"));
    for (const auto* n : { "A", "B", "C", "D", "E", "B" })
        m.noteUsed ("lane:drums", n);
    CHECK (m.recentOf ("lane:drums") == juce::StringArray { "B", "E", "D", "C" });
    const auto back = OptionMemory::fromText (OptionMemory::toText (m.recent));
    CHECK (back.at ("lane:drums") == m.recentOf ("lane:drums"));
    m.setFavourite ("lane:drums", "BREAKBEAT", false);
    CHECK (m.favouritesOf ("lane:drums").isEmpty());

    Studio studio (false);
    studio.setOptionFavourite ("lane:keys", "ECHOING PLUCK", true);
    studio.noteOptionUsed ("lane:keys", "TRIAD ARPEGGIO IN SIXTEENTHS");
    CHECK (studio.getOptionMemory().isFavourite ("lane:keys", "ECHOING PLUCK"));
    CHECK (studio.getOptionMemory().recentOf ("lane:keys")[0] == "TRIAD ARPEGGIO IN SIXTEENTHS");
}

namespace
{
    int countItems (const juce::PopupMenu& menu, std::function<bool (const juce::PopupMenu::Item&)> which)
    {
        auto n = 0;
        for (juce::PopupMenu::MenuItemIterator it (menu); it.next();)
        {
            const auto& item = it.getItem();
            if (item.subMenu != nullptr)
                n += countItems (*item.subMenu, which);
            else if (which (item))
                ++n;
        }
        return n;
    }

    bool hasItem (const juce::PopupMenu& menu, int id)
    {
        return countItems (menu, [id] (const juce::PopupMenu::Item& i) { return i.itemID == id; }) > 0;
    }
}

TEST_CASE ("0.156.0: a lane's short menu and its option browser", "[options][composer][ui]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (3);
    page.setSection (DrumSection::chorus);
    page.setSectionType ("SIXTEENTH WALL, HALF-TIME KIT");
    REQUIRE (page.getSectionType() == "SIXTEENTH WALL, HALF-TIME KIT");

    const auto options = page.laneOptions (L::drums);
    const auto names = sectionApproaches (ComposerState::fromString (studio.getComposerState()), L::drums);
    REQUIRE (options.size() == (size_t) sectionApproaches (ComposerState::fromString (studio.getComposerState()), L::drums).size());
    juce::ignoreUnused (names);
    auto own = 0, songs = 0, measured = 0;
    for (const auto& o : options)
    {
        own += o.group == ComposerPage::typeOwnGroup ? 1 : 0;
        songs += o.group == ComposerPage::songsGroup ? 1 : 0;
        measured += o.measure.feel != OptionFeel::unknown ? 1 : 0;
        CHECK (o.id >= ComposerPage::approachId);
    }
    CHECK (own > 0);
    CHECK (songs >= 20);   // (the drums options from the owner's songs)
    CHECK (measured == (int) options.size());

    // The short menu: Automatic, the type's own, MORE OPTIONS; not every option.
    auto menu = page.makeStyleMenu (L::drums);
    CHECK (hasItem (menu, ComposerPage::moreOptionsItem));
    const auto shownOptions = countItems (menu, [] (const juce::PopupMenu::Item& i)
                                          { return i.itemID > ComposerPage::approachItems && i.itemID < ComposerPage::approachItems + 100; });
    CHECK (shownOptions == own);

    // A favourite from the songs' options shows in the short menu; choosing it puts it on and notes it as recent.
    const auto& pick = *std::find_if (options.begin(), options.end(), [] (const auto& o) { return o.group == ComposerPage::songsGroup; });
    studio.setOptionFavourite (page.optionListKey (L::drums), pick.name, true);
    menu = page.makeStyleMenu (L::drums);
    const auto pickItem = ComposerPage::approachItems + 1 + (pick.id - ComposerPage::approachId);
    CHECK (hasItem (menu, pickItem));
    page.styleMenuChosen (L::drums, pickItem);
    CHECK (page.getApproach (L::drums) == pick.id - ComposerPage::approachId);
    CHECK (studio.getOptionMemory().recentOf (page.optionListKey (L::drums))[0] == pick.name);

    // The browser: every option, groups, the feel and energy keys narrowing it.
    auto browser = page.makeOptionBrowser (L::drums);
    CHECK (browser->getChosen() == pick.id);
    CHECK (browser->getShown().size() == options.size());
    CHECK (browser->getGroups().contains (ComposerPage::songsGroup));
    CHECK (browser->countIn (OptionBrowser::favouritesGroup) == 1);
    browser->setGroup (OptionBrowser::favouritesGroup);
    CHECK (browser->getShown() == std::vector<int> { pick.id });
    browser->setGroup (OptionBrowser::everythingGroup);
    auto total = 0;
    for (const auto f : { OptionFeel::straight, OptionFeel::half, OptionFeel::doubleTime, OptionFeel::broken })
    {
        browser->setFeel (f);
        for (const auto id : browser->getShown())
            for (const auto& o : options)
                if (o.id == id)
                    CHECK (o.measure.feel == f);
        total += (int) browser->getShown().size();
    }
    CHECK (total == (int) options.size());
    browser->setFeel (OptionFeel::unknown);
    browser->setEnergy (3);
    for (const auto id : browser->getShown())
        for (const auto& o : options)
            if (o.id == id)
                CHECK (o.measure.energy == 3);
    browser->setEnergy (0);
    browser->setSearch ("tom");
    REQUIRE_FALSE (browser->getShown().empty());
    for (const auto id : browser->getShown())
        for (const auto& o : options)
            if (o.id == id)
                CHECK ((o.name + o.description).toLowerCase().contains ("tom"));
    browser->setSearch ({});

    // HEAR tries an option; closed without USE IT, the lane goes back.
    const auto other = options.back().id;
    REQUIRE (other != pick.id);
    browser->choose (other);
    browser->hear();
    CHECK (page.getApproach (L::drums) == other - ComposerPage::approachId);
    browser.reset();
    CHECK (page.getApproach (L::drums) == pick.id - ComposerPage::approachId);

    // USE IT keeps it.
    browser = page.makeOptionBrowser (L::drums);
    browser->choose (other);
    browser->hear();
    browser->use();
    browser.reset();
    CHECK (page.getApproach (L::drums) == other - ComposerPage::approachId);
    CHECK (studio.getOptionMemory().recentOf (page.optionListKey (L::drums))[0] == options.back().name);
    page.stop();

    // A lane without a section type: its styles, measured, in one group.
    page.setSectionType ({});
    const auto styles = page.laneOptions (L::bass);
    REQUIRE_FALSE (styles.empty());
    for (const auto& o : styles)
    {
        CHECK (o.group == ComposerPage::stylesGroup);
        CHECK (o.id < ComposerPage::approachId);
    }
    menu = page.makeStyleMenu (L::bass);
    if ((int) styles.size() <= ComposerPage::shortListMost)
        CHECK (hasItem (menu, styles.front().id + 1));
    page.styleMenuChosen (L::bass, styles.back().id + 1);
    CHECK (page.getStyle (L::bass) == styles.back().id);
}

TEST_CASE ("0.156.0: the option browser drawn", "[snapshot-optionbrowser]")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT", {});
    if (path.isEmpty())
        return;
    const auto finish = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FINISH", {});
    Finish::set (finish == "black" ? Finish::Type::pearlBlack : finish == "neon" ? Finish::Type::pearlWhiteNeon : Finish::Type::pearlWhite, false);
    Studio studio (false);
    ParagonLookAndFeel lookAndFeel;
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    juce::Image image;
    {
        ComposerPage page (studio);
        page.setRandomSeed (3);
        page.setSection (DrumSection::chorus);
        page.setSectionType ("SIXTEENTH WALL, HALF-TIME KIT");
        const auto options = page.laneOptions (L::drums);
        for (const auto& o : options)
            if (o.name == "BREAKBEAT" || o.name == "TOM ROLL CARPET")
                studio.setOptionFavourite (page.optionListKey (L::drums), o.name, true);
        auto browser = page.makeOptionBrowser (L::drums);
        const auto what = juce::SystemStats::getEnvironmentVariable ("PARAGON_SNAPSHOT_FEEL", {});
        if (what == "half")
            browser->setFeel (OptionFeel::half);
        else if (what == "search")
            browser->setSearch ("snare");
        else if (what == "songs")
            browser->setGroup (ComposerPage::songsGroup);
        for (const auto& o : options)
            if (o.name == "BREAKBEAT")
                browser->choose (o.id);
        image = browser->createComponentSnapshot (browser->getLocalBounds(), true, 2.0f);
    }
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    juce::File file (path);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (image, out);
}

TEST_CASE ("0.156.0: every drums option's measure printed", "[.optiondump]")
{
    Studio studio (false);
    ComposerPage page (studio);
    page.setRandomSeed (3);
    page.setSection (DrumSection::chorus);
    page.setSectionType (juce::SystemStats::getEnvironmentVariable ("PARAGON_TYPE", "SIXTEENTH WALL, HALF-TIME KIT"));
    for (const auto& o : page.laneOptions (L::drums))
    {
        juce::String snare, kick;
        for (int s = 0; s < 64; ++s)
        {
            auto sn = '-', k = '-';
            for (const auto& n : o.idea.notes)
                if (std::abs (n.beat - s * 0.25) < 0.02)
                {
                    if ((n.pitch == 38 || n.pitch == 40)) sn = n.velocity >= 0.6f ? 'X' : 'g';
                    if (n.pitch == 36 || n.pitch == 35) k = 'x';
                }
            snare << sn; kick << k;
        }
        std::cout << o.name << "  " << optionFeelName (o.measure.feel) << " " << o.measure.energy << " hpb " << o.measure.hitsPerBeat << "\n   S " << snare << "\n   K " << kick << "\n";
    }
}
