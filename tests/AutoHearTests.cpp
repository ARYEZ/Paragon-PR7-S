// (0.180.0, the owner: "the groove automatically play when we highlight it ... navigate this window with the up and
// down arrow keys") The option browser hears what's chosen as it's chosen, and Up / Down / Enter move through it.

#include <catch2/catch_test_macros.hpp>

#include "engine/Composer.h"
#include "engine/Studio.h"
#include "ui/ComposerPage.h"
#include "ui/OptionBrowser.h"

using namespace pr7s;

namespace
{
    std::vector<OptionBrowser::Option> threeOptions()
    {
        std::vector<OptionBrowser::Option> out;
        for (int i = 0; i < 3; ++i)
        {
            OptionBrowser::Option o;
            o.id = 10 + i;
            o.name = "OPTION " + juce::String (i);
            o.group = "STYLES";
            out.push_back (o);
        }
        return out;
    }

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
}

TEST_CASE ("0.180.0: Up / Down move through the options and hear each one; Enter uses it", "[autohear][options]")
{
    OptionBrowser browser ("TEST", threeOptions(), 10, {}, "test");
    std::vector<int> heard, used;
    browser.onHear = [&] (int id) { heard.push_back (id); };
    browser.onUse = [&] (int id) { used.push_back (id); };

    // Without auto-hear: the keys move, nothing heard (the type browser).
    browser.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    CHECK (browser.getChosen() == 11);
    CHECK (heard.empty());

    browser.setAutoHear (true);
    browser.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    CHECK (browser.getChosen() == 12);
    browser.keyPressed (juce::KeyPress (juce::KeyPress::downKey));   // (the last: stays, not heard again)
    CHECK (browser.getChosen() == 12);
    browser.keyPressed (juce::KeyPress (juce::KeyPress::upKey));
    CHECK (browser.getChosen() == 11);
    CHECK (heard == std::vector<int> { 12, 11 });
    browser.pick (11);   // (a click on the one already chosen: not heard again)
    CHECK (heard.size() == 2);
    browser.pick (10);
    CHECK (heard.back() == 10);

    browser.keyPressed (juce::KeyPress (juce::KeyPress::returnKey));
    CHECK (used == std::vector<int> { 10 });

    // A search narrows the list: the keys move through what's shown.
    browser.setSearch ("OPTION 2");
    browser.keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    CHECK (browser.getChosen() == 12);

    // A browser that can't be tried (a built block's TYPE list) moves without hearing.
    browser.setSearch ({});
    browser.setHearable (false);
    const auto before = heard.size();
    browser.keyPressed (juce::KeyPress (juce::KeyPress::upKey));
    CHECK (heard.size() == before);
}

TEST_CASE ("0.180.0: in the drums lane's library grooves, Down plays the next groove in time with the song", "[autohear][libraryoptions]")
{
    juce::TemporaryFile temp;
    const auto root = temp.getFile();
    root.createDirectory();
    const auto presets = root.getChildFile ("Rigs"), packs = root.getChildFile ("Packs");
    presets.createDirectory();
    juce::XmlElement index ("GrooveIndex");
    for (int i = 0; i < 4; ++i)
    {
        const auto file = writeGroove (packs.getChildFile ("Toontrack/Metal/Verse/Verse " + juce::String (i + 1) + ".mid"), 2 + i);
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

    Studio studio (false);
    studio.setPr7dFolderForTests (presets);
    ComposerPage page (studio);
    page.setSize (1700, 940);
    page.setLibraryDrums (true);
    page.setSection (DrumSection::verse);
    const auto pool = page.sectionGrooves();
    REQUIRE (pool.size() == 4);
    page.setLibraryGroove (pool[0].file.getFullPathName());

    auto browser = page.makeOptionBrowser (ComposerLayer::drums);
    REQUIRE (browser != nullptr);
    CHECK (browser->isAutoHear());
    CHECK (browser->getChosen() == ComposerPage::grooveId);
    browser->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    CHECK (browser->getChosen() == ComposerPage::grooveId + 1);
    CHECK (page.getState().libraryGroove == pool[1].file.getFullPathName());   // (heard: the lane plays it)
    browser->keyPressed (juce::KeyPress (juce::KeyPress::downKey));
    CHECK (page.getState().libraryGroove == pool[2].file.getFullPathName());
    // Closed without USE IT: back to the one chosen before.
    browser.reset();
    CHECK (page.getState().libraryGroove == pool[0].file.getFullPathName());
    page.stop();
}
