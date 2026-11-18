#include <catch2/catch_test_macros.hpp>

#include "engine/MediaBay.h"
#include "engine/Studio.h"
#include "ui/MediaBayPage.h"
#include "ui/MediaRack.h"

using namespace pr7s;

// (0.163.0) The owner's MediaBay holds about 120,000 scanned files and was "basically unusable".
// These cases fill it with that many made-up items (no files on disk) and time what the window does.
namespace
{
    std::vector<MediaItem> manyItems (int count)
    {
        std::vector<MediaItem> items;
        items.reserve ((size_t) count);
        const juce::File root ("/library");
        const char* words[] = { "Kick", "Snare", "Hat", "Bass", "Pad", "Lead", "Loop", "Fill", "Perc", "Vox" };
        const char* keys[] = { "Am", "C", "F#m", "", "Eb", "" };
        juce::Random random (7);
        for (int i = 0; i < count; ++i)
        {
            MediaItem item;
            const auto pack = i / 2000, folder = (i / 200) % 10;
            item.file = root.getChildFile ("Pack " + juce::String (pack)).getChildFile ("Folder " + juce::String (folder))
                            .getChildFile (juce::String (words[i % 10]) + "_" + juce::String (80 + i % 90) + "bpm_" + juce::String (i) + ".wav");
            item.type = i % 9 == 0 ? MediaType::midi : MediaType::audio;
            if (item.type == MediaType::midi)
                item.file = item.file.withFileExtension ("mid");
            item.name = item.file.getFileNameWithoutExtension();
            item.size = 10000 + random.nextInt (1000000);
            item.modified = juce::Time (1700000000000LL + random.nextInt (100000000));
            item.seconds = random.nextFloat() * 10.0;
            item.sampleRate = 44100.0;
            item.channels = 2;
            item.bpm = 80 + i % 90;
            item.key = keys[i % 6];
            items.push_back (item);
        }
        return items;
    }

    double msOf (const std::function<void()>& f, int times = 1)
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < times; ++i)
            f();
        return (juce::Time::getMillisecondCounterHiRes() - start) / times;
    }
}

TEST_CASE ("0.163.0: the MediaBay with 120,000 files (timings)", "[.perf-mediabay]")
{
    const auto count = juce::SystemStats::getEnvironmentVariable ("PARAGON_PERF_FILES", "120000").getIntValue();
    auto items = manyItems (count);
    // A few tagged and rated, as the owner's.
    for (size_t i = 0; i < items.size(); i += 37)
        items[i].tags.category = i % 2 ? "Drums" : "Bass";

    juce::TemporaryFile store (".xml");
    MediaDatabase db (store.getFile());
    db.addLocation (juce::File ("/library"));
    std::printf ("taking the scan's %d results (sort + save): %.1f ms\n", count, msOf ([&] { db.setItemsForTests (items); }));
    std::printf ("save: %.1f ms\n", msOf ([&] { db.save(); }));
    MediaQuery all;
    std::printf ("find all: %.1f ms\n", msOf ([&] { db.find (all); }, 3));
    MediaQuery text;
    text.text = "kick";
    std::printf ("find 'kick' (first, cache cold): %.1f ms\n", msOf ([&] { db.find (text); }));
    std::printf ("find 'kick' (again): %.1f ms\n", msOf ([&] { db.find (text); }, 3));
    text.text = "kick 120";
    std::printf ("find 'kick 120': %.1f ms\n", msOf ([&] { db.find (text); }, 3));
    MediaQuery folder;
    folder.folder = juce::File ("/library/Pack 3");
    std::printf ("find in a folder: %.1f ms\n", msOf ([&] { db.find (folder); }, 3));
    auto found = db.find (all);
    std::printf ("sort by name: %.1f ms\n", msOf ([&] { auto f = found; db.sort (f, MediaColumn::name, true); }));
    std::printf ("sort by folder: %.1f ms\n", msOf ([&] { auto f = found; db.sort (f, MediaColumn::folder, true); }));
    std::printf ("sort by tempo: %.1f ms\n", msOf ([&] { auto f = found; db.sort (f, MediaColumn::tempo, true); }));
    std::printf ("valueCounts category: %.1f ms\n", msOf ([&] { db.valueCounts (all, { MediaAttribute::category, {} }); }, 3));
    std::printf ("a rating set (saved soon) + save: %.1f ms\n", msOf ([&] { db.setRating (items[5].file, 3); db.save(); }));

    Studio studio (false);
    studio.getMediaBay().addLocation (juce::File ("/library"));
    studio.getMediaBay().setItemsForTests (items);
    std::unique_ptr<MediaBayPage> page;
    std::printf ("open the MediaBay window: %.1f ms\n", msOf ([&] { page = std::make_unique<MediaBayPage> (studio); page->setSize (1600, 900); }));
    page->setAutoPlay (false);
    std::printf ("type a letter ('k'): %.1f ms\n", msOf ([&] { page->setSearch ("k"); }));
    std::printf ("type the next ('ki'): %.1f ms\n", msOf ([&] { page->setSearch ("ki"); }));
    std::printf ("clear the search: %.1f ms\n", msOf ([&] { page->setSearch (""); }));
    std::printf ("click a row: %.1f ms\n", msOf ([&] { page->selectRow (10); }, 3));
    std::printf ("rate a row: %.1f ms\n", msOf ([&] { page->setRating (10, 4); }));
    std::printf ("sort by name: %.1f ms\n", msOf ([&] { page->sortBy (MediaColumn::name, false); }));
    std::printf ("filter to audio only: %.1f ms\n", msOf ([&] { page->setTypeShown (MediaType::midi, false); }));
    std::printf ("choose a folder: %.1f ms\n", msOf ([&] { page->selectFolder (juce::File ("/library/Pack 3")); }));
    std::printf ("back to all: %.1f ms\n", msOf ([&] { page->selectFolder (juce::File()); }));
    std::printf ("set a category on the selection: %.1f ms\n", msOf ([&] { page->setAttribute ({ MediaAttribute::category, {} }, "Drums"); }));
    juce::Image image (juce::Image::RGB, 1600, 900, true);
    std::printf ("paint the window: %.1f ms\n", msOf ([&] { juce::Graphics g (image); page->paintEntireComponent (g, true); }));
    std::printf ("timer tick: %.1f ms\n", msOf ([&] { page->runTimerForTests(); }, 10));
    page.reset();

    std::unique_ptr<MediaRack> rack;
    std::printf ("open the Media rack: %.1f ms\n", msOf ([&] { rack = std::make_unique<MediaRack> (studio); rack->setSize (360, 900); }));
    std::printf ("rack's LOOPS page: %.1f ms\n", msOf ([&] { rack->openPage (MediaRack::Page::loops); }));
    std::printf ("rack type a letter: %.1f ms\n", msOf ([&] { rack->setSearch ("k"); }));
    std::printf ("rack home: %.1f ms\n", msOf ([&] { rack->openPage (MediaRack::Page::home); }));
}
