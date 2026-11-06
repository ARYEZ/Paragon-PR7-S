#include "KeyCommandsPage.h"

#include "engine/Project.h"
#include "ui/ParagonLookAndFeel.h"

namespace
{
    const auto pageColour = juce::Colour (0xff0c0a0d);
    const auto listColour = juce::Colour (0xff120e13);
    juce::Font font (float size) { return ParagonLookAndFeel::textFont (size); }

    void styleKey (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff151217));
        b.setColour (juce::TextButton::textColourOffId, ParagonColours::text);
        b.setColour (juce::TextButton::textColourOnId, ParagonColours::accent);
        b.setWantsKeyboardFocus (false);
    }
}

//==============================================================================
// The commands, by category (and the macros, last), as filtered.
class KeyCommandsPage::CommandList final : public juce::Component,
                                           private juce::ListBoxModel
{
public:
    explicit CommandList (KeyCommandsPage& p) : page (p)
    {
        list.setModel (this);
        list.setRowHeight (24);
        list.setColour (juce::ListBox::backgroundColourId, listColour);
        list.setWantsKeyboardFocus (false);
        addAndMakeVisible (list);
    }

    struct Row { bool header = false; juce::String text, id; };
    std::vector<Row> rows;

    void rebuild()
    {
        rows.clear();
        auto& map = page.map;
        const auto query = page.search.getText().trim();
        juce::String category;
        const auto consider = [&] (const juce::String& id, const juce::String& cat, const juce::String& name)
        {
            const auto keys = map.keysFor (id);
            if (page.searchedKey.isValid())
            {
                if (map.commandFor (page.searchedKey) != id)
                    return;
            }
            else if (query.isNotEmpty() && ! name.containsIgnoreCase (query) && ! cat.containsIgnoreCase (query))
                return;
            switch (page.filter)
            {
                case Filter::assigned:   if (keys.isEmpty()) return; break;
                case Filter::customized: if (! map.isCustomized (id)) return; break;
                case Filter::unassigned: if (! keys.isEmpty()) return; break;
                case Filter::all:        break;
            }
            if (cat != category)
            {
                category = cat;
                rows.push_back ({ true, cat.toUpperCase(), {} });
            }
            rows.push_back ({ false, name, id });
        };
        for (const auto& c : map.getCommands())
            consider (c.id, c.category, c.name);
        for (const auto& m : map.getMacros())
            consider (KeyCommands::Map::macroId (m.name), "Macro", m.name);
        list.updateContent();
        list.repaint();
        showSelected();
    }

    void showSelected()
    {
        for (int i = 0; i < (int) rows.size(); ++i)
            if (! rows[(size_t) i].header && rows[(size_t) i].id == page.selected)
            {
                list.selectRow (i, false, true);
                return;
            }
        list.deselectAllRows();
    }

    int getNumRows() override { return (int) rows.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool isSelected) override
    {
        if (! juce::isPositiveAndBelow (row, (int) rows.size()))
            return;
        const auto& r = rows[(size_t) row];
        const auto area = juce::Rectangle<int> (0, 0, width, height);
        if (r.header)
        {
            g.setColour (juce::Colour (0xff1b1419));
            g.fillRect (area);
            g.setColour (ParagonColours::accent);
            g.setFont (ParagonLookAndFeel::titleFont (11.0f));
            g.drawText (r.text, area.reduced (10, 0), juce::Justification::centredLeft, false);
            return;
        }
        if (isSelected)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.22f));
            g.fillRect (area);
        }
        auto& map = page.map;
        if (map.isCustomized (r.id))
        {
            g.setColour (pr7s::FamilyColours::pink);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ 12.0f, (float) height * 0.5f }));
        }
        g.setColour (ParagonColours::text);
        g.setFont (font (13.0f));
        g.drawText (r.text, area.withTrimmedLeft (22).withTrimmedRight (width / 3), juce::Justification::centredLeft, true);
        juce::StringArray keys;
        for (const auto& key : map.keysFor (r.id))
            keys.add (KeyCommands::describe (key));
        g.setColour (keys.isEmpty() ? ParagonColours::textDim : ParagonColours::accent);
        g.drawText (keys.isEmpty() ? juce::String ("-") : keys.joinIntoString ("   "), area.withTrimmedLeft (width * 2 / 3 - 30).withTrimmedRight (10),
                    juce::Justification::centredRight, true);
    }

    void selectedRowsChanged (int row) override
    {
        if (juce::isPositiveAndBelow (row, (int) rows.size()) && ! rows[(size_t) row].header && rows[(size_t) row].id != page.selected)
            page.selectCommand (rows[(size_t) row].id);
    }

    void resized() override { list.setBounds (getLocalBounds()); }

    KeyCommandsPage& page;
    juce::ListBox list;
};

