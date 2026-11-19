// (0.178.0, the owner asked) Favourite groove folders: marked on the Hub's GROOVE FOLDERS tab or by right-clicking a
// groove in ARCHITECT's options; a quarter of each section's 200 library grooves come from them.

#include <catch2/catch_test_macros.hpp>

#include "engine/Composer.h"
#include "engine/Studio.h"
#include "ui/ComposerPage.h"
#include "ui/GrooveFoldersPage.h"
#include "ui/Hub.h"

using namespace pr7s;

namespace
{
    juce::File writeGroove (const juce::File& file, int kickAt)
    {
        juce::MidiMessageSequence track;
        for (int bar = 0; bar < 4; ++bar)
            for (const auto [beat, pitch] : std::initializer_list<std::pair<double, int>> { { 0.0, 36 }, { 1.0, 38 }, { 3.0, 38 }, { 0.25 * kickAt, 36 } })
            {
                track.addEvent (juce::MidiMessage::noteOn (10, pitch, (juce::uint8) 100), (bar * 4.0 + beat) * 480.0);
                track.addEvent (juce::MidiMessage::noteOff (10, pitch), (bar * 4.0 + beat) * 480.0 + 120.0);
            }
        track.updateMatchedPairs();
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        midi.addTrack (track);
        file.getParentDirectory().createDirectory();
        juce::FileOutputStream out (file);
        midi.writeTo (out);
        return file;
    }

    // PR7-D's library: Toontrack/Metal/Verse (6), Toontrack/Groove/Verse (4), Other/Rock/Verse (5).
    struct Library
    {
        juce::TemporaryFile temp;
        juce::File presets, packs;

        Library()
        {
            const auto root = temp.getFile();
            root.createDirectory();
            presets = root.getChildFile ("Rigs");
            presets.createDirectory();
            packs = root.getChildFile ("Packs");
            juce::XmlElement index ("GrooveIndex");
            auto n = 0;
            for (const auto& [folder, count] : std::initializer_list<std::pair<const char*, int>> { { "Toontrack/Metal/Verse", 6 },
                                                                                                    { "Toontrack/Groove/Verse", 4 },
                                                                                                    { "Other/Rock/Verse", 5 } })
                for (int i = 0; i < count; ++i)
                {
                    const auto file = writeGroove (packs.getChildFile (juce::String (folder) + "/Verse " + juce::String (++n) + ".mid"), 2 + n % 9);
                    auto* g = index.createNewChildElement ("G");
                    g->setAttribute ("path", file.getFullPathName());
                    g->setAttribute ("stamp", "1|2|1");
                    g->setAttribute ("bpm", 120.0);
                    g->setAttribute ("bpmKnown", 1);
                    g->setAttribute ("num", 4);
                    g->setAttribute ("den", 4);
                    g->setAttribute ("bars", 4);
                    g->setAttribute ("tags", "Genre:Metal|Type:Beat");
                }
            juce::XmlElement settings ("Grooves");
            auto* f = settings.createNewChildElement ("Folder");
            f->setAttribute ("path", packs.getFullPathName());
            f->setAttribute ("map", 1);
            settings.writeTo (presets.getChildFile ("Grooves.xml"));
            index.writeTo (presets.getChildFile ("Groove Index.xml"));
        }
    };
}

TEST_CASE ("0.178.0: the studio keeps the favourite groove folders", "[favourites]")
{
    Studio studio (false);
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("grooves");
    const auto a = root.getChildFile ("A"), b = root.getChildFile ("B");
    CHECK (studio.getGrooveFavouriteFolders().isEmpty());
    const auto before = studio.getChangeVersion();
    studio.setGrooveFolderFavourite (a, true);
    studio.setGrooveFolderFavourite (b, true);
    studio.setGrooveFolderFavourite (a, true);   // (once only)
    CHECK (studio.getGrooveFavouriteFolders().size() == 2);
    CHECK (studio.isGrooveFolderFavourite (a));
    CHECK_FALSE (studio.isGrooveFolderFavourite (root));
    CHECK (studio.getChangeVersion() > before);   // (ARCHITECT composes again)
    studio.setGrooveFolderFavourite (a, false);
    CHECK_FALSE (studio.isGrooveFolderFavourite (a));
    CHECK (studio.getComposerContext (0.0).favouriteFolders == juce::StringArray { b.getFullPathName() });
    studio.setGrooveFavouriteFolders ({ "  " + b.getFullPathName() + juce::File::getSeparatorString() + "  ", "" });
    CHECK (studio.getGrooveFavouriteFolders() == juce::StringArray { b.getFullPathName() });
}

