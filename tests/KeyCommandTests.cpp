#include <catch2/catch_test_macros.hpp>

#include "engine/Studio.h"
#include "ui/Finish.h"
#include "ui/KeyCommands.h"
#include "ui/KeyCommandsPage.h"
#include "ui/MainView.h"

using namespace pr7s;

namespace
{
    juce::KeyPress keyOf (const juce::String& text)
    {
        const auto key = KeyCommands::parse (text);
        // As the keyboard sends it: a printable key carries its character.
        const auto code = key.getKeyCode();
        const auto printable = code > 32 && code < 127 && ! key.getModifiers().isCommandDown();
        return juce::KeyPress (code, key.getModifiers(), printable ? (juce::juce_wchar) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) code) : 0);
    }
}

TEST_CASE ("Key commands: keys written and read, assigned, taken away, reset, saved (0.11.0)", "[keys]")
{
    // Written as the manual writes them, and read back.
    for (const auto* text : { "Ctrl+Alt+F13", "Shift+H", "Space", "Num 1", "Ctrl+Shift+S", "Alt+F3", "F24", "/" })
        CHECK (KeyCommands::describe (KeyCommands::parse (text)) == text);
    CHECK (KeyCommands::describe (juce::KeyPress ('h', juce::ModifierKeys::shiftModifier, 'H')) == "Shift+H");

    KeyCommands::Map map;
    int a = 0, b = 0, c = 0;
    map.add ({ "Test.A", "Test", "A", { KeyCommands::parse ("H") }, [&] { ++a; } });
    map.add ({ "Test.B", "Test", "B", { KeyCommands::parse ("G") }, [&] { ++b; } });
    map.add ({ "Test.C", "Test", "C", {}, [&] { ++c; } });
    auto changes = 0;
    map.onChange = [&] { ++changes; };

    // Defaults: the built-in handling does them (handle says no, nothing swallowed).
    bool swallowed = true;
    CHECK_FALSE (map.handle (keyOf ("H"), swallowed));
    CHECK_FALSE (swallowed);
    CHECK (map.commandFor (keyOf ("H")) == "Test.A");
    CHECK (a == 0);

    // A new key for C (a Stream Deck's): it runs C.
    CHECK (map.assign ("Test.C", KeyCommands::parse ("Ctrl+Alt+F13")).isEmpty());
    CHECK (map.handle (keyOf ("Ctrl+Alt+F13"), swallowed));
    CHECK (c == 1);
    CHECK (map.isCustomized ("Test.C"));
    CHECK (changes > 0);

    // H given to C: taken from A (a key does one thing), and it says so.
    CHECK (map.assign ("Test.C", KeyCommands::parse ("H")) == "Test.A");
    CHECK (map.keysFor ("Test.A").isEmpty());
    CHECK (map.keysFor ("Test.C").size() == 2);
    CHECK (map.handle (keyOf ("H"), swallowed));
    CHECK (c == 2);
    CHECK (a == 0);

    // G taken off B: it does nothing now (swallowed), not B.
    map.removeKey ("Test.B", KeyCommands::parse ("G"));
    CHECK_FALSE (map.handle (keyOf ("G"), swallowed));
    CHECK (swallowed);

    // Reset A: H is A's again (and off C).
    map.reset ("Test.A");
    CHECK (map.commandFor (keyOf ("H")) == "Test.A");
    CHECK (map.keysFor ("Test.C").size() == 1);
    // Defaults: everything as at first.
    map.resetAll();
    CHECK (map.keysFor ("Test.B").size() == 1);
    CHECK (map.keysFor ("Test.C").isEmpty());
    // Clear All: no keys at all, the defaults swallowed.
    map.clearAll();
    CHECK (map.keysFor ("Test.A").isEmpty());
    CHECK_FALSE (map.handle (keyOf ("H"), swallowed));
    CHECK (swallowed);
    map.resetAll();

    // Macros: commands one after another, a key of their own; renamed, its key follows.
    const auto m = map.addMacro ("Both");
    map.addToMacro (m, "Test.A");
    map.addToMacro (m, "Test.B");
    map.addToMacro (m, "Test.C", 0);   // (after the first)
    CHECK (map.getMacros()[0].commands == juce::StringArray { "Test.A", "Test.C", "Test.B" });
    map.assign (KeyCommands::Map::macroId ("Both"), KeyCommands::parse ("Ctrl+Alt+F14"));
    a = b = c = 0;
    CHECK (map.handle (keyOf ("Ctrl+Alt+F14"), swallowed));
    CHECK ((a == 1 && b == 1 && c == 1));
    map.moveInMacro (m, 2, -1);
    CHECK (map.getMacros()[0].commands[1] == "Test.B");
    map.renameMacro (m, "All three");
    CHECK (map.commandFor (keyOf ("Ctrl+Alt+F14")) == KeyCommands::Map::macroId ("All three"));
    // A macro running itself stops (it doesn't run for ever).
    map.addToMacro (m, KeyCommands::Map::macroId ("All three"));
    a = 0;
    CHECK (map.run (KeyCommands::Map::macroId ("All three")));
    CHECK (a > 0);
    CHECK (a < 10);
    const auto copy = map.duplicateMacro (m);
    CHECK (map.getMacros()[(size_t) copy].name == "All three copy");

    // Saved and read back (the settings file, a preset); the HTML list.
    const auto xml = map.toXml();
    KeyCommands::Map other;
    int c2 = 0;
    other.add ({ "Test.A", "Test", "A", { KeyCommands::parse ("H") }, [] {} });
    other.add ({ "Test.B", "Test", "B", { KeyCommands::parse ("G") }, [] {} });
    other.add ({ "Test.C", "Test", "C", {}, [&] { ++c2; } });
    other.fromXml (*xml);
    CHECK (other.getMacros().size() == 2);
    CHECK (other.commandFor (keyOf ("Ctrl+Alt+F14")) == KeyCommands::Map::macroId ("All three"));
    const auto html = map.toHtml();
    CHECK (html.contains ("<h2>Test</h2>"));
    CHECK (html.contains ("Ctrl+Alt+F14"));
    map.deleteMacro (0);
    CHECK (map.getMacros().size() == 1);
    CHECK_FALSE (map.getMacros()[0].commands.contains (KeyCommands::Map::macroId ("All three")));
}

