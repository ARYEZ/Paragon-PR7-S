// (0.179.0, the owner: "do all of it so long as it doesn't negatively affect the experience") The GROOVE FOLDERS tab
// and the drums lane's options timed with a library the size of the owner's (100,000 grooves in PR7-D's index), and
// checks that the quicker ways give the same answers.

#include <catch2/catch_test_macros.hpp>

#include "engine/Composer.h"
#include "engine/Studio.h"
#include "ui/ComposerPage.h"
#include "ui/GrooveFoldersPage.h"
#include "ui/OptionBrowser.h"

#include <chrono>
#include <iostream>
#include <set>

using namespace pr7s;

namespace
{
    using L = ComposerLayer;

    template <typename F>
    double timeMs (F&& f, int times = 1)
    {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < times; ++i)
            f();
        return std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count() / times;
    }

    juce::File writeGroove (const juce::File& file, int kickAt)
    {
        juce::MidiMessageSequence track;
        for (int bar = 0; bar < 4; ++bar)
        {
            for (int e = 0; e < 8; ++e)
                track.addEvent (juce::MidiMessage::noteOn (10, 42, (juce::uint8) 90), (bar * 4.0 + e * 0.5) * 480.0);
            for (const auto [beat, pitch] : std::initializer_list<std::pair<double, int>> { { 0.0, 36 }, { 1.0, 38 }, { 3.0, 38 }, { 0.25 * kickAt, 36 } })
                track.addEvent (juce::MidiMessage::noteOn (10, pitch, (juce::uint8) 100), (bar * 4.0 + beat) * 480.0);
        }
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        midi.addTrack (track);
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream out (file);
        midi.writeTo (out);
        return file;
    }

    // PR7-D's presets folder with `count` four-bar grooves listed: Toontrack's packs and others, by section. Files are
    // written only when asked for (`writeFiles`): the index is all that's read until one is played.
    struct Library
    {
        juce::TemporaryFile temp;
        juce::File presets, packs;

        explicit Library (int count)
        {
            const auto root = temp.getFile();
            root.createDirectory();
            presets = root.getChildFile ("Rigs");
            presets.createDirectory();
            packs = root.getChildFile ("Packs");
            juce::XmlElement settings ("Grooves");
            auto* f = settings.createNewChildElement ("Folder");
            f->setAttribute ("path", packs.getFullPathName());
            f->setAttribute ("map", 1);
            settings.writeTo (presets.getChildFile ("Grooves.xml"));
            const char* sections[] { "Intro", "Verse", "Chorus", "Bridge", "Breakdown", "Outro" };
            const char* genres[] { "Metal", "Rock", "Prog", "Djent", "Funk" };
            juce::XmlElement index ("GrooveIndex");
            for (int i = 0; i < count; ++i)
            {
                auto* g = index.createNewChildElement ("G");
                const auto maker = (i / 500) % 3 == 2 ? juce::String ("Other Packs") : juce::String ("Toontrack");
                const auto path = packs.getChildFile (maker + "/Pack " + juce::String (i / 500) + "/" + genres[i % 5] + "/"
                                                      + sections[(i / 7) % 6] + "/" + sections[(i / 7) % 6] + " " + juce::String (i) + ".mid");
                g->setAttribute ("path", path.getFullPathName());
                g->setAttribute ("stamp", "1|2|1");
                g->setAttribute ("bpm", 90 + i % 90);
                g->setAttribute ("bpmKnown", 1);
                g->setAttribute ("num", 4);
                g->setAttribute ("den", 4);
                g->setAttribute ("bars", 4);
                g->setAttribute ("tags", juce::String ("Genre:") + genres[i % 5] + "|Type:Beat");
            }
            index.writeTo (presets.getChildFile ("Groove Index.xml"));
        }
    };

    int perfCount() { return juce::jmax (100, juce::SystemStats::getEnvironmentVariable ("PARAGON_PERF_GROOVES", "100000").getIntValue()); }
}