TEST_CASE ("0.178.0: the Hub's GROOVE FOLDERS: the library's folders, their counts, a star to mark one", "[favourites][ui]")
{
    Library library;
    Studio studio (false);
    studio.setPr7dFolderForTests (library.presets);
    Hub hub (studio);
    hub.setSize (1400, 900);
    hub.showGrooveFoldersPage();
    REQUIRE (hub.isShowingGrooveFolders());
    auto* page = hub.getGrooveFoldersPage();
    REQUIRE (page != nullptr);
    CHECK (page->isVisible());
    CHECK (page->countIn (library.packs) == 15);
    CHECK (page->countIn (library.packs.getChildFile ("Toontrack")) == 10);
    CHECK (page->childrenOf (library.packs) == juce::StringArray { "Other", "Toontrack" });
    CHECK (page->childrenOf (library.packs.getChildFile ("Toontrack")) == juce::StringArray { "Groove", "Metal" });
    CHECK (page->getSummary().startsWith ("NO FAVOURITE"));
    const auto groove = library.packs.getChildFile ("Toontrack/Groove");
    page->toggle (groove);
    CHECK (studio.isGrooveFolderFavourite (groove));
    CHECK (page->getSummary() == "1 FAVOURITE FOLDER, 4 GROOVES IN THEM");
    page->toggle (groove);
    CHECK_FALSE (studio.isGrooveFolderFavourite (groove));
    // The tree: the linked folder, opened to show its subfolders.
    auto* top = page->getTree().getRootItem();
    REQUIRE (top != nullptr);
    REQUIRE (top->getNumSubItems() == 1);
    top->getSubItem (0)->setOpen (true);
    CHECK (top->getSubItem (0)->getNumSubItems() == 2);
}

TEST_CASE ("0.178.0: a groove's folder marked from ARCHITECT's options: its grooves under FAVOURITE FOLDERS, Automatic one of them", "[favourites][ui][composer]")
{
    Library library;
    Studio studio (false);
    studio.setPr7dFolderForTests (library.presets);
    ComposerPage page (studio);
    page.setSize (1700, 940);
    page.setLibraryDrums (true);
    page.setSection (DrumSection::verse);
    REQUIRE (page.showsLibraryGrooves (ComposerLayer::drums));
    auto pool = page.sectionGrooves();
    REQUIRE (pool.size() == 15);
    // A groove in Other/Rock: its folder marked.
    int rock = -1;
    for (int i = 0; i < (int) pool.size(); ++i)
        if (pool[(size_t) i].file.getFullPathName().contains ("Rock"))
            rock = i;
    REQUIRE (rock >= 0);
    CHECK (page.favouriteFolderHolding (ComposerPage::grooveId + rock) == juce::File());
    page.markGrooveFolder (ComposerPage::grooveId + rock, true);
    CHECK (studio.isGrooveFolderFavourite (pool[(size_t) rock].file.getParentDirectory()));
    pool = page.sectionGrooves();
    auto inFavourites = 0;
    for (const auto& o : page.laneOptions (ComposerLayer::drums))
        if (o.group == ComposerPage::favouriteFoldersGroup)
        {
            ++inFavourites;
            CHECK (o.description.contains ("Rock"));   // (the full path is in the description; the name has the last folder)
        }
    CHECK (inFavourites == 5);
    // Automatic plays one of them.
    const auto playing = composerLibraryGroove (page.getState(), page.getSpans(), studio.getComposerContext (0.0));
    REQUIRE (playing.has_value());
    CHECK (playing->favourite);
    // Unmarked again from any of its grooves.
    for (int i = 0; i < (int) pool.size(); ++i)
        if (pool[(size_t) i].favourite)
        {
            CHECK (page.favouriteFolderHolding (ComposerPage::grooveId + i) != juce::File());
            page.markGrooveFolder (ComposerPage::grooveId + i, false);
            break;
        }
    CHECK (studio.getGrooveFavouriteFolders().isEmpty());
}
