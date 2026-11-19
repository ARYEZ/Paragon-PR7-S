// (0.177.0, the owner: "if it is selecting 100 riffs from my library why wouldn't I literally hear 100 different
// options") While the drums play the library's grooves, the drums lane's options are the section's grooves, each its
// own part; and FIT THE KICK starts off (older saved text reads as off too).

#include <catch2/catch_test_macros.hpp>

#include "engine/Composer.h"
#include "engine/Studio.h"
#include "ui/ComposerPage.h"

#include <set>

using namespace pr7s;

namespace
{
    juce::File writeGroove (const juce::File& file, const std::vector<std::pair<double, int>>& hits)
    {
        juce::MidiMessageSequence track;
        for (const auto& [beat, pitch] : hits)
        {
            track.addEvent (juce::MidiMessage::noteOn (10, pitch, (juce::uint8) 100), beat * 480.0);
            track.addEvent (juce::MidiMessage::noteOff (10, pitch), beat * 480.0 + 120.0);
        }
        track.updateMatchedPairs();
        juce::MidiFile midi;
        midi.setTicksPerQuarterNote (480);
        midi.addTrack (track);
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream out (file);
        midi.writeTo (out);
        return file;
    }

    // PR7-D's presets folder with eight four-bar verse grooves, each with its own kick, five in a Toontrack folder.
    struct Library
    {
        juce::TemporaryFile temp;
        juce::File presets, packs;
        std::vector<juce::File> grooves;

        Library()
        {
            const auto root = temp.getFile();
            root.createDirectory();
            presets = root.getChildFile ("Rigs");
            presets.createDirectory();
            packs = root.getChildFile ("Packs");
            juce::XmlElement index ("GrooveIndex");
            for (int i = 0; i < 8; ++i)
            {
                std::vector<std::pair<double, int>> hits;
                for (int bar = 0; bar < 4; ++bar)
                {
                    const auto b = bar * 4.0;
                    for (int e = 0; e < 8; ++e)
                        hits.push_back ({ b + e * 0.5, 61 });
                    hits.push_back ({ b + 1.0, 38 });
                    hits.push_back ({ b + 3.0, 38 });
                    hits.push_back ({ b, 36 });
                    hits.push_back ({ b + 0.25 * (2 + i), 36 });   // (each groove's own kick)
                }
                const auto folder = i < 5 ? juce::String ("Toontrack/Metal/Verse") : juce::String ("Other/Rock/Verse");
                const auto file = writeGroove (packs.getChildFile (folder + "/Verse " + juce::String (i + 1) + ".mid"), hits);
                grooves.push_back (file);
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

    juce::String notesOf (const LineIdea& idea)
    {
        juce::String s;
        for (const auto& n : idea.notes)
            s << n.pitch << "@" << juce::String (n.beat, 3) << ";";
        return s;
    }

    std::set<int> menuIds (const juce::PopupMenu& menu)
    {
        std::set<int> ids;
        for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
            ids.insert (it.getItem().itemID);
        return ids;
    }
}

TEST_CASE ("0.177.0: the drums lane's options are the section's library grooves, each its own part", "[libraryoptions][composer][library]")
{
    Library library;
    Studio studio (false);
    studio.setPr7dFolderForTests (library.presets);
    ComposerPage page (studio);
    page.setSize (1700, 940);
    page.setLibraryDrums (true);
    page.setSection (DrumSection::verse);
    REQUIRE (page.showsLibraryGrooves (ComposerLayer::drums));
    const auto pool = page.sectionGrooves();
    REQUIRE (pool.size() == 8);

    const auto options = page.laneOptions (ComposerLayer::drums);
    REQUIRE (options.size() == pool.size());
    std::set<juce::String> names, parts;
    auto fromToontrack = 0;
    for (size_t i = 0; i < options.size(); ++i)
    {
        const auto& o = options[i];
        CHECK (o.id == ComposerPage::grooveId + (int) i);
        CHECK (o.name.startsWith (pool[i].name));
        CHECK (o.group == (pool[i].toontrack ? ComposerPage::toontrackGroup : ComposerPage::otherPacksGroup));
        CHECK (o.idea.style == "My library: " + pool[i].name);
        names.insert (o.name);
        parts.insert (notesOf (o.idea));
        fromToontrack += pool[i].toontrack ? 1 : 0;
    }
    CHECK (fromToontrack == 5);
    CHECK (names.size() == options.size());   // (each its own name)
    CHECK (parts.size() == options.size());   // (each a different part: not one groove under every name)
    // The favourites and recent kept apart from the type's options.
    CHECK (page.optionsKeyFor (ComposerLayer::drums).endsWith (":library"));

    // USE IT: that groove kept on the drums, and it's what the lane plays.
    page.useOption (ComposerLayer::drums, ComposerPage::grooveId + 3);
    CHECK (page.getState().libraryGroove == pool[3].file.getFullPathName());
    // HEAR another one, then close without USE IT: back to the one chosen.
    page.tryOption (ComposerLayer::drums, ComposerPage::grooveId + 6);
    CHECK (page.getState().libraryGroove == pool[6].file.getFullPathName());
    page.endTrial (false);
    CHECK (page.getState().libraryGroove == pool[3].file.getFullPathName());

    // The short menu: Automatic and MORE OPTIONS; the section's list's items in the Library Groove submenu.
    const auto ids = menuIds (page.makeStyleMenu (ComposerLayer::drums));
    CHECK (ids.count (ComposerPage::libraryItems + 1) == 1);
    CHECK (ids.count (ComposerPage::moreOptionsItem) == 1);
    CHECK (ids.count (ComposerPage::libraryItems + 200 + 7) == 1);
    page.styleMenuChosen (ComposerLayer::drums, ComposerPage::libraryItems + 200 + 5);
    CHECK (page.getState().libraryGroove == pool[5].file.getFullPathName());
    // The browser opens on the one playing.
    const auto browser = page.makeOptionBrowser (ComposerLayer::drums);
    REQUIRE (browser != nullptr);

    // Library drums off: the type's and the generator's options again.
    page.setLibraryDrums (false);
    CHECK_FALSE (page.showsLibraryGrooves (ComposerLayer::drums));
    for (const auto& o : page.laneOptions (ComposerLayer::drums))
        CHECK (o.id < ComposerPage::grooveId);
}

TEST_CASE ("0.177.0: FIT THE KICK starts off, and saved text from before reads as off", "[libraryoptions][composer]")
{
    ComposerState s;
    CHECK_FALSE (s.fitKicks);
    CHECK_FALSE (ComposerState::fromString (s.toString()).fitKicks);
    s.fitKicks = true;   // (chosen from now on: kept)
    CHECK (ComposerState::fromString (s.toString()).fitKicks);
    // A 0.137.0-0.176.0 text (17 parts, FIT THE KICK on as it started then): off.
    auto old = juce::StringArray::fromTokens (s.toString(), "|", "");
    REQUIRE (old.size() == 18);
    old.remove (17);
    CHECK (old[16] == "1");
    CHECK_FALSE (ComposerState::fromString (old.joinIntoString ("|")).fitKicks);
}