TEST_CASE ("0.179.0: GROOVE FOLDERS and the drums options, timed", "[.perf-179]")
{
    const auto count = perfCount();
    Library library (count);
    Studio studio (false);
    studio.setPr7dFolderForTests (library.presets);
    studio.appendSongSection ("Verse", 8);
    std::cout << "grooves " << count << "\n";
    std::cout << "first library read " << timeMs ([&] { studio.getComposerContext (0.0); }) << " ms\n";

    std::unique_ptr<GrooveFoldersPage> folders;
    std::cout << "GROOVE FOLDERS made " << timeMs ([&] { folders = std::make_unique<GrooveFoldersPage> (studio); folders->setSize (1200, 800); }) << " ms\n";
    std::cout << "  paint " << timeMs ([&] { folders->createComponentSnapshot (folders->getLocalBounds(), true, 1.0f); }, 3) << " ms\n";
    auto& tree = folders->getTree();
    std::cout << "  open the top folders " << timeMs ([&] {
        for (int i = 0; i < tree.getNumRowsInTree(); ++i)
            if (auto* item = tree.getItemOnRow (i); item != nullptr && i < 6)
                item->setOpen (true);
        folders->createComponentSnapshot (folders->getLocalBounds(), true, 1.0f); }) << " ms\n";
    std::cout << "  open every pack (" << tree.getNumRowsInTree() << " rows) " << timeMs ([&] {
        for (int i = 0; i < tree.getNumRowsInTree() && i < 80; ++i)
            if (auto* item = tree.getItemOnRow (i))
                item->setOpen (true);
        folders->createComponentSnapshot (folders->getLocalBounds(), true, 1.0f); }) << " ms\n";
    std::cout << "  paint again (" << tree.getNumRowsInTree() << " rows) " << timeMs ([&] { folders->createComponentSnapshot (folders->getLocalBounds(), true, 1.0f); }, 3) << " ms\n";
    std::cout << "  a star clicked " << timeMs ([&] { folders->toggle (library.packs.getChildFile ("Toontrack/Pack 0")); folders->createComponentSnapshot (folders->getLocalBounds(), true, 1.0f); }) << " ms\n";
    std::cout << "  refresh " << timeMs ([&] { folders->refresh(); }) << " ms\n";

    auto page = std::make_unique<ComposerPage> (studio);
    page->setSize (1700, 940);
    page->setLibraryDrums (true);
    page->setSection (DrumSection::verse);
    const auto pool = page->sectionGrooves();
    std::cout << "pool " << pool.size() << " grooves; writing their files\n";
    for (size_t i = 0; i < pool.size(); ++i)
        writeGroove (pool[i].file, 2 + (int) i % 11);
    std::cout << "the short drums menu (first) " << timeMs ([&] { page->makeStyleMenu (L::drums); }) << " ms\n";
    std::cout << "the short drums menu (again) " << timeMs ([&] { page->makeStyleMenu (L::drums); }, 3) << " ms\n";
    page->setSection (DrumSection::chorus);
    page->setSection (DrumSection::verse);
    std::cout << "MORE OPTIONS (browser made) " << timeMs ([&] { auto b = page->makeOptionBrowser (L::drums); b->setSize (1600, 900); b->createComponentSnapshot (b->getLocalBounds(), true, 1.0f); }) << " ms\n";
    std::cout << "MORE OPTIONS again " << timeMs ([&] { auto b = page->makeOptionBrowser (L::drums); }) << " ms\n";
}