//==============================================================================
class KeyCommandsPage::MacroList final : public juce::Component,
                                         private juce::ListBoxModel
{
public:
    explicit MacroList (KeyCommandsPage& p) : page (p)
    {
        list.setModel (this);
        list.setRowHeight (22);
        list.setColour (juce::ListBox::backgroundColourId, listColour);
        list.setWantsKeyboardFocus (false);
        addAndMakeVisible (list);
    }
    int getNumRows() override { return (int) page.map.getMacros().size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool isSelected) override
    {
        if (! juce::isPositiveAndBelow (row, getNumRows()))
            return;
        if (isSelected)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.22f));
            g.fillAll();
        }
        const auto& m = page.map.getMacros()[(size_t) row];
        juce::StringArray keys;
        for (const auto& key : page.map.keysFor (KeyCommands::Map::macroId (m.name)))
            keys.add (KeyCommands::describe (key));
        g.setColour (ParagonColours::text);
        g.setFont (font (13.0f));
        g.drawText (m.name + "  (" + juce::String (m.commands.size()) + ")", 10, 0, width - 20, height, juce::Justification::centredLeft, true);
        g.setColour (ParagonColours::accent);
        g.drawText (keys.joinIntoString ("   "), 10, 0, width - 20, height, juce::Justification::centredRight, true);
    }
    void selectedRowsChanged (int row) override
    {
        if (row != page.macroIndex)
            page.selectMacro (row);
    }
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { page.renameMacro.onClick(); }
    void resized() override { list.setBounds (getLocalBounds()); }
    KeyCommandsPage& page;
    juce::ListBox list;
};

class KeyCommandsPage::StepList final : public juce::Component,
                                        private juce::ListBoxModel
{
public:
    explicit StepList (KeyCommandsPage& p) : page (p)
    {
        list.setModel (this);
        list.setRowHeight (22);
        list.setColour (juce::ListBox::backgroundColourId, listColour);
        list.setWantsKeyboardFocus (false);
        addAndMakeVisible (list);
    }
    const juce::StringArray* steps() const
    {
        const auto& macros = page.map.getMacros();
        return juce::isPositiveAndBelow (page.macroIndex, (int) macros.size()) ? &macros[(size_t) page.macroIndex].commands : nullptr;
    }
    int getNumRows() override { return steps() != nullptr ? steps()->size() : 0; }
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool isSelected) override
    {
        const auto* list2 = steps();
        if (list2 == nullptr || ! juce::isPositiveAndBelow (row, list2->size()))
            return;
        if (isSelected)
        {
            g.setColour (ParagonColours::accent.withAlpha (0.22f));
            g.fillAll();
        }
        g.setColour (ParagonColours::textDim);
        g.setFont (font (12.0f));
        g.drawText (juce::String (row + 1) + ".", 6, 0, 24, height, juce::Justification::centredRight, false);
        g.setColour (ParagonColours::text);
        g.drawText (page.map.nameOf ((*list2)[row]), 36, 0, width - 42, height, juce::Justification::centredLeft, true);
    }
    void selectedRowsChanged (int row) override { page.stepIndex = row; }
    void resized() override { list.setBounds (getLocalBounds()); }
    KeyCommandsPage& page;
    juce::ListBox list;
};