TEST_CASE ("Key commands in the studio: a key of the owner's runs a command, a macro runs several, a default taken away does nothing (0.11.0)", "[keys][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    view.setSize (1500, 900);
    auto& map = view.getKeyMap();

    // Every command the studio has, with the defaults it always had.
    CHECK (map.find ("Transport.Click") != nullptr);
    CHECK (map.keysFor ("Transport.Click").size() == 1);
    CHECK (map.find ("Tool.Draw") != nullptr);
    CHECK (map.commandFor (keyOf ("8")) == "Tool.Draw");
    CHECK (map.commandFor (keyOf ("Ctrl+Shift+S")) == "File.SaveAs");
    CHECK (map.commandFor (keyOf ("F3")) == "Window.Mixer");
    for (const auto& command : map.getCommands())
    {
        CHECK (command.action != nullptr);
        CHECK (command.category.isNotEmpty());
    }

    // The default key works as before.
    CHECK_FALSE (studio.getProject().click);
    CHECK (view.keyPressed (keyOf ("C")));
    CHECK (studio.getProject().click);

    // A Stream Deck's key on Click: it toggles it back.
    map.assign ("Transport.Click", KeyCommands::parse ("Ctrl+Alt+F13"));
    CHECK (view.keyPressed (keyOf ("Ctrl+Alt+F13")));
    CHECK_FALSE (studio.getProject().click);
    // (C still does it too: an additional key.)
    CHECK (view.keyPressed (keyOf ("C")));
    CHECK (studio.getProject().click);

    // C taken away: nothing happens.
    map.removeKey ("Transport.Click", KeyCommands::parse ("C"));
    CHECK (view.keyPressed (keyOf ("C")));
    CHECK (studio.getProject().click);

    // A command with no key at first: Add Tempo Track, on a key.
    map.assign ("Project.AddTempo", KeyCommands::parse ("Ctrl+Alt+T"));
    view.keyPressed (keyOf ("Ctrl+Alt+T"));
    CHECK (studio.getProject().tempoTrackShown);

    // A macro: Cycle on, then Click on, on one key.
    const auto m = map.addMacro ("Practice");
    map.addToMacro (m, "Transport.Cycle");
    map.addToMacro (m, "Transport.Click");
    map.assign (KeyCommands::Map::macroId ("Practice"), KeyCommands::parse ("Ctrl+Alt+F15"));
    const auto cycleBefore = studio.getProject().loopOn;
    const auto clickBefore = studio.getProject().click;
    CHECK (view.keyPressed (keyOf ("Ctrl+Alt+F15")));
    CHECK (studio.getProject().loopOn != cycleBefore);
    CHECK (studio.getProject().click != clickBefore);

    // A tool on a new key.
    map.assign ("Tool.Draw", KeyCommands::parse ("Ctrl+Alt+D"));
    view.keyPressed (keyOf ("Ctrl+Alt+D"));
    CHECK (view.getArrangeView().getTool() == ArrangeView::Tool::draw);
}