TEST_CASE ("0.179.0: GROOVE FOLDERS counts and subfolders found by halving match counting them one by one", "[speed179][favourites]")
{
    Library library (3000);
    Studio studio (false);
    studio.setPr7dFolderForTests (library.presets);
    GrooveFoldersPage page (studio);
    std::vector<juce::String> paths;
    for (const auto& g : studio.getComposerContext (0.0).libraryGrooves)
        paths.push_back (g.file.getFullPathName());
    REQUIRE (paths.size() > 100);
    const auto sep = juce::String::charToString (juce::File::getSeparatorChar());
    // Every folder holding grooves, at every depth.
    std::set<juce::String> folders;
    for (const auto& p : paths)
        for (auto f = juce::File (p).getParentDirectory(); f.isAChildOf (library.packs.getParentDirectory()); f = f.getParentDirectory())
            folders.insert (f.getFullPathName());
    folders.insert (library.packs.getFullPathName());
    for (const auto& folder : folders)
    {
        const auto prefix = folder + sep;
        auto count = 0;
        std::set<juce::String> subs;
        for (const auto& p : paths)
            if (p.startsWith (prefix))
            {
                ++count;
                const auto rest = p.substring (prefix.length());
                if (rest.containsChar (juce::File::getSeparatorChar()))
                    subs.insert (rest.upToFirstOccurrenceOf (sep, false, false));
            }
        CHECK (page.countIn (juce::File (folder)) == count);
        const auto found = page.childrenOf (juce::File (folder));
        CHECK (found.size() == (int) subs.size());
        for (const auto& s : subs)
            CHECK (found.contains (s));
        // (asked again: the kept answer)
        CHECK (page.countIn (juce::File (folder)) == count);
    }
    // "Pack 1" doesn't take in "Pack 10".
    const auto toontrack = library.packs.getChildFile ("Toontrack");
    CHECK (page.countIn (toontrack.getChildFile ("Pack 1")) == 500);
    // A star marked elsewhere (ARCHITECT's right-click) shows here too.
    studio.setGrooveFolderFavourite (toontrack.getChildFile ("Pack 0"), true);
    page.refresh();
    CHECK (page.getSummary().startsWith ("1 FAVOURITE FOLDER, 500 GROOVES"));
}

TEST_CASE ("0.179.0: the drums options open with the names, the parts made after, the same as before", "[speed179][libraryoptions]")
{
    Library library (3000);
    Studio studio (false);
    studio.setPr7dFolderForTests (library.presets);
    studio.appendSongSection ("Verse", 8);
    ComposerPage page (studio);
    page.setSize (1700, 940);
    page.setLibraryDrums (true);
    page.setSection (DrumSection::verse);
    const auto pool = page.sectionGrooves();
    REQUIRE (pool.size() == 200);
    for (size_t i = 0; i < pool.size(); ++i)
        writeGroove (pool[i].file, 2 + (int) i % 11);

    // The short menu and the browser: names only, nothing made yet.
    page.makeStyleMenu (L::drums);
    for (const auto& o : page.laneOptions (L::drums, false))
        CHECK (o.pending);
    auto browser = page.makeOptionBrowser (L::drums);
    REQUIRE (browser != nullptr);
    CHECK (browser->countPending() == 200);
    CHECK (browser->isMeasured());   // (the FEEL / ENERGY keys there from the start)
    browser->fillAllNow();
    CHECK (browser->countPending() == 0);

    // What it made is what making them all at once gives (and what each groove plays).
    const auto whole = page.laneOptions (L::drums);
    REQUIRE (whole.size() == 200);
    for (size_t i = 0; i < whole.size(); ++i)
    {
        CHECK_FALSE (whole[i].pending);
        auto each = page.getState();
        each.libraryGroove = pool[i].file.getFullPathName();
        each.libraryPick = {};
        each.groovesHeard = {};
        const auto idea = composeLayer (L::drums, each, page.getSpans(), studio.getComposerContext (0.0));
        REQUIRE (whole[i].idea.notes.size() == idea.notes.size());
        for (size_t n = 0; n < idea.notes.size(); ++n)
            CHECK (whole[i].idea.notes[n].pitch == idea.notes[n].pitch);
        CHECK (whole[i].measure.feel == measureOption (idea, studio.getComposerContext (0.0).beatsPerBar, true).feel);
    }
    // A browser made again: already made, nothing pending.
    CHECK (page.makeOptionBrowser (L::drums)->countPending() == 0);
}