//==============================================================================
// CLICK HERE, THEN PRESS A KEY: the next key pressed (Esc: never mind).
class KeyCommandsPage::KeyCatcher final : public juce::Component
{
public:
    std::function<void (const juce::KeyPress&)> onKey;
    bool forSearch = false;

    KeyCatcher() { setWantsKeyboardFocus (true); setMouseClickGrabsKeyboardFocus (true); }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat().reduced (1.0f);
        const auto waiting = hasKeyboardFocus (false);
        g.setColour (waiting ? ParagonColours::accent.withAlpha (0.18f) : juce::Colour (0xff151217));
        g.fillRoundedRectangle (area, 4.0f);
        g.setColour (waiting ? ParagonColours::accent : ParagonColours::panelEdge);
        g.drawRoundedRectangle (area, 4.0f, waiting ? 1.6f : 1.0f);
        g.setColour (waiting ? ParagonColours::text : ParagonColours::textDim);
        g.setFont (ParagonLookAndFeel::titleFont (14.0f));
        g.drawText (waiting ? (forSearch ? juce::String ("PRESS THE KEY TO LOOK FOR (ESC: NEVER MIND)") : juce::String ("PRESS THE KEY (ESC: NEVER MIND)"))
                            : juce::String ("CLICK HERE, THEN PRESS A KEY"),
                    area, juce::Justification::centred, true);
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { forSearch = false; repaint(); }
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key != juce::KeyPress::escapeKey && onKey)
            onKey (key);
        forSearch = false;
        giveAwayKeyboardFocus();
        repaint();
        return true;
    }
};