TEST_CASE ("The Key Commands window: search, PRESS KEY, filters, assigning (it says what had the key), macros (0.11.0)", "[keys][ui]")
{
    Finish::set (Finish::Type::pearlWhite, false);
    Studio studio (false);
    MainView view (studio);
    auto& map = view.getKeyMap();
    KeyCommandsPage page (map);
    page.setSize (KeyCommandsPage::preferredWidth, KeyCommandsPage::preferredHeight);

    const auto all = page.getShownCommands().size();
    CHECK (all == map.getCommands().size());
    page.setSearch ("zoom");
    for (const auto& id : page.getShownCommands())
        CHECK (map.nameOf (id).containsIgnoreCase ("zoom"));
    page.setSearch ({});
    page.searchKey (keyOf ("F3"));
    REQUIRE (page.getShownCommands().size() == 1);
    CHECK (page.getShownCommands()[0] == "Window.Mixer");
    CHECK (page.getStatus().contains ("Open/Close Mixer"));
    page.setSearch ({});
    page.setFilter (KeyCommandsPage::Filter::unassigned);
    for (const auto& id : page.getShownCommands())
        CHECK (map.keysFor (id).isEmpty());
    page.setFilter (KeyCommandsPage::Filter::customized);
    CHECK (page.getShownCommands().empty());

    // Assigning: the key catcher's key to the chosen command; what had it is said.
    page.setFilter (KeyCommandsPage::Filter::all);
    page.selectCommand ("Transport.Click");
    page.assignKey (keyOf ("H"));
    CHECK (map.commandFor (keyOf ("H")) == "Transport.Click");
    CHECK (page.getStatus().contains ("Zoom In"));
    page.setFilter (KeyCommandsPage::Filter::customized);
    CHECK (page.getShownCommands().size() == 2);   // Click, and Zoom In (its H gone)

    // A macro made here shows in the list (the Macro category) and takes a key.
    page.setFilter (KeyCommandsPage::Filter::all);
    page.selectMacro (map.addMacro ("Mix"));
    CHECK (page.getSelectedCommand() == KeyCommands::Map::macroId ("Mix"));
    page.assignKey (keyOf ("Ctrl+Alt+F16"));
    CHECK (map.commandFor (keyOf ("Ctrl+Alt+F16")) == KeyCommands::Map::macroId ("Mix"));
    CHECK (page.getShownCommands().back() == KeyCommands::Map::macroId ("Mix"));

    // Presets: saved, changed, loaded back.
    const auto file = juce::File::createTempFile (KeyCommands::Map::presetExtension);
    page.savePresetAs (file);
    map.resetAll();
    CHECK (map.commandFor (keyOf ("H")) == "Zoom.In");
    CHECK (page.loadPreset (file));
    CHECK (map.commandFor (keyOf ("H")) == "Transport.Click");
    file.deleteFile();

    // Its window opens from KEYS.
    view.openKeyCommands();
    REQUIRE (view.getKeyCommandsWindow() != nullptr);
}
