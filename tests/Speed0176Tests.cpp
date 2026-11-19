// (0.176.0, the owner: "still a decent amount of lag opening and closing the architect window") PR7-D's grooves labelled
// once per library read (not on every tempo, style or bar change), and the Architect's window kept hidden between
// openings instead of built and taken down each time.

#include <catch2/catch_test_macros.hpp>

#include "engine/DrumIdeas.h"
#include "engine/Studio.h"
#include "ui/ComposerPage.h"
#include "ui/MainView.h"

using namespace pr7s;

namespace
{
    // Grooves with ties (the same name, the same score) and every kind of word the scores read.
    Pr7dLibrary library()
    {
        Pr7dLibrary lib;
        lib.found = true;
        const char* folders[] { "Toontrack/Metal Machine/Verse", "Packs/Rock Pop/Chorus", "Packs/Thrash Speed/Fills", "Djent Core/Breakdown",
                                "Prog Power/Bridge", "Jazz Brush/Intro", "Groove Pantera/Outro", "Nu Metal Bounce/Solo" };
        for (int i = 0; i < 400; ++i)
        {
            Pr7dGroove g;
            const auto folder = juce::String (folders[i % 8]);
            const auto name = (i % 5 == 0 ? juce::String ("Groove") : "Beat " + juce::String (i % 37)) + (i % 3 == 0 ? " Fill" : "");
            g.file = juce::File ("/library").getChildFile (folder).getChildFile (name + " " + juce::String (i) + ".mid");
            g.name = name;   // (names repeat: the order of ties counts)
            g.folder = folder.fromLastOccurrenceOf ("/", false, false);
            g.bpm = i % 7 == 0 ? 0.0 : 70.0 + (i * 13) % 140;
            g.numerator = i % 11 == 0 ? 3 : 4;
            g.denominator = 4;
            g.bars = 1 + i % 8;
            g.tags = juce::StringArray { "Genre:" + juce::String (i % 2 == 0 ? "Metal" : "Rock"), juce::String ("Type:") + (i % 9 == 0 ? "Fill" : "Beat") };
            g.layout = i % 3;
            lib.grooves.push_back (g);
        }
        return lib;
    }

    bool same (const std::vector<LibraryGroove>& a, const std::vector<LibraryGroove>& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].file != b[i].file || a[i].score != b[i].score || a[i].sectionMask != b[i].sectionMask || a[i].fill != b[i].fill
                || a[i].toontrack != b[i].toontrack || a[i].excluded != b[i].excluded || a[i].bars != b[i].bars || a[i].layout != b[i].layout
                || a[i].tags != b[i].tags)
                return false;
        return true;
    }
}

TEST_CASE ("0.176.0: PR7-D's grooves from the list labelled once are the ones found the old way, in the same order", "[speed176][library]")
{
    const auto lib = library();
    const auto prepared = preparePr7dGrooves (lib);
    REQUIRE (prepared->grooves.size() == lib.grooves.size());
    for (const auto style : guitarStyles())
        for (const auto bpm : { 0.0, 92.0, 120.0, 187.5 })
            for (const auto bar : { 0.0, 4.0, 3.0 })
            {
                INFO ("style " << guitarStyleName (style) << " bpm " << bpm << " bar " << bar);
                CHECK (same (findPr7dGrooves (*prepared, style, bpm, bar), findPr7dGrooves (lib, style, bpm, bar)));
            }
}

TEST_CASE ("0.176.0: the Architect's window is hidden when closed and shown again, not built again", "[speed176][ui][composer]")
{
    Studio studio (false);
    MainView view (studio);
    view.setSize (1600, 1000);
    view.openComposer();
    auto* first = view.getComposerWindow();
    REQUIRE (first != nullptr);
    CHECK (first->isVisible());
    first->closeButtonPressed();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    REQUIRE (view.getComposerWindow() == first);   // (kept)
    CHECK_FALSE (first->isVisible());
    view.openComposer();
    CHECK (view.getComposerWindow() == first);
    CHECK (first->isVisible());
    // Opened on a block when the song has a SECTIONS row (as a fresh window was).
    first->closeButtonPressed();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    REQUIRE (studio.appendSongSection ("Verse", 4) >= 0);
    view.openComposer();
    CHECK (first->getPage().getTargetPlace().has_value());
    first->closeButtonPressed();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}