//==============================================================================
KeyCommandsPage::KeyCommandsPage (KeyCommands::Map& m) : map (m)
{
    commandList = std::make_unique<CommandList> (*this);
    macroList = std::make_unique<MacroList> (*this);
    stepList = std::make_unique<StepList> (*this);
    catcher = std::make_unique<KeyCatcher>();
    addAndMakeVisible (*commandList);
    addAndMakeVisible (*macroList);
    addAndMakeVisible (*stepList);
    addAndMakeVisible (*catcher);

    search.setTextToShowWhenEmpty ("Search: a command's name", ParagonColours::textDim);
    search.onTextChange = [this] { searchedKey = {}; refresh(); };
    addAndMakeVisible (search);

    filterBox.addItemList ({ "All", "Assigned", "Customized", "Unassigned" }, 1);
    filterBox.setSelectedId (1, juce::dontSendNotification);
    filterBox.onChange = [this] { setFilter ((Filter) (filterBox.getSelectedId() - 1)); };
    filterBox.setWantsKeyboardFocus (false);
    addAndMakeVisible (filterBox);

    status.setColour (juce::Label::textColourId, ParagonColours::textDim);
    status.setFont (font (13.0f));
    addAndMakeVisible (status);

    for (auto* b : { &pressKey, &defaults, &clearAll, &presets, &exportList, &reset, &newMacro, &duplicateMacro, &deleteMacro, &renameMacro,
                     &addStep, &removeStep, &stepUp, &stepDown })
    {
        styleKey (*b);
        addAndMakeVisible (*b);
    }
    presets.getProperties().set (ParagonLookAndFeel::menuArrowProperty, true);
    pressKey.setTooltip ("Press Key: then press a key to see which command has it");
    pressKey.onClick = [this] { catcher->forSearch = true; catcher->grabKeyboardFocus(); catcher->repaint(); };
    catcher->onKey = [this] (const juce::KeyPress& key)
    {
        if (catcher->forSearch)
            searchKey (key);
        else
            assignKey (key);
    };
    reset.setTooltip ("Reset Key Command: its keys as they were at first");
    reset.onClick = [this] { if (selected.isNotEmpty()) { map.reset (selected); status.setText ("Reset: " + map.nameOf (selected), juce::dontSendNotification); refresh(); } };
    defaults.setTooltip ("Every command's keys as they were at first (your macros stay)");
    defaults.onClick = [this]
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Defaults",
                                            "Put every key command back as it was at first? Save a preset first to keep these.",
                                            "Reset all", "Cancel", nullptr,
                                            juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<KeyCommandsPage> (this)] (int result)
                                            {
                                                if (safe != nullptr && result == 1) { safe->map.resetAll(); safe->refresh(); }
                                            }));
    };
    clearAll.setTooltip ("Clear All: no command has a key (the defaults too)");
    clearAll.onClick = [this]
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Clear all",
                                            "Take every key away, the default ones too? (DEFAULTS brings them back.)", "Clear all", "Cancel", nullptr,
                                            juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<KeyCommandsPage> (this)] (int result)
                                            {
                                                if (safe != nullptr && result == 1) { safe->map.clearAll(); safe->refresh(); }
                                            }));
    };
    presets.onClick = [this] { showPresetMenu(); };
    exportList.setTooltip ("A page listing every key command (it opens in a web browser)");
    exportList.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export the key commands list", KeyCommands::Map::presetFolder().getChildFile ("PR7-S Key Commands.html"), "*.html");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult() != juce::File())
                                  {
                                      fc.getResult().getParentDirectory().createDirectory();
                                      fc.getResult().replaceWithText (map.toHtml());
                                      status.setText ("Exported: " + fc.getResult().getFullPathName(), juce::dontSendNotification);
                                  }
                              });
    };

    newMacro.onClick = [this] { selectMacro (map.addMacro ("Macro")); renameMacro.onClick(); };
    duplicateMacro.onClick = [this] { selectMacro (map.duplicateMacro (macroIndex)); };
    deleteMacro.onClick = [this]
    {
        if (macroIndex < 0)
            return;
        map.deleteMacro (macroIndex);
        selectMacro (juce::jmin (macroIndex, (int) map.getMacros().size() - 1));
    };
    renameMacro.onClick = [this]
    {
        if (! juce::isPositiveAndBelow (macroIndex, (int) map.getMacros().size()))
            return;
        auto* box = new FamilyAlertWindow ("Macro", "Its name:", juce::MessageBoxIconType::NoIcon);
        box->addTextEditor ("name", map.getMacros()[(size_t) macroIndex].name);
        box->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        box->enterModalState (true, juce::ModalCallbackFunction::create ([box, safe = juce::Component::SafePointer<KeyCommandsPage> (this)] (int result)
        {
            if (safe != nullptr && result == 1)
            {
                safe->map.renameMacro (safe->macroIndex, box->getTextEditorContents ("name"));
                safe->selectMacro (safe->macroIndex);
            }
        }), true);
    };
    addStep.setTooltip ("Add Command: the command chosen on the left, after the step chosen here");
    addStep.onClick = [this]
    {
        if (macroIndex < 0 || selected.isEmpty() || selected == KeyCommands::Map::macroId (map.getMacros()[(size_t) macroIndex].name))
            return;
        map.addToMacro (macroIndex, selected, stepIndex);
        stepIndex = stepIndex >= 0 ? stepIndex + 1 : map.getMacros()[(size_t) macroIndex].commands.size() - 1;
        refresh();
    };
    removeStep.onClick = [this] { map.removeFromMacro (macroIndex, stepIndex); stepIndex = -1; refresh(); };
    stepUp.onClick = [this] { if (stepIndex > 0) { map.moveInMacro (macroIndex, stepIndex, -1); --stepIndex; refresh(); } };
    stepDown.onClick = [this]
    {
        if (macroIndex >= 0 && stepIndex >= 0 && stepIndex + 1 < map.getMacros()[(size_t) macroIndex].commands.size())
        {
            map.moveInMacro (macroIndex, stepIndex, 1);
            ++stepIndex;
            refresh();
        }
    };

    if (! map.getCommands().empty())
        selected = map.getCommands().front().id;
    setSize (preferredWidth, preferredHeight);
    refresh();
}

KeyCommandsPage::~KeyCommandsPage() = default;

void KeyCommandsPage::setSearch (const juce::String& text)
{
    search.setText (text, juce::dontSendNotification);
    searchedKey = {};
    refresh();
}

void KeyCommandsPage::setFilter (Filter f)
{
    filter = f;
    filterBox.setSelectedId ((int) f + 1, juce::dontSendNotification);
    refresh();
}

void KeyCommandsPage::searchKey (const juce::KeyPress& key)
{
    searchedKey = KeyCommands::normalise (key);
    search.setText ({}, juce::dontSendNotification);
    const auto owner = map.commandFor (searchedKey);
    status.setText (KeyCommands::describe (key) + (owner.isEmpty() ? juce::String (": no command has it") : ": " + map.nameOf (owner)),
                    juce::dontSendNotification);
    if (owner.isNotEmpty())
        selected = owner;
    refresh();
}

std::vector<juce::String> KeyCommandsPage::getShownCommands() const
{
    std::vector<juce::String> ids;
    for (const auto& r : commandList->rows)
        if (! r.header)
            ids.push_back (r.id);
    return ids;
}

void KeyCommandsPage::selectCommand (const juce::String& id)
{
    selected = id;
    rebuildKeys();
    commandList->showSelected();
    repaint();
}

void KeyCommandsPage::assignKey (const juce::KeyPress& key)
{
    if (selected.isEmpty())
        return;
    const auto before = map.assign (selected, key, true);
    status.setText (KeyCommands::describe (key) + " now runs " + map.nameOf (selected)
                        + (before.isNotEmpty() ? " (it was on " + map.nameOf (before) + ")" : juce::String()),
                    juce::dontSendNotification);
    refresh();
}

juce::String KeyCommandsPage::getStatus() const
{
    return status.getText();
}

void KeyCommandsPage::selectMacro (int index)
{
    macroIndex = juce::isPositiveAndBelow (index, (int) map.getMacros().size()) ? index : -1;
    stepIndex = -1;
    if (macroIndex >= 0)
        selected = KeyCommands::Map::macroId (map.getMacros()[(size_t) macroIndex].name);
    refresh();
}

void KeyCommandsPage::refresh()
{
    commandList->rebuild();
    macroList->list.updateContent();
    if (macroIndex >= 0)
        macroList->list.selectRow (macroIndex, false, true);
    else
        macroList->list.deselectAllRows();
    macroList->list.repaint();
    stepList->list.updateContent();
    if (stepIndex >= 0)
        stepList->list.selectRow (stepIndex, false, true);
    stepList->list.repaint();
    const auto hasMacro = macroIndex >= 0;
    for (auto* b : { &duplicateMacro, &deleteMacro, &renameMacro, &addStep, &removeStep, &stepUp, &stepDown })
        b->setEnabled (hasMacro);
    rebuildKeys();
    repaint();
}

void KeyCommandsPage::rebuildKeys()
{
    keyRows.clear();
    for (const auto& key : map.keysFor (selected))
    {
        auto row = std::make_unique<juce::TextButton> (KeyCommands::describe (key) + "   REMOVE");
        styleKey (*row);
        row->setTooltip ("Remove Key Command: " + KeyCommands::describe (key) + " off this command");
        row->onClick = [this, key]
        {
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<KeyCommandsPage> (this), key]
            {
                if (safe == nullptr)
                    return;
                safe->map.removeKey (safe->selected, key);
                safe->status.setText (KeyCommands::describe (key) + " removed from " + safe->map.nameOf (safe->selected), juce::dontSendNotification);
                safe->refresh();
            });
        };
        addAndMakeVisible (*row);
        keyRows.push_back (std::move (row));
    }
    reset.setEnabled (map.isCustomized (selected));
    resized();
}

void KeyCommandsPage::savePresetAs (const juce::File& file)
{
    file.getParentDirectory().createDirectory();
    if (const auto xml = map.toXml())
        xml->writeTo (file);
    status.setText ("Preset saved: " + file.getFileNameWithoutExtension(), juce::dontSendNotification);
}

bool KeyCommandsPage::loadPreset (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("KeyCommands"))
    {
        status.setText ("Not a key commands preset: " + file.getFileName(), juce::dontSendNotification);
        return false;
    }
    map.fromXml (*xml);   // (the manual's: it replaces the key commands and macros)
    macroIndex = -1;
    status.setText ("Preset loaded: " + file.getFileNameWithoutExtension(), juce::dontSendNotification);
    refresh();
    return true;
}

void KeyCommandsPage::showPresetMenu()
{
    const auto folder = KeyCommands::Map::presetFolder();
    auto files = folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + KeyCommands::Map::presetExtension);
    files.sort();
    juce::PopupMenu menu;
    menu.addItem (1, "Save Preset...");
    menu.addSeparator();
    for (int i = 0; i < files.size(); ++i)
        menu.addItem (100 + i, files[i].getFileNameWithoutExtension());
    if (files.isEmpty())
        menu.addItem (-1, "(No presets yet)", false);
    menu.addSeparator();
    menu.addItem (2, "Load a Preset File...");
    menu.addItem (3, "Show the Presets Folder");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presets),
                        [safe = juce::Component::SafePointer<KeyCommandsPage> (this), files, folder] (int chosen)
    {
        if (safe == nullptr || chosen == 0)
            return;
        if (chosen >= 100)
            safe->loadPreset (files[chosen - 100]);
        else if (chosen == 3)
        {
            folder.createDirectory();
            folder.revealToUser();
        }
        else if (chosen == 2)
            juce::MessageManager::callAsync ([safe, folder]
            {
                if (safe == nullptr)
                    return;
                safe->chooser = std::make_unique<juce::FileChooser> ("Load key commands", folder, juce::String ("*") + KeyCommands::Map::presetExtension);
                safe->chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                            [safe] (const juce::FileChooser& fc) { if (safe != nullptr && fc.getResult().existsAsFile()) safe->loadPreset (fc.getResult()); });
            });
        else if (chosen == 1)
        {
            auto* box = new FamilyAlertWindow ("Save Preset", "A name for these key commands and macros:", juce::MessageBoxIconType::NoIcon);
            box->addTextEditor ("name", "My keys");
            box->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
            box->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            box->enterModalState (true, juce::ModalCallbackFunction::create ([box, safe, folder] (int result)
            {
                if (safe == nullptr || result != 1)
                    return;
                const auto name = juce::File::createLegalFileName (box->getTextEditorContents ("name").trim());
                if (name.isNotEmpty())
                    safe->savePresetAs (folder.getChildFile (name + KeyCommands::Map::presetExtension));
            }), true);
        }
    });
}

//==============================================================================
void KeyCommandsPage::paint (juce::Graphics& g)
{
    g.fillAll (pageColour);
    g.setColour (ParagonColours::text);
    g.setFont (ParagonLookAndFeel::titleFont (14.0f));
    g.drawText ("KEY COMMANDS", commandArea.getX(), 10, 300, 22, juce::Justification::centredLeft, false);
    g.drawText (selected.startsWith (KeyCommands::Map::macroPrefix) ? "THE MACRO'S KEYS" : "ITS KEYS", keysTitle, juce::Justification::centredLeft, false);
    g.drawText ("MACROS", macrosTitle, juce::Justification::centredLeft, false);
    g.setColour (ParagonColours::accent);
    g.setFont (font (14.0f));
    g.drawText (map.nameOf (selected), keysTitle.translated (0, 22), juce::Justification::centredLeft, true);
    g.setColour (ParagonColours::textDim);
    g.setFont (font (12.0f));
    g.drawFittedText ("A Stream Deck's Hotkey button presses a key combination: give that combination to a command or a macro here. "
                      "A pink dot: changed from the default.",
                      keysArea.withTop (keysArea.getBottom() - 34), juce::Justification::topLeft, 2, 0.9f);
}

void KeyCommandsPage::resized()
{
    auto area = getLocalBounds().reduced (14);
    area.removeFromTop (28);
    auto foot = area.removeFromBottom (30);
    area.removeFromBottom (8);
    commandArea = area.removeFromLeft (area.getWidth() * 56 / 100);
    area.removeFromLeft (16);

    // Left: search, PRESS KEY, the filter; the list.
    auto left = commandArea;
    auto bar = left.removeFromTop (28);
    filterBox.setBounds (bar.removeFromRight (130));
    bar.removeFromRight (6);
    pressKey.setBounds (bar.removeFromRight (96));
    bar.removeFromRight (6);
    search.setBounds (bar);
    left.removeFromTop (6);
    commandList->setBounds (left);

    // Right: the command's keys; the macros.
    auto right = area;
    keysArea = right.removeFromTop (right.getHeight() * 45 / 100);
    auto keys = keysArea;
    keysTitle = keys.removeFromTop (22);
    keys.removeFromTop (24);
    for (auto& row : keyRows)
    {
        row->setBounds (keys.removeFromTop (26).withWidth (juce::jmin (keys.getWidth(), 260)));
        keys.removeFromTop (4);
    }
    auto catcherRow = keys.removeFromTop (34);
    reset.setBounds (catcherRow.removeFromRight (80));
    catcherRow.removeFromRight (6);
    catcher->setBounds (catcherRow);
    right.removeFromTop (6);
    macrosTitle = right.removeFromTop (22);
    auto macroKeys = right.removeFromTop (28);
    const auto keyWidth = (macroKeys.getWidth() - 18) / 4;
    for (auto* b : { &newMacro, &duplicateMacro, &deleteMacro, &renameMacro })
    {
        b->setBounds (macroKeys.removeFromLeft (keyWidth));
        macroKeys.removeFromLeft (6);
    }
    right.removeFromTop (6);
    auto macroRow = right.removeFromTop (right.getHeight() * 40 / 100);
    macroList->setBounds (macroRow);
    right.removeFromTop (6);
    auto stepKeys = right.removeFromBottom (28);
    right.removeFromBottom (6);
    stepList->setBounds (right);
    addStep.setBounds (stepKeys.removeFromLeft (120));
    stepKeys.removeFromLeft (6);
    for (auto* b : { &removeStep, &stepUp, &stepDown })
    {
        b->setBounds (stepKeys.removeFromLeft (80));
        stepKeys.removeFromLeft (6);
    }

    // Foot: DEFAULTS, CLEAR ALL, PRESETS, EXPORT LIST, and what just happened.
    for (auto* b : { &defaults, &clearAll, &presets, &exportList })
    {
        b->setBounds (foot.removeFromLeft (b == &exportList ? 110 : 96));
        foot.removeFromLeft (6);
    }
    foot.removeFromLeft (10);
    status.setBounds (foot);
}

//==============================================================================
KeyCommandsWindow::KeyCommandsWindow (KeyCommands::Map& map)
    : DocumentWindow ("Paragon PR7-S: key commands and macros", pageColour, DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    page = new KeyCommandsPage (map);
    setContentOwned (page, true);
    setResizable (true, false);
    setResizeLimits (860, 560, 4000, 3000);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void KeyCommandsWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}
